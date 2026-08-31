#include "serialize.hpp"

#include <fstream>
#include <sstream>
#include <utility>

namespace terminadventure::io
{
    namespace
    {
        std::string Escape(const std::string& s);

        void AppendPlayer(std::string& out, const players::Player& p)
        {
            out += "    {\n";
            out += "      \"name\": " + Escape(p.name) + ",\n";
            out += "      \"race\": " + Escape(p.race) + ",\n";
            out += "      \"char_class\": " + Escape(p.char_class) + ",\n";
            out += "      \"background\": " + Escape(p.background) + ",\n";
            out += "      \"alignment\": " + Escape(p.alignment) + ",\n";
            out += "      \"level\": " + std::to_string(p.level) + ",\n";
            out += "      \"ac\": " + std::to_string(p.ac) + ",\n";
            out += "      \"hp\": " + std::to_string(p.hp) + ",\n";
            out += "      \"speed\": " + Escape(p.speed) + ",\n";
            out += "      \"str\": " + std::to_string(p.str) + ",\n";
            out += "      \"dex\": " + std::to_string(p.dex) + ",\n";
            out += "      \"con\": " + std::to_string(p.con) + ",\n";
            out += "      \"intel\": " + std::to_string(p.intel) + ",\n";
            out += "      \"wis\": " + std::to_string(p.wis) + ",\n";
            out += "      \"cha\": " + std::to_string(p.cha) + ",\n";
            out += "      \"ac_bonus\": " + Escape(p.ac_bonus) + ",\n";
            out += "      \"hit_dice\": " + Escape(p.hit_dice) + ",\n";
            out += "      \"equipment\": " + Escape(p.equipment) + ",\n";
            out += "      \"features\": " + Escape(p.features) + ",\n";
            out += "      \"saves\": [";
            for (std::size_t i = 0; i < p.proficient_saves.size(); ++i)
            {
                if (i) out += ", ";
                out += Escape(p.proficient_saves[i]);
            }
            out += "],\n";
            out += "      \"skills\": [";
            for (std::size_t i = 0; i < p.proficient_skills.size(); ++i)
            {
                if (i) out += ", ";
                out += Escape(p.proficient_skills[i]);
            }
            out += "]\n";
            out += "    }";
        }

        std::string Escape(const std::string& s)
        {
            std::string out;
            out += '"';
            for (unsigned char c : s)
            {
                switch (c)
                {
                    case '"': out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    case '\b': out += "\\b"; break;
                    case '\f': out += "\\f"; break;
                    default:
                        if (c < 0x20)
                        {
                            static const char hex[] = "0123456789abcdef";
                            out += "\\u00";
                            out += hex[c >> 4];
                            out += hex[c & 0xF];
                        }
                        else
                        {
                            out += static_cast<char>(c);
                        }
                }
            }
            out += '"';
            return out;
        }

        void AppendNode(std::string& out, const TreeNode& node, int depth)
        {
            std::string pad(depth * 2, ' ');
            std::string pad2((depth + 1) * 2, ' ');

            out += pad + "{\n";
            out += pad2 + "\"id\": " + Escape(node.id) + ",\n";
            out += pad2 + "\"name\": " + Escape(node.name) + ",\n";
            out += pad2 + "\"type\": " + Escape(treeview::NodeTypeToString(node.type)) + ",\n";
            out += pad2 + "\"expanded\": " + (node.expanded ? "true" : "false") + ",\n";
            out += pad2 + "\"text\": " + Escape(node.text) + ",\n";
            out += pad2 + "\"children\": [\n";
            for (std::size_t i = 0; i < node.children.size(); ++i)
            {
                AppendNode(out, node.children[i], depth + 1);
                out += (i + 1 < node.children.size()) ? ",\n" : "\n";
            }
            out += pad2 + "]\n";
            out += pad + "}";
        }

        class Parser
        {
            public:
                explicit Parser(const std::string& s) : s_(s) {}

