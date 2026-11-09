#include "game.h"

#include <algorithm>
#include <cstdlib>
#include <format>
#include <fstream>

namespace sq {

Rewards& Rewards::operator+=(const Rewards& o) {
    xp += o.xp;
    gold += o.gold;
    return *this;
}

std::string Rewards::describe() const {
    std::string r;
    auto add = [&](int v, const char* what) {
        if (v) r += std::format("{}+{} {}", r.empty() ? "" : " · ", v, what);
    };
    add(xp, "опыта");
    add(gold, "золота");
    if (!artifact.empty()) r += (r.empty() ? "" : " · ") + std::string("трофей: ") + artifact;
    return r;
}

const std::vector<Artifact>& artifacts() {
    static const std::vector<Artifact> list = {
        {"Перчатки ярости", "+1 к атаке войск", 1, 0, 0},
        {"Амулет стойкости", "+1 к защите войск", 0, 1, 0},
        {"Посох искр", "+1 к силе магии", 0, 0, 1},
        {"Знамя Цитадели", "+1 к атаке и защите войск", 1, 1, 0},
        {"Плащ теней", "+2 к защите войск", 0, 2, 0},
        {"Клинок рассвета", "+2 к атаке войск", 2, 0, 0},
        {"Око мудреца", "+2 к силе магии", 0, 0, 2},
        {"Корона Хранителя", "+1 к атаке, защите и магии", 1, 1, 1},
    };
    return list;
}

const Artifact* find_artifact(const std::string& name) {
    for (auto& a : artifacts())
        if (name == a.name) return &a;
    return nullptr;
}

const Task* State::find(const std::string& id) const {
    for (auto& t : tasks)
        if (t.id == id) return &t;
    return nullptr;
}

Level level_of(int xp) {
    int level = 1, need = 100;
    while (xp >= need) {
        xp -= need;
        ++level;
        need = 100 + 50 * (level - 1);
    }
    return {level, xp, need};
}

const char* side_difficulty_name(int d) {
    static const char* names[] = {"лёгкий", "средний", "сложный"};
    return names[std::clamp(d, 1, 3) - 1];
}

// Трудозатраты квеста подобраны так, чтобы час работы стоил примерно как на миссии 1–2.
const char* side_difficulty_hint(int d) {
    static const char* hints[] = {
        "До часа: созвон, ответ коллеге, мелкая заявка",
        "Час-три: разбор с командой, консультация, небольшой отчёт",
        "Полдня и больше: месячный отчёт, внеплановый разбор инцидента",
    };
    return hints[std::clamp(d, 1, 3) - 1];
}

const char* mission_difficulty_hint(int d) {
    static const char* hints[] = {
        "Рутина до часа-двух",
        "Небольшая задача на полдня",
        "Полноценный рабочий день",
        "Нетривиальная задача на несколько дней",
        "Большая работа на неделю: механизм, серьёзное ревью",
    };
    return hints[std::clamp(d, 1, 5) - 1];
}

namespace {

uint32_t hash(std::string_view s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) h = (h ^ c) * 16777619u;
    return h;
}

// «10, 20, 35» из config.ini; если значения нет или оно короче нужного — значения по умолчанию.
int table(const Config& cfg, const char* key, int index, std::initializer_list<int> defaults) {
    std::vector<int> values(defaults);
    std::string raw = cfg.ini.get("rewards", key);
    if (!raw.empty()) {
        auto parts = split(raw, ',');
        for (size_t i = 0; i < parts.size() && i < values.size(); ++i) values[i] = std::atoi(trim(parts[i]).c_str());
    }
    return values[size_t(std::clamp(index, 0, int(values.size()) - 1))];
}

}  // namespace

Rewards mission_rewards(const Config& cfg, int d) {
    int i = std::clamp(d, 1, 5) - 1;
    Rewards r;
    r.xp = table(cfg, "mission_xp", i, {10, 20, 35, 55, 80});
    r.gold = table(cfg, "mission_gold", i, {30, 60, 100, 160, 240});
    return r;
}

