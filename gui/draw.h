#pragma once

// Мелкие помощники векторной графики, общие для карты, города и боя.

#include "theme.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

namespace sq::quest {
struct Stack;
}

namespace sq::gui {

// Фигурка бойца или монстра — та же, что на поле боя (battle.cpp). feet — точка между ступнями.
void draw_figure(ImDrawList* dl, ImVec2 feet, float h, const quest::Stack& s, bool ours, float time, int seed = 0);
// Маленькая сцена: небо и земля в цвете главы — фон для портретов врагов.
void draw_scene(ImDrawList* dl, ImVec2 p0, ImVec2 size, uint32_t color);
// Эмблема программы: щит с замочной скважиной и скрещённые мечи, как на значке exe.
void draw_emblem(ImDrawList* dl, ImVec2 center, float size);

// Награда на победном экране: опыт и золото считаются вверх, трофей раскрывается, новый уровень — отдельной плашкой.
struct Loot {
    int xp = 0, gold = 0;
    std::string trophy, trophy_effect;
    int level_up = 0;  // новый уровень героя или 0
};
// Рисует награду по центру c шириной w; age — секунды с появления, a — прозрачность. Возвращает высоту.
float draw_loot(ImDrawList* dl, ImVec2 c, float w, const Loot& loot, float age, float a);
float loot_height(const Loot& loot);

inline constexpr float kPi = std::numbers::pi_v<float>;

inline const ImU32 kSpellColor[5] = {IM_COL32(255, 140, 60, 255), IM_COL32(244, 210, 110, 255), IM_COL32(255, 250, 220, 255),
                                     IM_COL32(120, 190, 255, 255), IM_COL32(200, 140, 255, 255)};
inline constexpr ImU32 kOurs = IM_COL32(52, 92, 168, 255);

inline ImU32 mix(ImU32 a, ImU32 b, float t) {
    ImVec4 x = ImGui::ColorConvertU32ToFloat4(a), y = ImGui::ColorConvertU32ToFloat4(b);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(x.x + (y.x - x.x) * t, x.y + (y.y - x.y) * t, x.z + (y.z - x.z) * t, x.w + (y.w - x.w) * t));
}

// Земля главы — её цвет из файла кампании, приглушённый к тёмному фону.
inline ImU32 land(uint32_t color) {
    return mix(color ? color : IM_COL32(150, 150, 160, 255), IM_COL32(26, 30, 28, 255), 0.72f);
}

inline ImU32 tint(uint32_t color) {
    return color ? color : IM_COL32(210, 90, 180, 255);
}

inline float ease(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}

inline ImVec2 add(ImVec2 a, ImVec2 b) { return ImVec2(a.x + b.x, a.y + b.y); }
inline ImVec2 lerp(ImVec2 a, ImVec2 b, float t) { return ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t); }
inline ImVec2 at(ImVec2 c, float r, float x, float y) { return ImVec2(c.x + x * r, c.y + y * r); }

inline void poly(ImDrawList* dl, ImVec2 c, float r, std::initializer_list<ImVec2> pts, ImU32 fill, ImU32 edge = 0, float thick = 1.5f) {
    std::vector<ImVec2> p;
    for (auto& v : pts) p.push_back(at(c, r, v.x, v.y));
    dl->AddConcavePolyFilled(p.data(), int(p.size()), fill);
    if (edge) dl->AddPolyline(p.data(), int(p.size()), edge, thick, ImDrawFlags_Closed);
}

inline void text_center(ImDrawList* dl, ImFont* font, float size, ImVec2 c, ImU32 col, const char* text) {
    ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0, text);
    dl->AddText(font, size, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), col, text);
}

inline uint32_t hash_of(std::string_view s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) h = (h ^ c) * 16777619u;
    return h;
}

}  // namespace sq::gui
