#include <rgui/rgui.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>


#include <GLFW/glfw3.h>

#include <iostream>
#include <memory>
#include <random>
#include <string>

namespace {

void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

} // namespace

int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        return 1;
    }

#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    GLFWwindow* window = glfwCreateWindow(
        960, 540, "rgui — ImGui GLFW demo", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    int clicks = 0;

    // The application owns the retained tree and its state. The ImGui adapter
    // only emits the current tree into the caller-owned ImGui frame.
    auto ui_window = std::make_shared<rgui::Window>("Retained rgui");

    auto description = std::make_shared<rgui::Text>(
        "This window is built from a retained rgui tree.");

    auto controls = std::make_shared<rgui::Stack>(rgui::Axis::horizontal);
    auto add_random_label = std::make_shared<rgui::Button>("Add random label");
    auto increment = std::make_shared<rgui::Button>("Increment");
    auto reset = std::make_shared<rgui::Button>("Reset");

    auto status = std::make_shared<rgui::Text>();
    const auto update_status = [&] {
        status->set_value("Button clicks: " + std::to_string(clicks));
    };
    increment->set_on_click([&](rgui::Button&) {
        ++clicks;
        update_status();
    });
    update_status();

    auto generated_labels = std::make_shared<rgui::Stack>();
    std::mt19937 random_engine{std::random_device{}()};
    std::uniform_int_distribution<int> random_number{0, 9999};
    add_random_label->set_on_click([&](rgui::Button&) {
        auto label = std::make_shared<rgui::Text>(
            "Random number: " + std::to_string(random_number(random_engine)));
        generated_labels->append(std::move(label));
    });
    reset->set_on_click([&](rgui::Button&) {
        clicks = 0;
        generated_labels->clear();
        update_status();
    });

    ui_window->append(description);
    ui_window->append(status);
    controls->append(add_random_label);
    controls->append(increment);
    controls->append(reset);
    ui_window->append(controls);
    ui_window->append(generated_labels);

    rgui::UiTree tree;
    tree.set_root(ui_window);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Layout can be skipped when no layout-affecting property changed;
        // this compact example runs it every frame to keep the demo
        // straightforward and to exercise the retained layout path.
        tree.draw();

        // Dispatch after rendering, when callbacks can safely change the
        // retained tree. Layout and rendering observe those changes next frame.
        static_cast<void>(tree.flush_events());

        ImGui::Render();
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        glViewport(0, 0, framebuffer_width, framebuffer_height);
        glClearColor(0.08F, 0.09F, 0.12F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
