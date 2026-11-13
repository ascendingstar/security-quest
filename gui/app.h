#pragma once

// Состояние окна: задачи, кампания, всплывающие уведомления, страницы и диалоги.
// Хост (main_gui.cpp) отвечает за окно, DirectX и трей.

#include "config.h"
#include "events.h"
#include "game.h"
#include "quest.h"

#include "draw.h"
#include "imgui.h"

#include <windows.h>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace sq::gui {

enum class Page { Overview, Missions, Sides, Campaign, Town, History, Settings };

struct Toast {
    std::string title, text;
    ImU32 color;
    double born;
};

class App {
public:
    explicit App(HWND hwnd);

    void frame();
    void tick();  // таймеры и изменения журнала из консоли; вызывается каждый кадр
    bool busy() const;
    int frame_cap_ms() const;

    Page page = Page::Overview;
    bool window_visible = true;
    std::function<void(const std::string&, const std::string&)> notify;  // всплывашка в трее

    std::string tray_tip() const;
    bool close_to_tray() const;
    void open_dialog(const std::string& name);  // для снимков: mission, select:<рубеж>, battle:<рубеж>@<ход>

private:
    double clock() const;

    // данные
    void load();
    void refresh();
    const quest::CampaignDef& campaign() const;  // выбранная, или встроенная, если файла выбранной больше нет
    void after_work(size_t first_event, int level_before);
    void toast(std::string title, std::string text, ImU32 color);
    void complete(std::string id);  // копия: refresh() пересоздаёт задачи, ссылка на t.id стала бы висячей
    // «Выполнено» и «Убрать» из списков задач срабатывают в начале следующего кадра: список держит
    // указатели на задачи, а выполнение их пересчитывает.
    std::string pending_done_, pending_drop_;

    // каркас
    void draw_sidebar();
    void draw_header();
    void draw_toasts();

    // страницы (pages.cpp)
    void page_overview();
    void page_missions();
    void page_sides();
    void page_history();
    void page_settings();
    void task_row(const Task& t);
    void dialog_mission();

    // кампания и город (game_view.cpp)
    void page_campaign();
    void campaign_map();
    void campaign_panel();
    void campaign_chronicle();
    void page_town();
    void battle_view();
    void watch(quest::BattleResult result, std::string title, std::string reward, std::string verdict);
    void start_manual(const std::string& frontier_id);
    void start_training();  // Ристалище: тренировочный бой всеми родами войск
    void battle_controls(bool waiting, const std::vector<ImVec2>& centers, float hex_r, float sy, ImVec2 origin, ImVec2 size, float fig);
    void game_act(const std::function<std::string()>& action, const std::string& done);

    HWND hwnd_;
    Config cfg_;
    std::unique_ptr<EventLog> log_, game_log_;
    State st_;
    quest::CampaignLibrary library_;  // встроенная кампания и файлы из папок campaigns
    std::string data_warning_;        // данные созданы более новой версией программы
    quest::World world_;
    fs::file_time_type log_mtime_{}, game_mtime_{};
    double next_file_check_ = 0;
    std::chrono::steady_clock::time_point started_ = std::chrono::steady_clock::now();
    std::vector<Toast> toasts_;

    // формы
    struct MissionForm {
        bool open = false;
        std::string title;
        int difficulty = 2;
        int plan = 0;  // 0 сегодня, 1 неделя
    } mission_form_;
    struct SideForm {
        std::string title;
        int difficulty = 1;
    } side_form_;
    // Правка задачи: всплывающее окно из карточки.
    struct EditForm {
        bool open = false;
        std::string id, title;
        bool side = false;
        int difficulty = 1;
        int plan = 0;
    } edit_form_;
    void dialog_edit();
    std::string confirm_drop_;
    std::string history_filter_;

    // кампания
    int campaign_tab_ = 0;  // 0 карта, 1 летопись
    ImVec2 map_pan_{0, 0};
    float map_zoom_ = 0;
    std::string selected_frontier_;
    std::map<int, int> hire_counts_;
    bool chronicle_scroll_ = true;
    // Частица эффекта боя: искры, пыль, кольца, павшие бойцы, монеты, всплывающие числа.
    struct Particle {
        enum Kind { Spark, Dust, Ring, Corpse, Coin, Text, Shard };
        Particle(Kind k, ImVec2 at, ImVec2 velocity) : kind(k), p(at), v(velocity) {}
        Kind kind;
        ImVec2 p, v;
        float age = 0, life = 1, size = 1, rot = 0, spin = 0;
        ImU32 color = 0;
        int side = 0, stack = -1;  // для павших: чей боец
        std::string text;
    };
    struct BattleView {
        bool active = false;
        quest::BattleResult result;
        std::string title, reward, verdict;
        float progress = 0;  // в «ходах»; отрицательное — вступление
        float speed = 1;
        // Эффекты: живут только на экране, на итог боя не влияют.
        std::vector<Particle> fx;
        int impacts = -1;            // последний ход, чей удар уже отрисован эффектами
        std::vector<int> shown[2];   // сколько фигурок было в отрядах на прошлом кадре
        int round = 0;
        double round_banner = -10;   // когда начался текущий раунд — для надписи
        float shake = 0;
        bool finale = false;
        // Ручной бой: ходы делает игрок, итог записывается, когда бой кончится.
        std::shared_ptr<quest::Battle> live;
        std::string frontier;
        bool recorded = false, autoplay = false;
        int pending_spell = -1;  // выбранное заклинание ждёт цель
        Loot loot;               // награда для победного экрана; пусто — только текст reward
        double card_shown = -1e9;  // когда появилась итоговая плашка (-1e9 — ещё нет) — для счёта наград
    } battle_;

    // Катсцена выполненной задачи (cutscene.cpp): армия Хранителя громит тварей из разлома, затем победная
    // плашка с наградой. Размах — quest::celebration_tier: от трёх бесёнков до Исполина Бездны.
    struct Cutscene {
        bool active = false;
        int tier = 1;
        std::string title, verdict;
        Loot loot;
        std::vector<quest::Stack> ours, foes;
        double born = 0;
        double card_shown = -1e9;  // когда появилась плашка; -1e9 — ещё идёт действие
        float frozen = -1;         // для снимков: сцена стоит на этой секунде
    } cutscene_;
    void celebrate(const Task& t, int level_before);  // катсцена и победная плашка, размах — по сложности
    void play_cutscene(const Task& t, const Loot& loot);
    float freeze_cutscene_ = -1;  // для проверки: катсцена встанет на этой секунде (freeze@)
    void cutscene_view();

    // Страница летописи после взятого рубежа (story.cpp): сюжетная сцена и что дальше.
    struct StoryPage {
        bool active = false;
        std::string frontier;
        double born = 0;
        float frozen = -1;  // для снимков: страница стоит на этой секунде
    } story_;
    void open_story(const std::string& frontier);
    void story_view();
};

}  // namespace sq::gui
