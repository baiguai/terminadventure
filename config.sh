#!/bin/bash

# Central configuration - edit values here, all scripts pick them up

APP_NAME="terminadventure"

SOURCES=(
    "src/main.cpp"
    "src/menu/menu.cpp"
    "src/command/command.cpp"
    "src/screen/screen.cpp"
    "src/screen/gm_tools/gm_tools.cpp"
    "src/screen/roller/roller.cpp"
    "src/screen/notes/notes.cpp"
)

LIBS=(
    "ftxui::screen"
    "ftxui::dom"
    "ftxui::component"
)

HEADERS=(
    "src/main.hpp"
    "src/menu/menu.hpp"
    "src/command/command.hpp"
    "src/screen/screen.hpp"
    "src/screen/gm_tools/gm_tools.hpp"
    "src/screen/roller/roller.hpp"
    "src/screen/notes/notes.hpp"
)
