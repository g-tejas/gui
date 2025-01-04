#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION

// Define this to use kitty graphics protocol instead of native window
#define USE_KITTY_PROTOCOL

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif

#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <cstring>
#include <iostream>
#include <vector>
#include <zlib.h>
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
#include "kgp.hpp"

static void glfw_error_callback(int error, const char *description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Shader sources
const char *vertexShaderSource = R"(
    #version 150
    in vec2 position;
    void main() {
        gl_Position = vec4(position, 0.0, 1.0);
    }
)";

const char *fragmentShaderSource = R"(
    #version 150
    out vec4 fragColor;
    void main() {
        fragColor = vec4(1.0, 0.0, 0.0, 1.0);  // Bright red
    }
)";

int main(int, char **) {
#ifdef USE_KITTY_PROTOCOL
    setup_terminal();

    // Get terminal size
    struct winsize sz;
    ioctl(0, TIOCGWINSZ, &sz);
    int display_w = 800;
    int display_h = 600;

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // GL 3.2 + GLSL 150
    const char *glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    // Create an invisible window
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow *window =
        glfwCreateWindow(display_w, display_h, "Hidden OpenGL Window", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        restore_terminal();
        return 1;
    }
    glfwMakeContextCurrent(window);

    // Setup framebuffer
    GLuint fb, rb, tex;
    glGenFramebuffers(1, &fb);
    glGenRenderbuffers(1, &rb);
    glGenTextures(1, &tex);

    glBindFramebuffer(GL_FRAMEBUFFER, fb);

    // Setup depth renderbuffer
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, display_w, display_h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rb);

    // Setup texture
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, display_w, display_h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    // Check framebuffer status
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer is not complete! Status: " << status << std::endl;
        glfwTerminate();
        restore_terminal();
        return 1;
    }

    // Create and compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    // Check vertex shader compilation
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "Vertex shader compilation failed:\n" << infoLog << std::endl;
        return 1;
    }

    // Create and compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    // Check fragment shader compilation
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "Fragment shader compilation failed:\n" << infoLog << std::endl;
        return 1;
    }

    // Create shader program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Check program linking
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "Shader program linking failed:\n" << infoLog << std::endl;
        return 1;
    }

    // Triangle vertices
    float vertices[] = {
        -0.5f, -0.5f, // Bottom left
        0.5f,  -0.5f, // Bottom right
        0.0f,  0.5f   // Top
    };

    // Create VAO and VBO
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    // Bind VAO first
    glBindVertexArray(VAO);

    // Bind and set VBO
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Set vertex attribute
    GLint posAttrib = glGetAttribLocation(shaderProgram, "position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);

    std::vector<unsigned char> pixelData(display_w * display_h * 4);

    // Debug: Print OpenGL info
    // std::cerr << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    // std::cerr << "OpenGL Vendor: " << glGetString(GL_VENDOR) << std::endl;
    // std::cerr << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
    // std::cerr << "Display size: " << display_w << "x" << display_h << std::endl;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Bind our framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        glViewport(0, 0, display_w, display_h);

        // Clear the framebuffer
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw triangle
        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // Make sure rendering is complete
        glFinish();

        // Read pixels from framebuffer
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, display_w, display_h, GL_RGBA, GL_UNSIGNED_BYTE,
                     pixelData.data());

        // Flip the image vertically
        std::vector<unsigned char> flippedData(display_w * display_h * 4);
        for (int y = 0; y < display_h; y++) {
            memcpy(&flippedData[y * display_w * 4],
                   &pixelData[(display_h - 1 - y) * display_w * 4], display_w * 4);
        }

        // Debug: Save frame as PNG
        static int frame_count = 0;
        std::string filename = "frame_" + std::to_string(frame_count++) + ".png";
        stbi_write_png(filename.c_str(), display_w, display_h, 4, flippedData.data(),
                       display_w * 4);

        // Send frame using kitty protocol
        std::string cmd =
            "a=T,f=32,s=" + std::to_string(display_w) + ",v=" + std::to_string(display_h);
        kitty_send_command(cmd, flippedData.data(), flippedData.size());

        // Move cursor to top-left after sending frame
        std::cout << CSI << "H" << std::flush;
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glDeleteShader(fragmentShader);
    glDeleteShader(vertexShader);
    glDeleteFramebuffers(1, &fb);
    glDeleteRenderbuffers(1, &rb);
    glDeleteTextures(1, &tex);

    glfwDestroyWindow(window);
    glfwTerminate();
    restore_terminal();

#endif
    return 0;
}