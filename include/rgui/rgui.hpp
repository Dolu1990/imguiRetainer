#pragma once

#include <string_view>

#include <rgui/ui.hpp>

namespace rgui {

/// Returns the version of the linked rgui library.
[[nodiscard]] std::string_view version() noexcept;

} // namespace rgui
