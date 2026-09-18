#pragma once

#include <ftxui/dom/elements.hpp>

namespace terminadventure::screen
{

    enum class Type { GmTools, Roller, Stuff };
    ftxui::Element render(Type screen);

}
