#include "main.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#endif

using namespace ftxui;

// main function
int main(int, char** argv)
{
    auto state = std::make_shared<AppState>();

    auto treeview_comp = terminadventure::treeview::MakeTreeView(state);
    auto dm_tools_comp = terminadventure::dm_tools::MakeDmTools(state);

    auto treeview_wrap = treeview_comp;
    auto dm_tools_wrap = dm_tools_comp;

    auto main_split = ResizableSplitLeft(treeview_wrap, dm_tools_wrap, &state->treeview_width);


    auto screen = ScreenInteractive::Fullscreen();
    auto quit = screen.ExitLoopClosure();


    int active_child = 0;

    auto container = Container::Vertical({
        main_split | flex
    }, &active_child);

    // auto modals = {};



    auto root = container;
    // for (auto& [comp, show] : modals)
    // {
    //     root = Modal(root, comp, show);
    // }

    screen.Loop(root);
}
