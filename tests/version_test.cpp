#include <rgui/rgui.hpp>

#include <string_view>

int main() {
    return rgui::version() == std::string_view{"0.1.0"} ? 0 : 1;
}
