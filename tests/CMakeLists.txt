add_executable(rgui_version_test version_test.cpp)
target_link_libraries(rgui_version_test PRIVATE rgui::rgui)

add_test(NAME rgui.version COMMAND rgui_version_test)

add_executable(rgui_ui_test ui_test.cpp)
target_link_libraries(rgui_ui_test PRIVATE rgui::rgui)
add_test(NAME rgui.ui COMMAND rgui_ui_test)
