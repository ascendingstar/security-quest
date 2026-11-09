#pragma once

// Работа: миссии (задачи со сложностью 1–5) и побочные квесты (лёгкий, средний, сложный).
// Всё создаётся вручную при планировании дня или недели. Выполненное уходит в историю.
// Состояние и награды каждый раз пересчитываются из журнала events.log, поэтому
// таблицу наград в config.ini можно менять задним числом.

#include "config.h"
#include "events.h"
#include "util.h"

#include <map>
#include <string>
#include <vector>

namespace sq {

// Единственная валюта — золото; опыт отдельно поднимает уровень героя.
struct Rewards {
    int xp = 0, gold = 0;
    std::string artifact;  // трофей, если выпал

    Rewards& operator+=(const Rewards& o);
    std::string describe() const;  // «+35 опыта · +100 золота · трофей: …»
};

struct Artifact {
    const char* name;
    const char* effect;  // описание для игрока
    int atk, def, power;
};
const std::vector<Artifact>& artifacts();
const Artifact* find_artifact(const std::string& name);

struct Task {
    enum Status { Active, Done, Dropped };
    std::string id, title;  // M3 или S7
    bool side = false;
    int difficulty = 1;     // миссия 1–5, побочный квест 1–3
    std::string plan;       // day | week — только для миссий
    std::string created, finished;
    Status status = Active;
    Rewards rewards;        // для выполненных — полученные, для активных — ожидаемые
};

struct State {
    std::vector<Task> tasks;  // в порядке создания
    Rewards earned;           // всё, что принесла работа
    std::vector<std::string> artifacts;
    std::map<std::string, int> weeks;  // понедельник недели -> опыт
    int total_xp() const { return earned.xp; }
    const Task* find(const std::string& id) const;
};

struct Level {
    int level, into, need;
};
Level level_of(int xp);

const char* side_difficulty_name(int d);    // лёгкий / средний / сложный
const char* mission_difficulty_hint(int d); // подсказка, как выбирать сложность
const char* side_difficulty_hint(int d);    // то же для побочного квеста
Rewards mission_rewards(const Config& cfg, int difficulty);
// created — когда квест создан: квесты до снижения наград (migrate_rewards) считаются по прежней таблице.
Rewards side_rewards(const Config& cfg, int difficulty, const std::string& created = {});
int artifact_chance(const Config& cfg, int difficulty);  // в процентах

State replay(const Config& cfg, const EventLog& log);

struct ActionResult {
    bool ok = true;
    std::string message;  // при ошибке — причина, при успехе — необязательно (например, id)

    static ActionResult fail(std::string m) { return {false, std::move(m)}; }
};

ActionResult init_config(Config& cfg);
bool reload_config(Config& cfg);
// В 0.5 снижена награда за лёгкий побочный квест. Если в config.ini стоят прежние значения по умолчанию,
// они меняются на новые, а уже созданные квесты сохраняют прежнюю награду: казна не уходит в минус.
void migrate_rewards(Config& cfg);

// side: false — миссия (difficulty 1–5, plan day|week), true — побочный квест (difficulty 1–3).
// При успехе message — id новой задачи.
ActionResult add_task(EventLog& log, bool side, const std::string& title, int difficulty, const std::string& plan);
ActionResult finish_task(EventLog& log, const State& st, const std::string& id);
ActionResult drop_task(EventLog& log, const State& st, const std::string& id);
// Правка активной задачи. Пустое название, difficulty 0 и пустой plan — оставить как было.
ActionResult edit_task(EventLog& log, const State& st, const std::string& id, const std::string& title, int difficulty,
                       const std::string& plan);

}  // namespace sq
