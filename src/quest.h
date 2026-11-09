#pragma once

// Кампании: сюжет из глав и рубежей-сражений. Рубежи с задачами не связаны:
// чтобы взять рубеж, нужны уровень героя, армия, снаряжение и магия, а всё это
// покупается в городе за золото, которое приносят миссии и побочные квесты (game.h).
//
// Кампании — это файлы *.ini (формат — docs/campaign-format.md). Одна встроена в
// программу, остальные подхватываются из папки campaigns рядом с exe и из
// data\campaigns. Золото, армия, кузня и магия у героя общие, прогресс рубежей —
// свой у каждой кампании.
//
// Мир не хранится: он строится из наград за работу (State), игрового журнала
// game.log и выбранной кампании.

#include "config.h"
#include "events.h"
#include "game.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sq::quest {

// Цвет в формате ImU32 (ABGR), чтобы ядро не зависело от ImGui.
constexpr uint32_t rgb(int r, int g, int b) {
    return 0xFF000000u | (uint32_t(b & 255) << 16) | (uint32_t(g & 255) << 8) | uint32_t(r & 255);
}

struct UnitType {
    const char* id;
    const char* name;
    const char* elite_name;
    int cost, level_req;
    int hp, atk, def, dmg_min, dmg_max, speed;
    bool ranged;
    bool flying = false;          // летит над камнями и отрядами
    bool charge = false;          // натиск: удар сильнее за каждую пройденную клетку
    bool no_retaliation = false;  // враг не отвечает на удар
    bool long_range = false;      // стреляет без штрафа за дальность
    const char* ability = "";     // способность словами, для города
};
const std::vector<UnitType>& unit_types();
int unit_index(const std::string& id);
int upgrade_cost(int unit);  // золота за одного бойца

struct Spell {
    const char* id;
    const char* name;
    const char* effect;
    std::vector<int> costs;  // золота за каждый уровень
};
const std::vector<Spell>& spells();
int spell_index(const std::string& id);

struct ForgeItem {
    const char* id;  // armor | weapon | walls
    const char* name;
    const char* effect;
    std::vector<int> costs;  // золота за каждый уровень
};
const std::vector<ForgeItem>& forge_items();
int forge_index(const std::string& id);

// Отряд в бою.
struct Stack {
    std::string name;
    std::string icon;  // guard archer knight mage griffin | monster | boss | hero
    int count = 0, hp = 1;
    int atk = 0, def = 0, dmg_min = 1, dmg_max = 1, speed = 1;  // скорость — клеток за ход
    bool ranged = false;
    bool flying = false;  // летит над препятствиями и отрядами
    bool charge = false;          // натиск: +7% урона за каждую клетку, пройденную перед ударом
    bool no_retaliation = false;  // атакованный им отряд не отвечает
    bool long_range = false;      // стрелок без штрафа за дальность
    int unit = -1;        // свои отряды: индекс в unit_types()
    bool elite = false;
    bool shielded = false;
    uint32_t color = 0;  // цвет монстров — цвет их главы
};

struct Army {
    std::vector<Stack> stacks;
    int atk_bonus = 0, def_bonus = 0, power = 1;
    int hp_pct = 0;  // прибавка к здоровью бойцов от уровня героя, в процентах
    int armor = 0, weapon = 0, walls = 0;
    std::vector<int> spell_levels;  // по spells()
};

struct Enemy {
    std::string name;
    std::vector<Stack> stacks;
    int ward = -1;         // заклинание, без которого предводитель неуязвим
    bool defense = false;  // оборона: стреляют стены
};

// Поле боя — шестиугольные клетки kCols × kRows, нечётные ряды сдвинуты вправо на полклетки.
// Клетка задаётся номером row * kCols + col.
constexpr int kCols = 12, kRows = 8, kCells = kCols * kRows;
constexpr int kFullRange = 6;  // дальше этого стрелки бьют вполсилы
int hex_distance(int a, int b);
std::vector<int> hex_neighbors(int cell);

struct BattleAction {
    enum Kind { Melee, Ranged, Spell, ShieldBreak, Tower, Defend, Move };
    Kind kind = Melee;
    int side = 0, actor = -1;  // side 0 — мы, 1 — противник; actor -1 — герой или стены
    int target = -1;           // отряд на другой стороне (для Defend и Move — сам отряд)
    int damage = 0, kills = 0;
    int spell = -1;
    int round = 0;
    bool counter = false;    // ответный удар атакованного отряда
    bool half = false;       // удар вполсилы: дальний выстрел или стрелок в ближнем бою
    std::vector<int> path;   // Move: клетки от старта до финиша
};

