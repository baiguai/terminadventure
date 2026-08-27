#!/bin/bash

# Central configuration - edit values here, all scripts pick them up

APP_NAME="terminadventure"

SOURCES=(
    "src/main.cpp"
    "src/treeview/treeview.cpp"
    "src/dm_tools/dm_tool.cpp"
)

HEADERS=(
    "src/main.hpp"
    "src/app_state/app_state.hpp"
    "src/treeview/treeview.hpp"
    "src/dm_tools/dm_tool.hpp"
)

LIBS=(
    "ftxui::screen"
    "ftxui::dom"
    "ftxui::component"
)
