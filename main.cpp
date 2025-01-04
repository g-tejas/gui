#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

#include <chrono>
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <zlib.h>

// Base64 encoding table
static const uint8_t base64enc_tab[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int base64_encode(size_t in_len, const uint8_t *in, size_t out_len, char *out) {
    int ii, io;
    uint_least32_t v;
    int rem;

    for (io = 0, ii = 0, v = 0, rem = 0; ii < in_len; ii++) {
        uint8_t ch = in[ii];
        v = (v << 8) | ch;
        rem += 8;
        while (rem >= 6) {
            rem -= 6;
            if (io >= out_len)
                return -1; /* truncation is failure */
            out[io++] = base64enc_tab[(v >> rem) & 63];
        }
    }
    if (rem) {
        v <<= (6 - rem);
        if (io >= out_len)
            return -1; /* truncation is failure */
        out[io++] = base64enc_tab[v & 63];
    }
    while (io & 3) {
        if (io >= out_len)
            return -1; /* truncation is failure */
        out[io++] = '=';
    }
    if (io >= out_len)
        return -1; /* no room for null terminator */
    out[io] = 0;
    return io;
}

// GUI Application class that handles ImGui logic
class GuiApp {
protected:
    float someFloat = 0.0f;
    int clickCount = 0;

public:
    virtual ~GuiApp() = default;

    // Render ImGui UI elements
    void renderUI() {
        ImGui::Begin("Hello, Kitty!");
        ImGui::Text("This is rendered in the terminal!");
        ImGui::SliderFloat("Float", &someFloat, 0.0f, 1.0f);
        if (ImGui::Button("Click Me!")) {
            clickCount++;
        }
        ImGui::Text("Button clicked %d times", clickCount);
        ImGui::End();
    }
};

// Native window renderer
class NativeRenderer : public GuiApp {
private:
    GLFWwindow *window;
    int width, height;
    const char *glsl_version;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

public:
    NativeRenderer(int w, int h) : width(w), height(h) {
        glfwSetErrorCallback([](int error, const char *description) {
            fprintf(stderr, "GLFW Error %d: %s\n", error, description);
        });

        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }

// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
        glsl_version = "#version 100";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
        glsl_version = "#version 300 es";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
        glsl_version = "#version 150";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
        glsl_version = "#version 130";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

        // Create window with graphics context
        window = glfwCreateWindow(width, height, "Dear ImGui Test", nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create window");
        }

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1); // Enable vsync

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init(glsl_version);

        fprintf(stderr, "Native window initialized with GLSL version: %s\n",
                glsl_version);
    }

    void render() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            renderUI(); // Render the same UI

            // Rendering
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                         clear_color.z * clear_color.w, clear_color.w);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
        }
    }

    ~NativeRenderer() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

// Terminal renderer using Kitty protocol
class KittyRenderer : public GuiApp {
private:
    GLFWwindow *window;
    GLuint frameBuffer;
    GLuint textureColorBuffer;
    std::vector<unsigned char> pixelData;
    int frameCounter = 0;
    const size_t CHUNK_SIZE = 4096;
    int width, height;
    const char *glsl_version;

    void initFramebuffer() {
        // Create VAO (required for Core Profile)
        GLuint vao;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glGenFramebuffers(1, &frameBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);

        // Create texture to render to
        glGenTextures(1, &textureColorBuffer);
        glBindTexture(GL_TEXTURE_2D, textureColorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               textureColorBuffer, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("Failed to create framebuffer");
        }

        pixelData.resize(width * height * 4);
    }

