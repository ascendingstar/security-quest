// Страницы работы: сводка, миссии, побочные квесты, история, настройки.

#include "app.h"

#include "draw.h"
#include "theme.h"
#include "util.h"
#include "version.h"
#include "win.h"

#include "imgui_stdlib.h"

#include <algorithm>
#include <format>

namespace sq::gui {

namespace {

const ImU32 kDifficulty[5] = {col::teal, col::blue, col::gold, col::orange, col::red};

ImU32 difficulty_color(const Task& t) {
    return t.side ? kDifficulty[(t.difficulty - 1) * 2] : kDifficulty[std::clamp(t.difficulty, 1, 5) - 1];
}

// Сложность миссии: ряд кружков, заполнено столько, сколько сложность.
void pips(int n, int of, ImU32 color) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float r = px(5), step = px(14);
    float y = p.y + ImGui::GetTextLineHeight() * 0.5f;
    for (int i = 0; i < of; ++i) {
        ImVec2 c(p.x + r + i * step, y);
        if (i < n) dl->AddCircleFilled(c, r, color);
        else dl->AddCircle(c, r, col::faint, 0, 1.5f);
    }
    ImGui::Dummy(ImVec2(of * step, ImGui::GetTextLineHeight()));
}

void difficulty_badge(const Task& t) {
    if (t.side) pill(side_difficulty_name(t.difficulty), difficulty_color(t));
    else pips(t.difficulty, 5, difficulty_color(t));
}

// Выбор сложности: кнопки 1–5 (или три для побочных) с цветом сложности.
bool difficulty_picker(const char* id, int& value, int count) {
    bool changed = false;
    ImGui::PushID(id);
    for (int i = 1; i <= count; ++i) {
        if (i > 1) ImGui::SameLine(0, px(6));
        bool on = value == i;
        ImU32 c = count == 5 ? kDifficulty[i - 1] : kDifficulty[(i - 1) * 2];
        ImGui::PushStyleColor(ImGuiCol_Button, on ? c : col::panel_hi);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, on ? c : alpha(c, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_Text, on ? col::bg : col::text);
        std::string label = count == 5 ? std::to_string(i) : std::string(side_difficulty_name(i));
        if (ImGui::Button(label.c_str(), ImVec2(count == 5 ? px(44) : px(100), 0))) {
            value = i;
            changed = true;
        }
        ImGui::PopStyleColor(3);
    }
    ImGui::PopID();
    return changed;
}

// ---------------------------------------------------------------------------
// Строка задачи: печать сложности, название, награда, монстры и кнопки.

// Печать задания: щит с римской цифрой сложности у миссии, сургучная печать у побочного квеста.
void quest_seal(ImDrawList* dl, ImVec2 c, float r, const Task& t, ImU32 color) {
    static const char* roman[5] = {"I", "II", "III", "IV", "V"};
    if (t.side) {
        dl->AddCircleFilled(ImVec2(c.x, c.y + px(2)), r, IM_COL32(0, 0, 0, 110), 28);
        for (int k = 0; k < 10; ++k) {  // неровный край сургуча
            float a = float(k) / 10.0f * 2 * kPi;
            dl->AddCircleFilled(ImVec2(c.x + std::cos(a) * r * 0.8f, c.y + std::sin(a) * r * 0.8f), r * 0.28f, mix(color, IM_COL32(0, 0, 0, 255), 0.35f), 12);
        }
        dl->AddCircleFilled(c, r * 0.86f, mix(color, IM_COL32(0, 0, 0, 255), 0.25f), 28);
        dl->AddCircle(c, r * 0.62f, mix(color, IM_COL32(255, 255, 255, 255), 0.25f), 28, px(1.5f));
        text_center(dl, fonts.regular, r * 0.85f, ImVec2(c.x, c.y - px(1)), IM_COL32(255, 240, 220, 235), icon::people);
        return;
    }
    poly(dl, ImVec2(c.x, c.y + px(2)), r, {{-0.82f, -0.95f}, {0.82f, -0.95f}, {0.78f, 0.12f}, {0, 1.0f}, {-0.78f, 0.12f}}, IM_COL32(0, 0, 0, 110));
    poly(dl, c, r, {{-0.82f, -0.95f}, {0.82f, -0.95f}, {0.78f, 0.12f}, {0, 1.0f}, {-0.78f, 0.12f}}, col::gold_dark);
    poly(dl, c, r, {{-0.68f, -0.82f}, {0.68f, -0.82f}, {0.64f, 0.08f}, {0, 0.84f}, {-0.64f, 0.08f}}, mix(color, IM_COL32(20, 10, 10, 255), 0.35f));
    poly(dl, c, r, {{-0.68f, -0.82f}, {0, -0.82f}, {0, 0.84f}, {-0.64f, 0.08f}}, IM_COL32(255, 255, 255, 26));
    float fs = r * (t.difficulty >= 4 ? 0.82f : 0.95f);
    text_center(dl, fonts.display, fs, ImVec2(c.x + 1, c.y - r * 0.08f + 1), IM_COL32(0, 0, 0, 170), roman[std::clamp(t.difficulty, 1, 5) - 1]);
    text_center(dl, fonts.display, fs, ImVec2(c.x, c.y - r * 0.08f), IM_COL32(255, 238, 200, 255), roman[std::clamp(t.difficulty, 1, 5) - 1]);
}

// Монстры, которых победит задача: те же, что в схватке после выполнения (quest::task_monsters).
void quest_monsters(ImDrawList* dl, ImVec2 feet, float h, const Task& t) {
    // Предводитель — один и крупнее, иначе — до двух тварей самого грозного отряда.
    auto foes = quest::task_monsters(t);
    const quest::Stack& m = foes.back();
    bool boss = m.icon == "boss";
    int n = boss ? 1 : std::min(2, int(foes.size()));
    int tier = quest::celebration_tier(t);
    for (int k = n - 1; k >= 0; --k)
        draw_figure(dl, ImVec2(feet.x - float(k) * h * 0.38f, feet.y - float(k) * h * 0.06f), h * (boss ? 1.15f : 0.8f + 0.05f * float(tier)), m, false, 0, k);
}

}  // namespace

