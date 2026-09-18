#pragma once

#include <ftxui/dom/elements.hpp>

namespace terminadventure::screen
{

    enum class Type { GmTools };
    ftxui::Element render(Type screen);

}
