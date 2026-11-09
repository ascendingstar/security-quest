// Награды как в игре: наградная плашка победного экрана — счётчики опыта и золота, трофей, новый уровень.

#include "app.h"

#include "draw.h"
#include "theme.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace sq::gui {

namespace {

// Число растёт от нуля за 0.8 секунды — как счётчик добычи.
int count_up(int value, float age) {
    return int(std::lround(float(value) * ease(age / 0.8f)));
}

void rays(ImDrawList* dl, ImVec2 c, float r, float spin, ImU32 color) {
    for (int k = 0; k < 12; ++k) {
        float a = spin + float(k) * kPi / 6;
        ImVec2 tip(c.x + std::cos(a) * r, c.y + std::sin(a) * r);
        ImVec2 l(c.x + std::cos(a - 0.09f) * r * 0.15f, c.y + std::sin(a - 0.09f) * r * 0.15f);
        ImVec2 rr(c.x + std::cos(a + 0.09f) * r * 0.15f, c.y + std::sin(a + 0.09f) * r * 0.15f);
        dl->AddTriangleFilled(l, tip, rr, color);
    }
}

}  // namespace

float loot_height(const Loot& loot) {
    return px(56) + (loot.trophy.empty() ? 0 : px(58)) + (loot.level_up ? px(46) : 0);
}

float draw_loot(ImDrawList* dl, ImVec2 c, float w, const Loot& loot, float age, float a) {
    float y = c.y;
    // Опыт и золото: значок, число книжным шрифтом, подпись.
    float vs = 30.0f * ui_scale;
    std::string xp = std::format("+{}", count_up(loot.xp, age - 0.15f)), gold = std::format("+{}", count_up(loot.gold, age - 0.3f));
    float wx = fonts.display->CalcTextSizeA(vs, FLT_MAX, 0, xp.c_str()).x, wg = fonts.display->CalcTextSizeA(vs, FLT_MAX, 0, gold.c_str()).x;
    float gap = px(40), icon = px(30);
    // Пустые строки награды не показываем: у рубежа, например, есть только золото.
    float total = (loot.xp ? icon + wx + px(70) : 0) + (loot.xp && loot.gold ? gap : 0) + (loot.gold ? icon + wg + px(70) : 0);
    float x = c.x - total * 0.5f;
    float pop = 1.0f + 0.25f * std::max(0.0f, 1.0f - std::abs(age - 1.0f) * 4);  // числа «вспыхивают», когда досчитали
    auto stat = [&](bool is_gold, const std::string& v, float vw, const char* label) {
        ImVec2 ic(x + icon * 0.5f, y + px(22));
        if (is_gold) coin(dl, ic, px(13) * pop);
        else xp_gem(dl, ic, px(14) * pop);
        dl->AddText(fonts.display, vs, ImVec2(x + icon + px(8), y + px(22) - vs * 0.55f), alpha(is_gold ? col::gold : col::teal, a), v.c_str());
        dl->AddText(fonts.regular, 15.0f * ui_scale, ImVec2(x + icon + px(14) + vw, y + px(22) - px(6)), alpha(col::dim, a), label);
        x += icon + vw + px(70) + gap;
    };
    if (loot.xp) stat(false, xp, wx, "опыта");
    if (loot.gold) stat(true, gold, wg, "золота");
    y += px(56);

    // Трофей: вспышка лучей и имя с эффектом.
    if (!loot.trophy.empty()) {
        float t = std::clamp((age - 1.1f) / 0.4f, 0.0f, 1.0f);
        if (t > 0) {
            ImVec2 sc(c.x - w * 0.5f + px(34), y + px(24));
            rays(dl, sc, px(34) * ease(t), age * 0.8f, alpha(IM_COL32(200, 160, 255, 255), 0.35f * a * t));
            text_center(dl, fonts.regular, 30.0f * ui_scale * (0.6f + 0.4f * ease(t)), sc, alpha(col::violet, a * t), icon::star);
            std::string name = "Трофей: " + loot.trophy;
            dl->AddText(fonts.display, 21.0f * ui_scale, ImVec2(sc.x + px(32), y + px(6)), alpha(col::violet, a * t), name.c_str());
            dl->AddText(fonts.regular, 14.5f * ui_scale, ImVec2(sc.x + px(32), y + px(32)), alpha(col::dim, a * t), loot.trophy_effect.c_str());
        }
        y += px(58);
    }

    // Новый уровень: щит с номером и надпись.
    if (loot.level_up) {
        float t = std::clamp((age - (loot.trophy.empty() ? 1.1f : 1.5f)) / 0.35f, 0.0f, 1.0f);
        if (t > 0) {
            std::string text = std::format("Уровень {}! Армия стала сильнее", loot.level_up);
            float ts = 22.0f * ui_scale;
            float tw = fonts.display->CalcTextSizeA(ts, FLT_MAX, 0, text.c_str()).x;
            ImVec2 sc(c.x - tw * 0.5f - px(22), y + px(20));
            float s = px(15) * (2.0f - ease(t));
            poly(dl, sc, s, {{-0.9f, -0.9f}, {0.9f, -0.9f}, {0.85f, 0.15f}, {0, 1.0f}, {-0.85f, 0.15f}}, alpha(IM_COL32(150, 30, 34, 255), a * t),
                 alpha(col::gold, a * t), px(1.5f));
            text_center(dl, fonts.display, 15.0f * ui_scale, ImVec2(sc.x, sc.y - px(1)), alpha(IM_COL32(255, 236, 200, 255), a * t), std::to_string(loot.level_up).c_str());
            dl->AddText(fonts.display, ts, ImVec2(sc.x + px(24), y + px(20) - ts * 0.55f), alpha(col::teal, a * t), text.c_str());
        }
        y += px(46);
    }
    return y - c.y;
}

}  // namespace sq::gui