struct BattleResult {
    bool won = false;
    std::vector<Stack> ours, theirs;  // составы на начало боя
    std::vector<int> start[2];        // клетки отрядов на начало боя
    std::vector<int> obstacles;       // клетки с камнями
    std::vector<BattleAction> actions;
    std::vector<int> our_losses;      // при поражении и отступлении — нули: армия уходит целой
    int rounds = 0;
    bool defense = false;  // оборона у стен Цитадели
};

// Пошаговый бой на клетках. Отряды ходят по очереди по скорости; в начале раунда стреляют
// стены, герой раз в раунд может колдовать. Рукопашный отряд должен дойти до врага;
// атакованный в рукопашную раз за раунд отвечает ударом. Стрелок бьёт издалека без ответа,
// но вплотную к врагу — только вполсилы. За нашу сторону ходит игрок или ИИ, за врага — ИИ.
class Battle {
public:
    Battle(const Army& army, const Enemy& enemy, uint32_t seed);

    bool finished() const { return finished_; }
    bool player_turn() const { return !finished_ && side_ == 0; }
    int actor() const { return idx_; }  // текущий отряд на стороне side()
    int side() const { return side_; }
    int round() const { return round_; }
    int alive(int side, int idx) const { return live_[side][size_t(idx)].count; }
    int cell(int side, int idx) const { return cell_[side][size_t(idx)]; }
    bool shielded(int idx) const { return shielded_[size_t(idx)]; }
    bool defending(int side, int idx) const { return live_[side][size_t(idx)].defending; }
    bool can_cast() const { return player_turn() && !cast_this_round_; }
    std::vector<int> castable() const;  // заклинания, которые герой может применить сейчас

    // Куда текущий отряд может дойти за этот ход: число шагов до клетки или -1.
    std::vector<int> reachable() const;
    bool can_shoot() const;                         // текущий — стрелок, и рядом нет врагов
    int attack_cell(int target, int prefer) const;  // откуда бить в рукопашную; -1 — не дойти
    std::vector<int> path_to(int cell) const;       // путь текущего отряда до клетки (с начальной)

    struct Estimate {
        int dmg_min, dmg_max, kills_min, kills_max;
        bool counter;  // цель ответит ударом
        bool immune;   // цель под чарами
        bool half;     // удар вполсилы
        bool reach;    // можно атаковать в этот ход
    };
    Estimate estimate(int target, int from = -1) const;  // для текущего отряда; from — клетка для рукопашной

    void move(int cell);
    void attack(int target, int from = -1);  // from — клетка, откуда бить в рукопашную
    void defend();
    void cast(int spell, int target);
    void retreat();
    void auto_turn();  // ИИ за текущий отряд (и за героя, если ход наш)

    const BattleResult& result() const { return r_; }

private:
    struct Live {
        int count, top;
        bool defending = false, retaliated = false;
    };
    void begin_round();
    void advance();
    void check_end();
    void finish(bool won);
    int hit(int side, int target, int damage);
    double raw_damage(int side, int actor, int target, int roll, bool half, double mult = 1.0) const;
    double charge_bonus(int side, int actor, int steps) const;  // натиск: множитель за пройденные клетки
    bool shot_half(int side, int actor, int target) const;      // выстрел вполсилы: далеко и нет «дальнобоя»
    int choose_target(int side, int actor, bool attackable_only) const;
    bool occupied(int cell, int ignore_side = -1, int ignore_idx = -1) const;
    std::vector<int> bfs(int side, int idx, std::vector<int>* parent) const;
    bool enemy_adjacent(int side, int idx) const;
    BattleAction action(BattleAction::Kind kind, int side, int actor, int target, int damage, int kills, int spell = -1, bool counter = false) const;

    BattleResult r_;
    std::vector<Live> live_[2];
    std::vector<int> cell_[2];
    std::vector<bool> rock_;
    std::vector<bool> shielded_;
    std::vector<std::pair<int, int>> order_;  // очередь хода раунда: сторона и отряд
    size_t pos_ = 0;
    int side_ = 0, idx_ = 0, round_ = 0;
    bool cast_this_round_ = false, finished_ = false;
    Army army_;
    Enemy enemy_;
    uint64_t rng_;
    int roll(int lo, int hi);
};

// Автобой: ИИ за обе стороны.
BattleResult simulate(const Army& army, const Enemy& enemy, uint32_t seed);
int power_of(const std::vector<Stack>& stacks);
int army_power(const Army& army);  // с бонусами героя: атака, защита, здоровье
// ---------------------------------------------------------------------------
// Определение кампании — то, что читается из файла.

