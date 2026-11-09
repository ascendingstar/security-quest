#include "app.h"

#include "draw.h"
#include "theme.h"
#include "util.h"
#include "version.h"

#include <algorithm>
#include <format>

namespace sq::gui {

App::App(HWND hwnd) : hwnd_(hwnd) {
    cfg_.home = data_home();
    data_warning_ = init_config(cfg_).message;
    // Резервная копия раз в день, пока данные не трогала эта сессия: страховка от неудачного обновления.
    std::error_code ec;
    auto backups = list_backups(cfg_);
    bool today_done = !backups.empty() && starts_with(from_path(backups.front().filename()), format_date(today()));
    if (!today_done && fs::exists(cfg_.events_file(), ec)) make_backup(cfg_, "");
    load();
}

double App::clock() const {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - started_).count();
}

// ---------------------------------------------------------------------------
// Данные

void App::load() {
    reload_config(cfg_);
    log_ = std::make_unique<EventLog>(cfg_.events_file());
    game_log_ = std::make_unique<EventLog>(cfg_.game_file());
    log_->load();
    game_log_->load();
    library_ = quest::load_campaigns(cfg_);
    std::error_code ec;
    log_mtime_ = fs::last_write_time(cfg_.events_file(), ec);
    game_mtime_ = fs::last_write_time(cfg_.game_file(), ec);
    refresh();
}

const quest::CampaignDef& App::campaign() const {
    if (const quest::CampaignDef* c = library_.find(quest::active_campaign_id(cfg_))) return *c;
    return library_.campaigns.front();
}

void App::refresh() {
    st_ = replay(cfg_, *log_);
    world_ = quest::build_world(cfg_, st_, *game_log_, campaign());
    std::error_code ec;
    log_mtime_ = fs::last_write_time(cfg_.events_file(), ec);
    game_mtime_ = fs::last_write_time(cfg_.game_file(), ec);
}

std::string App::tray_tip() const {
    Level lv = level_of(st_.earned.xp);
    return std::format("Security Quest · уровень {} · {} золота", lv.level, world_.gold);
}

bool App::close_to_tray() const {
    return cfg_.ini.num("gui", "close_to_tray", 1) != 0;
}

// Награды за задачи, выполненные с first_event (например, из консоли), и новый уровень.
void App::after_work(size_t first_event, int level_before) {
    refresh();
    const auto& events = log_->all();
    for (size_t i = first_event; i < events.size(); ++i) {
        if (events[i].type != "task.done") continue;
        if (const Task* t = st_.find(events[i].get("id"))) {
            toast(std::format("{} выполнен{}", t->side ? "Квест" : "Миссия", t->side ? "" : "а"), t->title + ": " + t->rewards.describe(), col::gold);
            if (!window_visible && notify) notify(t->title, t->rewards.describe());
        }
    }
    int after = level_of(st_.earned.xp).level;
    if (after > level_before) {
        toast(std::format("Уровень {}!", after), "Герою открываются новые войска и рубежи", col::teal);
        if (!window_visible && notify) notify(std::format("Уровень {}!", after), "Хранитель стал сильнее");
    }
}

void App::complete(const std::string& id) {
    int level_before = level_of(st_.earned.xp).level;
    ActionResult r = finish_task(*log_, st_, id);
    if (!r.ok) {
        toast("Не получилось", r.message, col::red);
        return;
    }
    refresh();
    if (const Task* t = st_.find(id)) celebrate(*t, level_before);
}

void App::celebrate(const Task& t, int level_before) {
    Loot loot;
    loot.xp = t.rewards.xp;
    loot.gold = t.rewards.gold;
    if (const Artifact* a = find_artifact(t.rewards.artifact)) {
        loot.trophy = a->name;
        loot.trophy_effect = a->effect;
    }
    int after = level_of(st_.earned.xp).level;
    if (after > level_before) loot.level_up = after;
    play_cutscene(t, loot);
}

