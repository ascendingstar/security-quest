// Анимация боя: проигрывание заранее рассчитанного журнала ходов.
//
// Итог боя уже записан (quest::simulate), здесь только зрелище: отряды строем из
// фигурок, атаки с разбегом, залпы стрел, заклинания, искры, павшие бойцы,
// поле в цветах главы. Всё рисуется векторами через ImDrawList, без текстур.

#include "app.h"

#include "draw.h"
#include "theme.h"
#include "util.h"

#include <cmath>
#include <format>

namespace sq::gui {

using namespace quest;

namespace {

constexpr float kActionTime = 0.8f;  // секунды на ход при скорости ×1
constexpr float kImpact = 0.55f;     // доля хода, на которой удар достигает цели

// Поза фигуры: координаты в долях её роста от точки между ступнями (y вверх — отрицательный),
// с отражением по направлению взгляда и поворотом (так падают павшие).
struct Pose {
    ImDrawList* dl;
    ImVec2 o;
    float h, f = 1, ang = 0, a = 1;

    ImVec2 P(float x, float y) const {
        float rx = x * f, cs = std::cos(ang), sn = std::sin(ang);
        return ImVec2(o.x + (rx * cs - y * sn) * h, o.y + (rx * sn + y * cs) * h);
    }
    ImU32 C(ImU32 c) const { return alpha(c, a); }
    void line(float x1, float y1, float x2, float y2, ImU32 c, float w) const { dl->AddLine(P(x1, y1), P(x2, y2), C(c), std::max(1.0f, w * h)); }
    void circle(float x, float y, float r, ImU32 c) const { dl->AddCircleFilled(P(x, y), r * h, C(c)); }
    void ring(float x, float y, float r, ImU32 c, float w) const { dl->AddCircle(P(x, y), r * h, C(c), 0, std::max(1.0f, w * h)); }
    void ellipse(float x, float y, float rx, float ry, ImU32 c) const { dl->AddEllipseFilled(P(x, y), ImVec2(rx * h, ry * h), C(c), ang); }
    void shape(std::initializer_list<ImVec2> pts, ImU32 c) const {
        ImVec2 buf[16];
        int n = 0;
        for (auto& q : pts)
            if (n < 16) buf[n++] = P(q.x, q.y);
        dl->AddConcavePolyFilled(buf, n, C(c));
    }
};

const ImU32 kSkin = IM_COL32(232, 198, 164, 255);
const ImU32 kSteel = IM_COL32(196, 204, 218, 255);
const ImU32 kDarkSteel = IM_COL32(120, 128, 144, 255);
const ImU32 kWood = IM_COL32(126, 86, 50, 255);

// --- Наши бойцы -------------------------------------------------------------

// walk — фаза шага (радианы), stride — размах шага 0..1, strike — замах 0..1.
void draw_soldier(const Pose& s, const std::string& icon, bool elite, float walk, float stride, float strike, float time, int seed) {
    float sw = std::sin(walk) * 0.13f * stride;
    float bob = std::abs(std::sin(walk)) * 0.03f * stride;
    ImU32 trim = elite ? col::gold : kDarkSteel;

    if (icon == "griffin") {
        float flap = std::sin(time * 9 + float(seed));
        s.line(-0.22f + sw, 0, -0.18f, -0.4f, IM_COL32(150, 120, 70, 255), 0.06f);
        s.line(0.18f - sw, 0, 0.16f, -0.4f, IM_COL32(150, 120, 70, 255), 0.06f);
        s.line(-0.35f, -0.48f, -0.62f, -0.62f + 0.06f * flap, IM_COL32(170, 140, 90, 255), 0.05f);
        s.shape({{-0.05f, -0.62f}, {-0.55f, -0.75f - 0.45f * flap}, {-0.15f, -0.9f - 0.2f * flap}, {0.18f, -0.65f}}, IM_COL32(214, 190, 140, 255));
        s.ellipse(0, -0.52f, 0.36f, 0.2f, IM_COL32(190, 150, 90, 255));
        s.circle(0.36f, -0.76f, 0.13f, IM_COL32(244, 240, 228, 255));
        s.shape({{0.46f, -0.8f}, {0.62f, -0.74f}, {0.46f, -0.7f}}, col::gold);
        s.circle(0.4f, -0.8f, 0.025f, IM_COL32(20, 20, 20, 255));
        s.shape({{0.05f, -0.6f}, {-0.35f, -0.95f - 0.4f * flap}, {0.1f, -0.75f}}, IM_COL32(234, 214, 168, 255));
        if (elite) s.ring(0.36f, -0.76f, 0.15f, col::gold, 0.03f);
        return;
    }

    if (icon == "mage") {
        ImU32 robe = elite ? IM_COL32(120, 70, 190, 255) : IM_COL32(86, 64, 160, 255);
        s.shape({{-0.12f, -0.82f - bob}, {0.12f, -0.82f - bob}, {0.22f, 0}, {-0.22f, 0}}, robe);
        s.line(-0.2f, 0, 0.2f, 0, trim, 0.04f);
        s.circle(0, -0.92f - bob, 0.1f, kSkin);
        s.shape({{-0.14f, -0.95f - bob}, {0.14f, -0.95f - bob}, {-0.04f, -1.3f - bob}}, robe);
        float lift = 0.15f * strike;
        s.line(0.22f, 0.02f - lift, 0.27f, -1.15f - lift, kWood, 0.045f);
        float glow = 0.6f + 0.4f * std::sin(time * 5 + float(seed)) + strike;
        s.circle(0.27f, -1.2f - lift, 0.09f + 0.05f * strike, alpha(IM_COL32(150, 200, 255, 255), 0.25f * glow));
        s.circle(0.27f, -1.2f - lift, 0.055f, IM_COL32(200, 230, 255, 255));
        return;
    }

    if (icon == "rider") {
        // Конь (смотрит вперёд) и всадник с копьём; при ударе копьё уходит вперёд.
        ImU32 horse = elite ? IM_COL32(222, 214, 200, 255) : IM_COL32(120, 82, 56, 255);
        ImU32 hoof = IM_COL32(50, 40, 34, 255);
        float gallop = std::sin(walk) * 0.12f * stride;
        s.line(-0.28f + gallop, 0, -0.26f, -0.4f, horse, 0.06f);
        s.line(-0.16f - gallop, 0, -0.18f, -0.4f, horse, 0.06f);
        s.line(0.22f - gallop, 0, 0.2f, -0.4f, horse, 0.06f);
        s.line(0.34f + gallop, 0, 0.3f, -0.4f, horse, 0.06f);
        for (float x : {-0.28f + gallop, -0.16f - gallop, 0.22f - gallop, 0.34f + gallop}) s.circle(x, 0, 0.035f, hoof);
        s.ellipse(0.02f, -0.5f - bob, 0.36f, 0.15f, horse);
        s.shape({{0.26f, -0.58f - bob}, {0.36f, -0.5f - bob}, {0.52f, -0.84f - bob}, {0.42f, -0.9f - bob}}, horse);
        s.shape({{0.42f, -0.9f - bob}, {0.66f, -0.8f - bob}, {0.64f, -0.72f - bob}, {0.48f, -0.74f - bob}}, horse);
        s.line(0.36f, -0.9f - bob, 0.28f, -0.66f - bob, IM_COL32(40, 30, 26, 255), 0.04f);  // грива
        s.line(-0.34f, -0.52f - bob, -0.5f, -0.3f - bob, IM_COL32(40, 30, 26, 255), 0.05f);  // хвост
        s.shape({{-0.14f, -0.62f - bob}, {0.16f, -0.62f - bob}, {0.12f, -0.42f - bob}, {-0.12f, -0.42f - bob}}, elite ? col::gold : IM_COL32(150, 40, 46, 255));
        // Всадник.
        s.shape({{-0.09f, -0.62f - bob}, {0.09f, -0.62f - bob}, {0.11f, -0.95f - bob}, {-0.11f, -0.95f - bob}}, kSteel);
        s.circle(0, -1.04f - bob, 0.09f, kSteel);
        s.shape({{-0.02f, -1.12f - bob}, {-0.18f, -1.22f - bob}, {-0.24f, -1.06f - bob}}, elite ? col::gold : IM_COL32(200, 50, 50, 255));
        float thrust = 0.25f * strike;
        s.line(-0.2f + thrust, -0.86f - bob, 0.9f + thrust, -1.0f - bob, kWood, 0.03f);
        s.shape({{0.9f + thrust, -1.03f - bob}, {1.04f + thrust, -1.0f - bob}, {0.9f + thrust, -0.97f - bob}}, kSteel);
        s.shape({{0.1f + thrust, -1.05f - bob}, {0.32f + thrust, -1.02f - bob}, {0.1f + thrust, -0.94f - bob}}, IM_COL32(200, 50, 50, 255));
        return;
    }

    bool hero = icon == "hero", knight = icon == "knight" || hero;
    bool berserk = icon == "berserk", crossbow = icon == "crossbow";
    ImU32 tunic = icon == "guard"     ? IM_COL32(58, 98, 178, 255)
                  : icon == "archer"  ? IM_COL32(66, 124, 86, 255)
                  : berserk           ? IM_COL32(214, 168, 132, 255)  // голый торс
                  : crossbow          ? IM_COL32(140, 52, 48, 255)
                                      : kSteel;
    if (hero) {
        // Плащ и знамя Хранителя.
        float wave = std::sin(time * 3);
        s.shape({{-0.1f, -0.82f}, {0.08f, -0.82f}, {-0.1f, -0.08f}, {-0.36f - 0.05f * wave, -0.12f}}, IM_COL32(170, 40, 50, 255));
        s.line(-0.32f, 0, -0.32f, -1.65f, kWood, 0.035f);
        s.shape({{-0.32f, -1.65f}, {0.18f, -1.58f + 0.05f * wave}, {0.12f, -1.38f + 0.04f * wave}, {-0.32f, -1.32f}}, col::gold);
        s.circle(-0.08f, -1.49f, 0.06f, IM_COL32(170, 40, 50, 255));
    }

    // Ноги, корпус, голова.
    ImU32 legs = knight ? kDarkSteel : IM_COL32(60, 52, 46, 255);
    s.line(-0.06f + sw, 0, -0.05f, -0.45f - bob, legs, 0.075f);
    s.line(0.06f - sw, 0, 0.05f, -0.45f - bob, legs, 0.075f);
    s.shape({{-0.13f, -0.45f - bob}, {0.13f, -0.45f - bob}, {0.16f, -0.83f - bob}, {-0.16f, -0.83f - bob}}, tunic);
    s.line(-0.13f, -0.47f - bob, 0.13f, -0.47f - bob, trim, 0.035f);
    s.circle(0, -0.93f - bob, 0.1f, kSkin);

    if (icon == "guard") {
        s.shape({{-0.12f, -0.96f - bob}, {0.12f, -0.96f - bob}, {0.1f, -1.06f - bob}, {-0.1f, -1.06f - bob}}, kSteel);
        float reach = 0.3f * strike;
        s.line(-0.05f + reach * 0.5f, -0.35f, 0.38f + reach, -1.2f + reach * 0.8f, kWood, 0.035f);
        s.shape({{0.38f + reach, -1.2f + reach * 0.8f}, {0.33f + reach, -1.06f + reach * 0.8f}, {0.44f + reach, -1.1f + reach * 0.8f}}, kSteel);
        s.shape({{0.08f, -0.84f}, {0.3f, -0.84f}, {0.3f, -0.5f}, {0.19f, -0.36f}, {0.08f, -0.5f}}, IM_COL32(44, 76, 150, 255));
        s.circle(0.19f, -0.64f, 0.04f, trim);
    } else if (icon == "archer") {
        s.shape({{-0.13f, -0.92f - bob}, {0.11f, -0.98f - bob}, {-0.02f, -1.12f - bob}}, IM_COL32(52, 98, 66, 255));
        s.line(-0.16f, -0.82f, -0.2f, -0.45f, kWood, 0.05f);
        float pull = 0.14f * strike;
        ImVec2 top = s.P(0.28f, -1.05f), mid = s.P(0.48f, -0.66f), bot = s.P(0.28f, -0.28f);
        s.dl->AddBezierQuadratic(top, mid, bot, s.C(kWood), std::max(1.0f, 0.04f * s.h));
        ImVec2 string = s.P(0.28f - pull, -0.66f);
        s.dl->AddLine(top, string, s.C(IM_COL32(230, 230, 220, 255)), 1.0f);
        s.dl->AddLine(string, bot, s.C(IM_COL32(230, 230, 220, 255)), 1.0f);
        if (strike > 0.05f) s.line(0.28f - pull, -0.66f, 0.62f - pull, -0.66f, IM_COL32(230, 220, 190, 255), 0.02f);
    } else if (berserk) {
        // Рогатый шлем, меховая юбка и два топора, которые взлетают при ударе.
        s.shape({{-0.15f, -0.44f - bob}, {0.15f, -0.44f - bob}, {0.18f, -0.3f}, {-0.18f, -0.3f}}, IM_COL32(110, 80, 50, 255));
        s.circle(0, -0.97f - bob, 0.105f, kDarkSteel);
        s.shape({{-0.08f, -1.0f - bob}, {-0.24f, -1.18f - bob}, {-0.12f, -0.96f - bob}}, IM_COL32(236, 226, 200, 255));
        s.shape({{0.08f, -1.0f - bob}, {0.24f, -1.18f - bob}, {0.12f, -0.96f - bob}}, IM_COL32(236, 226, 200, 255));
        s.line(-0.08f, -0.9f - bob, 0.08f, -0.86f - bob, IM_COL32(170, 90, 40, 255), 0.05f);  // рыжая борода
        for (int k = 0; k < 2; ++k) {
            float ang = -1.9f + 2.3f * strike + float(k) * 0.35f;
            float hx = k ? -0.12f : 0.15f, hy = -0.66f - bob;
            float tx = hx + std::cos(ang) * 0.48f, ty = hy + std::sin(ang) * 0.48f;
            s.line(hx, hy, tx, ty, kWood, 0.04f);
            float nx = -std::sin(ang), ny = std::cos(ang);
            s.shape({{tx - nx * 0.02f, ty - ny * 0.02f}, {tx + nx * 0.16f - std::cos(ang) * 0.06f, ty + ny * 0.16f - std::sin(ang) * 0.06f},
                     {tx + nx * 0.16f + std::cos(ang) * 0.1f, ty + ny * 0.16f + std::sin(ang) * 0.1f}},
                    elite ? col::gold : kSteel);
        }
    } else if (crossbow) {
        // Шапель, павеза за спиной и арбалет на уровне груди.
        s.shape({{-0.17f, -0.98f - bob}, {0.17f, -0.98f - bob}, {0.1f, -1.08f - bob}, {-0.1f, -1.08f - bob}}, kSteel);
        s.shape({{-0.3f, -0.9f}, {-0.12f, -0.9f}, {-0.12f, -0.35f}, {-0.3f, -0.35f}}, elite ? col::gold : IM_COL32(150, 40, 46, 255));
        float kick = 0.06f * strike;
        s.line(0.02f - kick, -0.68f - bob, 0.52f - kick, -0.72f - bob, kWood, 0.05f);
        ImVec2 a = s.P(0.46f - kick, -0.92f - bob), m = s.P(0.36f - kick, -0.72f - bob), b = s.P(0.46f - kick, -0.52f - bob);
        s.dl->AddBezierQuadratic(a, m, b, s.C(kDarkSteel), std::max(1.0f, 0.035f * s.h));
        if (strike < 0.05f) s.line(0.2f, -0.71f - bob, 0.5f, -0.73f - bob, IM_COL32(230, 220, 190, 255), 0.018f);
    } else {
        // Рыцарь и Хранитель: шлем, плюмаж, меч с размахом.
        s.circle(0, -0.95f - bob, 0.115f, kSteel);
        s.line(0.02f, -0.94f - bob, 0.11f, -0.94f - bob, IM_COL32(40, 40, 50, 255), 0.03f);
        if (hero) s.shape({{-0.1f, -1.04f - bob}, {-0.06f, -1.16f - bob}, {0, -1.07f - bob}, {0.06f, -1.16f - bob}, {0.1f, -1.04f - bob}}, col::gold);
        else s.shape({{-0.02f, -1.05f - bob}, {-0.22f, -1.18f - bob}, {-0.3f, -0.98f - bob}}, elite ? col::gold : IM_COL32(200, 50, 50, 255));
        float angle = -1.25f + 2.1f * strike;
        float hx = 0.16f, hy = -0.62f - bob;
        float tx = hx + std::cos(angle) * 0.62f, ty = hy + std::sin(angle) * 0.62f;
        s.line(hx, hy, tx, ty, kSteel, 0.05f);
        float gx = -std::sin(angle) * 0.1f, gy = std::cos(angle) * 0.1f;
        s.line(hx + std::cos(angle) * 0.1f - gx, hy + std::sin(angle) * 0.1f - gy, hx + std::cos(angle) * 0.1f + gx, hy + std::sin(angle) * 0.1f + gy, col::gold, 0.04f);
        if (strike > 0.3f && strike < 0.9f) {
            // След клинка.
            for (int k = 1; k <= 3; ++k) {
                float a2 = angle - 0.25f * float(k);
                s.line(hx + std::cos(a2) * 0.35f, hy + std::sin(a2) * 0.35f, hx + std::cos(a2) * 0.62f, hy + std::sin(a2) * 0.62f,
                       alpha(IM_COL32(230, 240, 255, 255), 0.35f / float(k)), 0.05f);
            }
        }
    }
}

// --- Монстры ----------------------------------------------------------------

void draw_monster(const Pose& s, uint32_t seed, ImU32 color, bool boss, float walk, float stride, float strike, float time) {
    ImU32 body = mix(color, IM_COL32(20, 18, 24, 255), 0.35f);
    ImU32 belly = mix(color, IM_COL32(240, 230, 210, 255), 0.25f);
    ImU32 bone = IM_COL32(230, 220, 196, 255);
    float sw = std::sin(walk) * 0.12f * stride;
    float fat = 0.85f + float(seed % 7) * 0.05f;
    float breathe = 0.015f * std::sin(time * 2.5f + float(seed % 13));

    if (boss) {
        float pulse = 0.5f + 0.5f * std::sin(time * 2.2f);
        s.circle(0.05f, -0.55f, 0.62f + 0.05f * pulse, alpha(color, 0.10f + 0.06f * pulse));
    }
    // Хвост.
    if ((seed >> 4) & 1) {
        ImVec2 a = s.P(-0.25f, -0.42f), b = s.P(-0.55f, -0.35f), c = s.P(-0.6f, -0.65f + 0.08f * std::sin(time * 3)), d = s.P(-0.75f, -0.7f);
        s.dl->AddBezierCubic(a, b, c, d, s.C(body), std::max(1.0f, 0.06f * s.h));
    }
    // Ноги, тело, брюхо.
    s.line(-0.12f + sw, 0, -0.1f, -0.36f, body, 0.1f);
    s.line(0.12f - sw, 0, 0.1f, -0.36f, body, 0.1f);
    s.ellipse(0.02f, -0.52f + breathe, 0.31f * fat, 0.31f, body);
    s.ellipse(0.08f, -0.45f + breathe, 0.17f * fat, 0.17f, belly);
    // Шипы на спине у одного вида монстров.
    if (seed % 3 == 2)
        for (int k = 0; k < 3; ++k) {
            float x = -0.22f + 0.13f * float(k);
            s.shape({{x - 0.05f, -0.74f + breathe}, {x + 0.05f, -0.76f + breathe}, {x - 0.02f, -0.95f + breathe}}, bone);
        }
    // Лапа с когтями — вытягивается при ударе.
    float hx = 0.36f + 0.28f * strike, hy = -0.5f - 0.22f * strike;
    s.line(0.16f, -0.62f, hx, hy, body, 0.08f);
    for (int k = -1; k <= 1; ++k) s.line(hx, hy, hx + 0.1f, hy + 0.05f * float(k) + 0.03f, bone, 0.025f);
    // Голова: рога, уши или ничего; светящиеся глаза.
    float head_y = -0.8f + breathe;
    s.circle(0.22f, head_y, 0.16f, body);
    if (seed % 3 == 0) {
        s.shape({{0.14f, head_y - 0.1f}, {0.2f, head_y - 0.12f}, {0.06f, head_y - 0.36f}}, bone);
        s.shape({{0.26f, head_y - 0.12f}, {0.32f, head_y - 0.1f}, {0.4f, head_y - 0.36f}}, bone);
    } else if (seed % 3 == 1) {
        s.shape({{0.1f, head_y - 0.08f}, {0.18f, head_y - 0.14f}, {0.08f, head_y - 0.3f}}, body);
        s.shape({{0.26f, head_y - 0.14f}, {0.34f, head_y - 0.08f}, {0.36f, head_y - 0.3f}}, body);
    }
    s.line(0.26f, head_y + 0.08f, 0.36f, head_y + 0.07f, bone, 0.02f);
    ImU32 eye = boss ? IM_COL32(255, 80, 60, 255) : IM_COL32(255, 220, 90, 255);
    for (float ex : {0.25f, 0.33f}) {
        s.circle(ex, head_y - 0.02f, 0.06f, alpha(eye, 0.25f));
        s.circle(ex, head_y - 0.02f, 0.028f, eye);
    }
    if (boss) {
        s.shape({{0.08f, head_y - 0.13f}, {0.08f, head_y - 0.3f}, {0.15f, head_y - 0.2f}, {0.22f, head_y - 0.34f}, {0.29f, head_y - 0.2f},
                 {0.36f, head_y - 0.3f}, {0.36f, head_y - 0.13f}},
                col::gold);
    }
}

// --- Поле боя ----------------------------------------------------------------

void draw_backdrop(ImDrawList* dl, ImVec2 p0, ImVec2 size, ImU32 color, float time, bool defense) {
    float W = size.x, H = size.y;
    ImVec2 p1 = add(p0, size);
    float horizon = p0.y + H * 0.46f;
    ImU32 sky_top = IM_COL32(12, 14, 28, 255);
    ImU32 sky_low = mix(mix(color, IM_COL32(60, 54, 80, 255), 0.55f), IM_COL32(0, 0, 0, 255), 0.25f);
    dl->AddRectFilledMultiColor(p0, ImVec2(p1.x, horizon), sky_top, sky_top, sky_low, sky_low);

    // Светило и звёзды.
    ImVec2 sun(p0.x + W * 0.78f, p0.y + H * 0.17f);
    for (int k = 4; k >= 1; --k) dl->AddCircleFilled(sun, H * 0.025f * float(k), alpha(mix(color, IM_COL32(255, 240, 210, 255), 0.6f), 0.06f));
    dl->AddCircleFilled(sun, H * 0.028f, IM_COL32(255, 244, 220, 235));
    for (int k = 0; k < 40; ++k) {
        uint32_t h = uint32_t(k) * 2654435761u;
        ImVec2 st(p0.x + float(h % 1000) / 1000.0f * W, p0.y + float((h >> 10) % 1000) / 1000.0f * H * 0.3f);
        float tw = 0.4f + 0.6f * std::abs(std::sin(time * 1.3f + float(k)));
        dl->AddCircleFilled(st, 1.1f, alpha(IM_COL32(255, 255, 255, 255), 0.35f * tw));
    }
    // Облака плывут медленно.
    for (int k = 0; k < 5; ++k) {
        float speed = 6.0f + float(k) * 3.0f;
        float x = std::fmod(float(k) * W * 0.27f + time * speed, W * 1.4f) - W * 0.2f;
        float y = p0.y + H * (0.08f + 0.06f * float(k % 3));
        ImU32 cloud = alpha(mix(sky_low, IM_COL32(220, 220, 235, 255), 0.35f), 0.14f);
        for (int b = 0; b < 4; ++b) dl->AddEllipseFilled(ImVec2(p0.x + x + float(b) * H * 0.05f, y + float(b % 2) * H * 0.012f), ImVec2(H * 0.07f, H * 0.025f), cloud);
    }
    // Дальние горы и ближние холмы.
    auto ridge = [&](float base, float amp, float freq, float phase, ImU32 c) {
        std::vector<ImVec2> pts;
        for (int k = 0; k <= 48; ++k) {
            float x = float(k) / 48.0f;
            float y = base - amp * (0.6f * std::abs(std::sin(x * freq + phase)) + 0.4f * std::sin(x * freq * 2.3f + phase * 1.7f));
            pts.push_back(ImVec2(p0.x + x * W, p0.y + y * H));
        }
        pts.push_back(ImVec2(p1.x, p1.y));
        pts.push_back(ImVec2(p0.x, p1.y));
        dl->AddConcavePolyFilled(pts.data(), int(pts.size()), c);
    };
    ridge(0.44f, 0.14f, 7.0f, 1.3f, mix(sky_low, IM_COL32(10, 10, 20, 255), 0.45f));
    ridge(0.48f, 0.06f, 4.0f, 0.4f, mix(land(color), IM_COL32(0, 0, 0, 255), 0.35f));
    // Земля с перспективой: светлее у горизонта, темнее к зрителю.
    ImU32 ground = land(color), deep = mix(ground, IM_COL32(0, 0, 0, 255), 0.45f);
    dl->AddRectFilledMultiColor(ImVec2(p0.x, p0.y + H * 0.5f), p1, ground, ground, deep, deep);
    dl->AddRectFilledMultiColor(ImVec2(p0.x, horizon), ImVec2(p1.x, p0.y + H * 0.56f), alpha(sky_low, 0.0f), alpha(sky_low, 0.0f),
                                alpha(sky_low, 0.35f), alpha(sky_low, 0.35f));
    // Трава и камни.
    ImU32 grass = mix(ground, IM_COL32(200, 220, 160, 255), 0.25f);
    for (int k = 0; k < 90; ++k) {
        uint32_t h = uint32_t(k + 7) * 2246822519u;
        float gx = p0.x + float(h % 1000) / 1000.0f * W;
        float depth = float((h >> 10) % 1000) / 1000.0f;
        float gy = p0.y + H * (0.53f + 0.45f * depth);
        float gh = H * (0.008f + 0.016f * depth);
        float sway = std::sin(time * 1.6f + float(k)) * gh * 0.3f;
        dl->AddLine(ImVec2(gx, gy), ImVec2(gx - gh * 0.4f + sway, gy - gh), alpha(grass, 0.5f + 0.4f * depth), 1.2f);
        dl->AddLine(ImVec2(gx, gy), ImVec2(gx + gh * 0.4f + sway, gy - gh), alpha(grass, 0.5f + 0.4f * depth), 1.2f);
        if (k % 11 == 0) dl->AddEllipseFilled(ImVec2(gx + gh * 2, gy), ImVec2(gh * 1.4f, gh * 0.7f), alpha(IM_COL32(90, 90, 96, 255), 0.7f));
    }
    // Стена Цитадели в оборонительном бою.
    if (defense) {
        ImU32 stone = IM_COL32(120, 116, 110, 255), dark = IM_COL32(80, 76, 72, 255);
        float x0 = p0.x + W * 0.03f, x1 = p0.x + W * 0.1f, top = p0.y + H * 0.38f;
        dl->AddRectFilled(ImVec2(x0, top), ImVec2(x1, p1.y), stone);
        for (float y = top + H * 0.05f; y < p1.y; y += H * 0.05f) dl->AddLine(ImVec2(x0, y), ImVec2(x1, y), dark, 1.0f);
        for (int k = 0; k < 3; ++k) {
            float bx = x0 + (x1 - x0) * float(k) / 3.0f;
            dl->AddRectFilled(ImVec2(bx, top - H * 0.03f), ImVec2(bx + (x1 - x0) * 0.2f, top), stone);
        }
        dl->AddRectFilled(ImVec2(x0 - W * 0.01f, top - H * 0.1f), ImVec2(x0 + W * 0.035f, top + H * 0.05f), stone);
        dl->AddTriangleFilled(ImVec2(x0 - W * 0.015f, top - H * 0.1f), ImVec2(x0 + W * 0.04f, top - H * 0.1f), ImVec2(x0 + W * 0.0125f, top - H * 0.17f),
                              IM_COL32(150, 60, 50, 255));
    }
}

// Сколько фигурок рисовать отряду: строй до 9 бойцов, грифонов меньше, босс один.
int squad_slots(const Stack& s) {
    if (s.icon == "boss" || s.icon == "hero") return 1;
    if (s.icon == "griffin") return 2;
    if (s.icon == "knight") return 4;
    return std::clamp(s.count, 1, 6);
}

int shown_of(const Stack& s, int alive) {
    if (alive <= 0) return 0;
    int slots = squad_slots(s);
    return std::clamp(int(std::ceil(float(slots) * float(alive) / float(std::max(1, s.count)))), 1, slots);
}

}  // namespace

void draw_figure(ImDrawList* dl, ImVec2 feet, float h, const quest::Stack& s, bool ours, float time, int seed) {
    dl->AddEllipseFilled(ImVec2(feet.x, feet.y + 1), ImVec2(h * (s.icon == "boss" ? 0.32f : 0.2f), h * 0.05f), IM_COL32(0, 0, 0, 90));
    Pose pose{dl, feet, h, ours ? 1.0f : -1.0f};
    if (ours) draw_soldier(pose, s.icon, s.elite, 0, 0, 0, time, seed);
    else draw_monster(pose, hash_of(s.name), tint(s.color), s.icon == "boss", 0, 0, 0, time);
}

void draw_actor(ImDrawList* dl, ImVec2 feet, float h, const quest::Stack& s, bool ours, float walk, float stride, float strike, float time, int seed,
                float angle, float fade) {
    if (angle == 0) dl->AddEllipseFilled(ImVec2(feet.x, feet.y + 1), ImVec2(h * (s.icon == "boss" ? 0.32f : 0.2f), h * 0.05f), alpha(IM_COL32(0, 0, 0, 255), 0.35f * fade));
    Pose pose{dl, feet, h, ours ? 1.0f : -1.0f, angle, fade};
    if (ours) draw_soldier(pose, s.icon, s.elite, walk, stride, strike, time, seed);
    else draw_monster(pose, hash_of(s.name), tint(s.color), s.icon == "boss", walk, stride, strike, time);
}

void draw_battlefield(ImDrawList* dl, ImVec2 p0, ImVec2 size, uint32_t color, float time) {
    draw_backdrop(dl, p0, size, tint(color), time, false);
}

void draw_scene(ImDrawList* dl, ImVec2 p0, ImVec2 size, uint32_t color) {
    ImU32 c = tint(color);
    ImVec2 p1 = add(p0, size);
    float horizon = p0.y + size.y * 0.55f;
    ImU32 sky_top = IM_COL32(16, 18, 32, 255), sky_low = mix(mix(c, IM_COL32(60, 54, 80, 255), 0.55f), IM_COL32(0, 0, 0, 255), 0.25f);
    dl->AddRectFilledMultiColor(p0, ImVec2(p1.x, horizon), sky_top, sky_top, sky_low, sky_low);
    std::vector<ImVec2> hills;
    for (int k = 0; k <= 24; ++k) {
        float x = float(k) / 24.0f;
        hills.push_back(ImVec2(p0.x + x * size.x, horizon - size.y * (0.08f + 0.06f * std::abs(std::sin(x * 6 + 1.2f)))));
    }
    hills.push_back(p1);
    hills.push_back(ImVec2(p0.x, p1.y));
    dl->AddConcavePolyFilled(hills.data(), int(hills.size()), mix(sky_low, IM_COL32(10, 10, 20, 255), 0.45f));
    ImU32 ground = land(color);
    dl->AddRectFilledMultiColor(ImVec2(p0.x, horizon), p1, ground, ground, mix(ground, IM_COL32(0, 0, 0, 255), 0.4f), mix(ground, IM_COL32(0, 0, 0, 255), 0.4f));
}

void draw_emblem(ImDrawList* dl, ImVec2 c, float s) {
    // Тот же рисунок, что на значке программы (tools/make_icon.ps1): щит с замочной скважиной и скрещённые мечи.
    float u = s / 100.0f;
    auto P = [&](float x, float y) { return ImVec2(c.x + (x - 50) * u, c.y + (y - 54) * u); };
    for (float side : {1.0f, -1.0f}) {
        ImVec2 hilt = P(50 - 30 * side, 82), tip = P(50 + 34 * side, 14);
        ImVec2 d(tip.x - hilt.x, tip.y - hilt.y);
        float len = std::hypot(d.x, d.y);
        ImVec2 n(d.x / len, d.y / len), guard(hilt.x + n.x * 13 * u, hilt.y + n.y * 13 * u);
        dl->AddLine(guard, tip, IM_COL32(222, 228, 238, 255), 6.5f * u);
        dl->AddLine(hilt, guard, IM_COL32(120, 72, 26, 255), 4.5f * u);
        dl->AddLine(ImVec2(guard.x - n.y * 9 * u, guard.y + n.x * 9 * u), ImVec2(guard.x + n.y * 9 * u, guard.y - n.x * 9 * u), IM_COL32(236, 180, 80, 255), 4.5f * u);
        dl->AddCircleFilled(hilt, 4 * u, IM_COL32(236, 180, 80, 255));
    }
    auto shield = [&](float in, ImU32 fill) {
        float l = 24 + in, r = 76 - in, t = 17 + in, b = 91 - in * 1.4f;
        dl->PathLineTo(P(l, t));
        dl->PathLineTo(P(r, t));
        dl->PathLineTo(P(r, 50));
        dl->PathBezierCubicCurveTo(P(r, 70), P(62, 82), P(50, b));
        dl->PathBezierCubicCurveTo(P(38, 82), P(l, 70), P(l, 50));
        dl->PathFillConcave(fill);
    };
    shield(-1.5f, IM_COL32(0, 0, 0, 110));
    shield(0, col::gold_dark);
    shield(2, col::gold);
    shield(6, IM_COL32(48, 20, 42, 255));
    ImU32 key = col::gold;
    dl->AddCircleFilled(P(50, 43), 9 * u, key);
    dl->AddQuadFilled(P(46.5f, 46), P(53.5f, 46), P(56, 70), P(44, 70), key);
}
void App::battle_view() {
    BattleView& b = battle_;
    // Ручной бой: когда анимация догнала журнал, ходит ИИ (враг или автобой), иначе ждём игрока.
    if (b.live) {
        bool caught_up = b.progress >= float(b.live->result().actions.size()) - 1e-3f;
        if (caught_up && !b.live->finished() && (!b.live->player_turn() || b.autoplay)) b.live->auto_turn();
        b.result = b.live->result();
        if (b.live->finished() && !b.recorded) {
            quest::record_battle(world_, *game_log_, b.frontier, b.result);
            b.recorded = true;
            refresh();
        }
    }
    const BattleResult& r = b.result;
    int n = int(r.actions.size());
    bool open_end = b.live && !b.live->finished();
    float dt = std::min(ImGui::GetIO().DeltaTime, 0.05f);
    b.progress = std::min(b.progress + dt * b.speed / kActionTime, open_end ? float(n) : float(n) + 2.0f);
    bool waiting = open_end && b.live->player_turn() && !b.autoplay && b.progress >= float(n) - 1e-3f;
    float time = float(clock());

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    ImGui::Dummy(size);
    float W = size.x, H = size.y;

    // Тряска экрана после мощных ударов.
    b.shake *= std::exp(-dt * 7.0f);
    ImVec2 p0(origin.x + std::sin(time * 71.0f) * b.shake, origin.y + std::cos(time * 63.0f) * b.shake);
    dl->PushClipRect(origin, add(origin, size), true);

    uint32_t chapter_color = r.theirs.empty() ? 0 : r.theirs.front().color;
    bool towers = r.defense;
    for (auto& a : r.actions) towers |= a.kind == BattleAction::Tower;
    draw_backdrop(dl, p0, size, tint(chapter_color), time, towers);

    // Текущее состояние: потери всех ходов, чей удар уже прошёл.
    int cur = int(std::floor(b.progress));
    float phase = b.progress - float(cur);
    std::vector<int> counts[2];
    for (auto& s : r.ours) counts[0].push_back(s.count);
    for (auto& s : r.theirs) counts[1].push_back(s.count);
    std::vector<bool> shield;
    for (auto& s : r.theirs) shield.push_back(s.shielded);
    int round = 0;
    for (int i = 0; i < n && (i < cur || (i == cur && phase >= kImpact)); ++i) {
        const BattleAction& a = r.actions[size_t(i)];
        round = std::max(round, a.round);
        // Перемещение и защита никого не бьют: их target — сам отряд на своей стороне, а не враг.
        if (a.kind == BattleAction::Move || a.kind == BattleAction::Defend || a.target < 0) continue;
        int tside = a.side == 0 ? 1 : 0;
        if (a.kind == BattleAction::ShieldBreak) shield[size_t(a.target)] = false;
        counts[tside][size_t(a.target)] = std::max(0, counts[tside][size_t(a.target)] - a.kills);
    }
    if (round != b.round) {
        b.round = round;
        b.round_banner = time;
    }

    // Сетка поля: шестиугольники на земле, ряды уходят вдаль.
    float gx0 = p0.x + W * 0.1f, gx1 = p0.x + W * 0.97f;
    float gy0 = p0.y + H * 0.5f, gy1 = p0.y + H * 0.95f;
    float hw = (gx1 - gx0) / (float(kCols) + 0.5f);
    float hex_r = hw / 1.7320508f;                     // радиус шестиугольника по горизонтали
    float rh = (gy1 - gy0 - hex_r * 0.5f) / float(kRows);  // шаг рядов
    float sy = rh / (1.5f * hex_r);                    // сплющивание по вертикали — перспектива
    std::vector<ImVec2> centers(kCells);
    for (int c = 0; c < kCells; ++c) {
        int row = c / kCols, col = c % kCols;
        centers[size_t(c)] = ImVec2(gx0 + hw * (float(col) + 0.5f + 0.5f * float(row & 1)), gy0 + rh * float(row) + hex_r * sy);
    }
    auto hex_points = [&](ImVec2 c, float scale, ImVec2* out) {
        for (int k = 0; k < 6; ++k) {
            float ang = (-90.0f + 60.0f * float(k)) * kPi / 180.0f;
            out[k] = ImVec2(c.x + std::cos(ang) * hex_r * scale, c.y + std::sin(ang) * hex_r * sy * scale);
        }
    };
    float fig = std::clamp(rh * 1.25f, 16.0f, 56.0f);

    // Клетки и камни.
    for (int c = 0; c < kCells; ++c) {
        ImVec2 pts[6];
        hex_points(centers[size_t(c)], 0.96f, pts);
        dl->AddPolyline(pts, 6, IM_COL32(255, 255, 255, 18), 1.0f, ImDrawFlags_Closed);
    }
    for (int c : r.obstacles) {
        ImVec2 cc = centers[size_t(c)];
        Pose rock{dl, cc, fig * 0.7f};
        rock.shape({{-0.6f, 0}, {-0.45f, -0.45f}, {-0.05f, -0.62f}, {0.4f, -0.5f}, {0.62f, -0.1f}, {0.5f, 0.05f}}, IM_COL32(96, 92, 90, 255));
        rock.shape({{-0.45f, -0.45f}, {-0.05f, -0.62f}, {0.15f, -0.4f}, {-0.2f, -0.25f}}, IM_COL32(130, 126, 122, 255));
        rock.shape({{0.3f, 0.02f}, {0.45f, -0.22f}, {0.75f, -0.18f}, {0.8f, 0.04f}}, IM_COL32(84, 80, 78, 255));
    }

    // Где стоят отряды: с начальных клеток применяем завершённые ходы.
    std::vector<int> cells[2] = {r.start[0], r.start[1]};
    cells[0].resize(r.ours.size(), 0);
    cells[1].resize(r.theirs.size(), kCols - 1);
    for (int i = 0; i < n && i < cur; ++i) {
        const BattleAction& a = r.actions[size_t(i)];
        if (a.kind == BattleAction::Move && !a.path.empty()) cells[a.side][size_t(a.actor)] = a.path.back();
    }
    float intro = b.progress < 0 ? ease(1.0f + b.progress) : 1.0f;
    const BattleAction* act = (cur >= 0 && cur < n) ? &r.actions[size_t(cur)] : nullptr;
    int tgt_side = act && act->kind != BattleAction::Defend && act->kind != BattleAction::Move ? (act->side == 0 ? 1 : 0) : -1;
    auto base = [&](int side, int idx) { return centers[size_t(std::clamp(cells[side][size_t(idx)], 0, kCells - 1))]; };

    // Где сейчас отряд: вступление, шаги по пути, выпад в атаке, отскок от удара.
    auto squad_pos = [&](int side, int idx, float& stride, float& strike) {
        ImVec2 p = base(side, idx);
        p.x += (side == 0 ? -1.0f : 1.0f) * (1.0f - intro) * W * 0.3f;
        stride = 1.0f - intro;
        strike = 0;
        if (act && act->side == side && act->actor == idx) {
            if (act->kind == BattleAction::Move && act->path.size() > 1) {
                float u = std::clamp(phase / 0.9f, 0.0f, 1.0f) * float(act->path.size() - 1);
                size_t k = std::min(size_t(u), act->path.size() - 2);
                p = lerp(centers[size_t(act->path[k])], centers[size_t(act->path[k + 1])], u - float(k));
                stride = phase < 0.9f ? 1.0f : 0.0f;
            } else if (act->kind == BattleAction::Melee) {
                ImVec2 t = base(tgt_side, act->target);
                float go = phase < 0.4f ? ease(phase / 0.4f) : 1.0f - ease((phase - 0.62f) / 0.38f);
                p = lerp(p, t, 0.45f * std::clamp(go, 0.0f, 1.0f));
                strike = std::clamp((phase - 0.36f) / 0.2f, 0.0f, 1.0f) * (phase < 0.7f ? 1.0f : 0.0f);
            } else if (act->kind == BattleAction::Ranged) {
                strike = phase < 0.15f ? phase / 0.15f : 0.0f;
            }
        }
        if (act && tgt_side == side && act->target == idx && phase >= kImpact && phase < 0.8f) {
            float k = 1.0f - (phase - kImpact) / 0.25f;
            p.x += (side == 0 ? -1.0f : 1.0f) * fig * 0.2f * k;
        }
        return p;
    };
    // Строй внутри клетки: до двух рядов по три бойца.
    auto slot = [&](int side, int k, ImVec2 c, float scale) {
        int row = k / 3, colm = k % 3;
        float dir = side == 0 ? 1.0f : -1.0f;
        return ImVec2(c.x + dir * ((float(colm) - 1.0f) * hw * 0.26f + float(row) * hw * 0.13f) * scale, c.y + (float(row) - 0.5f) * rh * 0.34f * scale);
    };
    auto stack_of = [&](int side, int idx) -> const Stack& { return side == 0 ? r.ours[size_t(idx)] : r.theirs[size_t(idx)]; };
    auto fig_height = [&](const Stack& s) {
        float k = s.icon == "boss" ? 2.0f : s.icon == "griffin" ? 1.1f : s.icon == "knight" ? 1.05f : 1.0f;
        return fig * k;
    };
    // Павшие: если фигурок в отряде стало меньше, лишние падают на землю.
    for (int side = 0; side < 2; ++side) {
        auto& prev = b.shown[side];
        size_t count = side == 0 ? r.ours.size() : r.theirs.size();
        if (prev.size() != count) prev.assign(count, -1);
        for (size_t i = 0; i < count; ++i) {
            const Stack& s = stack_of(side, int(i));
            int now = shown_of(s, counts[side][i]);
            if (prev[i] < 0) prev[i] = now;
            if (now < prev[i]) {
                float stride, strike;
                ImVec2 c = squad_pos(side, int(i), stride, strike);
                for (int k = now; k < prev[i] && b.fx.size() < 600; ++k) {
                    Particle corpse{Particle::Corpse, slot(side, k, c, 1.0f), ImVec2(0, 0)};
                    corpse.life = 2.4f;
                    corpse.size = fig_height(s);
                    corpse.side = side;
                    corpse.stack = int(i);
                    b.fx.push_back(corpse);
                }
            }
            prev[i] = now;
        }
    }

    // Эффекты удара — один раз на ход, когда удар долетел.
    auto spawn = [&](Particle::Kind kind, ImVec2 p, ImVec2 v, float life, float sz, ImU32 c) {
        if (b.fx.size() < 900) {
            Particle q{kind, p, v};
            q.life = life;
            q.size = sz;
            q.color = c;
            q.rot = float(hash_of(std::to_string(b.fx.size())) % 628) / 100.0f;
            q.spin = (float(hash_of(std::to_string(b.fx.size() * 7)) % 100) / 100.0f - 0.5f) * 10.0f;
            b.fx.push_back(q);
        }
    };
    int reached = phase >= kImpact ? cur : cur - 1;
    reached = std::min(reached, n - 1);
    if (reached > b.impacts) {
        int from = std::max(b.impacts + 1, reached - 3);  // при пропуске не устраиваем фейерверк из сотни ударов
        for (int i = from; i <= reached; ++i) {
            const BattleAction& a = r.actions[size_t(i)];
            if (a.kind == BattleAction::Move) continue;  // шаги рисует squad_pos, удара нет
            int ts = a.kind == BattleAction::Defend ? a.side : (a.side == 0 ? 1 : 0);
            float stride, strike;
            ImVec2 c = squad_pos(ts, a.target, stride, strike);
            ImVec2 center(c.x, c.y - fig * 0.5f);
            if (a.kind == BattleAction::Defend) {
                spawn(Particle::Ring, center, ImVec2(0, 0), 0.7f, fig * 1.6f, IM_COL32(120, 180, 255, 255));
                Particle text(Particle::Text, ImVec2(center.x, center.y - fig * 0.9f), ImVec2(0, -H * 0.06f));
                text.life = 1.1f;
                text.color = IM_COL32(150, 200, 255, 255);
                text.text = "Защита";
                b.fx.push_back(text);
                continue;
            }
            const Stack& target = stack_of(ts, a.target);
            ImU32 blood = ts == 1 ? mix(tint(target.color), IM_COL32(255, 255, 255, 255), 0.2f) : IM_COL32(220, 60, 50, 255);
            bool big = a.kind == BattleAction::Spell || a.kind == BattleAction::Tower || a.kind == BattleAction::ShieldBreak;
            int sparks = std::min(40, 8 + a.damage / 15 + (big ? 16 : 0));
            for (int k = 0; k < sparks; ++k) {
                float ang = float(hash_of(std::format("{}s{}", i, k)) % 628) / 100.0f;
                float sp = H * (0.25f + float(hash_of(std::format("{}v{}", i, k)) % 100) / 100.0f * 0.6f);
                ImU32 col = k % 3 == 0 ? IM_COL32(255, 240, 200, 255) : (a.kind == BattleAction::Spell ? kSpellColor[std::max(0, a.spell) % 5] : blood);
                spawn(Particle::Spark, center, ImVec2(std::cos(ang) * sp, std::sin(ang) * sp - H * 0.3f), 0.5f + 0.4f * float(k % 4) / 4.0f, 2.0f + float(k % 3), col);
            }
            for (int k = 0; k < 4; ++k)
                spawn(Particle::Dust, ImVec2(c.x + (float(k) - 1.5f) * fig * 0.35f, c.y), ImVec2((float(k) - 1.5f) * H * 0.05f, -H * 0.03f), 0.9f, fig * 0.25f,
                      mix(land(chapter_color), IM_COL32(200, 190, 170, 255), 0.4f));
            if (big) {
                spawn(Particle::Ring, center, ImVec2(0, 0), 0.6f, fig * 2.2f, a.kind == BattleAction::Tower ? col::gold : kSpellColor[std::max(0, a.spell) % 5]);
                b.shake = std::max(b.shake, a.kind == BattleAction::Spell ? 9.0f : 6.0f);
            } else if (a.kills >= 3) {
                b.shake = std::max(b.shake, 3.0f);
            }
            if (a.kind == BattleAction::ShieldBreak)
                for (int k = 0; k < 18; ++k) {
                    float ang = float(k) / 18.0f * 2 * kPi;
                    spawn(Particle::Shard, ImVec2(center.x + std::cos(ang) * fig * 1.3f, center.y + std::sin(ang) * fig * 1.3f),
                          ImVec2(std::cos(ang) * H * 0.5f, std::sin(ang) * H * 0.5f), 0.8f, fig * 0.18f, IM_COL32(170, 210, 255, 255));
                }
            std::string label = a.kind == BattleAction::ShieldBreak ? "Чары развеяны!" : a.kills > 0 ? std::format("−{}  ☠{}", a.damage, a.kills) : std::format("−{}", a.damage);
            if (a.counter) label = "Ответ: " + label;
            if (a.damage == 0 && a.kind != BattleAction::ShieldBreak && ts == 1 && r.theirs[size_t(a.target)].shielded) label = "Чары держат удар!";
            if (a.damage > 0 || a.kind == BattleAction::ShieldBreak || label == "Чары держат удар!") {
                Particle text{Particle::Text, ImVec2(center.x, center.y - fig * 0.9f), ImVec2(0, -H * 0.08f)};
                text.life = 1.3f;
                text.color = a.kind == BattleAction::ShieldBreak ? IM_COL32(170, 210, 255, 255) : ts == 1 ? col::gold : col::red;
                text.text = label;
                text.size = big ? 1.4f : 1.0f;
                b.fx.push_back(text);
            }
        }
        b.impacts = reached;
    }

    // Финал: золотой дождь за победу, дым за поражение.
    if (!open_end && b.progress >= float(n) + 0.4f && !b.finale) {
        b.finale = true;
        for (int k = 0; k < (r.won ? 70 : 24); ++k) {
            float x = p0.x + W * (0.2f + 0.6f * float(hash_of(std::format("c{}", k)) % 1000) / 1000.0f);
            if (r.won) spawn(Particle::Coin, ImVec2(x, p0.y - H * 0.05f * float(k % 7)), ImVec2(0, H * (0.2f + 0.1f * float(k % 5))), 3.0f, fig * 0.16f, col::gold);
            else spawn(Particle::Dust, ImVec2(x, p0.y + H * 0.72f), ImVec2(0, -H * 0.05f), 2.5f, fig * 0.45f, IM_COL32(30, 30, 36, 255));
        }
    }

    // Обновление частиц.
    for (auto& q : b.fx) {
        q.age += dt;
        q.p.x += q.v.x * dt;
        q.p.y += q.v.y * dt;
        q.rot += q.spin * dt;
        if (q.kind == Particle::Spark || q.kind == Particle::Shard) q.v.y += H * 1.6f * dt;
        if (q.kind == Particle::Coin) q.v.y += H * 0.9f * dt;
        if (q.kind == Particle::Dust) q.v = ImVec2(q.v.x * 0.96f, q.v.y * 0.96f);
    }
    b.fx.erase(std::remove_if(b.fx.begin(), b.fx.end(), [](const Particle& q) { return q.age >= q.life; }), b.fx.end());

    // Павшие лежат на земле под живыми.
    for (auto& q : b.fx) {
        if (q.kind != Particle::Corpse) continue;
        float fall = ease(q.age / 0.35f);
        float fade = q.age < q.life - 0.8f ? 1.0f : std::max(0.0f, (q.life - q.age) / 0.8f);
        float dir = q.side == 0 ? 1.0f : -1.0f;
        Pose pose{dl, q.p, q.size, dir, -dir * 1.45f * fall, 0.75f * fade};
        const Stack& s = stack_of(q.side, q.stack);
        if (q.side == 0) draw_soldier(pose, s.icon, s.elite, 0, 0, 0, time, q.stack);
        else draw_monster(pose, hash_of(s.name), tint(s.color), s.icon == "boss", 0, 0, 0, time);
    }

    // Живые фигурки, отсортированные по глубине.
    struct Figure {
        float y;
        int side, idx, slot;
        ImVec2 p;
        float stride, strike;
    };
    std::vector<Figure> figures;
    for (int side = 0; side < 2; ++side) {
        size_t count = side == 0 ? r.ours.size() : r.theirs.size();
        for (size_t i = 0; i < count; ++i) {
            float stride, strike;
            ImVec2 c = squad_pos(side, int(i), stride, strike);
            int shown = shown_of(stack_of(side, int(i)), counts[side][i]);
            for (int k = 0; k < shown; ++k) {
                ImVec2 p = slot(side, k, c, 1.0f);
                figures.push_back({p.y, side, int(i), k, p, stride, strike});
            }
        }
    }
    std::ranges::sort(figures, [](const Figure& x, const Figure& y) { return x.y < y.y; });
    for (auto& f : figures) {
        const Stack& s = stack_of(f.side, f.idx);
        float h = fig_height(s);
        bool hit = act && tgt_side == f.side && act->target == f.idx && phase >= kImpact && phase < 0.75f;
        dl->AddEllipseFilled(ImVec2(f.p.x, f.p.y + 1), ImVec2(h * (s.icon == "boss" ? 0.32f : 0.2f), h * 0.05f), IM_COL32(0, 0, 0, 90));
        Pose pose{dl, f.p, h, f.side == 0 ? 1.0f : -1.0f};
        float walk = time * 9.0f + float(f.slot) * 1.7f;
        float strike = f.strike > 0 ? std::clamp(f.strike - float(f.slot % 3) * 0.08f, 0.0f, 1.0f) : 0.0f;
        if (f.side == 0) draw_soldier(pose, s.icon, s.elite, walk, f.stride, strike, time, f.slot + f.idx * 9);
        else draw_monster(pose, hash_of(s.name), tint(s.color), s.icon == "boss", walk, f.stride, strike, time);
        if (hit) {
            float k = 1.0f - (phase - kImpact) / 0.2f;
            dl->AddCircleFilled(ImVec2(f.p.x, f.p.y - h * 0.55f), h * 0.45f, alpha(IM_COL32(255, 255, 255, 255), 0.25f * std::max(0.0f, k)));
        }
    }

    // Подписи и полосы здоровья отрядов.
    for (int side = 0; side < 2; ++side) {
        size_t count = side == 0 ? r.ours.size() : r.theirs.size();
        for (size_t i = 0; i < count; ++i) {
            const Stack& s = stack_of(side, int(i));
            float stride, strike;
            ImVec2 c = squad_pos(side, int(i), stride, strike);
            int cnt = counts[side][i];
            float top = c.y - fig_height(s) * (s.icon == "boss" ? 1.25f : 1.55f) - fig * 0.3f;
            float bw = fig * 1.6f;
            float frac = s.count ? float(cnt) / float(s.count) : 0;
            dl->AddRectFilled(ImVec2(c.x - bw * 0.5f, top), ImVec2(c.x + bw * 0.5f, top + 5), IM_COL32(0, 0, 0, 170), 2);
            dl->AddRectFilled(ImVec2(c.x - bw * 0.5f, top), ImVec2(c.x - bw * 0.5f + bw * frac, top + 5), side == 0 ? col::teal : col::red, 2);
            if (side == 1 && shield[i]) {
                float pulse = 0.5f + 0.5f * std::sin(time * 4);
                ImVec2 dome(c.x, c.y - fig_height(s) * 0.5f);
                dl->AddEllipseFilled(dome, ImVec2(fig_height(s) * 0.55f, fig_height(s) * 0.6f), alpha(IM_COL32(140, 180, 255, 255), 0.10f + 0.05f * pulse));
                dl->AddEllipse(dome, ImVec2(fig_height(s) * 0.55f, fig_height(s) * 0.6f), alpha(IM_COL32(170, 210, 255, 255), 0.6f + 0.3f * pulse), 0, 0, 2.5f);
            }
            std::string label = cnt > 0 ? std::format("{} × {}", s.name, cnt) : s.name + " — пали";
            float ts = 13.0f * ui_scale;
            ImVec2 tsz = fonts.regular->CalcTextSizeA(ts, FLT_MAX, 0, label.c_str());
            float ly = c.y + fig * 0.55f;
            dl->AddRectFilled(ImVec2(c.x - tsz.x * 0.5f - 6, ly), ImVec2(c.x + tsz.x * 0.5f + 6, ly + tsz.y + 2), IM_COL32(10, 12, 18, 170), 6);
            dl->AddText(fonts.regular, ts, ImVec2(c.x - tsz.x * 0.5f, ly + 1), cnt > 0 ? col::text : col::faint, label.c_str());
        }
    }

    // Ручной бой: кто в защите и чей ход.
    if (b.live) {
        for (int side = 0; side < 2; ++side)
            for (size_t i = 0; i < (side == 0 ? r.ours.size() : r.theirs.size()); ++i) {
                if (counts[side][i] <= 0 || !b.live->defending(side, int(i))) continue;
                float stride, strike;
                ImVec2 c = squad_pos(side, int(i), stride, strike);
                Pose shield_icon{dl, ImVec2(c.x + fig * 0.7f, c.y - fig * 1.1f), fig * 0.32f};
                shield_icon.shape({{-0.45f, -0.5f}, {0.45f, -0.5f}, {0.45f, 0.05f}, {0, 0.6f}, {-0.45f, 0.05f}}, IM_COL32(120, 180, 255, 230));
            }
        if (waiting) {
            float stride, strike;
            ImVec2 c = squad_pos(0, b.live->actor(), stride, strike);
            float pulse = 0.5f + 0.5f * std::sin(time * 5);
            float top = c.y - fig * 1.6f - pulse * fig * 0.12f;
            dl->AddTriangleFilled(ImVec2(c.x - fig * 0.16f, top), ImVec2(c.x + fig * 0.16f, top), ImVec2(c.x, top + fig * 0.2f), col::gold);
        }
    }
    // Хранитель позади строя: круг заклинания и посох при касте.
    ImVec2 hero_pos(p0.x + W * 0.045f - (1.0f - intro) * W * 0.2f, p0.y + H * 0.7f);
    if (towers) hero_pos = ImVec2(p0.x + W * 0.065f, p0.y + H * 0.38f);
    bool casting = act && (act->kind == BattleAction::Spell || act->kind == BattleAction::ShieldBreak) && phase < 0.7f;
    if (casting) {
        ImU32 sc = kSpellColor[std::max(0, act->spell) % 5];
        float k = std::sin(std::min(phase / 0.6f, 1.0f) * kPi);
        dl->AddEllipse(ImVec2(hero_pos.x, hero_pos.y), ImVec2(fig * 0.9f, fig * 0.28f), alpha(sc, 0.8f * k), time * 2, 0, 2.5f);
        dl->AddEllipse(ImVec2(hero_pos.x, hero_pos.y), ImVec2(fig * 0.65f, fig * 0.2f), alpha(sc, 0.6f * k), -time * 3, 0, 1.5f);
        for (int rr = 0; rr < 6; ++rr) {
            float ang = time * 2 + float(rr) * kPi / 3;
            dl->AddCircleFilled(ImVec2(hero_pos.x + std::cos(ang) * fig * 0.9f, hero_pos.y + std::sin(ang) * fig * 0.28f), 2.5f, alpha(sc, k));
        }
    }
    dl->AddEllipseFilled(ImVec2(hero_pos.x, hero_pos.y + 1), ImVec2(fig * 0.25f, fig * 0.06f), IM_COL32(0, 0, 0, 90));
    draw_soldier(Pose{dl, hero_pos, fig * 1.15f, 1.0f}, "hero", true, time * 9, 1.0f - intro, casting ? 0.6f : 0.0f, time, 0);
    text_center(dl, fonts.semibold, 13.0f * ui_scale, ImVec2(hero_pos.x, hero_pos.y + fig * 0.45f), col::gold, "Хранитель");

    // Снаряды текущего хода (у перемещения и защиты цели нет).
    if (act && tgt_side >= 0) {
        float stride, strike;
        ImVec2 tc = squad_pos(tgt_side, act->target, stride, strike);
        ImVec2 tgt(tc.x, tc.y - fig * 0.5f);
        if (act->kind == BattleAction::Ranged && phase > 0.1f && phase < kImpact + 0.04f) {
            const Stack& shooter = stack_of(act->side, act->actor);
            ImVec2 sc = squad_pos(act->side, act->actor, stride, strike);
            int volley = std::min(6, shown_of(shooter, counts[act->side][size_t(act->actor)]) + 1);
            for (int k = 0; k < volley; ++k) {
                float t = std::clamp((phase - 0.1f - float(k) * 0.025f) / (kImpact - 0.1f), 0.0f, 1.0f);
                if (t <= 0) continue;
                ImVec2 from = slot(act->side, k % 9, sc, 1.0f);
                from.y -= fig * 0.7f;
                ImVec2 to(tgt.x + (float(k % 3) - 1.0f) * fig * 0.3f, tgt.y + (float(k % 2)) * fig * 0.2f);
                auto arc = [&](float u) {
                    ImVec2 q = lerp(from, to, u);
                    q.y -= std::sin(u * kPi) * H * 0.14f;
                    return q;
                };
                ImVec2 head = arc(t), tail = arc(std::max(0.0f, t - 0.06f));
                if (act->side == 0) {
                    dl->AddLine(tail, head, IM_COL32(230, 220, 190, 255), 2.0f);
                    dl->AddCircleFilled(head, 2.0f, IM_COL32(255, 250, 230, 255));
                } else {
                    ImU32 spit = mix(tint(shooter.color), IM_COL32(200, 255, 140, 255), 0.4f);
                    dl->AddCircleFilled(head, fig * 0.08f, alpha(spit, 0.9f));
                    dl->AddCircleFilled(tail, fig * 0.05f, alpha(spit, 0.4f));
                }
            }
        } else if (act->kind == BattleAction::Tower && phase > 0.08f && phase < kImpact + 0.03f) {
            float t = std::clamp((phase - 0.08f) / (kImpact - 0.08f), 0.0f, 1.0f);
            ImVec2 from(p0.x + W * 0.07f, p0.y + H * 0.35f);
            ImVec2 q = lerp(from, tgt, t);
            q.y -= std::sin(t * kPi) * H * 0.25f;
            Pose rock{dl, q, fig * 0.22f, 1.0f, time * 8};
            rock.shape({{-0.6f, -0.2f}, {-0.2f, -0.7f}, {0.5f, -0.5f}, {0.7f, 0.1f}, {0.1f, 0.6f}, {-0.5f, 0.4f}}, IM_COL32(150, 140, 130, 255));
            for (int k = 1; k <= 4; ++k) {
                float u = std::max(0.0f, t - 0.04f * float(k));
                ImVec2 tq = lerp(from, tgt, u);
                tq.y -= std::sin(u * kPi) * H * 0.25f;
                dl->AddCircleFilled(tq, fig * 0.12f / float(k), alpha(IM_COL32(200, 190, 170, 255), 0.25f));
            }
        } else if ((act->kind == BattleAction::Spell || act->kind == BattleAction::ShieldBreak) && phase > 0.2f && phase < kImpact + 0.03f) {
            ImU32 sc = kSpellColor[std::max(0, act->spell) % 5];
            float t = ease((phase - 0.2f) / (kImpact - 0.2f));
            ImVec2 from(hero_pos.x + fig * 0.3f, hero_pos.y - fig * 1.3f);
            for (int k = 8; k >= 0; --k) {
                float u = std::max(0.0f, t - 0.025f * float(k));
                ImVec2 q = lerp(from, tgt, u);
                q.y -= std::sin(u * kPi) * H * 0.08f;
                float rad = fig * (0.22f - 0.02f * float(k));
                dl->AddCircleFilled(q, rad * 1.8f, alpha(sc, 0.08f));
                dl->AddCircleFilled(q, rad, alpha(sc, 0.9f - 0.09f * float(k)));
            }
            ImVec2 q = lerp(from, tgt, t);
            q.y -= std::sin(t * kPi) * H * 0.08f;
            dl->AddCircleFilled(q, fig * 0.1f, IM_COL32(255, 255, 240, 255));
        }
    }

    // Частицы поверх всего поля.
    for (auto& q : b.fx) {
        float life = 1.0f - q.age / q.life;
        switch (q.kind) {
            case Particle::Spark:
                dl->AddLine(q.p, ImVec2(q.p.x - q.v.x * 0.02f, q.p.y - q.v.y * 0.02f), alpha(q.color, life), q.size * 0.8f);
                break;
            case Particle::Dust:
                dl->AddCircleFilled(q.p, q.size * (1.0f + q.age * 1.2f), alpha(q.color, 0.35f * life));
                break;
            case Particle::Ring:
                dl->AddCircle(q.p, q.size * ease(q.age / q.life) + 4, alpha(q.color, life), 0, 2 + 5 * life);
                dl->AddCircleFilled(q.p, q.size * 0.4f * life, alpha(q.color, 0.25f * life));
                break;
            case Particle::Shard: {
                Pose sh{dl, q.p, q.size, 1.0f, q.rot, life};
                sh.shape({{0, -0.8f}, {0.4f, 0.2f}, {-0.3f, 0.5f}}, q.color);
                break;
            }
            case Particle::Coin: {
                float w = std::abs(std::cos(q.rot * 2)) * q.size;
                dl->AddEllipseFilled(q.p, ImVec2(std::max(1.0f, w), q.size), alpha(col::gold_dark, std::min(1.0f, life * 3)));
                dl->AddEllipseFilled(q.p, ImVec2(std::max(1.0f, w * 0.7f), q.size * 0.7f), alpha(col::gold, std::min(1.0f, life * 3)));
                break;
            }
            case Particle::Text: {
                float pop = q.age < 0.15f ? 0.6f + 0.4f * (q.age / 0.15f) * 1.3f : 1.0f;
                float fs = 20.0f * ui_scale * q.size * pop;
                text_center(dl, fonts.semibold, fs, ImVec2(q.p.x + 1.5f, q.p.y + 1.5f), alpha(IM_COL32(0, 0, 0, 255), 0.6f * life), q.text.c_str());
                text_center(dl, fonts.semibold, fs, q.p, alpha(q.color, std::min(1.0f, life * 1.5f)), q.text.c_str());
                break;
            }
            case Particle::Corpse: break;
        }
    }
    // Пылинки в воздухе — цвета главы: пепел, споры, снег.
    for (int k = 0; k < 36; ++k) {
        uint32_t h = uint32_t(k + 3) * 2654435761u;
        float sx = float(h % 1000) / 1000.0f, sy = float((h >> 10) % 1000) / 1000.0f;
        float x = std::fmod(sx * W + time * (8.0f + float(k % 5) * 4.0f), W);
        float y = std::fmod(sy * H + time * (6.0f + float(k % 3) * 5.0f), H);
        x += std::sin(time + float(k)) * 6.0f;
        dl->AddCircleFilled(ImVec2(p0.x + x, p0.y + y), 1.0f + float(k % 3) * 0.6f, alpha(mix(tint(chapter_color), IM_COL32(255, 255, 255, 255), 0.5f), 0.35f));
    }

    // Заголовок, надпись раунда, вступление.
    ImVec2 title_pos(origin.x + W * 0.5f, origin.y + 30 * ui_scale);
    ImVec2 tsz = fonts.display->CalcTextSizeA(26.0f * ui_scale, FLT_MAX, 0, b.title.c_str());
    dl->AddRectFilled(ImVec2(title_pos.x - tsz.x * 0.5f - 24, title_pos.y - tsz.y * 0.5f - 6), ImVec2(title_pos.x + tsz.x * 0.5f + 24, title_pos.y + tsz.y * 0.5f + 6),
                      IM_COL32(10, 12, 20, 170), 10);
    text_center(dl, fonts.display, 26.0f * ui_scale, title_pos, col::gold, b.title.c_str());
    if (b.progress < 0) {
        float k = ease(1.0f + b.progress);
        text_center(dl, fonts.display, 44.0f * ui_scale * (0.7f + 0.3f * k), ImVec2(origin.x + W * 0.5f, origin.y + H * 0.32f), alpha(col::gold, k),
                    "Бой начинается!");
    } else if (time - b.round_banner < 1.4 && b.progress < float(n)) {
        float age = float(time - b.round_banner);
        float k = age < 0.25f ? age / 0.25f : std::max(0.0f, 1.0f - (age - 0.9f) / 0.5f);
        std::string text = std::format("Раунд {}", std::max(1, round));
        text_center(dl, fonts.display, 34.0f * ui_scale, ImVec2(origin.x + W * 0.5f + (1.0f - std::min(1.0f, age / 0.25f)) * W * 0.1f, origin.y + H * 0.2f),
                    alpha(IM_COL32(240, 230, 210, 255), k), text.c_str());
    }
    dl->PopClipRect();

    ImGui::SetCursorScreenPos(ImVec2(origin.x + px(12), origin.y + H - ImGui::GetFrameHeight() - px(12)));
    for (float sp : {1.0f, 2.0f, 4.0f}) {
        ImGui::PushStyleColor(ImGuiCol_Button, b.speed == sp ? alpha(col::gold, 0.35f) : col::panel_hi);
        if (ImGui::Button(std::format("×{}", int(sp)).c_str())) b.speed = sp;
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }
    if (!b.live && b.progress < float(n) && ImGui::Button("Пропустить")) b.progress = float(n) + 0.4f;
    if (b.live && b.progress < float(n) - 0.05f && ImGui::Button("Перемотать")) b.progress = float(n);
    battle_controls(waiting, centers, hex_r, sy, origin, size, fig);

    // Итог.
    if (!open_end && b.progress >= float(n) + 0.4f) {
        float t = std::clamp((b.progress - float(n) - 0.4f) / 0.6f, 0.0f, 1.0f);
        ImVec2 c(origin.x + W * 0.5f, origin.y + H * 0.42f);
        if (b.card_shown < -1e8) b.card_shown = clock();
        float age = float(clock() - b.card_shown);
        bool loot = r.won && (b.loot.xp || b.loot.gold);  // наградной экран: миссия или взятый рубеж
        std::string losses;
        for (size_t i = 0; i < r.ours.size() && i < r.our_losses.size(); ++i)
            if (r.our_losses[i] > 0) losses += std::format("{}{} −{}", losses.empty() ? "" : ",  ", r.ours[i].name, r.our_losses[i]);
        float lh = loot ? loot_height(b.loot) + (losses.empty() ? 0 : px(24)) : 0;
        ImVec2 half(std::min(W * 0.4f, 440.0f * ui_scale), loot ? px(82) + lh * 0.5f : 120.0f * ui_scale);
        // Плашка не выходит за поле боя: на низком окне она поднимается к верхнему краю.
        c.y = std::max(origin.y + half.y + px(10), std::min(c.y, origin.y + H - half.y - px(10)));
        ImU32 edge = r.won ? col::gold : col::red;
        ImVec2 a(c.x - half.x, c.y - half.y), z(c.x + half.x, c.y + half.y);
        dl->AddRectFilled(ImVec2(a.x + 4, a.y + 6), ImVec2(z.x + 4, z.y + 6), alpha(IM_COL32(0, 0, 0, 255), 0.35f * t), 10);
        dl->AddRectFilledMultiColor(a, z, alpha(IM_COL32(46, 30, 32, 255), 0.95f * t), alpha(IM_COL32(46, 30, 32, 255), 0.95f * t),
                                    alpha(IM_COL32(20, 14, 18, 255), 0.95f * t), alpha(IM_COL32(20, 14, 18, 255), 0.95f * t));
        dl->AddRect(a, z, alpha(edge, t), 8.0f, 2.0f);
        ornate_frame(dl, a, z, alpha(edge, t), 8.0f);
        // Победа: за плашкой расходятся золотые лучи.
        if (r.won)
            for (int k = 0; k < 14; ++k) {
                float an = float(time) * 0.25f + float(k) * kPi / 7;
                ImVec2 tip(c.x + std::cos(an) * half.x * 1.3f, a.y + std::sin(an) * half.x * 0.5f);
                dl->AddTriangleFilled(ImVec2(c.x - 6, a.y + 2), tip, ImVec2(c.x + 6, a.y + 2), alpha(col::gold, 0.06f * t));
            }
        float y = a.y + px(46);
        const char* verdict = r.won ? b.verdict.c_str() : "Отступление";
        float vs = 46.0f * ui_scale, vw = fonts.display->CalcTextSizeA(vs, FLT_MAX, 0, verdict).x;
        if (vw > half.x * 2 - px(48)) vs *= (half.x * 2 - px(48)) / vw;  // длинная надпись уменьшается, а не вылезает за рамку
        text_center(dl, fonts.display, vs, ImVec2(c.x + 2, y + 2), alpha(IM_COL32(0, 0, 0, 255), 0.6f * t), verdict);
        text_center(dl, fonts.display, vs, ImVec2(c.x, y), alpha(edge, t), verdict);
        y += px(42);
        if (loot) {
            y += draw_loot(dl, ImVec2(c.x, y), half.x * 2 - px(40), b.loot, age, t);
            if (!losses.empty()) text_center(dl, fonts.regular, 15.0f * ui_scale, ImVec2(c.x, y + px(6)), alpha(col::dim, t), ("Потери: " + losses).c_str());
        } else {
            std::string reward = r.won ? (b.reward.empty() ? "" : "Награда: " + b.reward) : "Враг устоял. Усильте армию в городе и возвращайтесь.";
            text_center(dl, fonts.regular, 18.0f * ui_scale, ImVec2(c.x, y), alpha(col::text, t), reward.c_str());
            y += px(30);
            text_center(dl, fonts.regular, 15.0f * ui_scale, ImVec2(c.x, y), alpha(col::dim, t),
                        !r.won ? "Армия отступила без потерь" : losses.empty() ? "Без потерь" : ("Потери: " + losses).c_str());
        }
        ImGui::SetCursorScreenPos(ImVec2(c.x - px(90), z.y - px(56)));
        if (accent_button("Дальше", ImVec2(px(180), px(38)))) b.active = false;
    }
}

// Управление ручным боем: подсказка, кнопки, подсветка клеток, предпросмотр пути и прогноз, щелчки.
void App::battle_controls(bool waiting, const std::vector<ImVec2>& centers, float hex_r, float sy, ImVec2 origin, ImVec2 size, float fig) {
    BattleView& b = battle_;
    if (!b.live || b.live->finished()) return;
    Battle& L = *b.live;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float W = size.x, H = size.y;
    auto hex = [&](int c, float scale, ImU32 fill, ImU32 edge, float thick) {
        ImVec2 pts[6];
        for (int k = 0; k < 6; ++k) {
            float ang = (-90.0f + 60.0f * float(k)) * kPi / 180.0f;
            pts[k] = ImVec2(centers[size_t(c)].x + std::cos(ang) * hex_r * scale, centers[size_t(c)].y + std::sin(ang) * hex_r * sy * scale);
        }
        if (fill) dl->AddConvexPolyFilled(pts, 6, fill);
        if (edge) dl->AddPolyline(pts, 6, edge, thick, ImDrawFlags_Closed);
    };

    // Подсказка, чей ход.
    std::string hint;
    if (waiting) {
        const Stack& s = L.result().ours[size_t(L.actor())];
        if (b.pending_spell >= 0) hint = std::format("Цель для «{}» — щёлкните по врагу · Esc — отмена", spells()[size_t(b.pending_spell)].name);
        else if (L.can_shoot()) hint = std::format("{} × {}: стреляйте по врагу или выберите клетку для хода", s.name, L.alive(0, L.actor()));
        else hint = std::format("{} × {}: клетка — ход, враг — атака (ходит на {} кл.)", s.name, L.alive(0, L.actor()), s.speed);
    } else {
        hint = b.autoplay ? "Автобой: армией командует ИИ" : "Ход врага…";
    }
    ImVec2 hc(origin.x + W * 0.5f, origin.y + 64 * ui_scale);
    ImVec2 hs = fonts.regular->CalcTextSizeA(16.0f * ui_scale, FLT_MAX, 0, hint.c_str());
    dl->AddRectFilled(ImVec2(hc.x - hs.x * 0.5f - 14, hc.y - hs.y * 0.5f - 4), ImVec2(hc.x + hs.x * 0.5f + 14, hc.y + hs.y * 0.5f + 4),
                      waiting ? IM_COL32(60, 46, 20, 220) : IM_COL32(10, 12, 20, 170), 8);
    text_center(dl, fonts.regular, 16.0f * ui_scale, hc, waiting ? col::gold : col::dim, hint.c_str());

    // Кнопки справа внизу: защита, заклинания героя, автобой, отступление.
    struct Btn {
        std::string label;
        int kind;  // 0 защита, 1 заклинание, 2 автобой, 3 отступить
        int spell;
    };
    std::vector<Btn> buttons;
    if (waiting) {
        buttons.push_back({std::format("{}  Защита", icon::shield), 0, -1});
        for (int s : L.castable()) {
            bool ward = std::ranges::any_of(L.result().theirs, [&](const Stack& x) { return x.shielded; }) && s != 0;
            std::string name = spells()[size_t(s)].name;
            buttons.push_back({ward ? std::format("{}  Снять чары: {}", icon::star, name) : std::format("{}  {}", icon::star, name), 1, s});
        }
    }
    buttons.push_back({b.autoplay ? "Командовать самому" : "Автобой", 2, -1});
    buttons.push_back({"Отступить", 3, -1});
    float total = 0, sp = ImGui::GetStyle().ItemSpacing.x;
    for (auto& btn : buttons) total += ImGui::CalcTextSize(btn.label.c_str()).x + ImGui::GetStyle().FramePadding.x * 2 + sp;
    ImGui::SetCursorScreenPos(ImVec2(origin.x + W - total - px(12), origin.y + H - ImGui::GetFrameHeight() - px(12)));
    for (size_t i = 0; i < buttons.size(); ++i) {
        auto& btn = buttons[i];
        if (i) ImGui::SameLine();
        bool selected = btn.kind == 1 && b.pending_spell == btn.spell;
        ImU32 color = btn.kind == 1 ? (selected ? kSpellColor[btn.spell % 5] : alpha(kSpellColor[btn.spell % 5], 0.35f))
                    : btn.kind == 3 ? alpha(col::red, 0.35f) : col::panel_hi;
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::PushStyleColor(ImGuiCol_Text, selected ? col::bg : col::text);
        if (ImGui::Button(btn.label.c_str())) {
            if (btn.kind == 0) L.defend();
            if (btn.kind == 1) b.pending_spell = selected ? -1 : btn.spell;
            if (btn.kind == 2) b.autoplay = !b.autoplay;
            if (btn.kind == 3) L.retreat();
        }
        ImGui::PopStyleColor(2);
        if (btn.kind == 0) tooltip("Отряд остаётся на месте и до своего следующего хода получает на 40% меньше урона.");
        if (btn.kind == 1) tooltip(spells()[size_t(btn.spell)].effect);
        if (btn.kind == 3) tooltip("Бой закончится поражением. Армия отступит без потерь.");
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) b.pending_spell = -1;
    if (!waiting) return;

    // Куда можно дойти и кого можно ударить.
    std::vector<int> reach = L.reachable();
    bool shoot = L.can_shoot();
    int me = L.cell(0, L.actor());
    if (b.pending_spell < 0)
        for (int c = 0; c < kCells; ++c)
            if (reach[size_t(c)] > 0) hex(c, 0.9f, IM_COL32(244, 196, 90, 34), IM_COL32(244, 196, 90, 70), 1.0f);
    hex(me, 0.95f, 0, col::gold, 2.5f);
    std::vector<bool> can_hit(L.result().theirs.size(), false);
    for (size_t i = 0; i < L.result().theirs.size(); ++i) {
        if (L.alive(1, int(i)) <= 0) continue;
        can_hit[i] = b.pending_spell >= 0 || shoot || L.attack_cell(int(i), -1) >= 0;
        hex(L.cell(1, int(i)), 0.95f, can_hit[i] ? IM_COL32(230, 80, 70, 40) : 0, can_hit[i] ? IM_COL32(230, 90, 80, 170) : IM_COL32(150, 150, 160, 60), 1.5f);
    }

    // Клетка под мышью.
    ImVec2 m = ImGui::GetIO().MousePos;
    if (ImGui::IsAnyItemHovered() || !ImGui::IsWindowHovered()) return;
    int hovered = -1;
    float best = hex_r * 1.1f;
    for (int c = 0; c < kCells; ++c) {
        float dx = m.x - centers[size_t(c)].x, dy = (m.y - centers[size_t(c)].y) / sy;
        float d = std::sqrt(dx * dx + dy * dy);
        if (d < best) {
            best = d;
            hovered = c;
        }
    }
    if (hovered < 0) return;
    int target = -1;
    for (size_t i = 0; i < L.result().theirs.size(); ++i)
        if (L.alive(1, int(i)) > 0 && L.cell(1, int(i)) == hovered) target = int(i);

    auto draw_path = [&](const std::vector<int>& path) {
        for (size_t k = 1; k < path.size(); ++k) {
            ImVec2 a = centers[size_t(path[k - 1])], c = centers[size_t(path[k])];
            for (int s = 1; s <= 3; ++s) dl->AddCircleFilled(lerp(a, c, float(s) / 4.0f), std::max(2.5f, fig * 0.05f), alpha(col::gold, 0.8f));
        }
    };
    std::vector<std::string> lines;
    ImU32 mark = col::gold;
    int attack_from = -1;
    if (target >= 0) {
        const Stack& enemy = L.result().theirs[size_t(target)];
        lines.push_back(std::format("{} × {}", enemy.name, L.alive(1, target)));
        mark = b.pending_spell >= 0 ? kSpellColor[b.pending_spell % 5] : col::red;
        if (b.pending_spell == 0 && !L.shielded(target)) {
            int lvl = world_.army.spell_levels.empty() ? 0 : world_.army.spell_levels[0];
            int base = 10 * lvl * (2 + world_.army.power) / 2;
            lines.push_back(std::format("Огненная стрела: {}–{} урона", base, base + 6));
        } else if (b.pending_spell >= 0) {
            lines.push_back(L.shielded(target) ? "Развеять чары" : "Заклинание не подействует на эту цель");
        } else if (!can_hit[size_t(target)]) {
            lines.push_back("Не дойти за этот ход");
        } else {
            if (!shoot) {
                // Подходим с той стороны, где мышь.
                int prefer = -1;
                float pd = 1e9f;
                for (int nb : hex_neighbors(hovered)) {
                    if (reach[size_t(nb)] < 0) continue;
                    float dx = m.x - centers[size_t(nb)].x, dy = (m.y - centers[size_t(nb)].y) / sy;
                    if (dx * dx + dy * dy < pd) {
                        pd = dx * dx + dy * dy;
                        prefer = nb;
                    }
                }
                attack_from = L.attack_cell(target, prefer);
                if (attack_from >= 0 && attack_from != me) {
                    draw_path(L.path_to(attack_from));
                    hex(attack_from, 0.9f, IM_COL32(244, 196, 90, 70), col::gold, 2.0f);
                }
            }
            Battle::Estimate e = L.estimate(target, attack_from);
            const Stack& me_s = L.result().ours[size_t(L.actor())];
            if (e.immune) {
                lines.push_back("Под чарами: урон не пройдёт");
                lines.push_back("Сначала снимите чары заклинанием");
            } else {
                lines.push_back(std::format("Урон {}–{}, падёт {}–{}", e.dmg_min, e.dmg_max, e.kills_min, e.kills_max));
                if (e.half) lines.push_back(shoot ? "Далеко: урон вполсилы" : "Стрелок вплотную: урон вполсилы");
                if (shoot && me_s.long_range) lines.push_back("Дальнобой: без штрафа за дальность");
                if (!shoot && me_s.charge && attack_from >= 0 && reach[size_t(attack_from)] > 0)
                    lines.push_back(std::format("Натиск: +{}% за {} клеток разбега", 7 * reach[size_t(attack_from)], reach[size_t(attack_from)]));
                if (shoot) lines.push_back("Выстрел — без ответного удара");
                else if (me_s.no_retaliation) lines.push_back("Враг не успеет ответить");
                else if (e.counter) lines.push_back("Ответит ударом");
            }
        }
        hex(hovered, 1.0f, 0, mark, 3.0f);
    } else if (b.pending_spell < 0 && reach[size_t(hovered)] > 0) {
        draw_path(L.path_to(hovered));
        hex(hovered, 0.95f, IM_COL32(244, 196, 90, 80), col::gold, 2.0f);
        lines.push_back(std::format("Перейти сюда: {} кл.", reach[size_t(hovered)]));
    } else {
        return;
    }

    float lh = 15.0f * ui_scale, pad = 8;
    float w = 0;
    for (auto& l : lines) w = std::max(w, fonts.regular->CalcTextSizeA(lh, FLT_MAX, 0, l.c_str()).x);
    ImVec2 tp(std::min(m.x + 18, origin.x + W - w - pad * 2 - 4), std::min(m.y + 18, origin.y + H - float(lines.size()) * (lh + 3) - pad * 2 - 60));
    dl->AddRectFilled(tp, ImVec2(tp.x + w + pad * 2, tp.y + float(lines.size()) * (lh + 3) + pad * 2), IM_COL32(14, 16, 26, 235), 8);
    dl->AddRect(tp, ImVec2(tp.x + w + pad * 2, tp.y + float(lines.size()) * (lh + 3) + pad * 2), alpha(mark, 0.7f), 8.0f, 1.5f);
    for (size_t k = 0; k < lines.size(); ++k)
        dl->AddText(k == 0 && target >= 0 ? fonts.semibold : fonts.regular, lh, ImVec2(tp.x + pad, tp.y + pad + float(k) * (lh + 3)),
                    k == 0 ? col::text : col::dim, lines[k].c_str());

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (target >= 0 && b.pending_spell >= 0) {
            L.cast(b.pending_spell, target);
            b.pending_spell = -1;
        } else if (target >= 0 && can_hit[size_t(target)]) {
            L.attack(target, attack_from);
        } else if (target < 0 && reach[size_t(hovered)] > 0) {
            L.move(hovered);
        }
    }
}
}  // namespace sq::gui
