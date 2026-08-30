#include "visual_block.hpp"

#include <algorithm>

namespace terminadventure::visual_block
{
    bool LineSpan(const std::vector<std::string>& lines, const Block& b, int row,
                  int& lo, int& hi)
    {
        if (row < b.row_lo || row > b.row_hi) return false;
        const int size = static_cast<int>(lines[static_cast<std::size_t>(row)].size());
        lo = std::min(b.col_lo, size);
        hi = std::min(b.col_hi + 1, size);
        return lo < hi;
    }

    std::string Extract(const std::vector<std::string>& lines, const Block& b)
    {
        std::string out;
        for (int r = b.row_lo; r <= b.row_hi; ++r)
        {
            int lo = 0, hi = 0;
            if (!LineSpan(lines, b, r, lo, hi)) continue;
            if (!out.empty()) out += '\n';
            out += lines[static_cast<std::size_t>(r)].substr(
                static_cast<std::size_t>(lo), static_cast<std::size_t>(hi - lo));
        }
        return out;
    }

    void Erase(std::vector<std::string>& lines, const Block& b)
    {
        for (int r = b.row_lo; r <= b.row_hi; ++r)
        {
            int lo = 0, hi = 0;
            if (!LineSpan(lines, b, r, lo, hi)) continue;
            lines[static_cast<std::size_t>(r)].erase(
                static_cast<std::size_t>(lo), static_cast<std::size_t>(hi - lo));
        }
    }

    void Transform(std::vector<std::string>& lines, const Block& b,
                   char (*fold)(char))
    {
        for (int r = b.row_lo; r <= b.row_hi; ++r)
        {
            int lo = 0, hi = 0;
            if (!LineSpan(lines, b, r, lo, hi)) continue;
            std::string& line = lines[static_cast<std::size_t>(r)];
            for (int c = lo; c < hi; ++c)
            {
                line[static_cast<std::size_t>(c)] = fold(line[static_cast<std::size_t>(c)]);
            }
        }
    }

    void Insert(std::vector<std::string>& lines, const Block& b,
                const std::string& text, bool at_end)
    {
        if (text.empty()) return;
        for (int r = b.row_lo; r <= b.row_hi; ++r)
        {
            std::string& line = lines[static_cast<std::size_t>(r)];
            if (at_end)
            {
                const int pos = std::min(b.col_hi + 1, static_cast<int>(line.size()));
                line.insert(static_cast<std::size_t>(pos), text);
            }
            else
            {
                if (static_cast<int>(line.size()) < b.col_lo)
                {
                    line.append(static_cast<std::size_t>(b.col_lo) -
                                    static_cast<std::size_t>(line.size()),
                                ' ');
                }
                line.insert(static_cast<std::size_t>(b.col_lo), text);
            }
        }
    }
}
