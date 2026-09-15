#pragma once
#include <functional>
#include <sol/sol.hpp>

namespace sol {
class state_view;
}

namespace rgui {

/// Provide a default implementation ofr the bindLua callbackExecute, which syncronously execute le lua function with args.
extern void callbackExecuteDefault(sol::function& callback, std::vector<sol::object>& args);

/// Registers the script-facing rgui API in an embedding application's Lua state.
/// The caller owns the state, its allocator, libraries, and script execution policy.
extern void bindLua(sol::state_view state, std::function<void(sol::function&, std::vector<sol::object>&)> callbackExecute);

} // namespace rgui
