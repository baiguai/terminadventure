#pragma once

#include <map>
#include <string>
#include <vector>

namespace terminadventure::players
{
    // A single D&D 5E character. `proficient_saves` and `proficient_skills`
    // hold the names of the saving throws / skills the character is
    // proficient in.
    struct Player
    {
        std::string name;
        std::string race;
        std::string char_class;
        std::string background;
        std::string alignment;
        int level = 1;
        int ac = 10;
        int hp = 10;
        std::string speed = "30 ft.";
        int str = 10;
        int dex = 10;
        int con = 10;
        int intel = 10;
        int wis = 10;
        int cha = 10;
        std::string ac_bonus = "+0";
        std::string hit_dice = "1d8";
        std::string equipment;
        std::string features;
        std::vector<std::string> proficient_saves;
        std::vector<std::string> proficient_skills;
    };

    inline constexpr const char* kAbilityNames[6] = { "STR", "DEX", "CON", "INT", "WIS", "CHA" };

    struct ClassFeature
    {
        int level = 1;
        std::string name;
        std::string desc;
    };

    struct ClassData
    {
        int hit_die = 8;
        std::vector<std::string> saves;
        std::vector<std::string> class_skills;
        std::vector<std::string> equipment;
        std::vector<ClassFeature> features;
    };

    struct RaceData
    {
        int speed = 30;
        std::map<std::string, int> ability;
        std::vector<std::string> features;
    };

    struct DnDData
    {
        std::map<std::string, ClassData> classes;
        std::map<std::string, RaceData> races;
        std::map<std::string, std::vector<std::string>> backgrounds;
        std::map<std::string, std::string> skills; // skill name -> ability (STR/DEX/...)
        std::vector<std::string> alignments;
    };
}
