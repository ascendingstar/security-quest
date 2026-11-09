#include "config.h"
#include "events.h"
#include "game.h"
#include "platform.h"
#include "quest.h"
#include "util.h"
#include "version.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <print>

using namespace sq;

namespace {

constexpr const char* kHelp = R"(Security Quest — миссии, побочные квесты и кампания «Падение Великого Щита».

  sq                                 сводка: уровень, золото, задачи, следующий рубеж
  sq mission "что сделать" <1-5> [--week]
                                     новая миссия (по умолчанию — на сегодня)
  sq side "что сделать" <лёгкий|средний|сложный>
                                     новый побочный квест
  sq log "что сделал" <лёгкий|средний|сложный>
                                     побочный квест, который уже выполнен
  sq edit <id> "название" [сложность] [--day|--week]
                                     изменить задачу; название «-» — оставить прежнее
  sq done <id>                       выполнить миссию или квест (M3, S7)
  sq drop <id>                       убрать задачу без награды
  sq history [N]                     выполненные задачи
  sq game                            кампания: казна, армия, рубежи
  sq game hire <отряд> <n>           нанять: guard archer rider knight berserk mage
                                     crossbow griffin
  sq game upgrade <отряд> <n>        обучить до элиты
  sq game forge <armor|weapon|walls> выковать следующий уровень
  sq game learn <fire|bless|light|storm|seal>
                                     изучить заклинание
  sq game fight <рубеж>              сражение за рубеж (1.1, 1.2, …)
  sq game arena [N]                  автобой в Ристалище N раз: проверка баланса, ничего не пишет
  sq game gift <золото> [причина]    дар Цитадели: ручная поправка казны
  sq game reset --yes                начать кампании заново (журнал игры уходит в резервную копию)
  sq game campaigns                  список кампаний и ошибки в их файлах
  sq game campaign <id>              выбрать кампанию

Данные: папка data рядом с программой, если она есть (портативный режим),
иначе %LOCALAPPDATA%\SecurityQuest. Переопределяется переменной SQ_HOME.)";

struct Args {
    std::vector<std::string> pos;
    std::map<std::string, std::string> opt;
};

Args parse_args(const std::vector<std::string>& argv, size_t from) {
    Args a;
    for (size_t i = from; i < argv.size(); ++i) {
        if (starts_with(argv[i], "--") && argv[i].size() > 2) a.opt[argv[i].substr(2)] = "1";
        else a.pos.push_back(argv[i]);
    }
    return a;
}

int fail(const std::string& message) {
    std::println(stderr, "{}", message);
    return 1;
}

int side_difficulty(const std::string& s) {
    if (s == "лёгкий" || s == "легкий" || s == "low" || s == "1") return 1;
    if (s == "средний" || s == "medium" || s == "2") return 2;
    if (s == "сложный" || s == "hard" || s == "3") return 3;
    return 0;
}

std::string pips(int n, int of) {
    std::string r;
    for (int i = 0; i < of; ++i) r += i < n ? "●" : "○";
    return r;
}

void print_task(const Task& t) {
    if (t.side) std::println("  {:<4} {:<9} {}  ({})", t.id, side_difficulty_name(t.difficulty), t.title, t.rewards.describe());
    else std::println("  {:<4} {}  {}  ({})", t.id, pips(t.difficulty, 5), t.title, t.rewards.describe());
}

// Награды за только что выполненную задачу и новый уровень.
void report_done(const Config& cfg, const EventLog& log, const std::string& id, int level_before) {
    State st = replay(cfg, log);
    if (const Task* t = st.find(id)) std::println("{} выполнено: {}", id, t->rewards.describe());
    int after = level_of(st.earned.xp).level;
    if (after > level_before) std::println("★ Новый уровень героя: {}!", after);
}

