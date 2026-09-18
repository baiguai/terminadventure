#include "./screen.hpp"
#include "./gm_tools/gm_tools.hpp"
#include "./roller/roller.hpp"
#include "./notes/notes.hpp"

namespace terminadventure::screen
{
    ftxui::Element render(Type screen)
    {
        using namespace ftxui;

        switch (screen)
        {
            case Type::GmTools:
                return gm_tools::render();
            case Type::Roller:
                return roller::render();
            case Type::Notes:
                return notes::render();
            default:
                return text("Main Content Area");
        }
    }
}
