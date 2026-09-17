#include "./command.hpp"
#include <iostream>

namespace terminadventure::command
{
    ftxui::Element process(const std::string& input)
    {
        using namespace ftxui;

        if (input.empty())
        {
            return text("");
        }

        return text(":" + input);
    }

    ftxui::Component createCommandInput(std::string& command_line, ftxui::ScreenInteractive& screen)
    {
        using namespace ftxui;

        InputOption opt;
        opt.placeholder = "command";
        opt.multiline = false;
        opt.transform = [](InputState state)
        {
            if (state.focused)
            {
                state.element |= bgcolor(Color::GrayDark) | color(Color::White);
            }
            else
            {
                state.element |= bgcolor(Color::Black) | color(Color::White);
            }
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

        return Input(&command_line, opt);
    }
}