void App::task_row(const Task& t) {
    ImGui::PushID(t.id.c_str());
    card_begin("task", ImVec2(0, 0), col::panel);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 dcol = difficulty_color(t);
    std::string done_label = std::format("{}  Выполнено", icon::check);
    float done_w = button_width(done_label.c_str());
    float seal = px(44), monster_w = px(78);
    float buttons_w = done_w + 2 * ImGui::GetFrameHeight() + px(18);
    // Сначала уходят монстры, а на совсем узком окне кнопки встают под название, чтобы оно не сжималось в столбик.
    float avail = ImGui::GetContentRegionAvail().x;
    if (avail < buttons_w + monster_w + seal + px(160)) monster_w = 0;
    bool narrow = avail < buttons_w + seal + px(220);

    ImVec2 sp = ImGui::GetCursorScreenPos();
    float right = sp.x + ImGui::GetContentRegionAvail().x;
    quest_seal(dl, ImVec2(sp.x + seal * 0.5f, sp.y + seal * 0.5f), seal * 0.5f, t, dcol);
    ImGui::Dummy(ImVec2(seal, seal));
    ImGui::SameLine(0, px(14));

    ImGui::BeginGroup();
    float wrap = narrow ? 0.0f : ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - buttons_w - monster_w;
    ImGui::PushTextWrapPos(wrap);
    ImGui::PushFont(fonts.display, 18.0f);
    ImGui::TextWrapped("%s", t.title.c_str());
    ImGui::PopFont();
    ImGui::PopTextWrapPos();
    // Награда значками: кристалл опыта, монеты, шанс трофея; сложность словами.
    stat_inline(false, std::format("+{}", t.rewards.xp).c_str());
    ImGui::SameLine(0, px(12));
    stat_inline(true, std::format("+{}", t.rewards.gold).c_str());
    if (!t.side && artifact_chance(cfg_, t.difficulty) > 0) {
        ImGui::SameLine(0, px(12));
        colored_text(col::violet, std::format("{} трофей {}%", icon::star, artifact_chance(cfg_, t.difficulty)).c_str());
    }
    ImGui::SameLine(0, px(14));
    ImGui::PushStyleColor(ImGuiCol_Text, col::faint);
    ImGui::TextUnformatted(t.side ? std::format("{} квест · {}", side_difficulty_name(t.difficulty), t.id).c_str()
                                  : std::format("сложность {} · {}", t.difficulty, t.id).c_str());
    ImGui::PopStyleColor();
    if (!t.side) tooltip(mission_difficulty_hint(t.difficulty));
    ImGui::EndGroup();

    if (!narrow) {
        if (monster_w > 0) quest_monsters(dl, ImVec2(right - buttons_w - px(18), sp.y + seal + px(2)), px(46), t);
        ImGui::SameLine(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - buttons_w + px(10));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (seal - ImGui::GetFrameHeight()) * 0.5f);
    }
    if (accent_button(done_label.c_str(), ImVec2(done_w, 0))) pending_done_ = t.id;
    tooltip("Задача сделана: герой разгромит монстров и получит награду");
    ImGui::SameLine(0, px(4));
    if (icon_button(icon::edit, "Изменить название, сложность или срок"))
        edit_form_ = EditForm{true, t.id, t.title, t.side, t.difficulty, t.plan == "week" ? 1 : 0};
    ImGui::SameLine(0, px(2));
    if (icon_button(icon::trash, "Убрать без награды")) {
        confirm_drop_ = t.id;
        ImGui::OpenPopup("drop");
    }
    if (ImGui::BeginPopup("drop")) {
        ImGui::Text("Убрать «%s» без награды?", t.title.c_str());
        if (accent_button("Убрать", ImVec2(0, 0), col::red)) {
            pending_drop_ = confirm_drop_;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Отмена")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    card_end(dcol);
    ImGui::PopID();
}

// ---------------------------------------------------------------------------
// Новая миссия: всплывающее окно из шапки и та же форма на странице миссий.

namespace {

bool mission_fields(const Config& cfg, std::string& title, int& difficulty, int& plan, bool focus) {
    if (focus) ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(-1);
    bool enter = ImGui::InputTextWithHint("##title", "Что нужно сделать? Например: ревью модуля авторизации", &title,
                                          ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::Dummy(ImVec2(0, px(2)));
    dim_text("Сложность");
    difficulty_picker("diff", difficulty, 5);
    ImGui::SameLine(0, px(16));
    ImGui::AlignTextToFramePadding();
    dim_text("%s", mission_difficulty_hint(difficulty));

    dim_text("Когда");
    const char* plans[2] = {"Сегодня", "На неделю"};
    for (int i = 0; i < 2; ++i) {
        if (i) ImGui::SameLine(0, px(6));
        bool on = plan == i;
        ImGui::PushStyleColor(ImGuiCol_Button, on ? alpha(col::gold, 0.3f) : col::panel_hi);
        ImGui::PushStyleColor(ImGuiCol_Text, on ? col::gold : col::text);
        if (ImGui::Button(plans[i], ImVec2(px(110), 0))) plan = i;
        ImGui::PopStyleColor(2);
    }
    Rewards r = mission_rewards(cfg, difficulty);
    std::string reward = r.describe();
    if (artifact_chance(cfg, difficulty) > 0) reward += std::format(" · шанс трофея {}%", artifact_chance(cfg, difficulty));
    ImGui::SameLine(0, px(16));
    ImGui::AlignTextToFramePadding();
    colored_text(col::gold, ("Награда: " + reward).c_str());
    return enter;
}

}  // namespace

void App::dialog_mission() {
    const char* id = "Новая миссия";
    if (mission_form_.open) {
        ImGui::OpenPopup(id);
        mission_form_.open = false;
    }
    ImGui::SetNextWindowSize(ImVec2(px(640), 0));
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) return;
    bool enter = mission_fields(cfg_, mission_form_.title, mission_form_.difficulty, mission_form_.plan, ImGui::IsWindowAppearing());
    ImGui::Dummy(ImVec2(0, px(6)));
    if ((accent_button("Добавить", ImVec2(px(150), px(36))) || enter) && !trim(mission_form_.title).empty()) {
        ActionResult r = add_task(*log_, false, mission_form_.title, mission_form_.difficulty, mission_form_.plan ? "week" : "day");
        if (r.ok) {
            refresh();
            toast("Миссия добавлена", mission_form_.title, col::blue);
            mission_form_.title.clear();
            ImGui::CloseCurrentPopup();
        } else {
            toast("Не получилось", r.message, col::red);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Отмена", ImVec2(px(110), px(36))) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}


void App::dialog_edit() {
    EditForm& f = edit_form_;
    const char* id = "Изменить задачу";
    if (f.open) {
        ImGui::OpenPopup(id);
        f.open = false;
    }
    ImGui::SetNextWindowSize(ImVec2(px(640), 0));
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) return;
    bool enter;
    if (!f.side) {
        enter = mission_fields(cfg_, f.title, f.difficulty, f.plan, ImGui::IsWindowAppearing());
    } else {
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-1);
        enter = ImGui::InputTextWithHint("##title", "Что нужно сделать?", &f.title, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Dummy(ImVec2(0, px(2)));
        difficulty_picker("sidediff", f.difficulty, 3);
        ImGui::SameLine(0, px(16));
        ImGui::AlignTextToFramePadding();
        const Task* t = st_.find(f.id);
        colored_text(col::gold, ("Награда: " + side_rewards(cfg_, f.difficulty, t ? t->created : "").describe()).c_str());
        dim_text("%s", side_difficulty_hint(f.difficulty));
    }
    ImGui::Dummy(ImVec2(0, px(6)));
    if ((accent_button("Сохранить", ImVec2(px(150), px(36))) || enter) && !trim(f.title).empty()) {
        ActionResult r = edit_task(*log_, st_, f.id, f.title, f.difficulty, f.side ? "" : f.plan ? "week" : "day");
        if (r.ok) {
            refresh();
            if (r.message.empty()) toast("Задача изменена", trim(f.title), col::blue);
            ImGui::CloseCurrentPopup();
        } else {
            toast("Не получилось", r.message, col::red);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Отмена", ImVec2(px(110), px(36))) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

// ---------------------------------------------------------------------------
// Сводка

void App::page_overview() {
    // Широкое окно — две колонки, узкое — всё друг под другом.
    bool wide = ImGui::GetContentRegionAvail().x >= px(860);
    if (wide) {
        if (!ImGui::BeginTable("overview", 2, ImGuiTableFlags_SizingStretchProp)) return;
        ImGui::TableSetupColumn("left", ImGuiTableColumnFlags_WidthStretch, 0.58f);
        ImGui::TableSetupColumn("right", ImGuiTableColumnFlags_WidthStretch, 0.42f);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
    }

    bool any = false;
    for (const char* plan : {"day", "week"}) {
        std::vector<const Task*> list;
        for (auto& t : st_.tasks)
            if (!t.side && t.status == Task::Active && t.plan == plan) list.push_back(&t);
        if (list.empty()) continue;
        any = true;
        heading(std::string(plan) == "day" ? "Миссии на сегодня" : "Миссии на неделю", 20.0f);
        for (auto* t : list) task_row(*t);
        ImGui::Dummy(ImVec2(0, px(4)));
    }
    std::vector<const Task*> sides;
    for (auto& t : st_.tasks)
        if (t.side && t.status == Task::Active) sides.push_back(&t);
    if (!sides.empty()) {
        any = true;
        heading("Побочные квесты", 20.0f);
        for (auto* t : sides) task_row(*t);
    }
    if (!any) {
        card_begin("empty");
        ImGui::PushFont(fonts.display, 24.0f);
        colored_text(col::gold, std::format("{}  Запланируйте день", icon::flag).c_str());
        ImGui::PopFont();
        ImGui::TextWrapped("Миссии — это ваши задачи. Создайте их на сегодня или на неделю и поставьте сложность от 1 до 5. "
                           "Выполненная миссия приносит опыт и золото, а сложные — ещё и трофеи. На золото в городе "
                           "нанимают войска, куют снаряжение и изучают магию для кампании.");
        ImGui::Dummy(ImVec2(0, px(4)));
        if (accent_button(std::format("{}  Новая миссия", icon::add).c_str(), ImVec2(px(200), px(38)))) mission_form_.open = true;
        ImGui::SameLine();
        if (ImGui::Button(std::format("{}  Побочный квест", icon::people).c_str(), ImVec2(px(200), px(38)))) page = Page::Sides;
        card_end();
    }

    if (wide) ImGui::TableNextColumn();
    else ImGui::Dummy(ImVec2(0, px(6)));
    if (const quest::Frontier* f = world_.next()) {
        card_begin("next");
        dim_text("Глава %d · %s", f->chapter + 1, world_.chapters[size_t(f->chapter)].name.c_str());
        ImGui::PushFont(fonts.display, 23.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, col::gold);
        ImGui::TextWrapped("Рубеж %s: %s", f->label.c_str(), f->name.c_str());
        ImGui::PopStyleColor();
        ImGui::PopFont();
        {
            ImDrawList* sdl = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImVec2 sz(ImGui::GetContentRegionAvail().x, px(96));
            draw_scene(sdl, p, sz, world_.chapters[size_t(f->chapter)].color);
            int shown = std::min(3, int(f->enemy.stacks.size()));
            for (int k = 0; k < shown; ++k) {
                const quest::Stack& s = f->enemy.stacks[size_t(k)];
                float x = p.x + sz.x * (shown == 1 ? 0.5f : 0.28f + 0.44f * float(k) / float(shown - 1));
                draw_figure(sdl, ImVec2(x, p.y + sz.y - px(8)), s.icon == "boss" ? px(72) : px(46), s, false, 0);
            }
            sdl->AddRect(p, ImVec2(p.x + sz.x, p.y + sz.y), col::border, px(8), px(1.5f));
            ImGui::Dummy(sz);
        }
        wrapped_dim(f->intro);
        ImGui::Dummy(ImVec2(0, px(4)));
        for (auto& r : f->requirements) {
            colored_text(r.met ? col::teal : r.advisory ? col::blue : col::red, r.met ? icon::check : r.advisory ? icon::info : icon::close);
            ImGui::SameLine();
            ImGui::TextUnformatted(r.text.c_str());
        }
        int ours = quest::army_power(world_.army), theirs = quest::power_of(f->enemy.stacks);
        colored_text(ours >= theirs ? col::teal : col::orange, ours >= theirs ? icon::check : icon::warning);
        ImGui::SameLine();
        ImGui::Text("Сила армии: %d против %d", ours, theirs);
        bar(ours + theirs ? float(ours) / float(ours + theirs) : 0.5f, ImVec2(0, px(8)), col::blue, IM_COL32(120, 170, 255, 255), alpha(col::red, 0.5f));
        ImGui::Dummy(ImVec2(0, px(4)));
        if (accent_button(std::format("{}  К кампании", icon::map).c_str(), ImVec2(-1, 0))) {
            page = Page::Campaign;
            selected_frontier_ = f->id;
        }
        if (ImGui::Button(std::format("{}  В город: {} золота", icon::town, world_.gold).c_str(), ImVec2(-1, 0))) page = Page::Town;
        card_end();
    } else {
        card_begin("done");
        heading("Кампания пройдена");
        wrapped_dim("Великий Щит снова цел. Миссии по-прежнему приносят опыт и золото.");
        card_end();
    }

    std::vector<const Task*> done;
    for (auto& t : st_.tasks)
        if (t.status == Task::Done) done.push_back(&t);
    std::ranges::sort(done, [](auto* a, auto* b) { return a->finished > b->finished; });
    card_begin("recent");
    subheading("Недавно выполнено");
    if (done.empty()) wrapped_dim("Здесь появятся выполненные миссии и квесты.");
    for (size_t i = 0; i < done.size() && i < 6; ++i) {
        colored_text(col::teal, icon::check);
        ImGui::SameLine();
        ImGui::TextWrapped("%s", done[i]->title.c_str());
        dim_text("    %s", done[i]->rewards.describe().c_str());
    }
    if (!done.empty() && ImGui::SmallButton("Вся история")) page = Page::History;
    card_end();
    if (wide) ImGui::EndTable();
}

// ---------------------------------------------------------------------------
// Миссии

void App::page_missions() {
    heading("Миссии");
    wrapped_dim("Ваши задачи. Сложность честно оценивайте сами: от неё зависят опыт, золото и шанс трофея.");
    ImGui::Dummy(ImVec2(0, px(4)));

    card_begin("newmission");
    subheading("Новая миссия");
    bool enter = mission_fields(cfg_, mission_form_.title, mission_form_.difficulty, mission_form_.plan, false);
    if ((accent_button("Добавить", ImVec2(px(150), 0)) || enter) && !trim(mission_form_.title).empty()) {
        ActionResult r = add_task(*log_, false, mission_form_.title, mission_form_.difficulty, mission_form_.plan ? "week" : "day");
        if (r.ok) {
            refresh();
            mission_form_.title.clear();
        } else {
            toast("Не получилось", r.message, col::red);
        }
    }
    card_end();
    ImGui::Dummy(ImVec2(0, px(6)));

    bool any = false;
    for (const char* plan : {"day", "week"}) {
        std::vector<const Task*> list;
        for (auto& t : st_.tasks)
            if (!t.side && t.status == Task::Active && t.plan == plan) list.push_back(&t);
        if (list.empty()) continue;
        any = true;
        heading(std::string(plan) == "day" ? "На сегодня" : "На неделю", 20.0f);
        for (auto* t : list) task_row(*t);
        ImGui::Dummy(ImVec2(0, px(4)));
    }
    if (!any) wrapped_dim("Активных миссий нет. Выполненные лежат в истории.");
}

// ---------------------------------------------------------------------------
// Побочные квесты

void App::page_sides() {
    heading("Побочные квесты");
    wrapped_dim("Всё полезное вокруг основной работы: созвоны, помощь коллегам, отчёты. Можно запланировать заранее или "
                "записать уже сделанное.");
    ImGui::Dummy(ImVec2(0, px(4)));

    card_begin("newside");
    subheading("Новый побочный квест");
    ImGui::SetNextItemWidth(-1);
    bool enter = ImGui::InputTextWithHint("##sidetitle", "Например: созвон с коллегой", &side_form_.title, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::Dummy(ImVec2(0, px(2)));
    difficulty_picker("sidediff", side_form_.difficulty, 3);
    ImGui::SameLine(0, px(16));
    ImGui::AlignTextToFramePadding();
    colored_text(col::gold, ("Награда: " + side_rewards(cfg_, side_form_.difficulty).describe()).c_str());
    dim_text("%s", side_difficulty_hint(side_form_.difficulty));
    ImGui::Dummy(ImVec2(0, px(2)));
    bool has_title = !trim(side_form_.title).empty();
    ImGui::BeginDisabled(!has_title);
    if (accent_button(std::format("{}  Уже сделал", icon::check).c_str(), ImVec2(px(170), 0)) || (enter && has_title)) {
        int level_before = level_of(st_.earned.xp).level;
        ActionResult r = add_task(*log_, true, side_form_.title, side_form_.difficulty, "");
        std::string id = r.message;
        if (r.ok) r = finish_task(*log_, replay(cfg_, *log_), id);
        if (r.ok) {
            refresh();
            if (const Task* t = st_.find(id)) celebrate(*t, level_before);
            side_form_.title.clear();
        } else {
            toast("Не получилось", r.message, col::red);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Запланировать", ImVec2(px(150), 0))) {
        ActionResult r = add_task(*log_, true, side_form_.title, side_form_.difficulty, "");
        if (r.ok) {
            refresh();
            side_form_.title.clear();
        } else {
            toast("Не получилось", r.message, col::red);
        }
    }
    ImGui::EndDisabled();
    card_end();
    ImGui::Dummy(ImVec2(0, px(6)));

    std::vector<const Task*> list;
    for (auto& t : st_.tasks)
        if (t.side && t.status == Task::Active) list.push_back(&t);
    if (list.empty()) {
        wrapped_dim("Запланированных побочных квестов нет.");
        return;
    }
    heading("Запланировано", 20.0f);
    for (auto* t : list) task_row(*t);
}

// ---------------------------------------------------------------------------
// История

void App::page_history() {
    heading("История");
    int missions = 0, sides = 0;
    for (auto& t : st_.tasks)
        if (t.status == Task::Done) (t.side ? sides : missions)++;
    wrapped_dim(std::format("Выполнено миссий: {}, побочных квестов: {}. Всего заработано: {}.", missions, sides,
                            st_.earned.describe().empty() ? "пока ничего" : st_.earned.describe()));
    ImGui::SetNextItemWidth(px(320));
    ImGui::InputTextWithHint("##filter", "Поиск", &history_filter_);

    std::vector<const Task*> done;
    for (auto& t : st_.tasks)
        if (t.status == Task::Done && (history_filter_.empty() || t.title.find(history_filter_) != std::string::npos)) done.push_back(&t);
    std::ranges::sort(done, [](auto* a, auto* b) { return a->finished > b->finished; });

    if (ImGui::BeginTable("history", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
                          ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Когда", ImGuiTableColumnFlags_WidthFixed, px(110));
        ImGui::TableSetupColumn("Сложность", ImGuiTableColumnFlags_WidthFixed, px(110));
        ImGui::TableSetupColumn("Задача", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("Награда", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableHeadersRow();
        ImGuiListClipper clipper;
        clipper.Begin(int(done.size()));
        while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const Task& t = *done[size_t(i)];
                ImGui::PushID(t.id.c_str());
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                dim_text("%s", t.finished.substr(0, 10).c_str());
                ImGui::TableNextColumn();
                difficulty_badge(t);
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", t.title.c_str());
                ImGui::TableNextColumn();
                colored_text(t.rewards.artifact.empty() ? col::dim : col::gold, t.rewards.describe().c_str());
                ImGui::PopID();
            }
        ImGui::EndTable();
    }
}

// ---------------------------------------------------------------------------
// Настройки

void App::page_settings() {
    heading("Настройки");
    ImGui::Dummy(ImVec2(0, px(4)));

    card_begin("rewards");
    subheading("Награды");
    wrapped_dim("Таблица наград лежит в config.ini, раздел [rewards]. Награды пересчитываются из журнала, поэтому изменения "
                "применяются и к уже выполненным задачам.");
    if (ImGui::BeginTable("rt", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Сложность", ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableSetupColumn("Награда", ImGuiTableColumnFlags_WidthStretch, 1.6f);
        ImGui::TableSetupColumn("Когда ставить", ImGuiTableColumnFlags_WidthStretch, 2.4f);
        ImGui::TableHeadersRow();
        for (int d = 1; d <= 5; ++d) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Миссия %d", d);
            ImGui::TableNextColumn();
            std::string r = mission_rewards(cfg_, d).describe();
            if (artifact_chance(cfg_, d)) r += std::format(", трофей {}%", artifact_chance(cfg_, d));
            ImGui::TextUnformatted(r.c_str());
            ImGui::TableNextColumn();
            dim_text("%s", mission_difficulty_hint(d));
        }
        for (int d = 1; d <= 3; ++d) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Побочный, %s", side_difficulty_name(d));
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(side_rewards(cfg_, d).describe().c_str());
            ImGui::TableNextColumn();
        }
        ImGui::EndTable();
    }
    if (ImGui::Button(std::format("{}  Открыть config.ini", icon::edit).c_str())) shell_open(from_path(cfg_.config_file()));
    ImGui::SameLine();
    if (ImGui::Button(std::format("{}  Перечитать", icon::sync).c_str())) {
        load();
        toast("Готово", "Настройки и журналы перечитаны", col::teal);
    }
    card_end();

    card_begin("window");
    subheading("Окно");
    bool to_tray = close_to_tray();
    if (ImGui::Checkbox("Закрытие окна сворачивает в трей", &to_tray)) {
        set_ini_value(cfg_.config_file(), "gui", "close_to_tray", to_tray ? "1" : "0");
        reload_config(cfg_);
    }
    wrapped_dim("Выйти из программы — через меню значка в трее.");
    card_end();

    card_begin("data");
    subheading("Данные");
    ImGui::TextWrapped("%s", from_path(cfg_.home.lexically_normal()).c_str());
    wrapped_dim("events.log — задачи и их выполнение, game.log — действия в кампании, config.ini — награды. "
                "Чтобы перенести прогресс, скопируйте папку целиком.");
    if (ImGui::Button(std::format("{}  Открыть папку", icon::folder).c_str())) shell_open(from_path(cfg_.home));
    card_end();

    card_begin("updates");
    subheading("Обновления и резервные копии");
    ImGui::Text("Версия %s · формат данных %d", kAppVersion, kDataFormat);
    wrapped_dim("Чтобы обновить программу, закройте её через значок в трее, замените Security Quest.exe и sq.exe новыми и "
                "запустите снова. Папка data не меняется: прогресс сохраняется. Раз в день программа сама делает "
                "резервную копию журналов в data\\backups.");
    auto backups = list_backups(cfg_);
    if (backups.empty()) dim_text("Резервных копий пока нет.");
    else dim_text("Копий: %zu, последняя: %s", backups.size(), from_path(backups.front().filename()).c_str());
    if (ImGui::Button(std::format("{}  Сделать копию сейчас", icon::copy).c_str())) {
        fs::path folder;
        if (make_backup(cfg_, "вручную", &folder)) toast("Копия сохранена", from_path(folder.filename()), col::teal);
        else toast("Не получилось", "Нечего копировать или нет доступа к папке", col::red);
    }
    ImGui::SameLine();
    if (ImGui::Button(std::format("{}  Папка копий", icon::folder).c_str())) {
        std::error_code ec;
        fs::create_directories(cfg_.home / "backups", ec);
        shell_open(from_path(cfg_.home / "backups"));
    }
    card_end();

    card_begin("reset");
    subheading("Начать кампании заново");
    wrapped_dim("Армия, кузня, магия, бои и дары сбросятся во всех кампаниях. Задачи, опыт и золото за них останутся. "
                "Журнал игры перед сбросом сохраняется в резервную копию.");
    if (ImGui::Button(std::format("{}  Сбросить игру…", icon::sync).c_str())) ImGui::OpenPopup("reset_game");
    if (ImGui::BeginPopup("reset_game")) {
        ImGui::TextUnformatted("Начать все кампании заново?");
        if (accent_button("Да, сбросить", ImVec2(0, 0), col::red)) {
            std::string err = quest::reset_game(cfg_, *game_log_);
            if (err.empty()) toast("Игра сброшена", "Кампании начинаются заново. Копия журнала — в data\\backups.", col::teal);
            else toast("Не получилось", err, col::red);
            refresh();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Отмена")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    card_end();
}

}  // namespace sq::gui
