// Кампания: карта глав и рубежей с туманом, выбор кампании, летопись и город. Бой — в battle.cpp.
// Графика векторная, рисуется через ImDrawList: ни текстур, ни видеокарты почти не нужно.

#include "app.h"

#include "draw.h"
#include "theme.h"
#include "util.h"
#include "win.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

namespace sq::gui {

using namespace quest;

namespace {

constexpr ImU32 kFog = IM_COL32(58, 64, 82, 255);
const ImVec2 kCitadel(-370, 150);
void draw_fog(ImDrawList* dl, ImVec2 c, float radius, uint32_t seed, float time) {
    for (int i = 0; i < 12; ++i) {
        uint32_t h = seed * 2654435761u + uint32_t(i) * 40503u;
        float a = float(h % 628) / 100.0f;
        float d = (i == 0 ? 0.0f : 0.25f + float((h >> 9) % 50) / 100.0f) * radius;
        ImVec2 p(c.x + std::cos(a) * d + std::sin(time * 0.23f + i) * radius * 0.07f, c.y + std::sin(a) * d + std::cos(time * 0.19f + i * 1.7f) * radius * 0.06f);
        float b = radius * (0.32f + float((h >> 17) % 20) / 100.0f);
        dl->AddCircleFilled(p, b * 1.35f, alpha(kFog, 0.12f));
        dl->AddCircleFilled(p, b, alpha(kFog, 0.24f));
        dl->AddCircleFilled(p, b * 0.65f, alpha(kFog, 0.36f));
    }
}

void draw_castle(ImDrawList* dl, ImVec2 c, float s) {
    ImU32 stone = IM_COL32(176, 168, 150, 255), dark = IM_COL32(96, 88, 76, 255);
    dl->AddRectFilled(at(c, s, -1.0f, -0.2f), at(c, s, 1.0f, 0.8f), stone, 2);
    for (float x : {-1.0f, 0.55f}) dl->AddRectFilled(at(c, s, x, -0.75f), at(c, s, x + 0.45f, 0.8f), stone, 1);
    dl->AddRectFilled(at(c, s, -0.25f, -1.0f), at(c, s, 0.25f, 0.8f), stone, 1);
    for (float x : {-1.0f, -0.25f, 0.55f})
        for (int k = 0; k < 2; ++k) {
            float top = x == -0.25f ? -1.0f : -0.75f;
            dl->AddRectFilled(at(c, s, x + k * 0.27f, top - 0.18f), at(c, s, x + k * 0.27f + 0.17f, top), stone);
        }
    dl->AddRectFilled(at(c, s, -0.2f, 0.3f), at(c, s, 0.2f, 0.8f), dark, 4, ImDrawFlags_RoundTop);
    dl->AddLine(at(c, s, 0, -1.2f), at(c, s, 0, -1.75f), dark, 1.5f);
    poly(dl, c, s, {{0, -1.75f}, {0.6f, -1.6f}, {0, -1.45f}}, col::gold);
}

void draw_tower(ImDrawList* dl, ImVec2 c, float s, ImU32 stone) {
    dl->AddRectFilled(at(c, s, -0.5f, -0.9f), at(c, s, 0.5f, 0.8f), stone, 1);
    for (int k = 0; k < 3; ++k) dl->AddRectFilled(at(c, s, -0.5f + k * 0.38f, -1.15f), at(c, s, -0.26f + k * 0.38f, -0.9f), stone);
    dl->AddRectFilled(at(c, s, -0.15f, -0.4f), at(c, s, 0.15f, -0.05f), IM_COL32(40, 40, 50, 255));
}

// Рубеж на карте: флаг (взят), скрещённые мечи или корона (открыт), тёмная метка (закрыт).
void draw_frontier(ImDrawList* dl, ImVec2 c, float s, const Frontier& f, float time) {
    bool boss = f.id.size() == 3 && f.id[2] == '3';
    float size = boss ? s * 1.3f : s;
    if (f.status == Frontier::Cleared) {
        dl->AddCircleFilled(c, size, IM_COL32(30, 34, 30, 220));
        dl->AddLine(at(c, size, -0.3f, 0.7f), at(c, size, -0.3f, -0.8f), IM_COL32(200, 190, 170, 255), 2);
        poly(dl, c, size, {{-0.3f, -0.8f}, {0.6f, -0.55f}, {-0.3f, -0.25f}}, col::gold);
        return;
    }
    if (f.status == Frontier::Locked) {
        dl->AddCircleFilled(c, size * 0.75f, IM_COL32(40, 42, 50, 230));
        dl->AddCircle(c, size * 0.75f, IM_COL32(90, 92, 104, 200), 0, 1.5f);
        return;
    }
    float pulse = 0.5f + 0.5f * std::sin(time * 3.0f);
    ImU32 ring = f.ready() ? col::red : col::orange;
    dl->AddCircleFilled(c, size * (1.5f + 0.25f * pulse), alpha(ring, 0.12f + 0.12f * pulse));
    dl->AddCircleFilled(c, size, IM_COL32(40, 22, 22, 240));
    dl->AddCircle(c, size, ring, 0, 2);
    if (f.enemy.defense) {
        draw_tower(dl, c, size * 0.6f, IM_COL32(200, 196, 214, 255));
    } else if (boss) {
        poly(dl, c, size * 0.7f, {{-0.6f, 0.1f}, {-0.6f, -0.5f}, {-0.3f, -0.2f}, {0, -0.65f}, {0.3f, -0.2f}, {0.6f, -0.5f}, {0.6f, 0.1f}}, col::gold);
        dl->AddCircleFilled(at(c, size, -0.22f, 0.35f), size * 0.1f, col::red);
        dl->AddCircleFilled(at(c, size, 0.22f, 0.35f), size * 0.1f, col::red);
    } else {
        ImU32 steel = IM_COL32(220, 220, 230, 255);
        dl->AddLine(at(c, size, -0.55f, -0.55f), at(c, size, 0.55f, 0.55f), steel, 2.5f);
        dl->AddLine(at(c, size, 0.55f, -0.55f), at(c, size, -0.55f, 0.55f), steel, 2.5f);
        dl->AddLine(at(c, size, -0.55f, 0.25f), at(c, size, -0.25f, 0.55f), col::gold, 2.5f);
        dl->AddLine(at(c, size, 0.55f, 0.25f), at(c, size, 0.25f, 0.55f), col::gold, 2.5f);
    }
}

void draw_coin(ImDrawList* dl, ImVec2 c, float r) {
    dl->AddCircleFilled(c, r, col::gold_dark);
    dl->AddCircleFilled(c, r * 0.82f, col::gold);
    dl->AddCircle(c, r * 0.55f, col::gold_dark, 0, 1.5f);
}

}  // namespace

// ---------------------------------------------------------------------------

void App::game_act(const std::function<std::string()>& action, const std::string& done) {
    std::string err = action();
    if (!err.empty()) {
        toast("Не получилось", err, col::red);
        return;
    }
    refresh();
    if (!done.empty()) toast("Готово", done, col::teal);
}

void App::watch(BattleResult result, std::string title, std::string reward, std::string verdict) {
    float speed = battle_.speed > 0.01f ? battle_.speed : 1.0f;
    battle_ = BattleView{};
    battle_.active = true;
    battle_.result = std::move(result);
    battle_.title = std::move(title);
    battle_.reward = std::move(reward);
    battle_.verdict = std::move(verdict);
    battle_.progress = -1.0f;
    battle_.speed = speed;
}

void App::start_manual(const std::string& id) {
    auto battle = quest::start_battle(world_, *game_log_, id);
    const Frontier* f = world_.frontier(id);
    if (!battle || !f) {
        toast("Бой не начать", f ? battle_blocker(world_, *f) : "Нет такого рубежа", col::red);
        return;
    }
    watch(battle->result(), f->name, f->loot.describe(), "Рубеж взят!");
    battle_.loot.gold = f->loot.gold;
    battle_.live = std::shared_ptr<quest::Battle>(std::move(battle));
    battle_.frontier = id;
}

void App::start_training() {
    // Без рубежа record_battle ничего не пишет: в Ристалище нельзя ни потерять, ни заработать.
    auto seed = uint32_t(std::chrono::steady_clock::now().time_since_epoch().count());
    auto battle = quest::training_battle(world_, seed);
    watch(battle->result(), "Ристалище", "Учебный бой: бойцы не гибнут по-настоящему", "Учебный бой выигран!");
    battle_.live = std::shared_ptr<quest::Battle>(std::move(battle));
}

void App::page_campaign() {
    // Выбор кампании: встроенная и файлы *.ini из папок campaigns.
    ImGui::SetNextItemWidth(std::min(px(340), ImGui::GetContentRegionAvail().x * 0.45f));
    if (ImGui::BeginCombo("##campaign", world_.campaign_title.c_str())) {
        for (auto& c : library_.campaigns) {
            bool selected = c.id == world_.campaign_id;
            if (ImGui::Selectable((c.title + "##" + c.id).c_str(), selected)) {
                select_campaign(cfg_, c.id);
                map_zoom_ = 0;
                selected_frontier_.clear();
                refresh();
                toast("Кампания", c.title, col::gold);
            }
            std::string tip = c.description;
            if (!c.author.empty()) tip += (tip.empty() ? "" : "\n") + std::string("Автор: ") + c.author;
            tip += (tip.empty() ? "" : "\n") + std::string("Источник: ") + c.source;
            tooltip(tip.c_str());
        }
        ImGui::EndCombo();
    }
    tooltip("Прогресс рубежей у каждой кампании свой; золото, армия, кузня и магия общие.");
    ImGui::SameLine();
    if (ImGui::Button(std::format("{}  Папка кампаний", icon::folder).c_str())) {
        std::error_code ec;
        fs::create_directories(quest::user_campaigns_dir(cfg_), ec);
        shell_open(from_path(quest::user_campaigns_dir(cfg_)));
    }
    tooltip("Положите сюда файл кампании (*.ini) и нажмите «Перечитать».");
    ImGui::SameLine();
    if (ImGui::Button(std::format("{}  Перечитать", icon::sync).c_str())) {
        library_ = quest::load_campaigns(cfg_);
        map_zoom_ = 0;
        refresh();
        toast("Кампании перечитаны", std::format("Доступно: {}{}", library_.campaigns.size(),
                                                 library_.errors.empty() ? "" : std::format(", с ошибками: {}", library_.errors.size())),
              library_.errors.empty() ? col::teal : col::orange);
    }
    if (!library_.errors.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, col::orange);
        bool open = ImGui::CollapsingHeader(std::format("{}  Файлы кампаний с ошибками: {}", icon::warning, library_.errors.size()).c_str());
        ImGui::PopStyleColor();
        if (open)
            for (auto& e : library_.errors) wrapped_dim(e);
    }
    ImGui::Dummy(ImVec2(0, px(2)));