// Выбранная кампания, или встроенная, если файла выбранной больше нет.
const quest::CampaignDef& active_campaign(const Config& cfg, const quest::CampaignLibrary& lib) {
    if (const quest::CampaignDef* c = lib.find(quest::active_campaign_id(cfg))) return *c;
    return lib.campaigns.front();
}

int cmd_status(const Config& cfg, const EventLog& log, const EventLog& game) {
    State st = replay(cfg, log);
    quest::CampaignLibrary lib = quest::load_campaigns(cfg);
    quest::World w = quest::build_world(cfg, st, game, active_campaign(cfg, lib));
    Level lv = level_of(st.earned.xp);
    std::println("Хранитель · уровень {} · {}/{} опыта до следующего · казна {} золота", lv.level, lv.into, lv.need, w.gold);

    for (const char* plan : {"day", "week"}) {
        bool header = false;
        for (auto& t : st.tasks) {
            if (t.side || t.status != Task::Active || t.plan != plan) continue;
            if (!header) std::println("\nМиссии {}", std::string(plan) == "day" ? "на сегодня" : "на неделю");
            header = true;
            print_task(t);
        }
    }
    bool header = false;
    for (auto& t : st.tasks) {
        if (!t.side || t.status != Task::Active) continue;
        if (!header) std::println("\nПобочные квесты");
        header = true;
        print_task(t);
    }
    if (std::ranges::none_of(st.tasks, [](auto& t) { return t.status == Task::Active; }))
        std::println("\nАктивных задач нет. Новая миссия: sq mission \"что сделать\" <1-5>");

    if (const quest::Frontier* f = w.next()) {
        std::println("\nКампания «{}». Следующий рубеж {} «{}»", w.campaign_title, f->label, f->name);
        for (auto& r : f->requirements) std::println("  {} {}", r.met ? "✓" : r.advisory ? "≈" : "✗", r.text);
        std::println("  Сила: ваша армия {} против {}", quest::army_power(w.army), quest::power_of(f->enemy.stacks));
    } else {
        std::println("\nКампания пройдена. Великий Щит снова цел.");
    }
    return 0;
}

int cmd_history(const Config& cfg, const EventLog& log, const Args& a) {
    State st = replay(cfg, log);
    size_t n = a.pos.empty() ? 20 : size_t(std::max(1, std::atoi(a.pos[0].c_str())));
    std::vector<const Task*> done;
    for (auto& t : st.tasks)
        if (t.status == Task::Done) done.push_back(&t);
    std::ranges::sort(done, [](auto* x, auto* y) { return x->finished > y->finished; });
    if (done.empty()) std::println("История пуста.");
    for (size_t i = 0; i < done.size() && i < n; ++i) {
        std::print("{}  ", done[i]->finished.substr(0, 10));
        print_task(*done[i]);
    }
    return 0;
}

