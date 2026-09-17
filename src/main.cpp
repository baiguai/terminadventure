#include <iostream>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include "./menu/menu.hpp"
#include "./command/command.hpp"

int main()
{
    using namespace ftxui;

    // Data
    std::string command_line;
    int selected_menu = 0;

    // Screen
    auto screen = ScreenInteractive::Fullscreen();

    // Left Panel
    auto left_menu = terminadventure::menu::createLeftMenu(selected_menu);

    // Create the command element
    auto cmd_input = terminadventure::command::createCommandInput(command_line, screen);


    // Build the layout
    auto component = Container::Vertical({left_menu, cmd_input});

    auto layout = Renderer(component, [&] {
        return vbox({
            text("Hello Dave.") | bold | center,
            separator(),
            hbox({
                left_menu->Render() | border | size(WIDTH, EQUAL, 60),
                vbox({
                    cmd_input->Render() | size(HEIGHT, EQUAL, 1),
                    separator(),
                    text("Main Content Area") | flex,
                }) | flex,
            }) | flex,
        });
    });

    auto root = CatchEvent(layout, [&](Event event) {
        if (event == Event::Special(std::string(1, 'q' - 96)))  // Ctrl+q
        {
            screen.Exit();
            return true;
        }
        return false;
    });
    screen.Loop(root);

    return 0;
}