                bool Deserialize(std::vector<TreeNode>& roots, int* tree_width,
                                 std::vector<bookmark::Bookmark>* bookmarks,
                                 std::vector<std::string>* history,
                                 std::vector<std::string>* presets,
                                 std::vector<players::Player>* players)
                {
                    SkipWs();
                    if (!Consume('{')) return false;
                    SkipWs();

                    std::vector<TreeNode> result;
                    bool have_roots = false;
                    bool have_width = false;
                    int width = 0;
                    std::vector<bookmark::Bookmark> marks;
                    bool have_marks = false;
                    std::vector<std::string> hist;
                    bool have_history = false;
                    std::vector<std::string> pres;
                    bool have_presets = false;
                    std::vector<players::Player> plrs;
                    bool have_players = false;
                    int version = -1;

                    if (!Consume('}'))
                    {
                        while (true)
                        {
                            SkipWs();
                            std::string key;
                            if (!ParseString(key)) return false;
                            SkipWs();
                            if (!Consume(':')) return false;
                            SkipWs();

                            if (key == "version")
                            {
                                if (!ParseNumber(version)) return false;
                            }
                            else if (key == "roots")
                            {
                                if (!ParseArray(result)) return false;
                                have_roots = true;
                            }
                            else if (key == "tree_width")
                            {
                                if (!ParseNumber(width)) return false;
                                have_width = true;
                            }
                            else if (key == "bookmarks")
                            {
                                if (!ParseBookmarks(marks)) return false;
                                have_marks = true;
                            }
                            else if (key == "history")
                            {
                                if (!ParseHistory(hist)) return false;
                                have_history = true;
                            }
                            else if (key == "presets")
                            {
                                if (!ParseHistory(pres)) return false;
                                have_presets = true;
                            }
                            else if (key == "players")
                            {
                                if (!ParsePlayers(plrs)) return false;
                                have_players = true;
                            }
                            else
                            {
                                if (!SkipValue()) return false;
                            }

                            SkipWs();
                            if (Consume(',')) continue;
                            if (Consume('}')) break;
                            return false;
                        }
                    }

                    SkipWs();
                    if (i_ != s_.size()) return false;
                    if (version != -1 && version != 1) return false;
                    if (!have_roots) return false;

                    roots = std::move(result);
                    if (have_width && tree_width) *tree_width = width;
                    if (have_marks && bookmarks) *bookmarks = std::move(marks);
                    if (have_history && history) *history = std::move(hist);
                    if (have_presets && presets) *presets = std::move(pres);
                    if (have_players && players) *players = std::move(plrs);
                    return true;
                }

            private:
                const std::string& s_;
                std::size_t i_ = 0;

                void SkipWs()
                {
                    while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t'
                           || s_[i_] == '\n' || s_[i_] == '\r')) ++i_;
                }

                bool Consume(char c)
                {
                    if (i_ < s_.size() && s_[i_] == c) { ++i_; return true; }
                    return false;
                }

                bool ParseString(std::string& out)
                {
                    if (!Consume('"')) return false;
                    out.clear();
                    while (i_ < s_.size())
                    {
                        char c = s_[i_++];
                        if (c == '"') return true;
                        if (c != '\\') { out += c; continue; }
                        if (i_ >= s_.size()) return false;

                        char e = s_[i_++];
                        switch (e)
                        {
                            case '"':  out += '"';  break;
                            case '\\': out += '\\'; break;
                            case '/':  out += '/';  break;
                            case 'b':  out += '\b'; break;
                            case 'f':  out += '\f'; break;
                            case 'n':  out += '\n'; break;
                            case 'r':  out += '\r'; break;
                            case 't':  out += '\t'; break;
                            case 'u':
                            {
                                int cp = 0;
                                for (int k = 0; k < 4; ++k)
                                {
                                    char h = (i_ < s_.size()) ? s_[i_++] : '\0';
                                    cp <<= 4;
                                    if (h >= '0' && h <= '9')      cp |= h - '0';
                                    else if (h >= 'a' && h <= 'f') cp |= h - 'a' + 10;
                                    else if (h >= 'A' && h <= 'F') cp |= h - 'A' + 10;
                                    else return false;
                                }
                                if (cp < 0x80)
                                {
                                    out += static_cast<char>(cp);
                                }
                                else if (cp < 0x800)
                                {
                                    out += static_cast<char>(0xC0 | (cp >> 6));
                                    out += static_cast<char>(0x80 | (cp & 0x3F));
                                }
                                else
                                {
                                    out += static_cast<char>(0xE0 | (cp >> 12));
                                    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                                    out += static_cast<char>(0x80 | (cp & 0x3F));
                                }
                                break;
                            }
                            default: return false;
                        }
                    }
                    return false;
                }

