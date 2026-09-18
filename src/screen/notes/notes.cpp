#include "./notes.hpp"

namespace terminadventure::screen::notes
{
    ftxui::Element render()
    {
        using namespace ftxui;
        return text("Notes Content");
    }
}
