#pragma once

#include <string_view>

namespace rgui {

/// Returns the version of the linked rgui library.
[[nodiscard]] std::string_view version() noexcept;

} // namespace rgui
