#include <cstdio>
#include <optional>
#include <string>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>  // drags in the system OpenGL headers

#ifdef _WIN32
#include <cstdlib>
#endif

#include "app/config.hpp"
#include "gui/app.hpp"

namespace {

// std::getenv is fatal under MSVC /WX; read env vars the safe way.
std::optional<std::string> env(const char* name) {
#ifdef _WIN32
    char* buf = nullptr;
    std::size_t len = 0;
    if (_dupenv_s(&buf, &len, name) != 0 || buf == nullptr) {
        return std::nullopt;
    }
    std::string value(buf);
    std::free(buf);
    return value.empty() ? std::nullopt : std::optional<std::string>(value);
#else
    const char* v = std::getenv(name);
    return (v != nullptr && v[0] != '\0') ? std::optional<std::string>(v) : std::nullopt;
#endif
}

std::string db_path(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--db") {
            return argv[i + 1];
        }
    }
    if (auto e = env("PFDB_DATABASE")) {
        return *e;
    }
    return "pfdb.db";
}

void glfw_error(int error, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

#ifndef PFDB_VERSION
#define PFDB_VERSION "0.0.0-dev"
#endif

}  // namespace

int main(int argc, char** argv) {
    // Answer --version without opening a window; the self-update sanity check
    // runs `pfdb-gui --version` on a freshly downloaded build.
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--version") {
            std::printf("pfdb-gui %s\n", PFDB_VERSION);
            return 0;
        }
    }

    glfwSetErrorCallback(glfw_error);
    if (glfwInit() == 0) {
        std::fprintf(stderr, "pfdb-gui: failed to initialize GLFW\n");
        return 1;
    }

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 800, "PFDB", nullptr, nullptr);
    if (window == nullptr) {
        std::fprintf(stderr, "pfdb-gui: failed to create a window\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    int rc = 0;
    try {
        const std::string db = db_path(argc, argv);
        const std::string config =
            env("PFDB_CONFIG").value_or(pfdb::config::Config::default_path());
        // Scope the App so its TextureCache frees GL textures while the context
        // is still current (before shutdown below).
        pfdb::gui::App app(db, config);

        while (glfwWindowShouldClose(window) == 0 && !app.wants_quit()) {
            glfwPollEvents();
            if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
                glfwWaitEventsTimeout(0.1);
                continue;
            }

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            app.frame();

            ImGui::Render();
            int fb_w = 0;
            int fb_h = 0;
            glfwGetFramebufferSize(window, &fb_w, &fb_h);
            glViewport(0, 0, fb_w, fb_h);
            glClearColor(0.10F, 0.10F, 0.11F, 1.0F);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "pfdb-gui: %s\n", e.what());
        rc = 2;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return rc;
}