                bool ParseBool(bool& out)
                {
                    if (s_.compare(i_, 4, "true") == 0)  { i_ += 4; out = true;  return true; }
                    if (s_.compare(i_, 5, "false") == 0) { i_ += 5; out = false; return true; }
                    return false;
                }

                bool ParseNumber(int& out)
                {
                    bool neg = false;
                    if (Consume('-')) neg = true;
                    if (i_ >= s_.size() || s_[i_] < '0' || s_[i_] > '9') return false;
                    int value = 0;
                    while (i_ < s_.size() && s_[i_] >= '0' && s_[i_] <= '9')
                        value = value * 10 + (s_[i_++] - '0');
                    out = neg ? -value : value;
                    return true;
                }

                bool ParseArray(std::vector<TreeNode>& arr)
                {
                    if (!Consume('[')) return false;
                    SkipWs();
                    if (Consume(']')) return true;
                    while (true)
                    {
                        SkipWs();
                        TreeNode child{};
                        if (!ParseObject(child)) return false;
                        arr.push_back(std::move(child));
                        SkipWs();
                        if (Consume(',')) continue;
                        if (Consume(']')) return true;
                        return false;
                    }
                }

                bool ParseBookmarks(std::vector<bookmark::Bookmark>& marks)
                {
                    if (!Consume('[')) return false;
                    SkipWs();
                    if (Consume(']')) return true;
                    while (true)
                    {
                        SkipWs();
                        bookmark::Bookmark mark{};
                        if (!ParseBookmark(mark)) return false;
                        marks.push_back(std::move(mark));
                        SkipWs();
                        if (Consume(',')) continue;
                        if (Consume(']')) return true;
                        return false;
                    }
                }

                bool ParseHistory(std::vector<std::string>& ids)
                {
                    if (!Consume('[')) return false;
                    SkipWs();
                    if (Consume(']')) return true;
                    while (true)
                    {
                        SkipWs();
                        std::string id;
                        if (!ParseString(id)) return false;
                        ids.push_back(std::move(id));
                        SkipWs();
                        if (Consume(',')) continue;
                        if (Consume(']')) return true;
                        return false;
                    }
                }

                bool ParsePlayers(std::vector<players::Player>& out)
                {
                    if (!Consume('[')) return false;
                    SkipWs();
                    if (Consume(']')) return true;
                    while (true)
                    {
                        SkipWs();
                        players::Player p;
                        if (!ParsePlayer(p)) return false;
                        out.push_back(std::move(p));
                        SkipWs();
                        if (Consume(',')) continue;
                        if (Consume(']')) return true;
                        return false;
                    }
                }

                bool ParseStrings(std::vector<std::string>& out)
                {
                    if (!Consume('[')) return false;
                    SkipWs();
                    if (Consume(']')) return true;
                    while (true)
                    {
                        SkipWs();
                        std::string s;
                        if (!ParseString(s)) return false;
                        out.push_back(std::move(s));
                        SkipWs();
                        if (Consume(',')) continue;
                        if (Consume(']')) return true;
                        return false;
                    }
                }

