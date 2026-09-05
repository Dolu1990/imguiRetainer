#include <imgui_retainer/imgui_retainer.hpp>

#include <string_view>

int main() {
    return imgui_retainer::version() == std::string_view{"0.1.0"} ? 0 : 1;
}
