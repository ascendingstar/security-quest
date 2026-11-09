// Награды как в игре: наградная плашка победного экрана и сценка выполненного побочного квеста.

#include "app.h"

#include "draw.h"
#include "theme.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace sq::gui {

namespace {

constexpr ImU32 kInk = IM_COL32(74, 44, 22, 255);
constexpr ImU32 kParchment = IM_COL32(236, 218, 176, 255);

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

// Сундук: крышка открывается (open 0..1), у сложного квеста — золотая оковка.
void chest(ImDrawList* dl, ImVec2 feet, float s, float open, int tier) {
    ImU32 wood = IM_COL32(126, 76, 40, 255), dark = IM_COL32(78, 44, 22, 255), band = tier >= 3 ? col::gold : IM_COL32(150, 150, 160, 255);
    if (tier <= 1) {
        // Кошель: мешочек с завязкой.
        dl->AddEllipseFilled(ImVec2(feet.x, feet.y + s * 0.03f), ImVec2(s * 0.42f, s * 0.08f), IM_COL32(0, 0, 0, 90));
        dl->AddEllipseFilled(ImVec2(feet.x, feet.y - s * 0.3f), ImVec2(s * 0.34f, s * 0.3f), IM_COL32(150, 110, 70, 255));
        dl->AddTriangleFilled(ImVec2(feet.x - s * 0.12f, feet.y - s * 0.56f), ImVec2(feet.x + s * 0.12f, feet.y - s * 0.56f),
                              ImVec2(feet.x, feet.y - s * (0.68f + 0.12f * open)), IM_COL32(130, 92, 56, 255));
        dl->AddLine(ImVec2(feet.x - s * 0.13f, feet.y - s * 0.56f), ImVec2(feet.x + s * 0.13f, feet.y - s * 0.56f), col::gold, std::max(1.5f, s * 0.04f));
        return;
    }
    float w = s * 0.5f, h = s * 0.42f;
    ImVec2 a(feet.x - w, feet.y - h), b(feet.x + w, feet.y);
    dl->AddEllipseFilled(ImVec2(feet.x, feet.y + s * 0.03f), ImVec2(w * 1.15f, s * 0.08f), IM_COL32(0, 0, 0, 100));
    dl->AddRectFilled(a, b, wood, s * 0.03f);
    dl->AddRect(a, b, dark, s * 0.03f, std::max(1.0f, s * 0.02f));
    for (float x : {-0.7f, 0.7f}) dl->AddRectFilled(ImVec2(feet.x + x * w - s * 0.04f, a.y), ImVec2(feet.x + x * w + s * 0.04f, b.y), band);
    // Свет изнутри, когда крышка поднята.
    if (open > 0.05f) dl->AddRectFilledMultiColor(ImVec2(a.x + s * 0.04f, a.y - s * 0.02f), ImVec2(b.x - s * 0.04f, a.y + s * 0.1f),
                                                  alpha(IM_COL32(255, 220, 120, 255), open), alpha(IM_COL32(255, 220, 120, 255), open),
                                                  alpha(IM_COL32(255, 180, 60, 0), open), alpha(IM_COL32(255, 180, 60, 0), open));
    // Крышка: закрытая — выпуклый верх с замком; открытая — откинута назад, видна её тёмная изнанка.
    float lid = s * 0.22f;
    if (open <= 0.05f) {
        ImVec2 la(a.x - s * 0.02f, a.y - lid), lb(b.x + s * 0.02f, a.y);
        dl->AddRectFilled(la, lb, wood, s * 0.1f, ImDrawFlags_RoundTop);
        dl->AddRect(la, lb, dark, s * 0.1f, std::max(1.0f, s * 0.02f), ImDrawFlags_RoundTop);
        dl->AddRectFilled(ImVec2(feet.x - s * 0.06f, a.y - s * 0.05f), ImVec2(feet.x + s * 0.06f, a.y + s * 0.1f), col::gold, s * 0.02f);
    } else {
        ImVec2 la(a.x + s * 0.02f, a.y - lid * (1.0f + 1.5f * open)), lb(b.x - s * 0.02f, a.y - s * 0.02f);
        dl->AddRectFilled(la, lb, IM_COL32(60, 34, 18, 255), s * 0.06f, ImDrawFlags_RoundTop);
        dl->AddRect(la, lb, band, s * 0.06f, std::max(1.0f, s * 0.02f), ImDrawFlags_RoundTop);
    }
}

// Домики вдоль горизонта с огоньками в окнах — городская площадь в сумерках.
void town_skyline(ImDrawList* dl, ImVec2 p0, ImVec2 size, float horizon) {
    ImU32 dark = IM_COL32(30, 22, 30, 255);
    float x = p0.x - size.x * 0.02f;
    for (int k = 0; x < p0.x + size.x; ++k) {
        uint32_t h = hash_of(std::format("house{}", k));
        float w = size.x * (0.03f + float(h % 25) / 1000.0f), hh = size.y * (0.045f + float((h >> 8) % 45) / 1000.0f);
        if (k == 5) {  // башня Цитадели
            w = size.x * 0.035f;
            hh = size.y * 0.17f;
        }
        dl->AddRectFilled(ImVec2(x, horizon - hh), ImVec2(x + w, horizon + 2), dark);
        dl->AddTriangleFilled(ImVec2(x - w * 0.08f, horizon - hh), ImVec2(x + w * 1.08f, horizon - hh), ImVec2(x + w * 0.5f, horizon - hh - w * 0.55f), dark);
        if ((h >> 16) % 3 != 0)
            dl->AddRectFilled(ImVec2(x + w * 0.42f, horizon - hh * 0.6f), ImVec2(x + w * 0.58f, horizon - hh * 0.6f + w * 0.18f), IM_COL32(255, 196, 96, 220));
        x += w + size.x * (0.004f + float((h >> 20) % 20) / 1000.0f);
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

void App::quest_scene_view() {
    QuestScene& q = quest_scene_;
    float age = float(clock() - q.born);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    ImGui::Dummy(size);
    float W = size.x, H = size.y;
    dl->PushClipRect(p0, add(p0, size), true);

    // Площадь у Цитадели в сумерках.
    draw_scene(dl, p0, size, IM_COL32(214, 150, 90, 255));
    float horizon = p0.y + H * 0.6f;
    town_skyline(dl, p0, size, horizon);
    dl->AddRectFilledMultiColor(ImVec2(p0.x, horizon), add(p0, size), IM_COL32(60, 46, 40, 255), IM_COL32(60, 46, 40, 255), IM_COL32(28, 22, 22, 255),
                                IM_COL32(28, 22, 22, 255));
    float ground = p0.y + H * 0.9f;

    // Герой выходит на площадь.
    quest::Stack hero;
    hero.icon = "hero";
    hero.count = 1;
    hero.elite = true;
    float fig = std::clamp(H * 0.22f, 60.0f, 160.0f);
    float hx = p0.x + W * (0.08f + 0.2f * ease(age / 0.9f));
    draw_figure(dl, ImVec2(hx, ground), fig, hero, true, float(clock()));

    // Сундук (кошель у лёгкого квеста): открывается, из него бьют лучи и летят монеты.
    int tier = std::clamp(q.difficulty, 1, 3);
    float cs = fig * (0.55f + 0.15f * float(tier));
    ImVec2 cf(p0.x + W * 0.6f, ground);
    float open = std::clamp((age - 1.3f) / 0.35f, 0.0f, 1.0f);
    if (open > 0) rays(dl, ImVec2(cf.x, cf.y - cs * 0.5f), cs * (1.2f + 0.3f * std::sin(age * 2)), age * 0.4f, alpha(IM_COL32(255, 210, 120, 255), 0.12f * ease(open)));
    chest(dl, cf, cs, ease(open), tier);
    int coins = 8 + 10 * (tier - 1);
    for (int k = 0; k < coins; ++k) {
        float t = age - 1.4f - float(k) * 0.035f;
        if (t <= 0) continue;
        uint32_t h = hash_of(std::format("coin{}", k));
        float vx = (float(h % 200) / 100.0f - 1.0f) * W * 0.22f, vy = -H * (0.55f + float((h >> 8) % 40) / 100.0f), g = H * 1.9f;
        ImVec2 p(cf.x + vx * t, cf.y - cs * 0.45f + vy * t + 0.5f * g * t * t);
        if (p.y > ground - px(4)) p.y = ground - px(4) - float((h >> 16) % 6);
        float fade = std::clamp(1.0f - (t - 2.4f) / 0.6f, 0.0f, 1.0f);
        if (fade <= 0) continue;
        float r = px(6) * (0.8f + float((h >> 20) % 5) / 10.0f);
        dl->AddCircleFilled(p, r, alpha(col::gold_dark, fade));
        dl->AddCircleFilled(ImVec2(p.x - r * 0.15f, p.y - r * 0.15f), r * 0.75f, alpha(col::gold, fade));
    }

    // Свиток задания разворачивается, проступает текст, ложится сургучная печать.
    float unroll = ease((age - 0.25f) / 0.6f);
    ImVec2 sc(p0.x + W * 0.5f, p0.y + H * 0.2f);
    float sw = std::min(W * 0.62f, px(620)) * unroll, sh = px(118);
    if (unroll > 0.02f) {
        ImVec2 a(sc.x - sw * 0.5f, sc.y - sh * 0.5f), b(sc.x + sw * 0.5f, sc.y + sh * 0.5f);
        dl->AddRectFilled(ImVec2(a.x + px(4), a.y + px(6)), ImVec2(b.x + px(4), b.y + px(6)), IM_COL32(0, 0, 0, 90));
        dl->AddRectFilledMultiColor(a, b, kParchment, kParchment, IM_COL32(214, 190, 140, 255), IM_COL32(214, 190, 140, 255));
        for (float x : {a.x, b.x}) {  // скрутки по краям
            dl->AddRectFilledMultiColor(ImVec2(x - px(9), a.y - px(8)), ImVec2(x + px(9), b.y + px(8)), IM_COL32(170, 140, 96, 255),
                                        IM_COL32(236, 218, 176, 255), IM_COL32(236, 218, 176, 255), IM_COL32(170, 140, 96, 255));
            dl->AddRect(ImVec2(x - px(9), a.y - px(8)), ImVec2(x + px(9), b.y + px(8)), IM_COL32(120, 90, 56, 255), px(6), 1.0f);
        }
        float ta = std::clamp((age - 0.75f) / 0.4f, 0.0f, 1.0f);
        dl->PushClipRect(a, b, true);
        static const char* kinds[3] = {"Лёгкий побочный квест", "Побочный квест", "Сложный побочный квест"};
        text_center(dl, fonts.display, 15.0f * ui_scale, ImVec2(sc.x, a.y + px(22)), alpha(IM_COL32(130, 90, 50, 255), ta), kinds[tier - 1]);
        std::string title = q.title;
        float ts = 24.0f * ui_scale;
        while (fonts.display->CalcTextSizeA(ts, FLT_MAX, 0, title.c_str()).x > sw - px(70) && ts > 14.0f * ui_scale) ts -= 1.0f;
        text_center(dl, fonts.display, ts, ImVec2(sc.x, sc.y + px(2)), alpha(kInk, ta), title.c_str());
        text_center(dl, fonts.regular, 15.0f * ui_scale, ImVec2(sc.x, b.y - px(22)), alpha(IM_COL32(110, 74, 40, 255), ta), "Выполнено. Цитадель благодарит Хранителя.");
        dl->PopClipRect();
        // Печать падает сверху и «впечатывается».
        float st = std::clamp((age - 1.0f) / 0.25f, 0.0f, 1.0f);
        if (st > 0) {
            float k = 1.0f + 1.4f * (1.0f - ease(st));
            ImVec2 seal(b.x - px(46), b.y - px(10));
            float r = px(24) * k;
            for (int i = 0; i < 9; ++i) {
                float an = float(i) / 9.0f * 2 * kPi;
                dl->AddCircleFilled(ImVec2(seal.x + std::cos(an) * r * 0.8f, seal.y + std::sin(an) * r * 0.8f), r * 0.3f, alpha(IM_COL32(130, 26, 26, 255), st));
            }
            dl->AddCircleFilled(seal, r * 0.86f, alpha(IM_COL32(168, 36, 34, 255), st));
            dl->AddCircle(seal, r * 0.6f, alpha(IM_COL32(220, 90, 80, 255), st), 0, px(1.5f));
            text_center(dl, fonts.regular, r * 0.8f, seal, alpha(IM_COL32(255, 220, 200, 230), st), icon::check);
        }
    }

    // Награда под свитком.
    float la = std::clamp((age - 1.5f) / 0.3f, 0.0f, 1.0f);
    if (la > 0) {
        float lh = loot_height(q.loot);
        ImVec2 lc(p0.x + W * 0.5f, sc.y + sh * 0.5f + px(26));
        float lw = std::min(W * 0.6f, px(560));
        dl->AddRectFilled(ImVec2(lc.x - lw * 0.5f, lc.y - px(8)), ImVec2(lc.x + lw * 0.5f, lc.y + lh + px(4)), alpha(IM_COL32(20, 14, 18, 255), 0.82f * la), px(8));
        ornate_frame(dl, ImVec2(lc.x - lw * 0.5f, lc.y - px(8)), ImVec2(lc.x + lw * 0.5f, lc.y + lh + px(4)), col::border, px(8));
        draw_loot(dl, lc, lw, q.loot, age - 1.5f, la);
    }
    dl->PopClipRect();

    ImGui::SetCursorScreenPos(ImVec2(p0.x + W * 0.5f - px(90), p0.y + H - px(60)));
    if (accent_button(age < 1.5f ? "Пропустить" : "Дальше", ImVec2(px(180), px(40)))) {
        if (age < 1.5f) q.born = clock() - 1.5;
        else q.active = false;
    }
}

}  // namespace sq::gui
