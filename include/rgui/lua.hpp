#pragma once

namespace sol {
class state_view;
}

namespace rgui {

/// Registers the script-facing rgui API in an embedding application's Lua state.
/// The caller owns the state, its allocator, libraries, and script execution policy.
void bindLua(sol::state_view state);

} // namespace rgui