                bool ParsePlayer(players::Player& p)
                {
                    p = players::Player{};
                    if (!Consume('{')) return false;
                    SkipWs();
                    if (Consume('}')) return true;
                    while (true)
                    {
                        SkipWs();
                        std::string key;
                        if (!ParseString(key)) return false;
                        SkipWs();
                        if (!Consume(':')) return false;
                        SkipWs();

                        if      (key == "name")        { if (!ParseString(p.name)) return false; }
                        else if (key == "race")        { if (!ParseString(p.race)) return false; }
                        else if (key == "char_class")  { if (!ParseString(p.char_class)) return false; }
                        else if (key == "background")  { if (!ParseString(p.background)) return false; }
                        else if (key == "alignment")   { if (!ParseString(p.alignment)) return false; }
                        else if (key == "speed")       { if (!ParseString(p.speed)) return false; }
                        else if (key == "ac_bonus")    { if (!ParseString(p.ac_bonus)) return false; }
                        else if (key == "hit_dice")    { if (!ParseString(p.hit_dice)) return false; }
                        else if (key == "equipment")   { if (!ParseString(p.equipment)) return false; }
                        else if (key == "features")    { if (!ParseString(p.features)) return false; }
                        else if (key == "level")       { if (!ParseNumber(p.level)) return false; }
                        else if (key == "ac")          { if (!ParseNumber(p.ac)) return false; }
                        else if (key == "hp")          { if (!ParseNumber(p.hp)) return false; }
                        else if (key == "str")         { if (!ParseNumber(p.str)) return false; }
                        else if (key == "dex")         { if (!ParseNumber(p.dex)) return false; }
                        else if (key == "con")         { if (!ParseNumber(p.con)) return false; }
                        else if (key == "intel")       { if (!ParseNumber(p.intel)) return false; }
                        else if (key == "wis")         { if (!ParseNumber(p.wis)) return false; }
                        else if (key == "cha")         { if (!ParseNumber(p.cha)) return false; }
                        else if (key == "saves")       { if (!ParseStrings(p.proficient_saves)) return false; }
                        else if (key == "skills")      { if (!ParseStrings(p.proficient_skills)) return false; }
                        else { if (!SkipValue()) return false; }

                        SkipWs();
                        if (Consume(',')) continue;
                        if (Consume('}')) return true;
                        return false;
                    }
                }

                bool ParseBookmark(bookmark::Bookmark& mark)
                {
                    if (!Consume('{')) return false;
                    SkipWs();
                    if (Consume('}')) return true;
                    while (true)
                    {
                        SkipWs();
                        std::string key;
                        if (!ParseString(key)) return false;
                        SkipWs();
                        if (!Consume(':')) return false;
                        SkipWs();

                        if (key == "id")        { if (!ParseString(mark.id)) return false; }
                        else if (key == "line") { if (!ParseNumber(mark.line)) return false; }
                        else { if (!SkipValue()) return false; }

                        SkipWs();
                        if (Consume(',')) continue;
                        if (Consume('}')) return true;
                        return false;
                    }
                }

                bool ParseObject(TreeNode& node)
                {
                    node = TreeNode{};
                    if (!Consume('{')) return false;
                    SkipWs();
                    if (Consume('}')) return true;
                    while (true)
                    {
                        SkipWs();
                        std::string key;
                        if (!ParseString(key)) return false;
                        SkipWs();
                        if (!Consume(':')) return false;
                        SkipWs();

                        if (key == "id")       { if (!ParseString(node.id)) return false; }
                        else if (key == "name"){ if (!ParseString(node.name)) return false; }
                        else if (key == "type"){ std::string t; if (!ParseString(t)) return false; node.type = treeview::NodeTypeFromString(t); }
                        else if (key == "text") { if (!ParseString(node.text)) return false; }
                        else if (key == "expanded") { if (!ParseBool(node.expanded)) return false; }
                        else if (key == "children") { if (!ParseArray(node.children)) return false; }
                        else { if (!SkipValue()) return false; }

                        SkipWs();
                        if (Consume(',')) continue;
                        if (Consume('}')) return true;
                        return false;
                    }
                }

