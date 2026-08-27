#include "main.hpp"

#include <cstdlib>
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

    std::cout<<"Hello Dave\n";
}