    void sendFrame(const uint8_t *data, size_t data_size) {
        // Debug: Print first few pixels
        fprintf(stderr, "First 4 pixels (RGBA): ");
        for (size_t i = 0; i < 16 && i < data_size; i += 4) {
            fprintf(stderr, "(%d,%d,%d,%d) ", data[i], data[i + 1], data[i + 2],
                    data[i + 3]);
        }
        fprintf(stderr, "\n");

        // Compress the data
        fprintf(stderr, "Original size: %zu\n", data_size);

        // Calculate base64 size for compressed data
        size_t base64_size = ((compressed.size() + 2) / 3) * 4;
        std::vector<char> base64_data(base64_size + 1);

        // Encode the compressed data
        int ret = base64_encode(compressed.size(), compressed.data(), base64_size + 1,
                                base64_data.data());
        if (ret < 0) {
            fprintf(stderr, "Base64 encoding failed\n");
            return;
        }

        fprintf(stderr, "Base64 size: %zu bytes\n", base64_size);
        fprintf(stderr, "First 32 bytes of base64: %.32s\n", base64_data.data());

        // Format exactly according to Kitty graphics protocol documentation:
        // a=T: transmit and display
        // f=32: RGBA format
        // s: data size in bytes
        // v=1: version
        // o=z: zlib compression
        // w,h: width and height in pixels
        fprintf(stdout, "\033_Gf=32,a=T,w=%d,h=%d,v=1,o=z,s=%zu;", width,
                height,             // dimensions
                compressed.size()); // size of compressed data
        fwrite(base64_data.data(), 1, base64_size, stdout);
        fprintf(stdout, "\033\\");
        fflush(stdout);

        fprintf(stderr, "Command sent with format: f=32,a=T,w=%d,h=%d,v=1,o=z,s=%zu\n",
                width, height, compressed.size());
    }

public:
    KittyRenderer(int w, int h) : width(w), height(h) {
        glfwSetErrorCallback([](int error, const char *description) {
            fprintf(stderr, "GLFW Error %d: %s\n", error, description);
        });

        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        // Set OpenGL version and profile for macOS
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

        window = glfwCreateWindow(width, height, "Hidden Window", NULL, NULL);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create window");
        }

        glfwMakeContextCurrent(window);

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.DisplaySize = ImVec2(float(width), float(height));
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 150");

        initFramebuffer();
        fprintf(stderr, "Kitty renderer initialized with GLSL version: #version 150\n");
    }

    void render() {
        // Poll events to keep GLFW happy
        glfwPollEvents();

        // Start new frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Create a simple test window that should be very visible
        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::SetNextWindowSize(ImVec2(width - 20, height - 20));
        ImGui::Begin("Test Window", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "RED TEXT");
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "GREEN TEXT");
        ImGui::TextColored(ImVec4(0.0f, 0.0f, 1.0f, 1.0f), "BLUE TEXT");
        ImGui::End();

        // Render to framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
        glViewport(0, 0, width, height);

        // Clear with white background to make it obvious if ImGui renders
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Read pixels
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixelData.data());

        // Debug: Print some pixel values from different areas
        fprintf(stderr, "Pixel samples:\n");
        for (int y = 0; y < height; y += height / 4) {
            for (int x = 0; x < width; x += width / 4) {
                int idx = (y * width + x) * 4;
                fprintf(stderr, "Pixel at (%d,%d): (%d,%d,%d,%d)\n", x, y, pixelData[idx],
                        pixelData[idx + 1], pixelData[idx + 2], pixelData[idx + 3]);
            }
        }

        // Send frame
        sendFrame(pixelData.data(), pixelData.size());
        frameCounter++;
    }

    ~KittyRenderer() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glDeleteFramebuffers(1, &frameBuffer);
        glDeleteTextures(1, &textureColorBuffer);
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main(int argc, char *argv[]) {
    try {
        bool use_native = (argc > 1 && std::string(argv[1]) == "--native");

        // Use smaller dimensions for terminal rendering
        const int NATIVE_WIDTH = 800;
        const int NATIVE_HEIGHT = 600;
        const int TERM_WIDTH = 200;
        const int TERM_HEIGHT = 150;

        if (use_native) {
            NativeRenderer renderer(NATIVE_WIDTH, NATIVE_HEIGHT);
            renderer.render(); // Will run until window is closed
        } else {
            KittyRenderer renderer(TERM_WIDTH, TERM_HEIGHT);
            // Render just one frame for testing
            fprintf(stderr, "Rendering one test frame...\n");
            renderer.render();
            fprintf(stderr, "Frame rendered, waiting...\n");
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    } catch (const std::exception &e) {
        fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }

    return 0;
}