int cmd_game(Config& cfg, const EventLog& log, EventLog& game, const Args& a) {
    using namespace quest;
    State st = replay(cfg, log);
    CampaignLibrary lib = load_campaigns(cfg);
    World w = build_world(cfg, st, game, active_campaign(cfg, lib));
    std::string sub = a.pos.empty() ? "" : a.pos[0];
    auto arg = [&](size_t i) { return i < a.pos.size() ? a.pos[i] : std::string(); };
    auto done = [&](const std::string& err) {
        if (!err.empty()) return fail(err);
        std::println("Готово.");
        return 0;
    };

    if (sub == "campaigns") {
        for (auto& c : lib.campaigns)
            std::println("  {} {:<16} {}  ({})", c.id == w.campaign_id ? "▶" : " ", c.id, c.title, c.source);
        for (auto& f : lib.folders) std::println("Папка: {}", from_path(f));
        for (auto& e : lib.errors) std::println(stderr, "Ошибка: {}", e);
        return lib.errors.empty() ? 0 : 1;
    }
    if (sub == "campaign") {
        if (!lib.find(arg(1))) return fail("Кампании «" + arg(1) + "» нет. Список: sq game campaigns");
        select_campaign(cfg, arg(1));
        std::println("Выбрана кампания «{}».", lib.find(arg(1))->title);
        return 0;
    }
    if (sub == "reset") {
        if (arg(1) != "--yes" && !a.opt.count("yes")) return fail("Сброс армии, покупок и боёв всех кампаний. Задачи и их награды останутся. Подтвердите: sq game reset --yes");
        return done(reset_game(cfg, game));
    }
    if (sub == "gift") {
        std::string reason;
        for (size_t i = 2; i < a.pos.size(); ++i) reason += (reason.empty() ? "" : " ") + a.pos[i];
        return done(gift(game, std::atoi(arg(1).c_str()), reason));
    }
    if (sub == "hire") return done(hire(w, game, unit_index(arg(1)), std::atoi(arg(2).c_str())));
    if (sub == "upgrade") return done(upgrade(w, game, unit_index(arg(1)), std::atoi(arg(2).c_str())));
    if (sub == "forge") return done(forge(w, game, forge_index(arg(1))));
    if (sub == "learn") return done(learn(w, game, spell_index(arg(1))));
    if (sub == "fight") {
        const Frontier* f = w.find(arg(1));
        if (!f) return fail("Нет такого рубежа. Список: sq game");
        if (auto b = battle_blocker(w, *f); !b.empty()) return fail(b);
        BattleResult r = fight(w, game, f->id);
        std::println("{} «{}»: {} за {} раундов", f->label, f->name, r.won ? "победа" : "поражение", r.rounds);
        for (size_t i = 0; i < r.ours.size(); ++i)
            if (r.our_losses[i]) std::println("  потери: {} −{}", r.ours[i].name, r.our_losses[i]);
        if (r.won) std::println("  {}  Добыча: {}", f->victory, f->loot.describe());
        return 0;
    }
    if (sub == "arena") {
        // Автобой в Ристалище: ничего не записывает, удобно проверять баланс новых войск.
        int n = std::max(1, std::atoi(arg(1).c_str()));
        int wins = 0, rounds = 0;
        for (int i = 0; i < n; ++i) {
            auto b = training_battle(w, uint32_t(i + 1));
            while (!b->finished()) b->auto_turn();
            wins += b->result().won;
            rounds += b->result().rounds;
        }
        std::println("Ристалище: побед {} из {}, в среднем {} раундов", wins, n, rounds / n);
        return 0;
    }
    if (!sub.empty()) return fail("Неизвестная команда. Справка: sq help");

    std::println("Кампания «{}» ({})", w.campaign_title, w.campaign_id);
    std::println("Казна {} золота · уровень героя {} · атака +{} · защита +{} · магия {}", w.gold, w.level, w.army.atk_bonus,
                 w.army.def_bonus, w.army.power);
    std::println("Броня {} · оружие {} · стены {}", w.army.armor, w.army.weapon, w.army.walls);
    std::string known;
    for (size_t s = 0; s < spells().size(); ++s)
        if (w.army.spell_levels[s]) known += std::format("{}{} {}", known.empty() ? "" : ", ", spells()[s].name, w.army.spell_levels[s]);
    std::println("Заклинания: {}", known.empty() ? "нет" : known);
    std::println("Армия (сила {}):", army_power(w.army));
    for (auto& s : w.army.stacks) std::println("  {} × {}", s.name, s.count);
    std::println("Рубежи:");
    for (auto& f : w.frontiers) {
        const char* status = f.status == Frontier::Cleared ? "взят" : f.status == Frontier::Open ? "открыт" : "закрыт";
        std::println("  {:<4} {:<28} {:<7} сила врага {}", f.label, f.name, status, power_of(f.enemy.stacks));
    }
    return 0;
}

}  // namespace

