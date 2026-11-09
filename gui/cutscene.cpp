// Катсцена выполненной задачи: армия Хранителя выходит слева, из разлома справа лезут твари, сшибка,
// заклинание героя, победная поза — и плашка с наградой. Чем больше задача, тем больше тварей и эффектов.
// Состояния нет: каждый кадр рисуется заново по секундам с начала сцены, поэтому её можно остановить
// на любом моменте для снимка.

#include "app.h"

#include "draw.h"
#include "theme.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace sq::gui {

namespace {

// Доля пройденного отрезка [a, b].
float seg(float t, float a, float b) {
    return std::clamp((t - a) / (b - a), 0.0f, 1.0f);
}

// Детерминированное «случайное» число 0..1: сцена одинакова в каждом кадре.
float rnd(int k, int salt) {
    return float(hash_of(std::format("{}:{}", salt, k)) % 1000) / 1000.0f;
}

// Секунды действия до плашки: от 2.8 за лёгкий квест до 5.1 за миссию сложности 5.
float action_time(int tier) {
    return 2.35f + 0.46f * float(tier);
}

// Затухающий импульс после момента at: для вспышек и тряски.
float pulse(float t, float at, float decay) {
    return t < at ? 0.0f : std::exp(-(t - at) * decay);
}

// Разлом: тёмная трещина в воздухе со светящейся кромкой; open 0..1.
void rift(ImDrawList* dl, ImVec2 c, float h, float open, float time, ImU32 color) {
    if (open <= 0.01f) return;
    float w = h * 0.2f * open, hh = h * 0.6f * (0.35f + 0.65f * open);
    for (int k = 4; k >= 1; --k) dl->AddEllipseFilled(c, ImVec2(w * (1.0f + 0.6f * float(k)), hh * (1.0f + 0.12f * float(k))), alpha(color, 0.07f * open));
    dl->AddEllipseFilled(c, ImVec2(w, hh), alpha(IM_COL32(16, 4, 26, 255), 0.95f * open));
    for (int k = 0; k < 3; ++k) {
        float wob = 1.0f + 0.05f * std::sin(time * (3.0f + float(k)) + float(k));
        dl->AddEllipse(c, ImVec2(w * (0.95f - 0.25f * float(k)) * wob, hh * (0.97f - 0.18f * float(k))), alpha(mix(color, IM_COL32(255, 255, 255, 255), 0.25f * float(k)), open * (0.9f - 0.25f * float(k))),
                       0, 0, 2.0f);
    }
}

// Молния с неба: ломаная дрожит, меняясь 30 раз в секунду.
void lightning(ImDrawList* dl, ImVec2 top, ImVec2 bottom, float width, ImU32 color, float a, float time) {
    ImVec2 pts[10];
    auto seed = int(time * 30);
    for (int k = 0; k < 10; ++k) {
        pts[k] = lerp(top, bottom, float(k) / 9.0f);
        if (k > 0 && k < 9) pts[k].x += (rnd(k, seed) - 0.5f) * width * 3.0f;
    }
    dl->AddPolyline(pts, 10, alpha(color, 0.25f * a), width * 3, 0);
    dl->AddPolyline(pts, 10, alpha(color, 0.85f * a), width, 0);
    dl->AddPolyline(pts, 10, alpha(IM_COL32(255, 255, 255, 255), a), std::max(1.5f, width * 0.35f), 0);
}

// Сноп искр из точки c, начиная с момента at: разлетаются и падают за life секунд.
void sparks(ImDrawList* dl, ImVec2 c, float t, float at, int count, float speed, float life, ImU32 color, int salt) {
    float dt = t - at;
    if (dt < 0 || dt > life) return;
    float fade = 1.0f - dt / life;
    for (int k = 0; k < count; ++k) {
        float ang = rnd(k, salt) * 2 * kPi, sp = speed * (0.4f + 0.6f * rnd(k, salt + 1));
        ImVec2 v(std::cos(ang) * sp, std::sin(ang) * sp - speed * 0.4f);
        ImVec2 p(c.x + v.x * dt, c.y + v.y * dt + speed * 1.4f * dt * dt);
        dl->AddLine(p, ImVec2(p.x - v.x * 0.03f, p.y - v.y * 0.03f), alpha(k % 3 ? color : IM_COL32(255, 250, 230, 255), fade), 2.5f);
    }
}

}  // namespace

