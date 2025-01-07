#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <OpenGL/glext.h>
#include <cstdio>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <OpenGL/gl.h>
#include <iostream>
#include <kgp.hpp>
#include <sys/ioctl.h>
#include <zlib.h>

void catch_sigint(int) {
    restore_terminal();
    std::cout << "\n"; // Add newline after terminal restore
    std::exit(0);
}

class GUI {
public:
    static constexpr int CELL_WIDTH = 24;
    static constexpr int CELL_HEIGHT = 48;
    static constexpr int PADDING = 4;

    GUI(int w, int h) : m_width(w), m_height(h) {
        if (!glfwGetCurrentContext()) {
            std::cerr << "No OpenGL context found\n";
            std::exit(EXIT_FAILURE);
        }

        // initialize frame buffers
        glGenFramebuffers(1, &m_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

        // init render buffers
        glGenRenderbuffers(1, &m_rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, m_width, m_height);

        // attach render buffer to frame buffer
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  m_rbo);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Framebuffer is not complete!\n";
            std::exit(EXIT_FAILURE);
        }

        glViewport(0, 0, m_width, m_height);
    }

    ~GUI() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0); // reset to default frame buffer

        glDeleteRenderbuffers(1, &m_rbo);
        glDeleteFramebuffers(1, &m_fbo);
    }

    auto frame() const -> void {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Set next window to be fullscreen
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(m_width, m_height));
        ImGui::Begin("Demo", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImDrawList *draw_list = ImGui::GetWindowDrawList();

        // Draw vertical lines at cell boundaries (every 24 pixels)
        for (int x = 0; x < m_width; x += 24) {
            draw_list->AddLine(ImVec2(x, 0), ImVec2(x, m_height),
                               IM_COL32(255, 255, 255, 64) // dimmer lines
            );
            // Add column number every 5 columns
            if (x % (24 * 5) == 0) {
                char buf[32];
                snprintf(buf, sizeof(buf), "c%d", x / 24);
                draw_list->AddText(ImVec2(x + 2, 5), IM_COL32(255, 255, 255, 255), buf);
            }
        }

        // Draw horizontal lines at cell boundaries (every 48 pixels)
        for (int y = 0; y < m_height; y += 48) {
            draw_list->AddLine(ImVec2(0, y), ImVec2(m_width, y),
                               IM_COL32(255, 255, 255, 64) // dimmer lines
            );
            // Add row number
            char buf[32];
            snprintf(buf, sizeof(buf), "r%d", y / 48);
            draw_list->AddText(ImVec2(5, y + 2), IM_COL32(255, 255, 255, 255), buf);
        }

        // Draw bright markers at the corners
        const float marker_size = 48.0f; // Make markers one cell high
        // Top-left
        draw_list->AddLine(ImVec2(0, 0), ImVec2(marker_size, 0), IM_COL32(255, 0, 0, 255),
                           3.0f);
        draw_list->AddLine(ImVec2(0, 0), ImVec2(0, marker_size), IM_COL32(255, 0, 0, 255),
                           3.0f);
        draw_list->AddText(ImVec2(5, 5), IM_COL32(255, 0, 0, 255), "TL");

        // Top-right
        draw_list->AddLine(ImVec2(m_width, 0), ImVec2(m_width - marker_size, 0),
                           IM_COL32(0, 255, 0, 255), 3.0f);
        draw_list->AddLine(ImVec2(m_width, 0), ImVec2(m_width, marker_size),
                           IM_COL32(0, 255, 0, 255), 3.0f);
        draw_list->AddText(ImVec2(m_width - 24, 5), IM_COL32(0, 255, 0, 255), "TR");

        // Bottom-left
        draw_list->AddLine(ImVec2(0, m_height), ImVec2(marker_size, m_height),
                           IM_COL32(0, 0, 255, 255), 3.0f);
        draw_list->AddLine(ImVec2(0, m_height), ImVec2(0, m_height - marker_size),
                           IM_COL32(0, 0, 255, 255), 3.0f);
        draw_list->AddText(ImVec2(5, m_height - 24), IM_COL32(0, 0, 255, 255), "BL");

        // Bottom-right
        draw_list->AddLine(ImVec2(m_width, m_height),
                           ImVec2(m_width - marker_size, m_height),
                           IM_COL32(255, 255, 0, 255), 3.0f);
        draw_list->AddLine(ImVec2(m_width, m_height),
                           ImVec2(m_width, m_height - marker_size),
                           IM_COL32(255, 255, 0, 255), 3.0f);
        draw_list->AddText(ImVec2(m_width - 24, m_height - 24),
                           IM_COL32(255, 255, 0, 255), "BR");

        // Display dimensions in cells
        char dim_buf[64];
        snprintf(dim_buf, sizeof(dim_buf), "Grid: %dx%d cells (%dx%d px)",
                 m_width / GUI::CELL_WIDTH, m_height / GUI::CELL_HEIGHT, m_width,
                 m_height);
        // Move text up from center to ensure visibility
        ImGui::SetCursorPos(ImVec2(m_width / 2 - 150, m_height / 3));
        ImGui::Text("%s", dim_buf);

        ImGui::End();

        this->render();
    }

    auto render() const -> void {
        ImGui::Render();

        glClearColor(CLEAR_COLOR.x * CLEAR_COLOR.w, CLEAR_COLOR.y * CLEAR_COLOR.w,
                     CLEAR_COLOR.z * CLEAR_COLOR.w, CLEAR_COLOR.w);

        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    [[nodiscard]] auto get_pixel_data() const -> std::vector<uint8_t> {
        std::vector<uint8_t> data(m_width * m_height * 4);

        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

        // Check framebuffer status
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Framebuffer not complete when reading pixels! Status: "
                      << status << std::endl;
        }

        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glPixelStorei(GL_PACK_ALIGNMENT, 1); // Ensure proper byte alignment
        glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, data.data());

        // Check if we have any non-zero pixels
        bool all_zero = true;
        for (size_t i = 0; i < data.size() && i < 1000; ++i) {
            if (data[i] != 0) {
                all_zero = false;
                break;
            }
        }

        // Flip pixels vertically
        std::vector<uint8_t> flipped(m_width * m_height * 4);
        for (int y = 0; y < m_height; y++) {
            const int src_row = y;
            const int dst_row = m_height - 1 - y;
            std::memcpy(flipped.data() + dst_row * m_width * 4,
                        data.data() + src_row * m_width * 4, m_width * 4);
        }

        return flipped;
    }

