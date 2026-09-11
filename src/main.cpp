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

    // Screen
    auto screen = ScreenInteractive::Fullscreen();

    // Command Input
    InputOption opt;
    opt.placeholder = "command";
    opt.multiline = false;
    opt.transform = [](InputState state)
    {
        state.element |= bgcolor(Color::Black) | color(Color::White);
        return state.element;
    };
    opt.on_enter = [&]
    {
        std::cout << "cmd: " << command_line << "\n";
        if (command_line == "exit")
        {
            screen.Exit();
            return;
        }
        terminadventure::command::process(command_line);
        command_line.clear();
    };
    auto cmd_input = Input(&command_line, opt);


    // Build the layout
    auto component = Container::Vertical({cmd_input});

    auto layout = Renderer(component, [&] {
        return vbox({
            text("Hello Dave.") | bold | center,
            separator(),
            hbox({
                text("Left Panel") | border | size(WIDTH, EQUAL, 60),
                vbox({
                    text("Main Content Area") | flex,
                    separator(),
                    cmd_input->Render() | size(HEIGHT, EQUAL, 1),
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
