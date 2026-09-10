#!/bin/bash

# Central configuration - edit values here, all scripts pick them up

APP_NAME="terminadventure"

SOURCES=(
    "src/main.cpp"
    "src/menu/menu.cpp"
    "src/command/command.cpp"
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
)