Rewards side_rewards(const Config& cfg, int d, const std::string& created) {
    int i = std::clamp(d, 1, 3) - 1;
    Rewards r;
    std::string since = cfg.ini.get("rewards", "side_since");
    if (!created.empty() && !since.empty() && created < since) {
        r.xp = table(cfg, "side_xp_before", i, {5, 10, 20});
        r.gold = table(cfg, "side_gold_before", i, {20, 45, 90});
        return r;
    }
    r.xp = table(cfg, "side_xp", i, {4, 10, 20});
    r.gold = table(cfg, "side_gold", i, {15, 45, 90});
    return r;
}

int artifact_chance(const Config& cfg, int d) {
    if (d >= 5) return int(cfg.ini.num("rewards", "artifact_chance_5", 100));
    if (d == 4) return int(cfg.ini.num("rewards", "artifact_chance_4", 35));
    return 0;
}

State replay(const Config& cfg, const EventLog& log) {
    State st;
    auto find = [&](const std::string& id) -> Task* {
        for (auto& t : st.tasks)
            if (t.id == id) return &t;
        return nullptr;
    };
    for (auto& e : log.all()) {
        if (e.type == "task.add") {
            Task t;
            t.id = e.get("id");
            t.title = e.get("title");
            t.side = e.get("kind") == "side";
            t.difficulty = std::atoi(e.get("diff").c_str());
            t.plan = e.get("plan");
            t.created = e.time;
            t.rewards = t.side ? side_rewards(cfg, t.difficulty, t.created) : mission_rewards(cfg, t.difficulty);
            st.tasks.push_back(t);
        } else if (e.type == "task.edit") {
            Task* t = find(e.get("id"));
            if (!t || t->status != Task::Active) continue;
            if (auto title = e.get("title"); !title.empty()) t->title = title;
            if (int diff = std::atoi(e.get("diff").c_str()); diff > 0) t->difficulty = std::clamp(diff, 1, t->side ? 3 : 5);
            if (auto plan = e.get("plan"); !plan.empty() && !t->side) t->plan = plan;
            t->rewards = t->side ? side_rewards(cfg, t->difficulty, t->created) : mission_rewards(cfg, t->difficulty);
        } else if (e.type == "task.done") {
            Task* t = find(e.get("id"));
            if (!t || t->status != Task::Active) continue;
            t->status = Task::Done;
            t->finished = e.time;
            // Трофей: сложные миссии; какой именно — детерминированно по id, без повторов.
            if (!t->side && artifact_chance(cfg, t->difficulty) > int(hash(t->id) % 100)) {
                auto& list = artifacts();
                size_t start = hash(t->id + "#") % list.size();
                for (size_t k = 0; k < list.size() && t->rewards.artifact.empty(); ++k) {
                    const char* name = list[(start + k) % list.size()].name;
                    if (std::ranges::find(st.artifacts, name) == st.artifacts.end()) t->rewards.artifact = name;
                }
                if (t->rewards.artifact.empty()) t->rewards.gold += 100;  // все трофеи собраны — золото
                else st.artifacts.push_back(t->rewards.artifact);
            }
            st.earned += t->rewards;
            st.weeks[week_start(parse_date(e.time))] += t->rewards.xp;
        } else if (e.type == "task.drop") {
            Task* t = find(e.get("id"));
            if (t && t->status == Task::Active) {
                t->status = Task::Dropped;
                t->finished = e.time;
            }
        }
    }
    return st;
}

ActionResult init_config(Config& cfg) {
    std::error_code ec;
    fs::create_directories(cfg.home, ec);
    if (!fs::exists(cfg.config_file(), ec)) {
        std::ofstream out(cfg.config_file(), std::ios::binary);
        out << kDefaultConfig;
        if (!out) return ActionResult::fail("Не удалось создать " + from_path(cfg.config_file()));
    }
    reload_config(cfg);
    std::string warning = check_data_format(cfg);
    if (warning.empty()) migrate_rewards(cfg);
    return {true, warning};  // message — предупреждение о слишком новых данных
}