void App::play_cutscene(const Task& t, const Loot& loot) {
    static const char* verdicts[6] = {"Тварь изгнана!", "Монстры повержены!", "Славная победа!", "Блестящая победа!", "Великая победа!",
                                      "Легендарная победа!"};
    cutscene_ = Cutscene{};
    cutscene_.active = true;
    cutscene_.tier = quest::celebration_tier(t);
    cutscene_.title = (t.side ? "Квест «" : "Миссия «") + t.title + "»";
    cutscene_.verdict = verdicts[cutscene_.tier - 1];
    cutscene_.loot = loot;
    cutscene_.ours = world_.army.stacks;
    cutscene_.foes = quest::task_monsters(t);
    cutscene_.born = clock();
}

void App::tick() {
    double now = clock();
    // Журналы могли дописать из консоли: подхватываем и показываем награды.
    if (now >= next_file_check_) {
        next_file_check_ = now + 1.0;
        std::error_code ec1, ec2;
        auto m1 = fs::last_write_time(cfg_.events_file(), ec1);
        auto m2 = fs::last_write_time(cfg_.game_file(), ec2);
        if ((!ec1 && m1 != log_mtime_) || (!ec2 && m2 != game_mtime_)) {
            size_t first = log_->all().size();
            int level_before = level_of(st_.earned.xp).level;
            log_->load();
            game_log_->load();
            after_work(first, level_before);
        }
    }
    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(), [&](const Toast& t) { return now - t.born > 6.0; }), toasts_.end());
}

bool App::busy() const {
    return !toasts_.empty() || battle_.active || cutscene_.active || (page == Page::Campaign && window_visible);
}

int App::frame_cap_ms() const {
    // Бою и сценкам нужна плавность, а дрейфу тумана на карте хватает 30 кадров в секунду.
    return battle_.active || cutscene_.active || !toasts_.empty() ? 0 : 33;
}

void App::toast(std::string title, std::string text, ImU32 color) {
    toasts_.push_back({std::move(title), std::move(text), color, clock()});
    if (toasts_.size() > 5) toasts_.erase(toasts_.begin());
}

void App::open_dialog(const std::string& name) {
    if (name == "mission") mission_form_.open = true;
    // done:<id>[@<секунды>] — выполнить задачу, как кнопкой «Выполнено», и остановить катсцену (для проверки).
    if (starts_with(name, "done:")) {
        auto at = name.find('@');
        complete(name.substr(5, at == std::string::npos ? std::string::npos : at - 5));
        if (at != std::string::npos && cutscene_.active) cutscene_.frozen = std::strtof(name.c_str() + at + 1, nullptr);
    }
    // edit:<id> — окно правки задачи (для снимков).
    if (starts_with(name, "edit:"))
        if (const Task* t = st_.find(name.substr(5))) edit_form_ = EditForm{true, t->id, t->title, t->side, t->difficulty, t->plan == "week" ? 1 : 0};
    if (starts_with(name, "select:")) {
        page = Page::Campaign;
        selected_frontier_ = name.substr(7);
    }
    if (name == "chronicle") {
        page = Page::Campaign;
        campaign_tab_ = 1;
    }
    // battle:<рубеж>@<ход> — бой без записи в журнал, остановленный на нужном ходе; для снимков.
    if (starts_with(name, "battle:")) {
        auto at = name.find('@');
        std::string id = name.substr(7, at == std::string::npos ? std::string::npos : at - 7);
        float progress = at == std::string::npos ? 0.6f : std::strtof(name.c_str() + at + 1, nullptr);
        if (const quest::Frontier* f = world_.frontier(id)) {
            quest::Army army = world_.army;
            army.spell_levels.assign(quest::spells().size(), 1);
            page = Page::Campaign;
            watch(quest::simulate(army, f->enemy, 42), f->name, f->loot.describe(), "Рубеж взят!");
            battle_.progress = progress;
            battle_.speed = 0.0001f;
        }
    }
    // manual:<рубеж> — ручной бой сразу на первом ходе (для снимков; итог запишется, только если бой доиграть).
    if (starts_with(name, "manual:")) {
        page = Page::Campaign;
        start_manual(name.substr(7));
        battle_.progress = 0;
        battle_.speed = 60;  // для снимка: сразу догнать журнал до хода игрока
    }
    // arena — Ристалище сразу на первом ходе игрока (для снимков).
    if (name == "arena") {
        page = Page::Campaign;
        start_training();
        battle_.speed = 60;
    }
    // cutscene:<размах 1–6>[@<секунды>] — катсцена выполненной задачи: вживую или остановленная на нужном моменте (для снимков).
    if (starts_with(name, "cutscene:")) {
        auto at = name.find('@');
        int tier = std::clamp(std::atoi(name.c_str() + 9), 1, 6);
        Task t;
        t.id = std::format("M{}", tier);
        t.side = tier <= 3;
        t.difficulty = t.side ? tier : tier - 1;
        t.title = t.side ? "Созвон с коллегой по интеграции" : "Ревью модуля авторизации";
        t.rewards = t.side ? side_rewards(cfg_, t.difficulty) : mission_rewards(cfg_, t.difficulty);
        Loot loot{t.rewards.xp, t.rewards.gold, "", "", 0};
        if (tier >= 5) loot = Loot{t.rewards.xp, t.rewards.gold, "Корона Хранителя", "+1 к атаке, защите и магии", 3};
        play_cutscene(t, loot);
        if (at != std::string::npos) cutscene_.frozen = std::strtof(name.c_str() + at + 1, nullptr);  // без @ — играет вживую
    }
}

