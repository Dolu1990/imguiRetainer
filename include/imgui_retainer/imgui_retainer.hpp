#pragma once

#include <string_view>

namespace imgui_retainer {

/// Returns the version of the linked imguiRetainer library.
[[nodiscard]] std::string_view version() noexcept;

} // namespace imgui_retainer
