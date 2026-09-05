# This is a manual visual smoke test. Lua owns the retained tree while this
# harness owns the GLFW window, OpenGL context, and Dear ImGui frame lifecycle.
set(RGUI_IMGUI_SOURCE_DIR "${PROJECT_SOURCE_DIR}/ext/imgui" CACHE PATH
    "Path to the root of a Dear ImGui source checkout")
set(RGUI_GLFW_SOURCE_DIR "${PROJECT_SOURCE_DIR}/ext/glfw" CACHE PATH
    "Optional path to a GLFW source checkout when glfw3 is not installed")

if(NOT EXISTS "${RGUI_IMGUI_SOURCE_DIR}/imgui.h")
    message(FATAL_ERROR
        "RGUI_BUILD_LUA_SMOKE_TEST requires a Dear ImGui checkout. "
        "Set RGUI_IMGUI_SOURCE_DIR to its root directory.")
endif()

find_package(glfw3 CONFIG QUIET)
if(TARGET glfw)
    set(rgui_lua_smoke_glfw_target glfw)
elseif(TARGET glfw3::glfw)
    set(rgui_lua_smoke_glfw_target glfw3::glfw)
elseif(EXISTS "${RGUI_GLFW_SOURCE_DIR}/CMakeLists.txt")
    set(GLFW_BUILD_EXAMPLES OFF)
    set(GLFW_BUILD_TESTS OFF)
    set(GLFW_BUILD_DOCS OFF)
    set(GLFW_INSTALL OFF)
    add_subdirectory("${RGUI_GLFW_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/glfw" EXCLUDE_FROM_ALL)
    set(rgui_lua_smoke_glfw_target glfw)
else()
    message(FATAL_ERROR
        "The Lua smoke test requires GLFW. Install glfw3 (with a CMake package), "
        "provide a parent target named glfw or glfw3::glfw, or set "
        "RGUI_GLFW_SOURCE_DIR.")
endif()

find_package(OpenGL REQUIRED)

add_library(rgui_lua_smoke_imgui_vendor STATIC
    "${RGUI_IMGUI_SOURCE_DIR}/imgui.cpp"
    "${RGUI_IMGUI_SOURCE_DIR}/imgui_draw.cpp"
    "${RGUI_IMGUI_SOURCE_DIR}/imgui_tables.cpp"
    "${RGUI_IMGUI_SOURCE_DIR}/imgui_widgets.cpp"
)
target_compile_features(rgui_lua_smoke_imgui_vendor PUBLIC cxx_std_23)
target_include_directories(rgui_lua_smoke_imgui_vendor PUBLIC "${RGUI_IMGUI_SOURCE_DIR}")

add_executable(rgui_lua_smoke
    lua_smoke.cpp
    "${PROJECT_SOURCE_DIR}/src/imgui_backend.cpp"
    "${RGUI_IMGUI_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${RGUI_IMGUI_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
)
target_compile_features(rgui_lua_smoke PRIVATE cxx_std_23)
target_include_directories(rgui_lua_smoke PRIVATE
    "${RGUI_IMGUI_SOURCE_DIR}"
    "${RGUI_IMGUI_SOURCE_DIR}/backends")
target_link_libraries(rgui_lua_smoke PRIVATE
    rgui::rgui
    rgui::lua
    rgui_lua_smoke_imgui_vendor
    ${rgui_lua_smoke_glfw_target}
    OpenGL::GL
    ${RGUI_SOL2_TARGET}
    ${RGUI_LUA_TARGET})

if(MSVC)
    target_compile_options(rgui_lua_smoke PRIVATE /W4 /permissive-)
else()
    target_compile_options(rgui_lua_smoke PRIVATE -Wall -Wextra -Wpedantic)
endif()
