#!/bin/bash

# Central configuration - edit values here, all scripts pick them up

APP_NAME="terminadventure"

SOURCES=(
    "src/main.cpp"
)

HEADERS=(
    "src/main.hpp"
    "src/app_state/app_state.hpp"
)

LIBS=(
    "ftxui::screen"
    "ftxui::dom"
    "ftxui::component"
)
