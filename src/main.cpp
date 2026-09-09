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
            text("Left Panel") | border | size(WIDTH, EQUAL, 60),
            vbox({
                text("Main Content Area") | flex,
                separator(),
                text("A Footer") | dim,
            }) | flex,
        }) | flex,
    });

    auto screen = ScreenInteractive::Fullscreen();

    auto component = CatchEvent(
        Renderer([document] { return document; }),
        [&](Event event)
        {
            if (event == Event::Special(std::string(1, 'q' - 96)))  // Ctrl+q
            {
                screen.Exit();
                return true;
            }
            return false;
        });

    screen.Loop(component);

    return 0;
}
