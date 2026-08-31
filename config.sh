#!/bin/bash

# Central configuration - edit values here, all scripts pick them up

APP_NAME="terminadventure"

SOURCES=(
    "src/main.cpp"
    "src/editor/editor.cpp"
    "src/treeview/treeview.cpp"
    "src/diceroller/diceroller.cpp"
    "src/rightpane/rightpane.cpp"
    "src/keyboard/keymap.cpp"
    "src/op/op.cpp"
    "src/config/config.cpp"
    "src/io/serialize.cpp"
    "src/html/convert.cpp"
    "src/help/help.cpp"
    "src/search/search.cpp"
    "src/visual_block/visual_block.cpp"
    "src/bookmark/bookmark.cpp"
    "src/bookmarks/bookmarks.cpp"
    "src/links/links.cpp"
    "src/links/brokelinks.cpp"
    "src/history/history.cpp"
    "src/browser/browser.cpp"
    "src/recent/recent.cpp"
    "src/undo/undo.cpp"
)

LIBS=(
    "ftxui::screen"
    "ftxui::dom"
    "ftxui::component"
)
