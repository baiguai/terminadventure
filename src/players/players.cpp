#include "players.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace terminadventure::players
{
    namespace
    {
        std::string Trim(const std::string& s)
        {
            std::size_t b = 0, e = s.size();
            while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
            while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
            return s.substr(b, e - b);
        }

        // Splits `s` on the literal character `sep`. Empty segments are dropped
        // (so "a,,b" -> a,b). Leading/trailing segments are trimmed.
        std::vector<std::string> Split(const std::string& s, char sep)
        {
            std::vector<std::string> out;
            std::string cur;
            for (char c : s)
            {
                if (c == sep)
                {
                    out.push_back(Trim(cur));
                    cur.clear();
                }
                else
                {
                    cur += c;
                }
            }
            out.push_back(Trim(cur));
            out.erase(std::remove(out.begin(), out.end(), ""), out.end());
            return out;
        }

        // Parses a feature-list field: "1:Rage|desc^2:Unarmored Defense|desc"
        std::vector<ClassFeature> ParseFeatures(const std::string& field)
        {
            std::vector<ClassFeature> out;
            for (const std::string& item : Split(field, '^'))
            {
                const std::size_t colon = item.find(':');
                if (colon == std::string::npos) continue;
                int level = 0;
                try { level = std::stoi(item.substr(0, colon)); } catch (...) { continue; }
                const std::string rest = item.substr(colon + 1);
                const std::size_t bar = rest.find('|');
                ClassFeature f;
                f.level = level;
                if (bar == std::string::npos)
                {
                    f.name = Trim(rest);
                }
                else
                {
                    f.name = Trim(rest.substr(0, bar));
                    f.desc = Trim(rest.substr(bar + 1));
                }
                out.push_back(std::move(f));
            }
            return out;
        }

        // Parses a race ability field: "STR+1 DEX+1 CON+1 ..."
        std::map<std::string, int> ParseAbilities(const std::string& field)
        {
            std::map<std::string, int> out;
            std::istringstream ss(field);
            std::string tok;
            while (ss >> tok)
            {
                const std::size_t pos = tok.find_first_of("+-");
                if (pos == std::string::npos) continue;
                std::string stat = Trim(tok.substr(0, pos));
                std::string val = tok.substr(pos);
                try { out[stat] = std::stoi(val); } catch (...) {}
            }
            return out;
        }

        int ToInt(const std::string& s, int def)
        {
            try { return std::stoi(s); } catch (...) { return def; }
        }

        bool LoadRaces(DnDData& d, const std::string& line)
        {
            const std::size_t bar = line.find('|');
            if (bar == std::string::npos) return false;
            RaceData race;
            const std::string name = Trim(line.substr(0, bar));
            const std::string rest = line.substr(bar + 1);
            const auto fields = Split(rest, '|');
            // fields: [speed, abilities, features]
            if (fields.empty()) return false;
            race.speed = ToInt(fields[0], 30);
            if (fields.size() > 1) race.ability = ParseAbilities(fields[1]);
            if (fields.size() > 2) race.features = Split(fields[2], '^');
            d.races[name] = std::move(race);
            return true;
        }

        bool LoadClass(DnDData& d, const std::string& line)
        {
            const std::size_t bar = line.find('|');
            if (bar == std::string::npos) return false;
            ClassData c;
            const std::string name = Trim(line.substr(0, bar));
            const std::string rest = line.substr(bar + 1);
            const auto fields = Split(rest, '|');
            // fields: [hitdie, saves, skills, equipment, features]
            if (fields.empty()) return false;
            c.hit_die = ToInt(fields[0], 8);
            if (fields.size() > 1) c.saves = Split(fields[1], ',');
            if (fields.size() > 2) c.class_skills = Split(fields[2], ',');
            if (fields.size() > 3) c.equipment = Split(fields[3], ',');
            if (fields.size() > 4) c.features = ParseFeatures(fields[4]);
            d.classes[name] = std::move(c);
            return true;
        }
    }

    int AbilityMod(int score)
    {
        return static_cast<int>(std::floor((score - 10) / 2.0));
    }

    int ProficiencyBonus(int level)
    {
        return static_cast<int>(std::ceil(1.0 + level / 4.0));
    }

    std::string FormatMod(int mod)
    {
        return mod >= 0 ? "+" + std::to_string(mod) : std::to_string(mod);
    }

    std::shared_ptr<DnDData> LoadDnDData(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open()) return nullptr;

        auto data = std::make_shared<DnDData>();
        std::string section;
        std::string line;

        // Buffers for list-only sections (alignments).
        std::vector<std::string>& alignments = data->alignments;

        while (std::getline(file, line))
        {
            line = Trim(line);
            if (line.empty()) continue;
            if (line[0] == '#') continue;

            if (line.front() == '[' && line.back() == ']')
            {
                section = line.substr(1, line.size() - 2);
                section = Trim(section);
                continue;
            }

            if (section == "alignments")
            {
                alignments.push_back(line);
                continue;
            }

            if (section == "skills")
            {
                const std::size_t bar = line.find('|');
                if (bar == std::string::npos) continue;
                data->skills[Trim(line.substr(0, bar))] = Trim(line.substr(bar + 1));
                continue;
            }

            if (section == "backgrounds")
            {
                const std::size_t bar = line.find('|');
                if (bar == std::string::npos) continue;
                const std::string name = Trim(line.substr(0, bar));
                const std::string equip = line.substr(bar + 1);
                data->backgrounds[name] = Split(equip, '^');
                continue;
            }

            if (section == "races")
            {
                if (!LoadRaces(*data, line)) continue;
                continue;
            }

            if (section == "classes")
            {
                if (!LoadClass(*data, line)) continue;
                continue;
            }

            // Unknown section: ignore the line.
        }

        return data;
    }
}