int main() {
    console_init();
    auto argv = utf8_args();
    std::string cmd = argv.size() > 1 ? argv[1] : "";
    Args args = parse_args(argv, 2);

    if (cmd == "--version" || cmd == "version") {
        std::println("Security Quest {} · формат данных {}", kAppVersion, kDataFormat);
        return 0;
    }
    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        std::println("{}", kHelp);
        return 0;
    }

    Config cfg;
    cfg.home = data_home();
    auto init = init_config(cfg);
    if (!init.ok) return fail(init.message);
    if (!init.message.empty()) std::println(stderr, "Внимание: {}", init.message);
    EventLog log(cfg.events_file()), game(cfg.game_file());
    if (log.load() < 0 || game.load() < 0) return fail("Не удалось прочитать журнал в " + from_path(cfg.home));

    if (cmd.empty() || cmd == "status") return cmd_status(cfg, log, game);
    if (cmd == "history") return cmd_history(cfg, log, args);
    if (cmd == "game") return cmd_game(cfg, log, game, args);

    State st = replay(cfg, log);
    int level_before = level_of(st.earned.xp).level;
    auto arg = [&](size_t i) { return i < args.pos.size() ? args.pos[i] : std::string(); };

    if (cmd == "mission") {
        auto r = add_task(log, false, arg(0), std::atoi(arg(1).c_str()), args.opt.count("week") ? "week" : "day");
        if (!r.ok) return fail(r.message + "\nИспользование: sq mission \"что сделать\" <1-5> [--week]");
        std::println("Миссия {} добавлена. Выполнить: sq done {}", r.message, r.message);
        return 0;
    }
    if (cmd == "side" || cmd == "log") {
        int d = side_difficulty(arg(1));
        auto r = add_task(log, true, arg(0), d, "");
        if (!r.ok) return fail(r.message + "\nИспользование: sq " + cmd + " \"что сделать\" <лёгкий|средний|сложный>");
        if (cmd == "side") {
            std::println("Побочный квест {} добавлен. Выполнить: sq done {}", r.message, r.message);
            return 0;
        }
        std::string id = r.message;
        if (auto f = finish_task(log, replay(cfg, log), id); !f.ok) return fail(f.message);
        report_done(cfg, log, id, level_before);
        return 0;
    }
    if (cmd == "edit") {
        const char* usage = "\nИспользование: sq edit <id> \"новое название\" [сложность] [--day|--week]; название «-» — оставить прежнее";
        const Task* t = st.find(upper_ascii(trim(arg(0))));
        if (!t) return fail("Задача " + upper_ascii(trim(arg(0))) + " не найдена" + usage);
        std::string title = arg(1) == "-" ? "" : arg(1);
        int d = 0;
        if (!arg(2).empty()) {
            d = t->side ? side_difficulty(arg(2)) : std::atoi(arg(2).c_str());
            if (d <= 0) return fail(std::string(t->side ? "Сложность побочного квеста: лёгкий, средний или сложный" : "Сложность миссии — от 1 до 5") + usage);
        }
        std::string plan = args.opt.count("week") ? "week" : args.opt.count("day") ? "day" : "";
        auto r = edit_task(log, st, t->id, title, d, plan);
        if (!r.ok) return fail(r.message + usage);
        if (!r.message.empty()) {
            std::println("{}", r.message);
            return 0;
        }
        const Task* now = replay(cfg, log).find(t->id);
        std::println("{} изменена: {} ({})", t->id, now->title, now->rewards.describe());
        return 0;
    }
    if (cmd == "done" || cmd == "drop") {
        auto r = cmd == "done" ? finish_task(log, st, arg(0)) : drop_task(log, st, arg(0));
        if (!r.ok) return fail(r.message);
        if (cmd == "done") report_done(cfg, log, upper_ascii(arg(0)), level_before);
        else std::println("{} убрана.", upper_ascii(arg(0)));
        return 0;
    }
    return fail("Неизвестная команда «" + cmd + "». Справка: sq help");
}