void App::cutscene_view() {
    Cutscene& cs = cutscene_;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos(), size = ImGui::GetContentRegionAvail();
    ImGui::Dummy(size);
    float W = size.x, H = size.y;
    float time = float(clock());
    float t = cs.frozen >= 0 ? cs.frozen : float(clock() - cs.born);
    int tier = cs.tier;
    float D = action_time(tier);
    bool boss = !cs.foes.empty() && cs.foes.back().icon == "boss";
    uint32_t chapter = cs.foes.empty() ? 0 : cs.foes.front().color;
    ImU32 rift_color = tint(chapter);

    // Ключевые моменты, в долях действия.
    float clash = D * 0.42f, cast = D * 0.56f, blast = D * 0.64f, finisher = D * 0.8f, victory = D * 0.84f;
    bool spell = tier >= 3, sky = tier >= 4;
    float shake = 6.0f * pulse(t, clash, 7) + (spell ? 5.0f * pulse(t, blast, 7) : 0) + (boss ? 14.0f * pulse(t, finisher, 5) : 0);
    float flash = (sky ? 0.5f * pulse(t, blast, 5) : 0) + (boss ? (tier >= 6 ? 1.0f : 0.8f) * pulse(t, finisher, 4) : 0);

    ImVec2 p0(origin.x + std::sin(time * 71.0f) * shake, origin.y + std::cos(time * 63.0f) * shake);
    dl->PushClipRect(origin, add(origin, size), true);
    draw_battlefield(dl, p0, size, chapter, time);

    float base = p0.y + H * 0.8f;
    float fig = std::min(H * 0.17f, W * 0.075f);
    float enter = ease(seg(t, 0, D * 0.3f)), charge = ease(seg(t, D * 0.3f, clash));
    float cheer = t > victory ? std::abs(std::sin((t - victory) * 6.0f)) * fig * 0.08f : 0;

    // Разлом открывается в начале и схлопывается после победы.
    ImVec2 rift_c(p0.x + W * 0.88f, base - fig * 1.0f);
    float open = ease(seg(t, 0, D * 0.2f)) * (1.0f - ease(seg(t, victory, victory + 0.6f)));
    rift(dl, rift_c, fig * 2.2f, open, time, rift_color);

    // --- Твари: отряды-колонны у разлома, предводитель позади. Смерть каждой — по расписанию.
    struct Foe {
        const quest::Stack* s;
        ImVec2 home;
        float h, dies;
        int seed;
        bool by_spell;  // добивает заклинание героя
    };
    std::vector<Foe> foes;
    int minions = 0, groups = 0;
    for (auto& s : cs.foes)
        if (s.icon != "boss") {
            minions += std::min(s.count, 4);
            ++groups;
        }
    int m = 0, g = 0;
    for (auto& s : cs.foes) {
        if (s.icon == "boss") {
            foes.push_back({&s, ImVec2(p0.x + W * 0.79f, base - H * 0.03f), fig * 1.9f, finisher, 99, false});
            continue;
        }
        bool last_group = g == groups - 1;
        for (int k = 0; k < std::min(s.count, 4); ++k, ++m) {
            // Последнюю группу у больших задач добивает заклинание героя, остальных — армия в сшибке.
            float dies = spell && last_group ? blast + 0.04f * float(k) : D * (0.47f + 0.17f * float(m) / float(std::max(1, minions - 1)));
            ImVec2 home(p0.x + W * (0.62f + 0.06f * float(g)) + float(k % 2) * fig * 0.3f, base - float(k) * H * 0.035f);
            foes.push_back({&s, home, fig * (0.85f - 0.04f * float(k)), dies, g * 7 + k, spell && last_group});
        }
        ++g;
    }
    // Рисуем от дальних к ближним.
    std::sort(foes.begin(), foes.end(), [](const Foe& a, const Foe& b) { return a.home.y < b.home.y; });
    ImVec2 last_group_c(0, 0), boss_c(0, 0);
    int last_n = 0;
    for (auto& f : foes) {
        float emerge = ease(seg(t, D * 0.12f + 0.02f * float(f.seed % 5), D * 0.32f));
        ImVec2 p = f.home;
        p.x += (1.0f - emerge) * (rift_c.x - p.x) - charge * W * 0.1f;
        float hit = t > f.dies - 0.12f && t < f.dies ? std::sin((t - f.dies) * 80.0f) * fig * 0.05f : 0;
        p.x += hit;
        float fall = ease(seg(t, f.dies, f.dies + 0.3f));
        float fade = emerge * (1.0f - seg(t, f.dies + 0.6f, f.dies + 1.2f));
        if (fade <= 0.01f) continue;
        bool alive = t < f.dies;
        float strike = alive && t > clash ? std::pow(std::max(0.0f, std::sin(t * 7.0f + float(f.seed))), 2.0f) : 0;
        draw_actor(dl, p, f.h, *f.s, false, time * 9, emerge < 1 || (charge > 0 && charge < 1) ? 1.0f : 0.0f, strike, time, f.seed, 1.45f * fall, fade);
        ImVec2 center(p.x, p.y - f.h * 0.5f);
        if (f.s->icon == "boss") boss_c = center;
        else if (f.by_spell) {
            last_group_c = ImVec2(last_group_c.x + center.x, last_group_c.y + center.y);
            ++last_n;
        }
        sparks(dl, center, t, f.dies, 10, H * 0.5f, 0.45f, mix(rift_color, IM_COL32(255, 255, 255, 255), 0.3f), f.seed + 300);
    }
    if (last_n) last_group_c = ImVec2(last_group_c.x / float(last_n), last_group_c.y / float(last_n));

    // --- Армия Хранителя: герой впереди, за ним до трёх отрядов.
    quest::Stack hero;
    hero.name = "Хранитель";
    hero.icon = "hero";
    hero.count = 1;
    ImVec2 hero_feet(p0.x + W * (0.27f + 0.17f * charge) - (1.0f - enter) * W * 0.35f, base - cheer);
    bool marching = enter < 1 || (charge > 0 && charge < 1);
    for (size_t i = std::min<size_t>(cs.ours.size(), 3); i-- > 0;) {
        const quest::Stack& s = cs.ours[i];
        int n = std::clamp(s.icon == "griffin" ? 2 : s.count, 1, 3);
        for (int k = n - 1; k >= 0; --k) {
            ImVec2 p(hero_feet.x - W * (0.06f + 0.055f * float(i)) + float(k % 2) * fig * 0.25f, base - float(k) * H * 0.035f - cheer * (k % 2 ? 0.5f : 1.0f));
            float strike = t > clash && t < victory ? std::pow(std::max(0.0f, std::sin(t * 8.0f + float(i * 3 + k))), 2.0f) : 0;
            draw_actor(dl, p, fig * (0.9f - 0.04f * float(k)) * (s.icon == "griffin" ? 1.1f : 1.0f), s, true, time * 9 + float(k), marching ? 1.0f : 0.0f, strike, time, int(i * 9 + k));
        }
    }
    // Герой колдует перед заклинанием и поднимает меч в победе.
    float hero_strike = (spell && t > cast - 0.2f && t < blast) || t > victory ? 0.8f : (t > clash ? std::pow(std::max(0.0f, std::sin(t * 7.5f)), 2.0f) : 0);
    draw_actor(dl, hero_feet, fig * 1.15f, hero, true, time * 9, marching ? 1.0f : 0.0f, hero_strike, time, 0);
    if (spell && t > cast - 0.2f && t < blast + 0.1f)
        dl->AddCircleFilled(ImVec2(hero_feet.x + fig * 0.35f, hero_feet.y - fig * 1.35f), fig * 0.12f * (1.0f + 0.3f * std::sin(time * 20)),
                            alpha(kSpellColor[sky ? 3 : 0], 0.8f));

    // --- Сшибка: сноп искр в центре.
    ImVec2 clash_c(p0.x + W * 0.48f, base - fig * 0.6f);
    sparks(dl, clash_c, t, clash, 14 + 6 * tier, H * 0.7f, 0.7f, col::gold, 11);
    if (t > clash && t < clash + 0.5f) dl->AddCircle(clash_c, fig * 2.0f * ease(seg(t, clash, clash + 0.5f)), alpha(col::gold, 1.0f - seg(t, clash, clash + 0.5f)), 0, 3.0f);

    // --- Заклинание героя: огненный шар (квест посложнее, миссия 2) или молния с неба (миссии 3–5).
    if (spell && last_n) {
        ImU32 sc = kSpellColor[sky ? 3 : 0];
        if (!sky && t > cast && t < blast) {
            ImVec2 from(hero_feet.x + fig * 0.35f, hero_feet.y - fig * 1.35f);
            for (int k = 6; k >= 0; --k) {
                float u = std::max(0.0f, seg(t, cast, blast) - 0.03f * float(k));
                ImVec2 q = lerp(from, last_group_c, u);
                q.y -= std::sin(u * kPi) * H * 0.12f;
                dl->AddCircleFilled(q, fig * (0.2f - 0.02f * float(k)), alpha(sc, 0.9f - 0.12f * float(k)));
            }
        }
        if (sky && t > blast - 0.12f && t < blast + 0.18f)
            lightning(dl, ImVec2(last_group_c.x + fig * 0.3f, p0.y + H * 0.06f), ImVec2(last_group_c.x, base), fig * 0.25f, sc, 1.0f - seg(t, blast, blast + 0.18f), time);
        if (t > blast && t < blast + 0.6f) {
            float u = seg(t, blast, blast + 0.6f);
            dl->AddCircle(last_group_c, fig * (0.5f + 2.2f * ease(u)), alpha(sc, 1.0f - u), 0, 4.0f);
            dl->AddCircleFilled(last_group_c, fig * 1.2f * (1.0f - u), alpha(sc, 0.35f * (1.0f - u)));
        }
        sparks(dl, last_group_c, t, blast, 30, H * 0.8f, 0.8f, sc, 21);
    }
    // --- Предводитель: две молнии с неба и золотой взрыв.
    if (boss) {
        ImU32 sc = kSpellColor[tier >= 6 ? 4 : 3];
        for (int k = 0; k < 2; ++k) {
            float at = finisher - 0.1f + 0.1f * float(k);
            if (t > at - 0.15f && t < at + 0.25f)
                lightning(dl, ImVec2(boss_c.x + fig * (0.6f - 1.2f * float(k)), p0.y + H * 0.04f), ImVec2(boss_c.x, boss_c.y + fig * 0.6f), fig * 0.26f, sc,
                          1.0f - seg(t, at, at + 0.25f), time + float(k));
        }
        sparks(dl, boss_c, t, finisher, 30 + 15 * tier, H * 0.9f, 1.0f, col::gold, 31);
        if (t > finisher && t < finisher + 0.9f) {
            float u = seg(t, finisher, finisher + 0.9f);
            dl->AddCircle(boss_c, fig * (1.0f + 4.0f * ease(u)), alpha(col::gold, 1.0f - u), 0, 5.0f);
        }
    }

    // --- Победа: золотой дождь, у больших задач — салют.
    if (t > victory) {
        int coins = 12 + 10 * tier;
        for (int k = 0; k < coins; ++k) {
            float y = (t - victory - rnd(k, 41) * 1.5f) * H * 0.55f;
            if (y < 0 || y > H) continue;
            ImVec2 c(p0.x + W * rnd(k, 42), p0.y + y);
            float w = std::abs(std::cos(time * 6.0f + float(k))) * fig * 0.09f;
            dl->AddEllipseFilled(c, ImVec2(std::max(1.0f, w), fig * 0.09f), col::gold_dark);
            dl->AddEllipseFilled(c, ImVec2(std::max(1.0f, w * 0.7f), fig * 0.065f), col::gold);
        }
        if (tier >= 3) {
            static const ImU32 colors[] = {col::gold, col::teal, col::violet, IM_COL32(255, 110, 90, 255), IM_COL32(130, 200, 255, 255)};
            int limit = tier == 3 ? 2 : tier == 4 ? 3 : tier == 5 ? 5 : 1000;
            float every = tier >= 6 ? 0.35f : 0.5f;
            for (int i = std::max(0, int((t - victory) / every) - 3); i < limit; ++i) {
                float at = victory + float(i) * every;
                if (at > t) break;
                ImVec2 c(p0.x + W * (0.12f + 0.76f * rnd(i, 51)), p0.y + H * (0.08f + 0.2f * rnd(i, 52)));
                sparks(dl, c, t, at, 30 + 5 * tier, H * 0.35f, 1.2f, colors[i % 5], 60 + i);
            }
        }
    }
    if (flash > 0.01f) dl->AddRectFilled(origin, add(origin, size), alpha(IM_COL32(255, 250, 235, 255), 0.55f * std::min(1.0f, flash)));

    // Название задачи сверху.
    ImVec2 title_pos(origin.x + W * 0.5f, origin.y + px(30));
    ImVec2 tsz = fonts.display->CalcTextSizeA(26.0f * ui_scale, FLT_MAX, 0, cs.title.c_str());
    dl->AddRectFilled(ImVec2(title_pos.x - tsz.x * 0.5f - 24, title_pos.y - tsz.y * 0.5f - 6), ImVec2(title_pos.x + tsz.x * 0.5f + 24, title_pos.y + tsz.y * 0.5f + 6),
                      IM_COL32(10, 12, 20, 170), 10);
    text_center(dl, fonts.display, 26.0f * ui_scale, title_pos, col::gold, cs.title.c_str());

    // --- Плашка с наградой поверх притихшей сцены.
    if (t >= D) {
        float age = t - D, a = ease(age / 0.4f);
        dl->AddRectFilled(origin, add(origin, size), alpha(IM_COL32(0, 0, 0, 255), 0.3f * a));
        float stars = px(30), lh = loot_height(cs.loot);
        ImVec2 half(std::min(W * 0.4f, 440.0f * ui_scale), px(82) + lh * 0.5f + stars * 0.5f);
        ImVec2 c(origin.x + W * 0.5f, origin.y + H * 0.45f);
        c.y = std::max(origin.y + half.y + px(10), std::min(c.y, origin.y + H - half.y - px(10)));
        ImVec2 lo(c.x - half.x, c.y - half.y), hi(c.x + half.x, c.y + half.y);
        int rays = 8 + 4 * tier;
        for (int k = 0; k < rays; ++k) {
            float an = time * 0.25f + float(k) * 2 * kPi / float(rays), reach = 1.3f + 0.1f * float(tier);
            ImVec2 tip(c.x + std::cos(an) * half.x * reach, lo.y + std::sin(an) * half.x * 0.5f * reach);
            dl->AddTriangleFilled(ImVec2(c.x - 6, lo.y + 2), tip, ImVec2(c.x + 6, lo.y + 2), alpha(col::gold, (0.035f + 0.012f * float(tier)) * a));
        }
        dl->AddRectFilled(ImVec2(lo.x + 4, lo.y + 6), ImVec2(hi.x + 4, hi.y + 6), alpha(IM_COL32(0, 0, 0, 255), 0.35f * a), 10);
        dl->AddRectFilledMultiColor(lo, hi, alpha(IM_COL32(46, 30, 32, 255), 0.95f * a), alpha(IM_COL32(46, 30, 32, 255), 0.95f * a),
                                    alpha(IM_COL32(20, 14, 18, 255), 0.95f * a), alpha(IM_COL32(20, 14, 18, 255), 0.95f * a));
        ornate_frame(dl, lo, hi, alpha(col::gold, a), 8.0f);
        // Великая и легендарная победа: плашка пульсирует золотым ореолом.
        if (tier >= 5) {
            float glow = 0.6f + 0.4f * std::sin(time * 3.0f);
            for (int k = 1; k <= 3; ++k)
                dl->AddRect(ImVec2(lo.x - px(4) * float(k), lo.y - px(4) * float(k)), ImVec2(hi.x + px(4) * float(k), hi.y + px(4) * float(k)),
                            alpha(col::gold, (0.22f - 0.06f * float(k)) * glow * a), 10.0f + 4 * float(k), 2.0f);
        }
        // Звёзды размаха выскакивают по одной.
        for (int k = 0; k < tier; ++k) {
            float pop = seg(age, 0.12f * float(k), 0.12f * float(k) + 0.25f);
            if (pop <= 0) continue;
            float sx = c.x + (float(k) - float(tier - 1) * 0.5f) * px(28);
            float fs = 24.0f * ui_scale * (pop < 1 ? 1.6f - 0.6f * ease(pop) : 1.0f);
            text_center(dl, fonts.regular, fs, ImVec2(sx + 1, lo.y + px(24) + 1), alpha(IM_COL32(0, 0, 0, 255), 0.5f * a * pop), icon::star);
            text_center(dl, fonts.regular, fs, ImVec2(sx, lo.y + px(24)), alpha(tier >= 6 ? IM_COL32(255, 236, 150, 255) : col::gold, a * pop), icon::star);
        }
        float y = lo.y + px(46) + stars;
        float vs = 46.0f * ui_scale, vw = fonts.display->CalcTextSizeA(vs, FLT_MAX, 0, cs.verdict.c_str()).x;
        if (vw > half.x * 2 - px(48)) vs *= (half.x * 2 - px(48)) / vw;
        text_center(dl, fonts.display, vs, ImVec2(c.x + 2, y + 2), alpha(IM_COL32(0, 0, 0, 255), 0.6f * a), cs.verdict.c_str());
        text_center(dl, fonts.display, vs, ImVec2(c.x, y), alpha(col::gold, a), cs.verdict.c_str());
        draw_loot(dl, ImVec2(c.x, y + px(42)), half.x * 2 - px(40), cs.loot, age, a);
        dl->PopClipRect();
        ImGui::SetCursorScreenPos(ImVec2(c.x - px(90), hi.y - px(56)));
        if (accent_button("Дальше", ImVec2(px(180), px(38)))) cs.active = false;
        return;
    }
    dl->PopClipRect();
    ImGui::SetCursorScreenPos(ImVec2(origin.x + px(12), origin.y + H - ImGui::GetFrameHeight() - px(12)));
    if (ImGui::Button("Пропустить")) cs.born = clock() - D;
}

}  // namespace sq::gui
