#include <rgui/lua.hpp>
#include <rgui/rgui.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#include <sol/sol.hpp>

#include <iostream>

namespace {

void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

} // namespace

int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    GLFWwindow* window = glfwCreateWindow(960, 540, "rgui — Lua demo", nullptr, nullptr);
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

    sol::state lua;
    lua.open_libraries(sol::lib::base);
    std::recursive_mutex mutex;
    rgui::bindLua(lua, mutex, rgui::callbackExecuteDefault);

    const sol::protected_function_result result = lua.safe_script(R"(
        local root = rgui.window("Lua-built retained rgui")
        local description = rgui.text("This retained UI was constructed and is updated by Lua.")
        local status = rgui.text("Button clicks: 0")
        local action = rgui.button("Increment")
        local panel = rgui.anchoredPanel("fill", 90)
        local panel_text = rgui.text("This text is anchored to the panel's top-left.")
        local panel_action = rgui.button("Centred increment")
        local stats = rgui.table(2)
        local log = rgui.scrollArea(0, 110)
        local stat_value = rgui.text("0")
        stats:setHeader(1, "Item")
        stats:setHeader(2, "Value")
        stats:append(rgui.text("Retained nodes"))
        stats:append(rgui.text("Table cells are Lua-built"))
        stats:append(rgui.text("Click count"))
        stats:append(stat_value)
        for entry = 1, 16 do
            log:append(rgui.text("Scrollable Lua log entry " .. entry))
        end
        local clicks = 0
        local function increment(button)
            clicks = clicks + 1
            status.value = "Button clicks: " .. clicks
            stat_value.value = tostring(clicks)
        end
        action:onClick(increment)
        panel_action:onClick(increment)

        root:append(description)
        root:append(status)
        root:append(action)
        root:append(stats)
        root:append(log)
        panel:append(panel_text, 0, 0, 0, 0, 0, 0)
        --panel:append(panel_action, 0.5, 0, 0.5, 0, 0, 46)
        panel:append(
          panel_action,
          0, 0, 0, 0, 0, 32,
          1, 0, 1, 0, 0, 32
        )
        root:append(panel)
        tree = rgui.tree()
        tree:setRoot(root)
    )", sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error error = result;
        std::cerr << error.what() << '\n';
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    rgui::UiTree& tree = lua["tree"];
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        tree.draw();
        static_cast<void>(tree.flushEvents());

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