    const char* tabs[2] = {"Карта", "Летопись"};
    const char* icons[2] = {icon::map, icon::book};
    for (int i = 0; i < 2; ++i) {
        if (i) ImGui::SameLine();
        bool on = campaign_tab_ == i;
        ImGui::PushStyleColor(ImGuiCol_Button, on ? alpha(col::gold, 0.25f) : col::panel_hi);
        ImGui::PushStyleColor(ImGuiCol_Text, on ? col::gold : col::text);
        if (ImGui::Button(std::format("{}  {}", icons[i], tabs[i]).c_str())) {
            campaign_tab_ = i;
            if (i == 1) chronicle_scroll_ = true;
        }
        ImGui::PopStyleColor(2);
    }
    ImGui::SameLine(0, px(20));
    ImGui::AlignTextToFramePadding();
    dim_text("Рубежей взято: %d из %zu", world_.cleared, world_.frontiers.size());
    {
        std::string arena = std::format("{}  Ристалище", icon::game);
        float w = button_width(arena.c_str());
        float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;  // до SameLine: край строки
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), right - w));
        if (accent_button(arena.c_str(), ImVec2(w, 0), col::red)) start_training();
        tooltip("Тренировочный бой: все роды войск против тварей Разлома. Ничего не стоит и ничего не записывает — "
                "попробуйте новых бойцов и тактику.");
    }

    if (campaign_tab_ == 1) {
        campaign_chronicle();
        return;
    }
    float panel_w = std::min(px(360), ImGui::GetContentRegionAvail().x * 0.42f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));  // море до самой рамки
    ImGui::BeginChild("mapview", ImVec2(ImGui::GetContentRegionAvail().x - panel_w - px(10), 0), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    campaign_map();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("mappanel", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    campaign_panel();
    ImGui::EndChild();
}

void App::campaign_map() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    float time = float(clock());

    if (map_zoom_ <= 0) {
        float minx = kCitadel.x - 80, maxx = kCitadel.x + 80, miny = kCitadel.y - 80, maxy = kCitadel.y + 80;
        for (auto& c : world_.chapters) {
            minx = std::min(minx, c.x - c.radius);
            maxx = std::max(maxx, c.x + c.radius);
            miny = std::min(miny, c.y - c.radius);
            maxy = std::max(maxy, c.y + c.radius);
        }
        // Сначала показываем окрестности текущей главы, а не всю карту целиком.
        map_zoom_ = std::max(std::min(size.x / (maxx - minx + 60), size.y / (maxy - miny + 120)), std::min(size.x, size.y) / 900.0f);
        const Frontier* f = world_.next();
        float fx = f ? world_.chapters[size_t(f->chapter)].x : world_.chapters.back().x;
        float half = size.x * 0.5f / map_zoom_;
        map_pan_ = ImVec2(std::clamp(fx, minx + half, std::max(minx + half, maxx - half)), (miny + maxy) * 0.5f);
    }
    ImVec2 center(p0.x + size.x * 0.5f, p0.y + size.y * 0.5f);
    auto S = [&](float x, float y) { return ImVec2(center.x + (x - map_pan_.x) * map_zoom_, center.y + (y - map_pan_.y) * map_zoom_); };

    ImGui::InvisibleButton("canvas", size, ImGuiButtonFlags_MouseButtonLeft);
    bool hovered = ImGui::IsItemHovered();
    bool released = ImGui::IsItemDeactivated();
    static bool dragged = false;
    if (ImGui::IsItemActivated()) dragged = false;
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f)) {
        ImVec2 d = ImGui::GetIO().MouseDelta;
        map_pan_.x -= d.x / map_zoom_;
        map_pan_.y -= d.y / map_zoom_;
        dragged = true;
    }
    if (hovered && ImGui::GetIO().MouseWheel != 0) {
        ImVec2 m = ImGui::GetIO().MousePos;
        float wx = (m.x - center.x) / map_zoom_ + map_pan_.x, wy = (m.y - center.y) / map_zoom_ + map_pan_.y;
        map_zoom_ = std::clamp(map_zoom_ * std::pow(1.15f, ImGui::GetIO().MouseWheel), 0.2f, 3.0f);
        map_pan_.x = wx - (m.x - center.x) / map_zoom_;
        map_pan_.y = wy - (m.y - center.y) / map_zoom_;
    }

    dl->PushClipRect(p0, add(p0, size), true);
    // Ночное море с барашками волн; волны привязаны к карте и едут вместе с ней.
    dl->AddRectFilledMultiColor(p0, add(p0, size), IM_COL32(20, 34, 48, 255), IM_COL32(20, 34, 48, 255), IM_COL32(10, 18, 30, 255),
                                IM_COL32(10, 18, 30, 255));
    {
        float step = 70.0f;
        ImVec2 w0((p0.x - center.x) / map_zoom_ + map_pan_.x, (p0.y - center.y) / map_zoom_ + map_pan_.y);
        ImVec2 w1((p0.x + size.x - center.x) / map_zoom_ + map_pan_.x, (p0.y + size.y - center.y) / map_zoom_ + map_pan_.y);
        for (float gy = std::floor(w0.y / step) * step; gy <= w1.y + step; gy += step)
            for (float gx = std::floor(w0.x / step) * step; gx <= w1.x + step; gx += step) {
                uint32_t h = (uint32_t(int(gx)) * 73856093u ^ uint32_t(int(gy)) * 19349663u) * 2654435761u;
                h ^= h >> 15;
                if (h % 3 == 0) continue;
                ImVec2 c = S(gx + float(h % 40), gy + float((h >> 8) % 40));
                float wl = 9.0f * map_zoom_ + 3.0f;
                dl->PathArcTo(ImVec2(c.x - wl * 0.5f, c.y), wl * 0.5f, kPi * 1.1f, kPi * 1.9f, 6);
                dl->PathStroke(IM_COL32(150, 190, 220, 26), 1.2f, ImDrawFlags_None);
                dl->PathArcTo(ImVec2(c.x + wl * 0.5f, c.y), wl * 0.5f, kPi * 1.1f, kPi * 1.9f, 6);
                dl->PathStroke(IM_COL32(150, 190, 220, 26), 1.2f, ImDrawFlags_None);
            }
    }

    // Земли глав: песчаный берег, суша в цвете главы, светлая кромка.
    for (size_t i = 0; i < world_.chapters.size(); ++i) {
        const Chapter& c = world_.chapters[i];
        std::vector<ImVec2> blob, shore;
        for (int k = 0; k < 48; ++k) {
            float a = k * 2 * kPi / 48;
            float rr = c.radius * (1 + 0.07f * std::sin(a * 3 + float(i)) + 0.04f * std::sin(a * 7 + float(i) * 2));
            blob.push_back(S(c.x + std::cos(a) * rr, c.y + std::sin(a) * rr));
            shore.push_back(S(c.x + std::cos(a) * (rr + 9), c.y + std::sin(a) * (rr + 9)));
        }
        ImU32 fill = c.reached ? land(c.color) : IM_COL32(38, 41, 50, 255);
        dl->AddPolyline(shore.data(), int(shore.size()), IM_COL32(150, 190, 220, 30), 1.5f, ImDrawFlags_Closed);
        dl->AddConcavePolyFilled(shore.data(), int(shore.size()), c.reached ? IM_COL32(110, 92, 64, 255) : IM_COL32(44, 46, 54, 255));
        dl->AddConcavePolyFilled(blob.data(), int(blob.size()), fill);
        dl->AddPolyline(blob.data(), int(blob.size()), mix(fill, IM_COL32(255, 240, 210, 255), 0.18f), 1.5f, ImDrawFlags_Closed);
        // Рощи и холмы — пятнышки на суше, чтобы земля не была плоской.
        if (c.reached)
            for (int k = 0; k < 14; ++k) {
                uint32_t h = hash_of(std::format("{}:{}", i, k));
                float a = float(h % 628) / 100.0f, d = c.radius * 0.25f + float((h >> 10) % 100) / 100.0f * c.radius * 0.6f;
                ImVec2 t = S(c.x + std::cos(a) * d, c.y + std::sin(a) * d);
                float tr = (4.0f + float((h >> 18) % 4)) * map_zoom_;
                if (h % 2) dl->AddCircleFilled(t, tr, mix(fill, IM_COL32(20, 40, 20, 255), 0.35f), 10);
                else dl->AddTriangleFilled(ImVec2(t.x - tr * 1.4f, t.y + tr), ImVec2(t.x + tr * 1.4f, t.y + tr), ImVec2(t.x, t.y - tr * 1.2f),
                                           mix(fill, IM_COL32(0, 0, 0, 255), 0.25f));
            }
    }

    // Дорога: Цитадель, затем рубежи по порядку.
    ImVec2 prev = kCitadel;
    for (auto& f : world_.frontiers) {
        ImVec2 a = S(prev.x, prev.y), b = S(f.x, f.y);
        int dashes = std::max(2, int(std::hypot(b.x - a.x, b.y - a.y) / 10));
        ImU32 road = f.status == Frontier::Cleared ? IM_COL32(214, 176, 92, 230) : f.status == Frontier::Open ? IM_COL32(150, 130, 96, 220) : IM_COL32(80, 78, 74, 140);
        for (int k = 0; k < dashes; k += 2) dl->AddLine(lerp(a, b, float(k) / dashes), lerp(a, b, float(k + 1) / dashes), road, 3 * map_zoom_);
        prev = ImVec2(f.x, f.y);
    }

    draw_castle(dl, S(kCitadel.x, kCitadel.y), std::clamp(22 * map_zoom_, 12.0f, 30.0f));
    text_center(dl, fonts.display, std::clamp(16 * map_zoom_ * 1.3f, 11.0f, 20.0f) * ui_scale, S(kCitadel.x, kCitadel.y + 55), IM_COL32(230, 220, 196, 255), "Цитадель");

    float s = std::clamp(13.0f * map_zoom_ * 1.4f, 8.0f, 22.0f);
    const Frontier* hover = nullptr;
    ImVec2 mouse = ImGui::GetIO().MousePos;
    for (auto& f : world_.frontiers) {
        ImVec2 c = S(f.x, f.y);
        if (f.id == selected_frontier_) dl->AddCircle(c, s * 2.0f, col::gold, 0, 2.0f);
        draw_frontier(dl, c, s, f, time);
        if (world_.chapters[size_t(f.chapter)].reached)
            text_center(dl, fonts.semibold, std::clamp(13 * map_zoom_ * 1.3f, 10.0f, 15.0f) * ui_scale, ImVec2(c.x, c.y + s * 1.9f), IM_COL32(220, 214, 200, 230), f.id.c_str());
        if (hovered && std::hypot(mouse.x - c.x, mouse.y - c.y) < s * 1.5f) hover = &f;
    }

    // Туман неизведанного над главами, до которых ещё не дошли.
    for (size_t i = 0; i < world_.chapters.size(); ++i) {
        const Chapter& c = world_.chapters[i];
        if (c.reached) continue;
        for (int k = 0; k < 4; ++k) {
            float a = k * kPi / 2 + 0.4f;
            draw_fog(dl, S(c.x + std::cos(a) * c.radius * 0.45f, c.y + std::sin(a) * c.radius * 0.45f), c.radius * 0.75f * map_zoom_, uint32_t(i * 13 + k), time);
        }
        draw_fog(dl, S(c.x, c.y), c.radius * 0.8f * map_zoom_, uint32_t(i * 13 + 9), time);
    }
    for (size_t i = 0; i < world_.chapters.size(); ++i) {
        const Chapter& c = world_.chapters[i];
        std::string name = c.reached ? std::format("Глава {}. {}", i + 1, c.name) : std::format("Глава {} — неизведано", i + 1);
        float ns = std::clamp(18 * map_zoom_ * 1.3f, 12.0f, 24.0f);
        ImVec2 at = S(c.x, c.y - c.radius - 18);
        text_center(dl, fonts.display, ns, ImVec2(at.x + 1, at.y + 1), IM_COL32(0, 0, 0, 200), name.c_str());
        text_center(dl, fonts.display, ns, at, c.reached ? IM_COL32(244, 222, 170, 255) : IM_COL32(140, 140, 150, 255), name.c_str());
    }

    // Роза ветров в углу.
    {
        float cr = std::min(size.x, size.y) * 0.07f + 10;
        ImVec2 cc(p0.x + size.x - cr - px(16), p0.y + size.y - cr - px(44));
        dl->AddCircle(cc, cr * 0.72f, IM_COL32(220, 190, 130, 90), 32, 1.2f);
        for (int k = 0; k < 8; ++k) {
            float a = float(k) * kPi / 4 - kPi / 2, len = k % 2 ? cr * 0.55f : cr;
            ImVec2 tip(cc.x + std::cos(a) * len, cc.y + std::sin(a) * len);
            ImVec2 l(cc.x + std::cos(a - kPi / 2) * cr * 0.13f, cc.y + std::sin(a - kPi / 2) * cr * 0.13f);
            ImVec2 r(cc.x + std::cos(a + kPi / 2) * cr * 0.13f, cc.y + std::sin(a + kPi / 2) * cr * 0.13f);
            dl->AddTriangleFilled(l, tip, cc, k == 0 ? IM_COL32(220, 80, 60, 230) : IM_COL32(230, 200, 140, 200));
            dl->AddTriangleFilled(cc, tip, r, k == 0 ? IM_COL32(160, 50, 40, 230) : IM_COL32(150, 124, 84, 200));
        }
        text_center(dl, fonts.display, 14.0f * ui_scale, ImVec2(cc.x, cc.y - cr - px(9)), IM_COL32(230, 200, 140, 220), "С");
    }
    dl->PopClipRect();
    {
        ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
        ornate_frame(ImGui::GetWindowDrawList(), wp, add(wp, ws), col::border, ImGui::GetStyle().ChildRounding);
    }

    if (hover) {
        ImGui::BeginTooltip();
        ImGui::Text("%s · %s", hover->id.c_str(), hover->name.c_str());
        dim_text("%s", hover->status == Frontier::Cleared ? "взят" : hover->status == Frontier::Open ? "открыт" : "закрыт");
        ImGui::EndTooltip();
    }
    if (released && !dragged && hover) selected_frontier_ = hover->id;

    ImGui::SetCursorScreenPos(ImVec2(p0.x + px(10), p0.y + size.y - ImGui::GetFontSize() - px(10)));
    dim_text("Колесо — масштаб · перетаскивание — сдвиг · щелчок — рубеж");
}

