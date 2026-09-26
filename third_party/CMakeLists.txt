# ---------------------------------------------------------------------------
# Vendored third-party libraries. Built without our warnings-as-errors flags.
# ---------------------------------------------------------------------------

# Dear ImGui (trimmed) with the GLFW + OpenGL3 backends and the std::string
# InputText helper. The OpenGL3 backend carries its own GL loader, so no GLAD.
add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_demo.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp
    imgui/misc/cpp/imgui_stdlib.cpp
)

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/misc/cpp
)

# The GLFW backend includes <GLFW/glfw3.h>, so imgui needs GLFW's headers/lib.
target_link_libraries(imgui PUBLIC glfw)
