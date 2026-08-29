#pragma once

#include <functional>
#include <string>
#include <map>
#include <utility>
#include <vector>

inline constexpr int kDefaultTreeviewWidth = 30;
inline constexpr int kMinTreeviewWidth = 10;
inline constexpr int kMaxTreeviewWidth = 200;

struct AppState
{
    std::string init_path;
    int treeview_width = kDefaultTreeviewWidth;
};