private:
    GLuint m_fbo;
    GLuint m_rbo;
    int m_width;
    int m_height;

    const ImVec4 CLEAR_COLOR = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
};

static void glfw_error_callback(int error, const char *description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Main code
int main(int, char **) {
    signal(SIGINT, catch_sigint);
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    setup_terminal();

    // Decide GL+GLSL versions
    // GL 3.2 + GLSL 150
    const char *glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // Required on Mac

    // Calculate dimensions from grid size
    // Grid: 159 columns x 42 rows
    // Cell size: 24x48 pixels
    int width = 159 * GUI::CELL_WIDTH;  // 159 columns * 24px width
    int height = 42 * GUI::CELL_HEIGHT; // 42 rows * 48px height

    // Create window with graphics context
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow *window =
        glfwCreateWindow(width, height, "headless gui", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    GUI gui(width, height);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        gui.frame();

        std::vector<uint8_t> pixels = gui.get_pixel_data();

        // Scale down dimensions for terminal display
        // Try using half the size first
        int display_width = width / 2;
        int display_height = height / 2;

        // Compress the pixel data using zlib
        uLongf compressed_size = compressBound(pixels.size());
        std::vector<uint8_t> compressed_data(compressed_size);

        if (compress2(compressed_data.data(), &compressed_size, pixels.data(),
                      pixels.size(), Z_BEST_COMPRESSION) != Z_OK) {
            std::cerr << "Failed to compress pixel data" << std::endl;
            break;
        }
        compressed_data.resize(compressed_size);
        std::cout << "width: " << width << ", height: " << height << std::endl;

        // Format: a=T (transmit+display), o=z (zlib compression), f=32 (RGBA), s=width, v=height
        std::string cmd =
            "a=T,o=z,f=32,s=" + std::to_string(width) + ",v=" + std::to_string(height);
        kitty_send_command(cmd, compressed_data.data(), compressed_data.size());

        // Move cursor back to top-left after each frame
        std::cout << "\x1B[H" << std::flush;

        // Small sleep to prevent overwhelming the terminal
        glfwWaitEventsTimeout(0.016); // roughly 60 FPS
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    restore_terminal();

    return 0;
}
