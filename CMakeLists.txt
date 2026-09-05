cmake_minimum_required(VERSION 3.25)

project(rgui
    VERSION 0.1.0
    DESCRIPTION "Retainer-mode GUI layer for Dear ImGui with Lua bindings"
    LANGUAGES CXX
)

option(RGUI_BUILD_EXAMPLES "Build rgui examples" ON)
option(RGUI_BUILD_TESTS "Build rgui tests" ON)
option(RGUI_BUILD_IMGUI_SMOKE_TEST
    "Build the Dear ImGui + GLFW immediate-mode smoke-test application" OFF)
option(RGUI_BUILD_IMGUI_BACKEND
    "Build the optional Dear ImGui rendering adapter" OFF)
set(RGUI_IMGUI_TARGET "" CACHE STRING
    "Imported/normal CMake target that provides Dear ImGui (for example imgui::imgui)")

add_library(rgui src/rgui.cpp)
add_library(rgui::rgui ALIAS rgui)

target_compile_features(rgui PUBLIC cxx_std_23)
target_include_directories(rgui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(RGUI_BUILD_IMGUI_BACKEND)
    if(NOT RGUI_IMGUI_TARGET)
        message(FATAL_ERROR
            "RGUI_BUILD_IMGUI_BACKEND requires RGUI_IMGUI_TARGET; rgui does not acquire Dear ImGui itself.")
    endif()
    if(NOT TARGET ${RGUI_IMGUI_TARGET})
        message(FATAL_ERROR "RGUI_IMGUI_TARGET '${RGUI_IMGUI_TARGET}' is not a CMake target.")
    endif()

    add_library(rgui_imgui src/imgui_backend.cpp)
    add_library(rgui::imgui ALIAS rgui_imgui)
    target_compile_features(rgui_imgui PUBLIC cxx_std_23)
    target_link_libraries(rgui_imgui PUBLIC rgui::rgui PRIVATE ${RGUI_IMGUI_TARGET})
    target_include_directories(rgui_imgui PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
endif()

if(MSVC)
    target_compile_options(rgui PRIVATE /W4 /permissive-)
else()
    target_compile_options(rgui PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS rgui
    EXPORT rguiTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(TARGET rgui_imgui)
    install(TARGETS rgui_imgui EXPORT rguiTargets)
endif()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
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

if(RGUI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(RGUI_BUILD_IMGUI_SMOKE_TEST)
    add_subdirectory(smoke_tests/imgui_glfw)
endif()

if(RGUI_BUILD_TESTS)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
