#pragma once
#include <map>
#include <string>

// Mapeamento: CS2 Gun Icons
// Confirmado: A=AK47, C=AWP, M=Knife

static const std::map<std::string, const char*> weaponIcons = {
    // A = AK-47
    {"ak47",               "W"},

    // B = ?  (provavelmente rifle — M4A4 ou AUG)
    {"m4a1",               "T"},

    // C = AWP
    {"awp",                "Z"},

    // D-L = armas variadas (confirma no charmap se precisar)
    {"deagle",             "A"},
    {"famas",              "R"},
    {"galilar",            "Q"},
    {"glock",              "D"},
    {"hkp2000",            "E"},
    {"m4a1_silencer",      "T"},
    {"m4a1_silencer_off",  "S"},
    {"p250",               "F"},
    {"sg556",              "V"},
    {"aug",                "U"},

    // M = Knife
    {"knife",              "["},
    {"knife_t",            "]"},
    {"bayonet",            "1"},
    {"knife_flip",         "2"},
    {"knife_gut",          "3"},
    {"knife_karambit",     "4"},
    {"knife_m9_bayonet",   "5"},
    {"knife_tactical",     "6"},
    {"knife_falchion",     "0"},
    {"knife_survival_bowie","7"},
    {"knife_butterfly",    "8"},
    {"knife_push",         "9"},
    {"knife_ursus",        "["},
    {"knife_gypsy_jackknife","["},
    {"knife_stiletto",     "["},
    {"knife_widowmaker",   "["},
    {"knife_css",          "["},
    {"knife_cord",         "["},
    {"knife_canis",        "["},
    {"knife_outdoor",      "["},
    {"knife_skeleton",     "["},
    {"knife_kukri",        "["},

    // N-Z = resto das armas
    {"mac10",              "K"},
    {"mp9",                "O"},
    {"mp7",                "N"},
    {"mp5sd",              "x"},
    {"negev",              "f"},
    {"nova",               "e"},
    {"p90",                "P"},
    {"ssg08",              "a"},
    {"scar20",             "Y"},
    {"g3sg1",              "X"},
    {"sawedoff",           "c"},
    {"xm1014",             "b"},
    {"ump45",              "L"},
    {"m249",               "g"},

    // a-j = pistolas e SMGs restantes
    {"usp_silencer",       "G"},
    {"usp_silencer_off",   "G"},
    {"elite",              "B"},
    {"fiveseven",          "C"},
    {"tec9",               "H"},
    {"cz75a",              "I"},
    {"revolver",           "J"},
    {"bizon",              "M"},
    {"mag7",               "d"},

    // i-o = granadas / utilitários
    {"hegrenade",          "j"},
    {"flashbang",          "i"},
    {"smokegrenade",       "k"},
    {"molotov",            "l"},
    {"incgrenade",         "n"},
    {"decoy",              "m"},
    {"taser",              "h"},
    {"c4",                 "o"},
};

static const char* GetWeaponIcon(const std::string& rawName) {
    auto it = weaponIcons.find(rawName);
    if (it != weaponIcons.end()) return it->second;
    return "?";
}