void migrate_rewards(Config& cfg) {
    auto same = [](std::string a, std::string b) {
        std::erase(a, ' ');
        std::erase(b, ' ');
        return a == b;
    };
    std::string xp = cfg.ini.get("rewards", "side_xp"), gold = cfg.ini.get("rewards", "side_gold");
    if (!same(xp, "5,10,20") || !same(gold, "20,45,90") || !cfg.ini.get("rewards", "side_since").empty()) return;
    fs::path file = cfg.config_file();
    set_ini_value(file, "rewards", "side_since", now_iso());
    set_ini_value(file, "rewards", "side_gold_before", gold);
    set_ini_value(file, "rewards", "side_xp_before", xp);
    set_ini_value(file, "rewards", "side_gold", "15, 45, 90");
    set_ini_value(file, "rewards", "side_xp", "4, 10, 20");
    reload_config(cfg);
}

bool reload_config(Config& cfg) {
    return cfg.ini.load(cfg.config_file());
}

ActionResult add_task(EventLog& log, bool side, const std::string& title_arg, int difficulty, const std::string& plan) {
    std::string title = trim(title_arg);
    if (title.empty()) return ActionResult::fail("Напишите, что нужно сделать");
    if (title.size() > 300) return ActionResult::fail("Слишком длинное название");
    if (side && (difficulty < 1 || difficulty > 3)) return ActionResult::fail("Сложность побочного квеста: лёгкий, средний или сложный");
    if (!side && (difficulty < 1 || difficulty > 5)) return ActionResult::fail("Сложность миссии — от 1 до 5");

    int n = 0;
    for (auto& e : log.all()) n += e.type == "task.add" && (e.get("kind") == "side") == side;
    std::string id = (side ? "S" : "M") + std::to_string(n + 1);

    Event e;
    e.type = "task.add";
    e.set("id", id).set("kind", side ? "side" : "mission").set("title", title).set("diff", std::to_string(difficulty));
    if (!side) e.set("plan", plan == "week" ? "week" : "day");
    if (!log.append(std::move(e))) return ActionResult::fail("Не удалось записать событие");
    return {true, id};
}

namespace {

ActionResult close_task(EventLog& log, const State& st, const std::string& id_arg, const char* type) {
    std::string id = upper_ascii(trim(id_arg));
    const Task* t = st.find(id);
    if (!t) return ActionResult::fail("Задача " + id + " не найдена");
    if (t->status != Task::Active) return ActionResult::fail("Задача " + id + " уже в истории");
    Event e;
    e.type = type;
    e.set("id", id);
    if (!log.append(std::move(e))) return ActionResult::fail("Не удалось записать событие");
    return {};
}

}  // namespace

ActionResult finish_task(EventLog& log, const State& st, const std::string& id) {
    return close_task(log, st, id, "task.done");
}

ActionResult drop_task(EventLog& log, const State& st, const std::string& id) {
    return close_task(log, st, id, "task.drop");
}

ActionResult edit_task(EventLog& log, const State& st, const std::string& id_arg, const std::string& title_arg, int difficulty,
                       const std::string& plan) {
    std::string id = upper_ascii(trim(id_arg));
    const Task* t = st.find(id);
    if (!t) return ActionResult::fail("Задача " + id + " не найдена");
    if (t->status != Task::Active) return ActionResult::fail("Задача " + id + " уже в истории, её не изменить");
    std::string title = trim(title_arg);
    if (title.size() > 300) return ActionResult::fail("Слишком длинное название");
    if (t->side && (difficulty < 0 || difficulty > 3)) return ActionResult::fail("Сложность побочного квеста: лёгкий, средний или сложный");
    if (!t->side && (difficulty < 0 || difficulty > 5)) return ActionResult::fail("Сложность миссии — от 1 до 5");

    // В событие попадает только изменённое. Старые версии программы такие события пропускают.
    Event e;
    e.type = "task.edit";
    e.set("id", id);
    if (!title.empty() && title != t->title) e.set("title", title);
    if (difficulty > 0 && difficulty != t->difficulty) e.set("diff", std::to_string(difficulty));
    if (!t->side && (plan == "day" || plan == "week") && plan != t->plan) e.set("plan", plan);
    if (e.fields.size() == 1) return {true, "Ничего не изменилось"};
    if (!log.append(std::move(e))) return ActionResult::fail("Не удалось записать событие");
    return {};
}

}  // namespace sq
