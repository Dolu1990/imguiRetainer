cmake_minimum_required(VERSION 3.25)

project(imguiRetainer
    VERSION 0.1.0
    DESCRIPTION "Retainer-mode GUI layer for Dear ImGui with Lua bindings"
    LANGUAGES CXX
)

option(IMGUI_RETAINER_BUILD_EXAMPLES "Build imguiRetainer examples" ON)
option(IMGUI_RETAINER_BUILD_TESTS "Build imguiRetainer tests" ON)

add_library(imguiRetainer src/imgui_retainer.cpp)
add_library(imguiRetainer::imguiRetainer ALIAS imguiRetainer)

target_compile_features(imguiRetainer PUBLIC cxx_std_23)
target_include_directories(imguiRetainer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(MSVC)
    target_compile_options(imguiRetainer PRIVATE /W4 /permissive-)
else()
    target_compile_options(imguiRetainer PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS imguiRetainer
    EXPORT imguiRetainerTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT imguiRetainerTargets
    FILE imguiRetainerTargets.cmake
    NAMESPACE imguiRetainer::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/imguiRetainer
)

configure_package_config_file(
    cmake/imguiRetainerConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/imguiRetainerConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/imguiRetainer
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/imguiRetainerConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/imguiRetainerConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/imguiRetainerConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/imguiRetainer
)

if(IMGUI_RETAINER_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(IMGUI_RETAINER_BUILD_TESTS)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
