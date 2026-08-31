#include "bookmark.hpp"

#include <random>

namespace terminadventure::bookmark
{
    std::string NewId()
    {
        static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(0, static_cast<int>(sizeof(alphabet)) - 2);
        std::string id;
        id.reserve(8);
        for (int i = 0; i < 8; ++i) id += alphabet[dist(gen)];
        return id;
    }
}