struct Chapter {
    std::string key;  // как глава названа в файле: [chapter.<key>]
    std::string name, intro;
    uint32_t color = 0;
    float x = 0, y = 0, radius = 190;
    bool reached = false;  // заполняется в build_world
};

struct FrontierDef {
    std::string id;
    int chapter = 0;  // индекс в CampaignDef::chapters
    std::string name, intro, victory;
    int level = 1, armor = 0, weapon = 0, walls = 0;
    int spell = -1;
    bool defense = false;
    std::vector<Stack> enemies;
    int gold = 0;
    bool placed = false;  // координаты заданы в файле
    float x = 0, y = 0;
};

struct CampaignDef {
    std::string id, title, author, description, prologue, epilogue;
    std::string source;  // «встроенная» или путь к файлу
    std::vector<Chapter> chapters;
    std::vector<FrontierDef> frontiers;
};

struct CampaignLibrary {
    std::vector<CampaignDef> campaigns;  // встроенная первой
    std::vector<std::string> errors;     // файлы, которые не удалось загрузить, и почему
    std::vector<fs::path> folders;       // где искали

    const CampaignDef* find(const std::string& id) const;
};

// Разбор текста кампании. При ошибке — nullopt и сообщения в errors.
std::optional<CampaignDef> parse_campaign(const std::string& text, const std::string& source, std::vector<std::string>& errors);
CampaignLibrary load_campaigns(const Config& cfg);
fs::path user_campaigns_dir(const Config& cfg);
std::string active_campaign_id(const Config& cfg);
bool select_campaign(Config& cfg, const std::string& id);
extern const char* const kBuiltinCampaign;

// ---------------------------------------------------------------------------
// Мир выбранной кампании.

struct Requirement {
    std::string text;
    bool met;
    bool advisory;  // только рекомендация (уровень героя): в бой пускает и без неё
};

struct Frontier {
    enum Status { Locked, Open, Cleared };
    std::string id;
    int chapter = 0;
    std::string name, intro, victory;
    int min_level = 1, armor = 0, weapon = 0, walls = 0;
    int spell = -1;
    Enemy enemy;
    Rewards loot;
    Status status = Locked;
    std::vector<Requirement> requirements;
    float x = 0, y = 0;  // на карте

    bool ready() const;  // все требования выполнены
};

struct Chronicle {
    std::string time, text;
    enum Tone { Story, Good, Bad, Chapter } tone;
    std::string title;  // у глав, пролога и эпилога — заголовок, а text — сюжет
};

struct World {
    std::string campaign_id, campaign_title;
    int gold = 0;  // казна
    int level = 1;
    Army army;
    std::vector<Chapter> chapters;
    std::vector<Frontier> frontiers;
    std::vector<Chronicle> chronicle;
    int cleared = 0;

    const Frontier* frontier(const std::string& id) const;
    const Frontier* next() const;  // первый открытый, ещё не взятый рубеж
    int army_count(int unit, bool elite) const;
};

World build_world(const Config& cfg, const State& st, const EventLog& game, const CampaignDef& campaign);

// Действия в городе и бой. Пустая строка — успех, иначе причина отказа.
std::string hire(const World& w, EventLog& game, int unit, int count);
std::string upgrade(const World& w, EventLog& game, int unit, int count);
std::string forge(const World& w, EventLog& game, int item);
std::string learn(const World& w, EventLog& game, int spell);
std::string battle_blocker(const World& w, const Frontier& f);
std::unique_ptr<Battle> start_battle(const World& w, const EventLog& game, const std::string& frontier_id);  // nullptr, если нельзя
// Тренировочный бой «Ристалище»: все роды войск против тварей Разлома. Итог никуда не записывается.
std::unique_ptr<Battle> training_battle(const World& w, uint32_t seed);
void record_battle(const World& w, EventLog& game, const std::string& frontier_id, const BattleResult& r);
// Начать кампании заново: армия, кузня, магия, бои и дары сбрасываются (game.log уходит в
// data\backups), задачи и заработанные ими опыт и золото остаются.
std::string reset_game(const Config& cfg, EventLog& game);
// Дар Цитадели: ручная поправка казны (видна в летописи). Отрицательная сумма — списание.
std::string gift(EventLog& game, int gold, const std::string& reason);
BattleResult fight(const World& w, EventLog& game, const std::string& frontier_id);

// Победа над монстрами выполненной миссии: всегда победа, только анимация.
BattleResult mission_skirmish(const World& w, const Task& task);

}  // namespace sq::quest