// ---------------------------------------------------------------------------
// Каркас окна

void App::frame() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("root", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();

    draw_sidebar();
    ImGui::SameLine(0, 0);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    ImGui::BeginChild("main", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    {
        // За страницами — ночной пейзаж с крепостью Хранителя.
        ImVec2 mp = ImGui::GetWindowPos(), ms = ImGui::GetWindowSize();
        page_backdrop(ImGui::GetWindowDrawList(), mp, ImVec2(mp.x + ms.x, mp.y + ms.y));
    }
    draw_header();
    ImGui::Dummy(ImVec2(0, px(4)));
    if (!data_warning_.empty()) {
        card_begin("datawarning", ImVec2(0, 0), mix(col::panel, col::red, 0.25f));
        colored_text(col::red, std::format("{}  {}", icon::warning, data_warning_).c_str());
        card_end();
    }
    ImGui::BeginChild("page", ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, alpha(col::panel, 0.94f));  // вложенные панели (карта, летопись) — на камне
    if (cutscene_.active) {
        cutscene_view();
    } else if (battle_.active) {
        battle_view();
    } else {
        switch (page) {
            case Page::Overview: page_overview(); break;
            case Page::Missions: page_missions(); break;
            case Page::Sides: page_sides(); break;
            case Page::Campaign: page_campaign(); break;
            case Page::Town: page_town(); break;
            case Page::History: page_history(); break;
            case Page::Settings: page_settings(); break;
        }
    }
    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    dialog_mission();
    dialog_edit();
    ImGui::End();
    draw_toasts();
}

namespace {

// Медальон пункта меню: тёмный круг в бронзовой оправе с иконкой.
void medallion(ImDrawList* dl, ImVec2 c, float r, const char* glyph, bool active, bool hovered) {
    dl->AddCircleFilled(ImVec2(c.x, c.y + px(1.5f)), r + px(1), IM_COL32(0, 0, 0, 120), 28);
    dl->AddCircleFilled(c, r + px(1), active ? col::gold_dark : hovered ? col::bronze : col::border, 28);
    dl->AddCircleFilled(c, r - px(1), active ? IM_COL32(92, 36, 32, 255) : IM_COL32(24, 16, 18, 255), 28);
    dl->AddCircle(c, r - px(3), alpha(col::gold, active ? 0.5f : 0.15f), 28, 1.0f);
    text_center(dl, fonts.regular, ImGui::GetFontSize(), ImVec2(c.x, c.y - px(1)), active ? col::gold : hovered ? col::text : col::dim, glyph);
}

// Скрещённые мечи — значок силы армии.
void swords(ImDrawList* dl, ImVec2 c, float r) {
    ImU32 blade = IM_COL32(214, 220, 232, 255), hilt = col::gold_dark;
    for (float s : {1.0f, -1.0f}) {
        ImVec2 tip(c.x + s * r * 0.85f, c.y - r * 0.85f), guard(c.x - s * r * 0.45f, c.y + r * 0.45f);
        dl->AddLine(guard, tip, blade, std::max(1.5f, r * 0.22f));
        dl->AddLine(ImVec2(guard.x - r * 0.28f, guard.y - r * 0.28f * s), ImVec2(guard.x + r * 0.28f, guard.y + r * 0.28f * s), hilt,
                    std::max(1.5f, r * 0.2f));
        dl->AddLine(guard, ImVec2(c.x - s * r * 0.85f, c.y + r * 0.85f), hilt, std::max(1.5f, r * 0.2f));
    }
}

}  // namespace

void App::draw_sidebar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, col::sidebar);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(14), px(18)));
    ImGui::BeginChild("sidebar", ImVec2(px(226), 0), ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float w = ImGui::GetContentRegionAvail().x;
    bool compact = ImGui::GetWindowHeight() < px(700);  // низкое окно: герб и пункты меньше, чтобы всё влезло

    // Знамя: винная кожа сверху темнеет книзу, по правому краю — золотой кант.
    {
        ImVec2 a = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
        ImVec2 b(a.x + s.x, a.y + s.y);
        dl->AddRectFilledMultiColor(a, ImVec2(b.x, a.y + s.y * 0.5f), IM_COL32(62, 26, 30, 255), IM_COL32(52, 22, 26, 255),
                                    IM_COL32(34, 18, 22, 255), IM_COL32(38, 20, 24, 255));
        dl->AddRectFilledMultiColor(ImVec2(a.x, a.y + s.y * 0.5f), b, IM_COL32(38, 20, 24, 255), IM_COL32(34, 18, 22, 255),
                                    IM_COL32(18, 12, 16, 255), IM_COL32(20, 13, 17, 255));
        dl->AddRectFilled(ImVec2(b.x - px(4), a.y), ImVec2(b.x - px(3), b.y), alpha(col::gold, 0.55f));
        dl->AddRectFilled(ImVec2(b.x - px(2), a.y), b, IM_COL32(10, 6, 8, 255));
        // Вышитый узор: ряд ромбов вдоль канта.
        for (float y = a.y + px(12); y < b.y; y += px(22)) diamond(dl, ImVec2(b.x - px(9), y), px(2), alpha(col::gold, 0.22f));
    }

    // Герб и название.
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float e = px(compact ? 40 : 62);
        ImVec2 c(p.x + w * 0.5f, p.y + e * 0.5f);
        dl->AddCircleFilled(c, e * 0.62f, IM_COL32(255, 200, 110, 14), 40);
        dl->AddCircleFilled(c, e * 0.48f, IM_COL32(255, 200, 110, 16), 40);
        draw_emblem(dl, c, e);
        ImGui::Dummy(ImVec2(w, e + px(4)));
        p = ImGui::GetCursorScreenPos();
        float ts = (compact ? 20.0f : 23.0f) * ui_scale;
        text_center(dl, fonts.display, ts, ImVec2(p.x + w * 0.5f + 1, p.y + ts * 0.5f + 1), IM_COL32(0, 0, 0, 160), "Security Quest");
        text_center(dl, fonts.display, ts, ImVec2(p.x + w * 0.5f, p.y + ts * 0.5f), col::gold, "Security Quest");
        float ss = 14.0f * ui_scale;
        std::string title = world_.campaign_title;
        while (fonts.regular->CalcTextSizeA(ss, FLT_MAX, 0, title.c_str()).x > w && title.size() > 4) {
            size_t cut = title.size() - (title.ends_with("…") ? 4 : 1);
            while (cut > 0 && (static_cast<unsigned char>(title[cut]) & 0xC0) == 0x80) --cut;
            title = title.substr(0, cut) + "…";
        }
        text_center(dl, fonts.regular, ss, ImVec2(p.x + w * 0.5f, p.y + ts + ss * 0.5f + px(3)), col::dim, title.c_str());
        ImGui::Dummy(ImVec2(w, ts + ss + px(compact ? 4 : 10)));
        p = ImGui::GetCursorScreenPos();
        flourish(dl, ImVec2(p.x + px(6), p.y), ImVec2(p.x + w - px(6), p.y + px(8)), alpha(col::gold, 0.6f));
        ImGui::Dummy(ImVec2(w, px(compact ? 8 : 16)));
    }

    struct Item {
        Page page;
        const char* icon;
        const char* label;
        int badge;
    };
    int missions = 0, sides = 0;
    for (auto& t : st_.tasks)
        if (t.status == Task::Active) (t.side ? sides : missions)++;
    const Item items[] = {
        {Page::Overview, icon::home, "Цитадель", 0},           {Page::Missions, icon::flag, "Миссии", missions},
        {Page::Sides, icon::people, "Побочные квесты", sides}, {Page::Campaign, icon::map, "Кампания", 0},
        {Page::Town, icon::town, "Город", 0},                {Page::History, icon::history, "История", 0},
        {Page::Settings, icon::settings, "Настройки", 0},
    };
    float ih = px(compact ? 38 : 44);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, px(compact ? 2 : 4)));
    for (auto& it : items) {
        bool active = page == it.page && !battle_.active && !cutscene_.active;
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::PushID(it.label);
        if (ImGui::InvisibleButton("nav", ImVec2(w, ih))) {
            page = it.page;
            // Ушли с незаконченного ручного боя — это отступление, иначе можно было бы избегать потерь.
            if (battle_.live && !battle_.live->finished()) {
                battle_.live->retreat();
                quest::record_battle(world_, *game_log_, battle_.frontier, battle_.live->result());
                refresh();
                toast("Отступление", "Бой прерван, армия вернулась в город", col::orange);
            }
            battle_.active = false;
            cutscene_.active = false;
        }
        bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();
        ImVec2 q(p.x + w, p.y + ih);
        if (active) {
            dl->AddRectFilledMultiColor(p, q, alpha(col::gold, 0.26f), alpha(col::gold, 0.0f), alpha(col::gold, 0.0f), alpha(col::gold, 0.26f));
            dl->AddRectFilled(ImVec2(p.x - px(14), p.y + px(4)), ImVec2(p.x - px(10), q.y - px(4)), col::gold);
            dl->AddLine(ImVec2(p.x, p.y), ImVec2(p.x + w * 0.7f, p.y), alpha(col::gold, 0.25f));
            dl->AddLine(ImVec2(p.x, q.y), ImVec2(p.x + w * 0.7f, q.y), alpha(col::gold, 0.25f));
        } else if (hovered) {
            dl->AddRectFilledMultiColor(p, q, alpha(col::gold, 0.09f), alpha(col::gold, 0.0f), alpha(col::gold, 0.0f), alpha(col::gold, 0.09f));
        }
        float mr = px(compact ? 13 : 15);
        ImVec2 mc(p.x + px(4) + mr, p.y + ih * 0.5f);
        medallion(dl, mc, mr, it.icon, active, hovered);
        float fs = 17.5f * ui_scale;
        ImVec2 ts = fonts.display->CalcTextSizeA(fs, FLT_MAX, 0, it.label);
        if (ts.x > w - mr * 2 - px(14)) {  // длинная подпись на узкой панели — чуть мельче
            fs *= (w - mr * 2 - px(14)) / ts.x;
            ts = fonts.display->CalcTextSizeA(fs, FLT_MAX, 0, it.label);
        }
        ImVec2 tp(p.x + px(12) + mr * 2, p.y + (ih - ts.y) * 0.5f);
        dl->AddText(fonts.display, fs, ImVec2(tp.x + 1, tp.y + 1), IM_COL32(0, 0, 0, 150), it.label);
        dl->AddText(fonts.display, fs, tp, active ? col::gold : hovered ? col::text : col::dim, it.label);
        if (it.badge > 0) {
            // Сургучная печать с числом активных задач — на краю медальона.
            std::string b = std::to_string(it.badge);
            float br = px(8.5f);
            ImVec2 bc(mc.x + mr * 0.78f, mc.y - mr * 0.62f);
            dl->AddCircleFilled(ImVec2(bc.x, bc.y + px(1)), br, IM_COL32(0, 0, 0, 110), 20);
            dl->AddCircleFilled(bc, br, IM_COL32(160, 36, 32, 255), 20);
            dl->AddCircle(bc, br - px(2.5f), IM_COL32(210, 80, 64, 255), 20, 1.0f);
            text_center(dl, fonts.semibold, 12.0f * ui_scale, bc, IM_COL32(255, 232, 214, 255), b.c_str());
        }
    }
    ImGui::PopStyleVar();

    // Внизу — казна и ход кампании.
    // На совсем низком окне её нет: казна видна и в панели героя.
    float bottom = ImGui::GetWindowHeight() - px(136);
    if (ImGui::GetCursorPosY() <= bottom) {
        ImGui::SetCursorPosY(bottom);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 sz(w, px(86));
        ImVec2 q(p.x + sz.x, p.y + sz.y);
        dl->AddRectFilledMultiColor(p, q, IM_COL32(40, 28, 26, 240), IM_COL32(40, 28, 26, 240), IM_COL32(20, 14, 16, 240), IM_COL32(20, 14, 16, 240));
        dl->AddRect(p, q, col::border, px(4), 1.0f);
        ornate_frame(dl, p, q, col::border, px(4));
        float cr = px(12);
        ImVec2 cc(p.x + px(22), p.y + px(26));
        coin(dl, ImVec2(cc.x - px(3), cc.y + px(3)), cr * 0.85f);
        coin(dl, cc, cr);
        std::string gold = std::to_string(world_.gold);
        float gs = 24.0f * ui_scale;
        ImVec2 gts = fonts.display->CalcTextSizeA(gs, FLT_MAX, 0, gold.c_str());
        dl->AddText(fonts.display, gs, ImVec2(p.x + px(42), cc.y - gts.y * 0.5f), col::gold, gold.c_str());
        dl->AddText(ImVec2(p.x + px(48) + gts.x, cc.y - ImGui::GetFontSize() * 0.5f + px(2)), col::dim, "золота");
        float frac = world_.frontiers.empty() ? 0 : float(world_.cleared) / float(world_.frontiers.size());
        std::string camp = std::format("Рубежи взяты: {} из {}", world_.cleared, world_.frontiers.size());
        dl->AddText(ImVec2(p.x + px(12), p.y + px(46)), col::dim, camp.c_str());
        ImGui::SetCursorScreenPos(ImVec2(p.x + px(12), p.y + sz.y - px(18)));
        bar(frac, ImVec2(sz.x - px(24), px(8)), IM_COL32(40, 120, 84, 255), col::teal);
        ImGui::SetCursorScreenPos(ImVec2(p.x, q.y));
        ImGui::Dummy(ImVec2(sz.x, 0));
    }
    ImGui::Dummy(ImVec2(0, px(6)));
    std::error_code ec;
    bool portable = fs::equivalent(cfg_.home, exe_dir() / "data", ec);
    ImGui::PushStyleColor(ImGuiCol_Text, col::faint);
    ImGui::Text("%s · версия %s", portable ? "Портативная" : "Данные в профиле", kAppVersion);
    ImGui::PopStyleColor();
    tooltip(from_path(cfg_.home).c_str());

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void App::draw_header() {
    // Панель героя: портрет в золотой оправе, уровень, полоса опыта, казна, сила армии и главная кнопка.
    Level lv = level_of(st_.earned.xp);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float W = ImGui::GetContentRegionAvail().x, H = px(86);
    ImVec2 q(p.x + W, p.y + H);
    dl->AddRectFilled(ImVec2(p.x, p.y + px(3)), ImVec2(q.x, q.y + px(3)), IM_COL32(0, 0, 0, 110), px(6));
    dl->AddRectFilledMultiColor(p, q, IM_COL32(58, 36, 36, 245), IM_COL32(44, 30, 34, 245), IM_COL32(24, 17, 21, 245), IM_COL32(28, 19, 23, 245));
    dl->AddRect(p, q, col::border, px(6), px(1.5f));
    ornate_frame(dl, p, q, col::border, px(6));

    // Портрет.
    float r = px(33);
    ImVec2 c(p.x + px(16) + r, p.y + H * 0.5f);
    dl->AddCircleFilled(c, r + px(4), IM_COL32(14, 8, 10, 255), 40);
    dl->AddCircleFilled(c, r, IM_COL32(34, 38, 70, 255), 40);
    dl->PathArcTo(c, r, kPi * 0.12f, kPi * 0.88f, 24);  // земля — нижний сегмент круга
    dl->PathFillConvex(IM_COL32(40, 54, 44, 255));
    dl->PushClipRect(ImVec2(c.x - r * 0.8f, c.y - r), ImVec2(c.x + r * 0.8f, c.y + r * 0.82f), true);
    quest::Stack hero;
    hero.name = "Хранитель";
    hero.icon = "hero";
    hero.count = 1;
    hero.elite = true;
    draw_figure(dl, ImVec2(c.x - px(2), c.y + r * 0.78f), r * 1.55f, hero, true, 0);
    dl->PopClipRect();
    dl->AddCircle(c, r + px(1), col::gold_dark, 40, px(4));
    dl->AddCircle(c, r + px(2), col::gold, 40, px(1.5f));
    // Кольцо опыта по оправе.
    float frac = float(lv.into) / float(lv.need);
    if (frac > 0.001f) {
        dl->PathArcTo(c, r + px(5.5f), -kPi * 0.5f, -kPi * 0.5f + 2 * kPi * frac, 48);
        dl->PathStroke(col::teal, px(2.5f), ImDrawFlags_None);
    }
    // Щит с уровнем.
    {
        ImVec2 sc(c.x + r * 0.78f, c.y + r * 0.62f);
        float s = px(15);
        poly(dl, sc, s, {{-0.9f, -0.9f}, {0.9f, -0.9f}, {0.85f, 0.15f}, {0, 1.0f}, {-0.85f, 0.15f}}, IM_COL32(120, 28, 30, 255), col::gold, px(1.5f));
        std::string num = std::to_string(lv.level);
        text_center(dl, fonts.display, (num.size() > 1 ? 15.0f : 17.0f) * ui_scale, ImVec2(sc.x, sc.y - px(1)), IM_COL32(255, 236, 200, 255), num.c_str());
    }

    // Правая часть считается заранее, чтобы слева ничего не наехало.
    int week = st_.weeks.count(week_start(today())) ? st_.weeks.at(week_start(today())) : 0;
    int power = quest::army_power(world_.army);
    std::string add_btn = std::format("{}  Миссия", icon::add);
    std::string gold_text = std::to_string(world_.gold), power_text = std::to_string(power), week_text = std::format("+{}", week);
    float vs = 21.0f * ui_scale, ls = 13.0f * ui_scale;
    auto stat_w = [&](const std::string& v, const char* label) {
        return std::max(fonts.display->CalcTextSizeA(vs, FLT_MAX, 0, v.c_str()).x, fonts.regular->CalcTextSizeA(ls, FLT_MAX, 0, label).x) + px(48);
    };
    float wadd = fonts.display->CalcTextSizeA(ImGui::GetFontSize() * 1.06f, FLT_MAX, 0, add_btn.c_str()).x + px(40);
    float wg = stat_w(gold_text, "золото"), wp = stat_w(power_text, "сила армии"), ww = stat_w(week_text, "опыт за неделю");
    float left_x = c.x + r + px(22);
    float right = q.x - px(16);
    float min_left = px(250);
    bool show_week = right - wadd - wg - wp - ww - px(24) - left_x >= min_left;
    bool show_power = right - wadd - wg - wp - px(24) - left_x >= min_left;
    float stats_w = wg + (show_power ? wp : 0) + (show_week ? ww : 0);
    float room = right - wadd - stats_w - px(40) - left_x;  // место под имя и полосу опыта
    float bar_w = std::clamp(room, px(90), px(340));

    // Имя, уровень и полоса опыта; на узком окне имя мельче, чтобы не наехать на казну.
    {
        float ns = 22.0f * ui_scale;
        float nw = fonts.display->CalcTextSizeA(ns, FLT_MAX, 0, "Хранитель Цитадели").x;
        if (nw > room + px(30)) ns *= std::max(0.6f, (room + px(30)) / nw);
        float y = p.y + px(10);
        dl->AddText(fonts.display, ns, ImVec2(left_x + 1, y + 1), IM_COL32(0, 0, 0, 160), "Хранитель Цитадели");
        dl->AddText(fonts.display, ns, ImVec2(left_x, y), col::gold, "Хранитель Цитадели");
        std::string sub = std::format("Уровень {}", lv.level);
        dl->AddText(fonts.regular, 14.0f * ui_scale, ImVec2(left_x, y + ns + px(1)), col::dim, sub.c_str());
        ImGui::SetCursorScreenPos(ImVec2(left_x, p.y + H - px(28)));
        bar(frac, ImVec2(bar_w, px(15)), IM_COL32(30, 110, 80, 255), IM_COL32(110, 226, 166, 255));
        tooltip(std::format("До уровня {}: ещё {} опыта. Уровень героя усиливает армию.", lv.level + 1, lv.need - lv.into).c_str());
        std::string xp = std::format("{} / {} опыта", lv.into, lv.need);
        float xs = 12.5f * ui_scale;
        ImVec2 xc(left_x + bar_w * 0.5f, p.y + H - px(28) + px(7.5f));
        text_center(dl, fonts.semibold, xs, ImVec2(xc.x + 1, xc.y + 1), IM_COL32(0, 0, 0, 220), xp.c_str());
        text_center(dl, fonts.semibold, xs, xc, IM_COL32(255, 250, 236, 255), xp.c_str());
    }

    // Показатели: значок, число книжным шрифтом и подпись.
    float x = right - wadd - stats_w - px(14);
    auto stat = [&](int kind, const std::string& v, const char* label, float wdt, const char* tip) {
        dl->AddLine(ImVec2(x, p.y + px(18)), ImVec2(x, q.y - px(18)), alpha(col::border, 0.7f));
        ImVec2 ic(x + px(20), p.y + H * 0.5f);
        if (kind == 0) coin(dl, ic, px(11));
        else if (kind == 1) swords(dl, ic, px(11));
        else xp_gem(dl, ic, px(11));
        float tx = x + px(38);
        dl->AddText(fonts.display, vs, ImVec2(tx, p.y + H * 0.5f - vs * 0.78f), kind == 0 ? col::gold : kind == 1 ? col::text : col::teal, v.c_str());
        dl->AddText(fonts.regular, ls, ImVec2(tx, p.y + H * 0.5f + vs * 0.28f), col::dim, label);
        ImGui::SetCursorScreenPos(ImVec2(x, p.y + px(10)));
        ImGui::Dummy(ImVec2(wdt, H - px(20)));
        tooltip(tip);
        x += wdt;
    };
    stat(0, gold_text, "золото", wg, "Золото приносят миссии и побочные квесты. На него в городе нанимают войска, куют снаряжение и изучают заклинания.");
    if (show_power) stat(1, power_text, "сила армии", wp, "Сила всех отрядов с учётом снаряжения, магии и уровня героя. Сравнивайте с силой врага на рубеже.");
    if (show_week) stat(2, week_text, "опыт за неделю", ww, "Опыт за задачи, выполненные с понедельника.");

    ImGui::SetCursorScreenPos(ImVec2(right - wadd, p.y + (H - px(40)) * 0.5f));
    if (accent_button(add_btn.c_str(), ImVec2(wadd, px(40)))) mission_form_.open = true;
    tooltip("Запланировать новую задачу-миссию");

    ImGui::SetCursorScreenPos(ImVec2(p.x, q.y + px(10)));
    ImGui::Dummy(ImVec2(W, 0));
}

void App::draw_toasts() {
    if (toasts_.empty()) return;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    double now = clock();
    float y = vp->WorkPos.y + px(110);
    for (size_t i = 0; i < toasts_.size(); ++i) {
        const Toast& t = toasts_[i];
        double age = now - t.born;
        float a = float(std::min({1.0, age * 4.0, (6.0 - age) * 2.0}));
        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - px(20), y), ImGuiCond_Always, ImVec2(1, 0));
        ImGui::SetNextWindowBgAlpha(0.97f * a);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, a);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(px(18), px(12)));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(40, 28, 30, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, alpha(t.color, 0.8f));
        std::string id = std::format("##toast{}", i);
        ImGui::Begin(id.c_str(), nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
        ImGui::PushTextWrapPos(px(340));
        ImGui::PushFont(fonts.display, 19.0f);
        colored_text(t.color, t.title.c_str());
        ImGui::PopFont();
        ImGui::TextWrapped("%s", t.text.c_str());
        ImGui::PopTextWrapPos();
        ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
        ornate_frame(ImGui::GetWindowDrawList(), wp, ImVec2(wp.x + ws.x, wp.y + ws.y), t.color, px(6));
        y += ws.y + px(8);
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }
}

}  // namespace sq::gui