void App::campaign_panel() {
    const Frontier* f = selected_frontier_.empty() ? nullptr : world_.frontier(selected_frontier_);
    if (!f) f = world_.next();
    if (!f) {
        heading("Кампания пройдена");
        wrapped_dim("Великий Щит снова цел. Летопись хранит всю историю похода.");
        return;
    }
    const Chapter& ch = world_.chapters[size_t(f->chapter)];
    dim_text("Глава %d · %s", f->chapter + 1, ch.name.c_str());
    ImGui::PushFont(fonts.display, 23.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, col::gold);
    ImGui::TextWrapped("%s. %s", f->id.c_str(), f->name.c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();
    const char* status = f->status == Frontier::Cleared ? "Взят" : f->status == Frontier::Open ? "Открыт" : "Закрыт";
    pill(status, f->status == Frontier::Cleared ? col::teal : f->status == Frontier::Open ? col::gold : col::faint);
    if (f->enemy.defense) {
        ImGui::SameLine();
        pill("оборона", col::blue);
    }
    ImGui::Dummy(ImVec2(0, px(2)));
    ImGui::TextWrapped("%s", f->status == Frontier::Cleared ? f->victory.c_str() : f->intro.c_str());
    ImGui::Separator();

    if (f->status == Frontier::Locked) wrapped_dim("Откроется, когда будут взяты предыдущие рубежи.");
    subheading("Требования");
    for (auto& r : f->requirements) {
        ImU32 color = r.met ? col::teal : r.advisory ? col::blue : col::red;
        colored_text(color, r.met ? icon::check : r.advisory ? icon::info : icon::close);
        ImGui::SameLine();
        if (r.advisory && !r.met) dim_text("%s", r.text.c_str());
        else ImGui::TextUnformatted(r.text.c_str());
        if (r.advisory) tooltip("Не обязательно: уровень героя усиливает армию (+1 к атаке и защите за два уровня, +4% здоровья за уровень). "
                                "Растёт от опыта за миссии и побочные квесты.");
        else if (!r.met && starts_with(r.text, "Заклинание")) tooltip("Изучается в башне магов в городе.");
        else if (!r.met) tooltip("Куётся в кузне в городе.");
    }
    int ours = army_power(world_.army), theirs = power_of(f->enemy.stacks);
    colored_text(ours >= theirs ? col::teal : col::orange, ours >= theirs ? icon::check : icon::warning);
    ImGui::SameLine();
    ImGui::Text("Сила армии: %d против %d", ours, theirs);
    bar(ours + theirs ? float(ours) / float(ours + theirs) : 0.5f, ImVec2(0, px(8)), col::blue, IM_COL32(120, 170, 255, 255), alpha(tint(world_.chapters[size_t(f->chapter)].color), 0.6f));
    if (ours < theirs) wrapped_dim("Армия слабее врага: бой можно проиграть. Наймите и обучите войска в городе.");

    ImGui::Dummy(ImVec2(0, px(4)));
    {
        // Сценка с врагами рубежа.
        ImDrawList* sdl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 sz(ImGui::GetContentRegionAvail().x, px(120));
        draw_scene(sdl, p, sz, world_.chapters[size_t(f->chapter)].color);
        int shown = std::min(3, int(f->enemy.stacks.size()));
        for (int k = 0; k < shown; ++k) {
            const Stack& s = f->enemy.stacks[size_t(k)];
            float x = p.x + sz.x * (shown == 1 ? 0.5f : 0.25f + 0.5f * float(k) / float(shown - 1));
            float h = s.icon == "boss" ? px(88) : px(54);
            if (s.icon != "boss" && s.count > 1) draw_figure(sdl, ImVec2(x + px(18), p.y + sz.y - px(18)), h * 0.85f, s, false, float(clock()));
            draw_figure(sdl, ImVec2(x, p.y + sz.y - px(10)), h, s, false, float(clock()));
        }
        sdl->AddRect(p, ImVec2(p.x + sz.x, p.y + sz.y), col::border, px(8), px(1.5f));
        ImGui::Dummy(sz);
    }
    subheading("Враг");
    for (auto& s : f->enemy.stacks) {
        ImGui::BulletText("%s × %d", s.name.c_str(), s.count);
        dim_text("    здоровье %d, атака %d, защита %d", s.hp, s.atk, s.def);
    }
    dim_text("Добыча: %s", f->loot.describe().c_str());

    ImGui::Dummy(ImVec2(0, px(6)));
    if (f->status == Frontier::Open) {
        std::string blocker = battle_blocker(world_, *f);
        ImGui::BeginDisabled(!blocker.empty());
        if (accent_button(std::format("{}  Командовать боем", icon::game).c_str(), ImVec2(-1, px(40)), col::red)) start_manual(f->id);
        if (!blocker.empty()) tooltip(blocker.c_str());
        else tooltip("Пошаговый бой: вы выбираете цели, защиту и заклинания героя, за врага ходит ИИ.");
        if (ImGui::Button("Автобой", ImVec2(-1, 0))) {
            std::string id = f->id, name = f->name, loot = f->loot.describe();
            int gold = f->loot.gold;
            BattleResult r = fight(world_, *game_log_, id);
            refresh();
            watch(std::move(r), name, loot, "Рубеж взят!");
            battle_.loot.gold = gold;
        }
        ImGui::EndDisabled();
        if (ImGui::Button(std::format("{}  В город · {} золота", icon::town, world_.gold).c_str(), ImVec2(-1, 0))) page = Page::Town;
        wrapped_dim("Поражение ничего не отнимает: армия отступает в город без потерь, опыт и награды остаются. Рубеж можно атаковать снова.");
    }
}

void App::campaign_chronicle() {
    ImGui::BeginChild("chronicle", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    for (auto& c : world_.chronicle) {
        if (c.tone == Chronicle::Chapter) {
            // Глава летописи: заголовок, росчерк и сюжет главы.
            ImGui::Dummy(ImVec2(0, px(8)));
            ImGui::PushFont(fonts.display, 22.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, col::gold);
            ImGui::TextWrapped("%s", c.title.c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImVec2 a = ImGui::GetCursorScreenPos();
            flourish(ImGui::GetWindowDrawList(), ImVec2(a.x, a.y), ImVec2(a.x + std::min(px(260), ImGui::GetContentRegionAvail().x), a.y + px(6)),
                     alpha(col::gold, 0.6f));
            ImGui::Dummy(ImVec2(0, px(8)));
            ImGui::PushFont(fonts.regular, 17.5f);
            ImGui::TextWrapped("%s", c.text.c_str());
            ImGui::PopFont();
            ImGui::Dummy(ImVec2(0, px(6)));
            continue;
        }
        ImU32 color = c.tone == Chronicle::Good ? col::teal : c.tone == Chronicle::Bad ? col::red : col::dim;
        dim_text("%s", c.time.substr(0, 10).c_str());
        ImGui::SameLine(px(100));
        ImGui::PushStyleColor(ImGuiCol_Text, mix(color, col::text, 0.55f));
        ImGui::TextWrapped("%s", c.text.c_str());
        ImGui::PopStyleColor();
    }
    if (chronicle_scroll_) {
        ImGui::SetScrollHereY(1.0f);
        chronicle_scroll_ = false;
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// Город

void App::page_town() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ImGui::GetFrameHeight();
    draw_coin(dl, ImVec2(p.x + h * 0.5f, p.y + h * 0.5f), h * 0.45f);
    ImGui::Dummy(ImVec2(h, h));
    ImGui::SameLine();
    ImGui::PushFont(fonts.display, 26.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, col::gold);
    ImGui::Text("Город · %d золота", world_.gold);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    wrapped_dim(std::format("Золото приносят миссии и побочные квесты. Уровень героя ({}) открывает новые войска. Бонусы героя: атака +{}, "
                            "защита +{}, сила магии {}.",
                            world_.level, world_.army.atk_bonus, world_.army.def_bonus, world_.army.power));
    if (accent_button(std::format("{}  Ристалище: учебный бой", icon::game).c_str(), ImVec2(0, 0), col::red)) start_training();
    tooltip("Все роды войск, даже ещё не открытые, против тварей Разлома. Бесплатно и без последствий.");
    ImGui::Dummy(ImVec2(0, px(4)));

    heading("Казармы", 20.0f);
    auto& units = unit_types();
    int columns = std::max(1, int(ImGui::GetContentRegionAvail().x / px(300)));
    // В журнале роды войск хранятся по id, новые дописываются в конец списка, а в казармах — по уровню.
    std::vector<size_t> order(units.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::ranges::stable_sort(order, [&](size_t a, size_t b) { return units[a].level_req < units[b].level_req; });
    if (ImGui::BeginTable("units", columns, ImGuiTableFlags_SizingStretchSame)) {
        for (size_t i : order) {
            const UnitType& u = units[i];
            ImGui::TableNextColumn();
            ImGui::PushID(int(i));
            bool locked = world_.level < u.level_req;
            card_begin("unit");
            ImDrawList* cdl = ImGui::GetWindowDrawList();
            ImVec2 cp = ImGui::GetCursorScreenPos();
            Stack preview;
            preview.icon = u.id;
            preview.elite = world_.army_count(int(i), true) > 0;
            ImVec2 box(px(64), px(64));
            draw_scene(cdl, cp, box, IM_COL32(110, 150, 200, 255));
            draw_figure(cdl, ImVec2(cp.x + box.x * (u.id == std::string("griffin") ? 0.45f : u.id == std::string("rider") ? 0.36f : 0.5f), cp.y + box.y - px(6)), px(u.id == std::string("griffin") || u.id == std::string("rider") ? 38 : 46),
                        preview, true, float(clock()), int(i));
            if (locked) cdl->AddRectFilled(cp, ImVec2(cp.x + box.x, cp.y + box.y), IM_COL32(10, 12, 20, 160));
            cdl->AddRect(cp, ImVec2(cp.x + box.x, cp.y + box.y), col::border, px(6), px(1.5f));
            ImGui::Dummy(box);
            ImGui::SameLine();
            ImGui::BeginGroup();
            subheading(u.name);
            dim_text("элита: %s", u.elite_name);
            ImGui::EndGroup();
            wrapped_dim(std::format("{} здоровья · атака {} · защита {} · урон {}–{} · скорость {}{}", u.hp, u.atk, u.def, u.dmg_min, u.dmg_max, u.speed,
                                    u.ranged ? " · стрелок" : ""));
            if (*u.ability) {
                ImGui::PushStyleColor(ImGuiCol_Text, col::violet);
                ImGui::TextWrapped("%s %s", icon::star, u.ability);
                ImGui::PopStyleColor();
            }

            int have = world_.army_count(int(i), false), elite = world_.army_count(int(i), true);
            if (have || elite) dim_text("В строю: %d, элиты %d", have, elite);
            if (locked) {
                colored_text(col::faint, std::format("{} Откроется на {} уровне героя", icon::lock, u.level_req).c_str());
            } else {
                int& count = hire_counts_[int(i)];
                if (count <= 0) count = std::clamp(world_.gold / u.cost, 1, 5);
                ImGui::SetNextItemWidth(px(110));
                ImGui::InputInt("##count", &count, 1, 5);
                count = std::clamp(count, 1, 999);
                ImGui::SameLine();
                ImGui::BeginDisabled(u.cost * count > world_.gold);
                if (accent_button(std::format("Нанять · {}", u.cost * count).c_str(), ImVec2(-1, 0)))
                    game_act([&] { return hire(world_, *game_log_, int(i), count); }, std::format("{} × {} встали под знамёна", u.name, count));
                ImGui::EndDisabled();
                if (have > 0) {
                    int cost = upgrade_cost(int(i)) * have;
                    ImGui::BeginDisabled(cost > world_.gold);
                    if (ImGui::Button(std::format("Обучить {} до элиты · {}", have, cost).c_str(), ImVec2(-1, 0)))
                        game_act([&] { return upgrade(world_, *game_log_, int(i), have); }, std::format("Теперь это {}", u.elite_name));
                    ImGui::EndDisabled();
                }
            }
            card_end();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::Dummy(ImVec2(0, px(6)));
    heading("Кузня", 20.0f);
    if (ImGui::BeginTable("forge", 3, ImGuiTableFlags_SizingStretchSame)) {
        for (size_t i = 0; i < forge_items().size(); ++i) {
            const ForgeItem& f = forge_items()[i];
            int lvl = i == 0 ? world_.army.armor : i == 1 ? world_.army.weapon : world_.army.walls;
            ImGui::TableNextColumn();
            ImGui::PushID(int(i) + 100);
            card_begin("forge");
            subheading(f.name);
            ImGui::Text("Уровень %d из %zu", lvl, f.costs.size());
            wrapped_dim(f.effect);
            if (lvl < int(f.costs.size())) {
                int cost = f.costs[size_t(lvl)];
                ImGui::BeginDisabled(cost > world_.gold);
                if (accent_button(std::format("Выковать {}-й · {}", lvl + 1, cost).c_str(), ImVec2(-1, 0)))
                    game_act([&] { return forge(world_, *game_log_, int(i)); }, std::format("{}: уровень {}", f.name, lvl + 1));
                ImGui::EndDisabled();
            } else {
                colored_text(col::teal, std::format("{} Наивысший уровень", icon::check).c_str());
            }
            card_end();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::Dummy(ImVec2(0, px(6)));
    heading("Башня магов", 20.0f);
    if (ImGui::BeginTable("spells", 3, ImGuiTableFlags_SizingStretchSame)) {
        for (size_t i = 0; i < spells().size(); ++i) {
            const Spell& s = spells()[i];
            int lvl = world_.army.spell_levels[i];
            ImGui::TableNextColumn();
            ImGui::PushID(int(i) + 200);
            card_begin("spell");
            ImGui::PushStyleColor(ImGuiCol_Text, kSpellColor[i % 5]);
            subheading(s.name);
            ImGui::PopStyleColor();
            if (s.costs.size() > 1) ImGui::Text("Уровень %d из %zu", lvl, s.costs.size());
            else ImGui::TextUnformatted(lvl ? "Изучено" : "Не изучено");
            wrapped_dim(s.effect);
            if (lvl < int(s.costs.size())) {
                int cost = s.costs[size_t(lvl)];
                ImGui::BeginDisabled(cost > world_.gold);
                if (accent_button(std::format("{} · {}", lvl ? "Улучшить" : "Изучить", cost).c_str(), ImVec2(-1, 0)))
                    game_act([&] { return learn(world_, *game_log_, int(i)); }, std::format("Заклинание «{}»", s.name));
                ImGui::EndDisabled();
            } else {
                colored_text(col::teal, std::format("{} Освоено", icon::check).c_str());
            }
            card_end();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::Dummy(ImVec2(0, px(6)));
    heading("Трофеи", 20.0f);
    wrapped_dim("Выпадают за миссии сложности 4 (иногда) и 5 (всегда). Действуют сразу и навсегда.");
    for (auto& a : artifacts()) {
        if (std::ranges::find(st_.artifacts, std::string(a.name)) == st_.artifacts.end()) continue;
        colored_text(col::gold, std::format("{}  {}", icon::star, a.name).c_str());
        ImGui::SameLine();
        dim_text("— %s", a.effect);
    }
    dim_text("Найдено %zu из %zu", st_.artifacts.size(), artifacts().size());
}

}  // namespace sq::gui
