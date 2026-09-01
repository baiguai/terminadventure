#include <iostream>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

int main()
{
    using namespace ftxui;

    Element document = vbox({
        text("Hello Dave.") | bold | center,
        separator(),
        hbox({
            text("Left Panel") | border,
            vbox({
                text("Main Content Area") | flex,
                separator(),
                text("A Footer") | dim,
            }) | flex,
        }) | flex,
    });

    auto component = Renderer([document] { return document; });
    auto screen = ScreenInteractive::TerminalOutput();
    screen.Loop(component);

    return 0;
}
