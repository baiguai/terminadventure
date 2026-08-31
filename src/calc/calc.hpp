#pragma once

#include <string>

namespace terminadventure::calc
{
    // Evaluate a numeric expression (e.g. "2(1+3)/4" or "1.5 * (2 + 3)").
    // Supports + - * /, parentheses, unary minus and implicit multiplication
    // (a number adjacent to a parenthesis: "2(1+3)"). On success returns true
    // and sets `out` to a human-friendly formatted result; on error (bad
    // syntax or division by zero) returns false.
    bool Evaluate(const std::string& expr, std::string& out);
}
