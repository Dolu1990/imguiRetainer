# This is a demonstration executable, not part of the installed library API.
# rgui::rgui owns the pinned Dear ImGui library; this target only adds the
# GLFW/OpenGL platform and renderer backends.
set(RGUI_GLFW_SOURCE_DIR "${PROJECT_SOURCE_DIR}/ext/glfw" CACHE PATH
    "Optional path to a GLFW source checkout when glfw3 is not installed")

find_package(glfw3 CONFIG QUIET)
if(TARGET glfw)
    set(rgui_glfw_target glfw)
elseif(TARGET glfw3::glfw)
    set(rgui_glfw_target glfw3::glfw)
elseif(EXISTS "${RGUI_GLFW_SOURCE_DIR}/CMakeLists.txt")
    # These normal-scope settings are inherited by GLFW, avoiding its optional
    # examples, tests, documentation, and install rules in this superproject.
    set(GLFW_BUILD_EXAMPLES OFF)
    set(GLFW_BUILD_TESTS OFF)
    set(GLFW_BUILD_DOCS OFF)
    set(GLFW_INSTALL OFF)
    add_subdirectory("${RGUI_GLFW_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/glfw" EXCLUDE_FROM_ALL)
    set(rgui_glfw_target glfw)
else()
    message(FATAL_ERROR
        "The ImGui GLFW demo requires GLFW. Install glfw3 (with a CMake package), "
        "provide a parent target named glfw or glfw3::glfw, or set "
        "RGUI_GLFW_SOURCE_DIR.")
endif()

find_package(OpenGL REQUIRED)

add_executable(rgui_imgui_glfw_demo
    imgui_glfw_demo.cpp
    "${PROJECT_SOURCE_DIR}/ext/imgui/backends/imgui_impl_glfw.cpp"
    "${PROJECT_SOURCE_DIR}/ext/imgui/backends/imgui_impl_opengl3.cpp"
)
target_compile_features(rgui_imgui_glfw_demo PRIVATE cxx_std_23)
target_include_directories(rgui_imgui_glfw_demo PRIVATE
    "${PROJECT_SOURCE_DIR}/ext/imgui"
    "${PROJECT_SOURCE_DIR}/ext/imgui/backends")
target_link_libraries(rgui_imgui_glfw_demo PRIVATE
    rgui::rgui
    ${rgui_glfw_target}
    OpenGL::GL)

if(MSVC)
    target_compile_options(rgui_imgui_glfw_demo PRIVATE /W4 /permissive-)
else()
    target_compile_options(rgui_imgui_glfw_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()