                bool SkipValue()
                {
                    SkipWs();
                    if (i_ >= s_.size()) return false;
                    char c = s_[i_];
                    if (c == '"') { std::string tmp; return ParseString(tmp); }
                    if (c == '[')
                    {
                        ++i_;
                        SkipWs();
                        if (Consume(']')) return true;
                        while (true)
                        {
                            if (!SkipValue()) return false;
                            SkipWs();
                            if (Consume(',')) { SkipWs(); continue; }
                            if (Consume(']')) return true;
                            return false;
                        }
                    }
                    if (c == '{')
                    {
                        ++i_;
                        SkipWs();
                        if (Consume('}')) return true;
                        while (true)
                        {
                            SkipWs();
                            std::string key;
                            if (!ParseString(key)) return false;
                            SkipWs();
                            if (!Consume(':')) return false;
                            if (!SkipValue()) return false;
                            SkipWs();
                            if (Consume(',')) continue;
                            if (Consume('}')) return true;
                            return false;
                        }
                    }
                    if (c == 't' || c == 'f') { bool b; return ParseBool(b); }
                    if (c == 'n') { if (s_.compare(i_, 4, "null") == 0) { i_ += 4; return true; } return false; }
                    if (c == '-' || (c >= '0' && c <= '9'))
                    {
                        int n;
                        return ParseNumber(n);
                    }
                    return false;
                }
        };
    }

    std::string JsonEscape(const std::string& s)
    {
        return Escape(s);
    }

    std::string Serialize(const std::vector<TreeNode>& roots, int tree_width,
                          const std::vector<bookmark::Bookmark>& bookmarks,
                          const std::vector<std::string>& history,
                          const std::vector<std::string>& presets,
                          const std::vector<players::Player>& players)
    {
        std::string out = "{\n  \"version\": 1,\n  \"tree_width\": " +
                          std::to_string(tree_width) + ",\n  \"bookmarks\": [\n";
        for (std::size_t i = 0; i < bookmarks.size(); ++i)
        {
            const bookmark::Bookmark& mark = bookmarks[i];
            out += "    {\"id\": " + Escape(mark.id);
            if (mark.line >= 0)
            {
                out += ", \"line\": " + std::to_string(mark.line);
            }
            out += "}";
            out += (i + 1 < bookmarks.size()) ? ",\n" : "\n";
        }
        out += "  ],\n  \"history\": [\n";
        for (std::size_t i = 0; i < history.size(); ++i)
        {
            out += "    " + Escape(history[i]);
            out += (i + 1 < history.size()) ? ",\n" : "\n";
        }
        out += "  ],\n  \"presets\": [\n";
        for (std::size_t i = 0; i < presets.size(); ++i)
        {
            out += "    " + Escape(presets[i]);
            out += (i + 1 < presets.size()) ? ",\n" : "\n";
        }
        out += "  ],\n  \"players\": [\n";
        for (std::size_t i = 0; i < players.size(); ++i)
        {
            AppendPlayer(out, players[i]);
            out += (i + 1 < players.size()) ? ",\n" : "\n";
        }
        out += "  ],\n  \"roots\": [\n";
        for (std::size_t i = 0; i < roots.size(); ++i)
        {
            AppendNode(out, roots[i], 1);
            out += (i + 1 < roots.size()) ? ",\n" : "\n";
        }
        out += "  ]\n}\n";
        return out;
    }

    bool Deserialize(const std::string& json, std::vector<TreeNode>& roots,
                     int* tree_width, std::vector<bookmark::Bookmark>* bookmarks,
                     std::vector<std::string>* history, std::vector<std::string>* presets,
                     std::vector<players::Player>* players)
    {
        Parser parser(json);
        return parser.Deserialize(roots, tree_width, bookmarks, history, presets, players);
    }

    bool WriteFile(const std::string& path, const std::string& content)
    {
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open()) return false;
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        return static_cast<bool>(out);
    }

    bool ReadFile(const std::string& path, std::string& content)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) return false;
        std::ostringstream ss;
        ss << in.rdbuf();
        if (in.bad()) return false;
        content = ss.str();
        return true;
    }

    bool SaveDocumentFile(const std::string& path, const std::string& json)
    {
        return WriteFile(path, std::string(kDocumentMagic) + json);
    }

    LoadStatus LoadDocumentFile(const std::string& path, std::string& json)
    {
        std::string content;
        if (!ReadFile(path, content))
        {
            return LoadStatus::NotFound;
        }
        if (content.rfind(kDocumentMagic, 0) != 0)
        {
            return LoadStatus::NotTerminadventure;
        }
        json = content.substr(std::char_traits<char>::length(kDocumentMagic));
        return LoadStatus::Ok;
    }
}
