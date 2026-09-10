#include "./command.hpp"

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
}
