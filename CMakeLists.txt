cmake_minimum_required(VERSION 3.25)

project(rgui
    VERSION 0.1.0
    DESCRIPTION "Retainer-mode GUI layer for Dear ImGui with Lua bindings"
    LANGUAGES CXX
)

option(RGUI_BUILD_TESTS "Build rgui tests" ON)
option(RGUI_BUILD_IMGUI_GLFW_DEMO
    "Build the Dear ImGui + GLFW immediate-mode demo application" OFF)
option(RGUI_BUILD_LUA_BINDINGS
    "Build the optional sol2 Lua binding adapter" OFF)
option(RGUI_BUILD_LUA_DEMO
    "Build the sol2 Lua API demo application" OFF)
set(RGUI_LUA_TARGET "" CACHE STRING
    "Imported/normal CMake target that provides Lua (for example Lua::Lua)")
set(RGUI_SOL2_TARGET "" CACHE STRING
    "Optional imported/normal CMake target that provides sol2 (defaults to bundled ext/sol2)")

add_library(rgui_imgui STATIC
    ext/imgui/imgui.cpp
    ext/imgui/imgui_draw.cpp
    ext/imgui/imgui_tables.cpp
    ext/imgui/imgui_widgets.cpp
)
add_library(rgui::imgui ALIAS rgui_imgui)
target_compile_features(rgui_imgui PUBLIC cxx_std_23)
target_include_directories(rgui_imgui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ext/imgui>
    $<INSTALL_INTERFACE:include/rgui/third_party/imgui>
)

add_library(rgui src/rgui.cpp)
add_library(rgui::rgui ALIAS rgui)

target_compile_features(rgui PUBLIC cxx_std_23)
target_include_directories(rgui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(rgui PUBLIC rgui::imgui)

if(RGUI_BUILD_LUA_DEMO)
    set(RGUI_BUILD_LUA_BINDINGS ON)
    if(NOT RGUI_LUA_TARGET)
        find_package(Lua 5.4 REQUIRED)
        if(NOT TARGET Lua::Lua)
            add_library(rgui_discovered_lua INTERFACE)
            target_include_directories(rgui_discovered_lua INTERFACE "${LUA_INCLUDE_DIR}")
            target_link_libraries(rgui_discovered_lua INTERFACE "${LUA_LIBRARIES}")
            set(RGUI_LUA_TARGET rgui_discovered_lua)
        else()
            set(RGUI_LUA_TARGET Lua::Lua)
        endif()
    endif()
endif()

if(RGUI_BUILD_LUA_BINDINGS OR RGUI_BUILD_LUA_DEMO)
    if(NOT RGUI_SOL2_TARGET)
        if(NOT EXISTS "${PROJECT_SOURCE_DIR}/ext/sol2/CMakeLists.txt")
            message(FATAL_ERROR
                "Lua bindings require the sol2 submodule. Run: git submodule update --init --recursive")
        endif()
        set(SOL2_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
        add_subdirectory(ext/sol2 EXCLUDE_FROM_ALL)
        set(RGUI_SOL2_TARGET sol2::sol2)
    endif()
    if(NOT TARGET ${RGUI_SOL2_TARGET})
        message(FATAL_ERROR "RGUI_SOL2_TARGET '${RGUI_SOL2_TARGET}' is not a CMake target.")
    endif()
endif()

if(RGUI_BUILD_LUA_BINDINGS)
    if(NOT RGUI_LUA_TARGET)
        message(FATAL_ERROR
            "RGUI_BUILD_LUA_BINDINGS requires RGUI_LUA_TARGET; the embedding game owns Lua.")
    endif()
    if(NOT TARGET ${RGUI_LUA_TARGET})
        message(FATAL_ERROR "RGUI_LUA_TARGET '${RGUI_LUA_TARGET}' is not a CMake target.")
    endif()

    add_library(rgui_lua src/lua.cpp)
    add_library(rgui::lua ALIAS rgui_lua)
    target_compile_features(rgui_lua PUBLIC cxx_std_23)
    target_link_libraries(rgui_lua PUBLIC rgui::rgui PRIVATE ${RGUI_SOL2_TARGET} ${RGUI_LUA_TARGET})
    target_include_directories(rgui_lua PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    if(MSVC)
        target_compile_options(rgui_lua PRIVATE /W4 /permissive-)
    else()
        target_compile_options(rgui_lua PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(MSVC)
    target_compile_options(rgui PRIVATE /W4 /permissive-)
else()
    target_compile_options(rgui PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS rgui rgui_imgui
    EXPORT rguiTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES
    ext/imgui/imgui.h
    ext/imgui/imconfig.h
    ext/imgui/imstb_rectpack.h
    ext/imgui/imstb_textedit.h
    ext/imgui/imstb_truetype.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rgui/third_party/imgui)
install(EXPORT rguiTargets
    FILE rguiTargets.cmake
    NAMESPACE rgui::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rgui
)

configure_package_config_file(
    cmake/rguiConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/rguiConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rgui
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/rguiConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/rguiConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/rguiConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rgui
)

if(RGUI_BUILD_TESTS OR RGUI_BUILD_LUA_DEMO)
    include(CTest)
endif()

if(RGUI_BUILD_IMGUI_GLFW_DEMO)
    add_subdirectory(demo/imgui_glfw)
endif()

if(RGUI_BUILD_LUA_DEMO)
    add_subdirectory(demo/lua)
endif()

if(RGUI_BUILD_TESTS)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
