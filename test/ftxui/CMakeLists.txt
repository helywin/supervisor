add_executable(test_ftxui test_ftxui.cpp)
    target_link_libraries(test_ftxui
        ftxui::screen
        ftxui::dom
        ftxui::component
    )
