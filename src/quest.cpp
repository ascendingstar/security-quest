#include "quest.h"

#include "util.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <format>
#include <fstream>
#include <set>
#include <sstream>

namespace sq::quest {

// ---------------------------------------------------------------------------
// Справочники

const std::vector<UnitType>& unit_types() {
    static const std::vector<UnitType> units = {
        // Новые роды войск дописываются в конец: журнал хранит их id, а не номер, но порядок в городе — по уровню.
        {"guard", "Стражник", "Страж цитадели", 20, 1, 12, 3, 5, 2, 3, 4, false},
        {"archer", "Лучник", "Следопыт", 35, 2, 9, 5, 2, 3, 4, 5, true},
        {"knight", "Рыцарь", "Паладин", 90, 4, 30, 8, 8, 6, 9, 5, false},
        {"mage", "Рунописец", "Архимаг", 120, 6, 14, 9, 3, 8, 11, 4, true},
        {"griffin", "Грифон", "Королевский грифон", 200, 8, 45, 11, 9, 10, 14, 8, false, true, false, false, false, "летает над камнями и отрядами"},
        {"rider", "Всадник", "Латный всадник", 60, 3, 20, 6, 4, 3, 5, 7, false, false, true, false, false,
         "натиск: +7% урона за каждую клетку разбега"},
        {"berserk", "Берсерк", "Ярл-берсерк", 80, 5, 24, 9, 2, 5, 8, 6, false, false, false, true, false, "бьёт так, что враг не успевает ответить"},
        {"crossbow", "Арбалетчик", "Тяжёлый арбалетчик", 75, 7, 13, 8, 5, 6, 8, 3, true, false, false, false, true,
         "стреляет через всё поле без штрафа"},
    };
    return units;
}

int unit_index(const std::string& id) {
    auto& u = unit_types();
    for (size_t i = 0; i < u.size(); ++i)
        if (id == u[i].id) return int(i);
    return -1;
}

int upgrade_cost(int unit) {
    return int(std::lround(unit_types()[size_t(unit)].cost * 0.6));
}

const std::vector<Spell>& spells() {
    static const std::vector<Spell> list = {
        {"fire", "Огненная стрела", "Каждый раунд герой бьёт огнём по врагу; урон растёт с уровнем. Сжигает чары, которые боятся огня.", {120, 300, 550}},
        {"bless", "Благословение", "+2 к защите всех войск за каждый уровень.", {180, 420}},
        {"light", "Очищающий свет", "Развеивает чары нежити и тьмы.", {500}},
        {"storm", "Громоотвод", "Отводит молнии и бури.", {650}},
        {"seal", "Печать Щита", "Запечатывает разломы и порталы.", {900}},
    };
    return list;
}

int spell_index(const std::string& id) {
    auto& s = spells();
    for (size_t i = 0; i < s.size(); ++i)
        if (id == s[i].id) return int(i);
    return -1;
}

const std::vector<ForgeItem>& forge_items() {
    static const std::vector<ForgeItem> list = {
        {"armor", "Броня", "Урон по вашим войскам ниже на 8% за каждый уровень.", {120, 260, 450, 700, 1000}},
        {"weapon", "Оружие", "+2 к атаке всех войск за каждый уровень.", {120, 260, 450, 700, 1000}},
        {"walls", "Стены Цитадели", "В оборонительных боях стреляют башни: по одной за уровень.", {200, 450, 800}},
    };
    return list;
}

int forge_index(const std::string& id) {
    auto& f = forge_items();
    for (size_t i = 0; i < f.size(); ++i)
        if (id == f[i].id) return int(i);
    return -1;
}

namespace {

uint32_t hash(std::string_view s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) h = (h ^ c) * 16777619u;
    return h;
}

struct Rng {
    uint64_t state;
    explicit Rng(uint32_t seed) : state(seed * 0x9E3779B97F4A7C15ull + 1) {}
    uint32_t next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return uint32_t(state >> 16);
    }
    int range(int lo, int hi) { return hi <= lo ? lo : lo + int(next() % uint32_t(hi - lo + 1)); }
};

Stack player_stack(int unit, bool elite, int count) {
    const UnitType& u = unit_types()[size_t(unit)];
    Stack s;
    s.name = elite ? u.elite_name : u.name;
    s.icon = u.id;
    s.count = count;
    s.hp = elite ? int(u.hp * 1.4) : u.hp;
    s.atk = u.atk + (elite ? 2 : 0);
    s.def = u.def + (elite ? 2 : 0);
    s.dmg_min = elite ? int(std::lround(u.dmg_min * 1.3)) : u.dmg_min;
    s.dmg_max = elite ? int(std::lround(u.dmg_max * 1.3)) : u.dmg_max;
    s.speed = u.speed + (elite ? 1 : 0);
    s.ranged = u.ranged;
    s.flying = u.flying;
    s.charge = u.charge;
    s.no_retaliation = u.no_retaliation;
    s.long_range = u.long_range;
    s.unit = unit;
    s.elite = elite;
    return s;
}

Stack monster(const char* name, int count, int hp, int atk, int def, int dmin, int dmax, int speed, bool ranged = false, bool boss = false) {
    Stack s;
    s.name = name;
    s.icon = boss ? "boss" : "monster";
    s.count = count;
    s.hp = hp;
    s.atk = atk;
    s.def = def;
    s.dmg_min = dmin;
    s.dmg_max = dmax;
    s.speed = speed;
    s.ranged = ranged;
    return s;
}

// ---------------------------------------------------------------------------
// Разбор файла кампании

const uint32_t kPalette[] = {rgb(222, 128, 74), rgb(110, 196, 110), rgb(176, 160, 220), rgb(96, 170, 246),
                             rgb(220, 80, 170),  rgb(214, 196, 90),  rgb(90, 200, 196),  rgb(230, 110, 110)};

bool parse_int(std::string_view s, int& out, int lo, int hi) {
    std::string t = trim(s);
    if (t.empty()) return false;
    char* end = nullptr;
    long v = std::strtol(t.c_str(), &end, 10);
    if (!end || *end != '\0' || v < lo || v > hi) return false;
    out = int(v);
    return true;
}

bool parse_flag(const std::string& s, bool& out) {
    std::string t = upper_ascii(trim(s));
    if (t.empty() || t == "0" || t == "NO" || t == "FALSE" || s == "нет") {
        out = false;
        return true;
    }
    if (t == "1" || t == "YES" || t == "TRUE" || s == "да") {
        out = true;
        return true;
    }
    return false;
}

bool parse_enemy(const std::string& value, Stack& s, std::string& why) {
    std::vector<std::string> f;
    for (auto& part : split(value, ';')) f.push_back(trim(part));
    if (f.size() < 7 || f.size() > 8) {
        why = "ожидается «имя; количество; здоровье; атака; защита; урон; скорость; признаки»";
        return false;
    }
    if (f[0].empty()) {
        why = "нет имени";
        return false;
    }
    int count, hp, atk, def, speed, dmin, dmax;
    auto dmg = split(f[5], '-');
    if (!parse_int(f[1], count, 1, 9999)) why = "количество — целое от 1 до 9999";
    else if (!parse_int(f[2], hp, 1, 100000)) why = "здоровье — целое от 1 до 100000";
    else if (!parse_int(f[3], atk, 0, 500)) why = "атака — целое от 0 до 500";
    else if (!parse_int(f[4], def, 0, 500)) why = "защита — целое от 0 до 500";
    else if (dmg.size() > 2 || !parse_int(dmg[0], dmin, 1, 10000) || !parse_int(dmg.back(), dmax, dmin, 10000))
        why = "урон — число или диапазон вроде 4-7";
    else if (!parse_int(f[6], speed, 1, 20)) why = "скорость — целое от 1 до 20";
    if (!why.empty()) return false;
    bool ranged = false, boss = false, flying = false, charge = false, no_retaliation = false, long_range = false;
    if (f.size() == 8)
        for (auto& raw : split(f[7], ',')) {
            std::string flag = trim(raw);
            if (flag == "стрелок" || upper_ascii(flag) == "RANGED") ranged = true;
            else if (flag == "босс" || upper_ascii(flag) == "BOSS") boss = true;
            else if (flag == "летун" || upper_ascii(flag) == "FLYING") flying = true;
            else if (flag == "натиск" || upper_ascii(flag) == "CHARGE") charge = true;
            else if (flag == "без ответа" || upper_ascii(flag) == "NO_RETALIATION") no_retaliation = true;
            else if (flag == "дальнобой" || upper_ascii(flag) == "FAR") long_range = true;
            else if (!flag.empty()) {
                why = "неизвестный признак «" + flag + "»: можно «стрелок», «летун», «натиск», «без ответа», «дальнобой» и «босс»";
                return false;
            }
        }
    s = monster(f[0].c_str(), count, hp, atk, def, dmin, dmax, speed, ranged, boss);
    s.flying = flying;
    s.charge = charge;
    s.no_retaliation = no_retaliation;
    s.long_range = long_range;
    return true;
}
std::string join_losses(const std::vector<Stack>& ours, const std::vector<int>& losses) {
    std::string r;
    for (size_t i = 0; i < ours.size(); ++i)
        if (losses[i] > 0 && ours[i].unit >= 0)
            r += std::format("{}{}:{}:{}", r.empty() ? "" : ",", unit_types()[size_t(ours[i].unit)].id, ours[i].elite ? 1 : 0, losses[i]);
    return r;
}


}  // namespace

// ---------------------------------------------------------------------------
// Кампании: разбор, загрузка, выбор

std::optional<CampaignDef> parse_campaign(const std::string& text, const std::string& source, std::vector<std::string>& errors) {
    Ini ini;
    ini.parse(text);
    size_t before = errors.size();
    auto err = [&](const std::string& section, const std::string& what) {
        errors.push_back(source + ": " + (section.empty() ? "" : "[" + section + "] ") + what);
    };

    CampaignDef c;
    c.source = source;
    if (!ini.has("campaign")) {
        err("", "нет раздела [campaign]");
        return std::nullopt;
    }
    c.id = ini.get("campaign", "id");
    c.title = ini.get("campaign", "title");
    c.author = ini.get("campaign", "author");
    c.description = ini.get("campaign", "description");
    c.prologue = ini.get("campaign", "prologue");
    c.epilogue = ini.get("campaign", "epilogue");
    bool id_ok = !c.id.empty() && c.id.size() <= 40 &&
                 std::ranges::all_of(c.id, [](char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_'; });
    if (!id_ok) err("campaign", "id — от 1 до 40 латинских строчных букв, цифр, «-» или «_»");
    if (c.title.empty()) err("campaign", "нет title");

    // Главы — в порядке появления в файле.
    for (auto& sec : ini.sections("chapter.")) {
        Chapter ch;
        ch.key = sec.substr(8);
        ch.name = ini.get(sec, "name");
        ch.intro = ini.get(sec, "intro");
        if (ch.name.empty()) err(sec, "нет name");
        size_t i = c.chapters.size();
        ch.color = kPalette[i % std::size(kPalette)];
        if (std::string color = ini.get(sec, "color"); !color.empty()) {
            auto parts = split(color, ',');
            int r, g, b;
            if (parts.size() == 3 && parse_int(parts[0], r, 0, 255) && parse_int(parts[1], g, 0, 255) && parse_int(parts[2], b, 0, 255))
                ch.color = rgb(r, g, b);
            else err(sec, "color — три числа 0–255 через запятую, например 222, 128, 74");
        }
        ch.x = -40.0f + 410.0f * float(i);
        ch.y = i % 2 == 0 ? 60.0f : -110.0f;
        int x, y, radius;
        if (!ini.get(sec, "x").empty() || !ini.get(sec, "y").empty()) {
            if (parse_int(ini.get(sec, "x"), x, -20000, 20000) && parse_int(ini.get(sec, "y"), y, -20000, 20000)) {
                ch.x = float(x);
                ch.y = float(y);
            } else {
                err(sec, "x и y — целые числа, задаются вместе");
            }
        }
        if (!ini.get(sec, "radius").empty()) {
            if (parse_int(ini.get(sec, "radius"), radius, 60, 1000)) ch.radius = float(radius);
            else err(sec, "radius — целое от 60 до 1000");
        }
        c.chapters.push_back(ch);
    }
    if (c.chapters.empty()) err("", "нет ни одной главы [chapter.1]");
    if (c.chapters.size() > 20) err("", "не больше 20 глав");

    // Рубежи — тоже в порядке появления: открываются по одному.
    std::set<std::string> ids;
    for (auto& sec : ini.sections("frontier.")) {
        FrontierDef f;
        f.id = sec.substr(9);
        if (f.id.empty() || !ids.insert(f.id).second) err(sec, "id рубежа пустой или повторяется");
        std::string chapter_key = ini.get(sec, "chapter");
        f.chapter = -1;
        for (size_t i = 0; i < c.chapters.size(); ++i)
            if (c.chapters[i].key == chapter_key) f.chapter = int(i);
        if (f.chapter < 0) err(sec, "chapter = «" + chapter_key + "», а такой главы нет");
        f.name = ini.get(sec, "name");
        f.intro = ini.get(sec, "intro");
        f.victory = ini.get(sec, "victory");
        f.story = ini.get(sec, "story");
        if (f.name.empty()) err(sec, "нет name");

        auto number = [&](const char* key, int& out, int lo, int hi, int def) {
            std::string v = ini.get(sec, key);
            out = def;
            if (!v.empty() && !parse_int(v, out, lo, hi)) err(sec, std::format("{} — целое от {} до {}", key, lo, hi));
        };
        number("level", f.level, 1, 99, 1);
        number("armor", f.armor, 0, int(forge_items()[0].costs.size()), 0);
        number("weapon", f.weapon, 0, int(forge_items()[1].costs.size()), 0);
        number("walls", f.walls, 0, int(forge_items()[2].costs.size()), 0);
        number("gold", f.gold, 0, 1000000, 0);
        if (std::string s = ini.get(sec, "spell"); !s.empty()) {
            f.spell = spell_index(s);
            if (f.spell < 0) err(sec, "spell = «" + s + "»: можно fire, bless, light, storm или seal");
        }
        if (!parse_flag(ini.get(sec, "defense"), f.defense)) err(sec, "defense — да или нет");
        int x, y;
        if (!ini.get(sec, "x").empty() || !ini.get(sec, "y").empty()) {
            if (parse_int(ini.get(sec, "x"), x, -20000, 20000) && parse_int(ini.get(sec, "y"), y, -20000, 20000)) {
                f.placed = true;
                f.x = float(x);
                f.y = float(y);
            } else {
                err(sec, "x и y — целые числа, задаются вместе");
            }
        }
        for (int k = 1; k <= 9; ++k) {
            std::string v = ini.get(sec, std::format("enemy{}", k));
            if (v.empty()) continue;
            Stack s;
            std::string why;
            if (parse_enemy(v, s, why)) f.enemies.push_back(s);
            else err(sec, std::format("enemy{}: {}", k, why));
        }
        if (f.enemies.empty()) err(sec, "нужен хотя бы один враг: enemy1 = …");
        c.frontiers.push_back(f);
    }
    if (c.frontiers.empty()) err("", "нет ни одного рубежа [frontier.…]");
    if (c.frontiers.size() > 100) err("", "не больше 100 рубежей");
    if (errors.size() > before) return std::nullopt;

    // Рубежи без координат встают дугой вокруг центра своей главы.
    std::map<int, std::vector<FrontierDef*>> by_chapter;
    for (auto& f : c.frontiers) by_chapter[f.chapter].push_back(&f);
    for (auto& [ch, list] : by_chapter) {
        Chapter& chapter = c.chapters[size_t(ch)];
        int n = int(list.size());
        float span = std::min(chapter.radius * 1.3f, 70.0f * float(n - 1));
        for (int k = 0; k < n; ++k) {
            FrontierDef& f = *list[size_t(k)];
            f.label = std::format("{}.{}", ch + 1, k + 1);
            if (f.placed) continue;
            f.x = chapter.x + (n == 1 ? 0.0f : -span / 2 + span * float(k) / float(n - 1));
            f.y = chapter.y + (k % 2 == 0 ? 50.0f : -55.0f);
        }
    }
    return c;
}

namespace {

const CampaignDef& builtin() {
    static const CampaignDef c = [] {
        std::vector<std::string> errors;
        CampaignDef def = parse_campaign(kBuiltinCampaign, "встроенная", errors).value_or(CampaignDef{});
        def.source = "встроенная";
        return def;
    }();
    return c;
}

}  // namespace

const CampaignDef* CampaignLibrary::find(const std::string& id) const {
    for (auto& c : campaigns)
        if (c.id == id) return &c;
    return nullptr;
}

fs::path user_campaigns_dir(const Config& cfg) {
    return cfg.home / "campaigns";
}

CampaignLibrary load_campaigns(const Config& cfg) {
    CampaignLibrary lib;
    if (!builtin().id.empty()) lib.campaigns.push_back(builtin());
    // Сначала кампании, которые идут с программой, потом свои: свой файл с тем же id заменяет поставляемый.
    lib.folders = {exe_dir() / "campaigns", user_campaigns_dir(cfg)};
    std::set<fs::path> seen;
    for (auto& folder : lib.folders) {
        std::error_code ec;
        if (!fs::is_directory(folder, ec)) continue;
        fs::path canonical = fs::weakly_canonical(folder, ec);
        if (!seen.insert(canonical).second) continue;
        std::vector<fs::path> files;
        for (auto& entry : fs::directory_iterator(folder, ec))
            if (entry.is_regular_file() && entry.path().extension() == ".ini") files.push_back(entry.path());
        std::ranges::sort(files);
        for (auto& file : files) {
            std::ifstream in(file, std::ios::binary);
            std::stringstream ss;
            ss << in.rdbuf();
            std::string name = from_path(file.filename());
            auto c = parse_campaign(ss.str(), name, lib.errors);
            if (!c) continue;
            c->source = from_path(file);
            auto it = std::ranges::find_if(lib.campaigns, [&](const CampaignDef& x) { return x.id == c->id; });
            if (it != lib.campaigns.end()) *it = *c;
            else lib.campaigns.push_back(*c);
        }
    }
    return lib;
}

std::string active_campaign_id(const Config& cfg) {
    return cfg.ini.get("game", "campaign", "shield");
}

bool select_campaign(Config& cfg, const std::string& id) {
    bool ok = set_ini_value(cfg.config_file(), "game", "campaign", id);
    cfg.ini.load(cfg.config_file());
    return ok;
}
// ---------------------------------------------------------------------------
// Бой

int power_of(const std::vector<Stack>& stacks) {
    double p = 0;
    for (auto& s : stacks) p += s.count * (s.hp + (s.dmg_min + s.dmg_max) * 1.5 + s.atk * 2 + s.def * 2);
    return int(p);
}

int army_power(const Army& army) {
    std::vector<Stack> stacks = army.stacks;
    for (auto& s : stacks) {
        s.atk += army.atk_bonus;
        s.def += army.def_bonus;
        s.hp = s.hp * (100 + army.hp_pct) / 100;
    }
    return power_of(stacks);
}

// ---------------------------------------------------------------------------
// Клетки поля

namespace {

struct Cube {
    int x, y, z;
};

Cube to_cube(int cell) {
    int row = cell / kCols, col = cell % kCols;
    int x = col - (row - (row & 1)) / 2;
    return {x, -x - row, row};
}

}  // namespace

int hex_distance(int a, int b) {
    Cube p = to_cube(a), q = to_cube(b);
    return std::max({std::abs(p.x - q.x), std::abs(p.y - q.y), std::abs(p.z - q.z)});
}

std::vector<int> hex_neighbors(int cell) {
    static const int even[6][2] = {{1, 0}, {-1, 0}, {0, -1}, {-1, -1}, {0, 1}, {-1, 1}};
    static const int odd[6][2] = {{1, 0}, {-1, 0}, {1, -1}, {0, -1}, {1, 1}, {0, 1}};
    int row = cell / kCols, col = cell % kCols;
    const auto& d = (row & 1) ? odd : even;
    std::vector<int> out;
    for (auto& o : d) {
        int c = col + o[0], r = row + o[1];
        if (c >= 0 && c < kCols && r >= 0 && r < kRows) out.push_back(r * kCols + c);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Пошаговый бой

Battle::Battle(const Army& army, const Enemy& enemy, uint32_t seed) : army_(army), enemy_(enemy), rng_(seed * 0x9E3779B97F4A7C15ull + 1) {
    r_.defense = enemy.defense;
    r_.ours = army.stacks;
    for (auto& s : r_.ours) {
        s.atk += army.atk_bonus;
        s.def += army.def_bonus;
        s.hp = std::max(1, s.hp * (100 + army.hp_pct) / 100);
    }
    r_.theirs = enemy.stacks;
    for (auto& s : r_.theirs) s.shielded = enemy.ward >= 0 && s.icon == "boss";
    for (auto& s : r_.ours) live_[0].push_back({s.count, s.hp, false, false});
    for (auto& s : r_.theirs) live_[1].push_back({s.count, s.hp, false, false});
    for (auto& s : r_.theirs) shielded_.push_back(s.shielded);
    r_.our_losses.assign(r_.ours.size(), 0);

    // Расстановка: наши в левом столбце, враги в правом, ряды — равномерно.
    auto rows_for = [](size_t n, size_t k) { return int(std::lround((float(k) + 0.5f) * float(kRows) / float(n) - 0.5f)); };
    // Больше пяти отрядов — шахматкой в два столбца, иначе фигуры соседних рядов налезают друг на друга.
    auto column = [](size_t n, size_t k) { return n > 5 ? int(k % 2) : 0; };
    for (size_t k = 0; k < r_.ours.size(); ++k)
        cell_[0].push_back(std::clamp(rows_for(r_.ours.size(), k), 0, kRows - 1) * kCols + column(r_.ours.size(), k));
    for (size_t k = 0; k < r_.theirs.size(); ++k)
        cell_[1].push_back(std::clamp(rows_for(r_.theirs.size(), k), 0, kRows - 1) * kCols + kCols - 1 - column(r_.theirs.size(), k));
    // Больше отрядов, чем рядов: лишние встают во второй столбец.
    for (int side = 0; side < 2; ++side)
        for (size_t k = 0; k < cell_[side].size(); ++k)
            for (size_t j = 0; j < k; ++j)
                if (cell_[side][j] == cell_[side][k]) cell_[side][k] += side == 0 ? 1 : -1;
    // Несколько камней посередине — укрытия и обходы.
    rock_.assign(kCells, false);
    int rocks = roll(3, 5);
    for (int k = 0, tries = 0; k < rocks && tries < 50; ++tries) {
        int c = roll(0, kRows - 1) * kCols + roll(3, kCols - 4);
        if (rock_[size_t(c)]) continue;
        rock_[size_t(c)] = true;
        r_.obstacles.push_back(c);
        ++k;
    }
    r_.start[0] = cell_[0];
    r_.start[1] = cell_[1];
    if (r_.ours.empty()) {
        finish(false);
        return;
    }
    begin_round();
}

int Battle::roll(int lo, int hi) {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 7;
    rng_ ^= rng_ << 17;
    uint32_t v = uint32_t(rng_ >> 16);
    return hi <= lo ? lo : lo + int(v % uint32_t(hi - lo + 1));
}

BattleAction Battle::action(BattleAction::Kind kind, int side, int actor, int target, int damage, int kills, int spell, bool counter) const {
    BattleAction a;
    a.kind = kind;
    a.side = side;
    a.actor = actor;
    a.target = target;
    a.damage = damage;
    a.kills = kills;
    a.spell = spell;
    a.round = round_;
    a.counter = counter;
    return a;
}

void Battle::finish(bool won) {
    finished_ = true;
    r_.won = won;
    // Проигранный бой — отступление: армия уходит целой, теряется только время.
    if (!won) r_.our_losses.assign(r_.ours.size(), 0);
}

void Battle::begin_round() {
    ++round_;
    if (round_ > 40) {  // затянувшийся бой — отступление
        finish(false);
        return;
    }
    r_.rounds = round_;
    cast_this_round_ = false;
    for (auto& l : live_[0]) l.retaliated = false;
    for (auto& l : live_[1]) l.retaliated = false;

    // Стены стреляют в начале раунда.
    if (enemy_.defense && army_.walls > 0) {
        int t = choose_target(0, -1, false);
        if (t >= 0 && !shielded_[size_t(t)]) {
            int dmg = 15 * army_.walls + roll(0, 8 * army_.walls);
            r_.actions.push_back(action(BattleAction::Tower, 0, -1, t, dmg, hit(1, t, dmg)));
            check_end();
            if (finished_) return;
        }
    }

    order_.clear();
    for (int side = 0; side < 2; ++side)
        for (size_t i = 0; i < live_[side].size(); ++i) order_.push_back({side, int(i)});
    std::stable_sort(order_.begin(), order_.end(), [&](auto& a, auto& b) {
        const Stack& x = a.first == 0 ? r_.ours[size_t(a.second)] : r_.theirs[size_t(a.second)];
        const Stack& y = b.first == 0 ? r_.ours[size_t(b.second)] : r_.theirs[size_t(b.second)];
        return x.speed > y.speed;
    });
    pos_ = 0;
    advance();
}

// Переходит к следующему живому отряду в очереди; очередь кончилась — новый раунд.
void Battle::advance() {
    while (!finished_) {
        if (pos_ >= order_.size()) {
            begin_round();
            return;
        }
        auto [side, idx] = order_[pos_++];
        if (live_[side][size_t(idx)].count <= 0) continue;
        side_ = side;
        idx_ = idx;
        live_[side][size_t(idx)].defending = false;  // защита держится до следующего хода отряда
        return;
    }
}

void Battle::check_end() {
    auto alive = [&](int side) {
        for (auto& l : live_[side])
            if (l.count > 0) return true;
        return false;
    };
    bool ours = alive(0), theirs = alive(1);
    if (ours && theirs) return;
    finish(ours && !theirs);
}

int Battle::hit(int side, int target, int damage) {
    Live& l = live_[side][size_t(target)];
    int hp = (side == 0 ? r_.ours : r_.theirs)[size_t(target)].hp;
    int total = (l.count - 1) * hp + l.top - damage;
    int before = l.count;
    if (total <= 0) {
        l.count = 0;
        l.top = 0;
    } else {
        l.count = (total + hp - 1) / hp;
        l.top = total - (l.count - 1) * hp;
    }
    if (side == 0) r_.our_losses[size_t(target)] += before - l.count;
    return before - l.count;
}

bool Battle::occupied(int cell, int ignore_side, int ignore_idx) const {
    if (rock_[size_t(cell)]) return true;
    for (int side = 0; side < 2; ++side)
        for (size_t i = 0; i < cell_[side].size(); ++i)
            if (cell_[side][i] == cell && live_[side][i].count > 0 && !(side == ignore_side && int(i) == ignore_idx)) return true;
    return false;
}

// Шаги до каждой клетки без учёта скорости. Летуны проходят над всем.
std::vector<int> Battle::bfs(int side, int idx, std::vector<int>* parent) const {
    std::vector<int> dist(kCells, -1);
    if (parent) parent->assign(kCells, -1);
    bool fly = (side == 0 ? r_.ours : r_.theirs)[size_t(idx)].flying;
    int start = cell_[side][size_t(idx)];
    std::vector<int> queue = {start};
    dist[size_t(start)] = 0;
    for (size_t q = 0; q < queue.size(); ++q) {
        int c = queue[q];
        for (int n : hex_neighbors(c)) {
            if (dist[size_t(n)] >= 0) continue;
            if (!fly && occupied(n, side, idx)) continue;
            dist[size_t(n)] = dist[size_t(c)] + 1;
            if (parent) (*parent)[size_t(n)] = c;
            queue.push_back(n);
        }
    }
    return dist;
}

std::vector<int> Battle::reachable() const {
    std::vector<int> out(kCells, -1);
    if (finished_) return out;
    std::vector<int> dist = bfs(side_, idx_, nullptr);
    int speed = (side_ == 0 ? r_.ours : r_.theirs)[size_t(idx_)].speed;
    for (int c = 0; c < kCells; ++c)
        if (dist[size_t(c)] >= 0 && dist[size_t(c)] <= speed && (dist[size_t(c)] == 0 || !occupied(c))) out[size_t(c)] = dist[size_t(c)];
    return out;
}

std::vector<int> Battle::path_to(int cell) const {
    std::vector<int> parent;
    std::vector<int> dist = bfs(side_, idx_, &parent);
    std::vector<int> path;
    if (cell < 0 || cell >= kCells || dist[size_t(cell)] < 0) return path;
    for (int c = cell; c >= 0; c = parent[size_t(c)]) path.push_back(c);
    std::reverse(path.begin(), path.end());
    return path;
}

bool Battle::enemy_adjacent(int side, int idx) const {
    int other = 1 - side;
    for (size_t i = 0; i < cell_[other].size(); ++i)
        if (live_[other][i].count > 0 && hex_distance(cell_[side][size_t(idx)], cell_[other][i]) == 1) return true;
    return false;
}

bool Battle::can_shoot() const {
    if (finished_) return false;
    const Stack& a = (side_ == 0 ? r_.ours : r_.theirs)[size_t(idx_)];
    return a.ranged && !enemy_adjacent(side_, idx_);
}

int Battle::attack_cell(int target, int prefer) const {
    int other = 1 - side_;
    if (finished_ || target < 0 || target >= int(cell_[other].size()) || live_[other][size_t(target)].count <= 0) return -1;
    int tc = cell_[other][size_t(target)], my = cell_[side_][size_t(idx_)];
    std::vector<int> reach = reachable();
    int best = -1;
    for (int n : hex_neighbors(tc)) {
        if (reach[size_t(n)] < 0) continue;
        if (n == prefer) return n;
        if (best < 0 || reach[size_t(n)] < reach[size_t(best)] || (n == my)) best = n;
    }
    return best;
}

double Battle::charge_bonus(int side, int actor, int steps) const {
    const Stack& a = (side == 0 ? r_.ours : r_.theirs)[size_t(actor)];
    return a.charge ? 1.0 + 0.07 * std::max(0, steps) : 1.0;
}

bool Battle::shot_half(int side, int actor, int target) const {
    const Stack& a = (side == 0 ? r_.ours : r_.theirs)[size_t(actor)];
    return !a.long_range && hex_distance(cell_[side][size_t(actor)], cell_[1 - side][size_t(target)]) > kFullRange;
}

double Battle::raw_damage(int side, int actor, int target, int roll_value, bool half, double mult) const {
    const Stack& a = (side == 0 ? r_.ours : r_.theirs)[size_t(actor)];
    const Stack& d = (side == 0 ? r_.theirs : r_.ours)[size_t(target)];
    int other = 1 - side;
    if (other == 1 && shielded_[size_t(target)]) return 0;
    double dmg = double(live_[side][size_t(actor)].count) * roll_value * mult;
    dmg *= std::clamp(1.0 + 0.05 * (a.atk - d.def), 0.3, 3.0);
    if (side == 1) dmg *= 1.0 - std::min(5, army_.armor) * 0.08;
    if (live_[other][size_t(target)].defending) dmg *= 0.6;
    if (half) dmg *= 0.5;
    return dmg;
}

Battle::Estimate Battle::estimate(int target, int from) const {
    Estimate e{0, 0, 0, 0, false, false, false, false};
    if (finished_) return e;
    const Stack& a = (side_ == 0 ? r_.ours : r_.theirs)[size_t(idx_)];
    int other = 1 - side_;
    const Stack& d = (other == 0 ? r_.ours : r_.theirs)[size_t(target)];
    bool shoot = can_shoot();
    int cell = shoot ? -1 : attack_cell(target, from);
    e.reach = shoot || cell >= 0;
    e.half = shoot ? shot_half(side_, idx_, target) : a.ranged;
    e.immune = other == 1 && shielded_[size_t(target)];
    double mult = cell >= 0 ? charge_bonus(side_, idx_, reachable()[size_t(cell)]) : 1.0;
    e.dmg_min = int(std::lround(raw_damage(side_, idx_, target, a.dmg_min, e.half, mult)));
    e.dmg_max = int(std::lround(raw_damage(side_, idx_, target, a.dmg_max, e.half, mult)));
    const Live& l = live_[other][size_t(target)];
    auto kills = [&](int dmg) {
        int total = (l.count - 1) * d.hp + l.top - dmg;
        if (total <= 0) return l.count;
        return l.count - (total + d.hp - 1) / d.hp;
    };
    e.kills_min = kills(e.dmg_min);
    e.kills_max = kills(e.dmg_max);
    e.counter = !shoot && !a.no_retaliation && !l.retaliated && e.kills_max < l.count;
    return e;
}

std::vector<int> Battle::castable() const {
    std::vector<int> out;
    if (!can_cast()) return out;
    if (army_.spell_levels.size() > 0 && army_.spell_levels[0] > 0) out.push_back(0);  // огненная стрела
    if (enemy_.ward >= 0 && size_t(enemy_.ward) < army_.spell_levels.size() && army_.spell_levels[size_t(enemy_.ward)] > 0 &&
        std::ranges::any_of(shielded_, [](bool b) { return b; }) && enemy_.ward != 0)
        out.push_back(enemy_.ward);
    return out;
}

void Battle::move(int cell) {
    if (finished_ || cell < 0 || cell >= kCells) return;
    std::vector<int> reach = reachable();
    if (reach[size_t(cell)] <= 0) return;
    BattleAction a = action(BattleAction::Move, side_, idx_, idx_, 0, 0);
    a.path = path_to(cell);
    r_.actions.push_back(a);
    cell_[side_][size_t(idx_)] = cell;
    advance();
}

void Battle::attack(int target, int from) {
    int side = side_, actor = idx_, other = 1 - side;
    if (finished_ || target < 0 || target >= int(live_[other].size()) || live_[other][size_t(target)].count <= 0) return;
    const Stack& a = (side == 0 ? r_.ours : r_.theirs)[size_t(actor)];
    bool shoot = can_shoot();
    double mult = 1.0;
    if (!shoot) {
        int c = attack_cell(target, from);
        if (c < 0) return;  // не дойти в этот ход
        mult = charge_bonus(side, actor, reachable()[size_t(c)]);
        if (c != cell_[side][size_t(actor)]) {
            BattleAction m = action(BattleAction::Move, side, actor, actor, 0, 0);
            m.path = path_to(c);
            r_.actions.push_back(m);
            cell_[side][size_t(actor)] = c;
        }
    }
    bool half = shoot ? shot_half(side, actor, target) : a.ranged;
    int dmg = int(std::lround(raw_damage(side, actor, target, roll(a.dmg_min, a.dmg_max), half, mult)));
    if (dmg > 0) dmg = std::max(1, dmg);
    BattleAction strike = action(shoot ? BattleAction::Ranged : BattleAction::Melee, side, actor, target, dmg, hit(other, target, dmg));
    strike.half = half;
    r_.actions.push_back(strike);

    // Ответный удар: раз за раунд, только на рукопашную атаку и не на удар «без ответа».
    Live& t = live_[other][size_t(target)];
    if (!shoot && !a.no_retaliation && t.count > 0 && !t.retaliated && live_[side][size_t(actor)].count > 0) {
        t.retaliated = true;
        const Stack& d = (other == 0 ? r_.ours : r_.theirs)[size_t(target)];
        int back = int(std::lround(raw_damage(other, target, actor, roll(d.dmg_min, d.dmg_max), d.ranged)));
        if (back > 0) back = std::max(1, back);
        BattleAction c = action(BattleAction::Melee, other, target, actor, back, hit(side, actor, back), -1, true);
        c.half = d.ranged;
        r_.actions.push_back(c);
    }
    check_end();
    advance();
}

void Battle::defend() {
    if (finished_) return;
    live_[side_][size_t(idx_)].defending = true;
    r_.actions.push_back(action(BattleAction::Defend, side_, idx_, idx_, 0, 0));
    advance();  // флаг защиты снимется, когда до отряда снова дойдёт ход
}

void Battle::cast(int spell, int target) {
    if (!can_cast() || target < 0 || target >= int(live_[1].size()) || live_[1][size_t(target)].count <= 0) return;
    auto list = castable();
    if (std::ranges::find(list, spell) == list.end()) return;
    cast_this_round_ = true;
    if (spell == enemy_.ward && spell != 0) {
        for (size_t i = 0; i < shielded_.size(); ++i)
            if (shielded_[i]) {
                shielded_[i] = false;
                r_.actions.push_back(action(BattleAction::ShieldBreak, 0, -1, int(i), 0, 0, spell));
            }
        return;
    }
    if (spell == 0) {
        // Огонь сжигает и чары, которые боятся огня (если враг защищён именно огнём).
        if (enemy_.ward == 0 && shielded_[size_t(target)]) {
            for (size_t i = 0; i < shielded_.size(); ++i)
                if (shielded_[i]) {
                    shielded_[i] = false;
                    r_.actions.push_back(action(BattleAction::ShieldBreak, 0, -1, int(i), 0, 0, 0));
                }
            return;
        }
        if (shielded_[size_t(target)]) return;
        int dmg = 10 * army_.spell_levels[0] * (2 + army_.power) / 2 + roll(0, 6);
        r_.actions.push_back(action(BattleAction::Spell, 0, -1, target, dmg, hit(1, target, dmg), 0));
        check_end();
    }
}

void Battle::retreat() {
    if (!finished_) finish(false);
}

// ИИ: цель, по которой удар принесёт больше всего пользы, — убитая сила врага.
int Battle::choose_target(int side, int actor, bool attackable_only) const {
    int other = side == 0 ? 1 : 0;
    const auto& stacks = other == 0 ? r_.ours : r_.theirs;
    bool shoot = actor >= 0 && side == side_ && actor == idx_ && can_shoot();
    int best = -1;
    double best_score = -1;
    for (size_t i = 0; i < live_[other].size(); ++i) {
        const Live& l = live_[other][i];
        if (l.count <= 0) continue;
        if (other == 1 && shielded_[i]) continue;
        if (attackable_only && !shoot && attack_cell(int(i), -1) < 0) continue;
        const Stack& s = stacks[i];
        double unit_value = s.hp + (s.dmg_min + s.dmg_max) * 1.5 + s.atk * 2 + s.def * 2;
        double score = unit_value * std::min(double(l.count), 3.0) + (s.ranged ? unit_value : 0);
        if (actor >= 0) {
            const Stack& a = (side == 0 ? r_.ours : r_.theirs)[size_t(actor)];
            bool half = shoot ? shot_half(side, actor, int(i)) : a.ranged;
            double dmg = raw_damage(side, actor, int(i), (a.dmg_min + a.dmg_max) / 2, half);
            double killed = std::min(double(l.count), dmg / std::max(1, s.hp));
            score = killed * unit_value + (s.ranged ? unit_value * 0.6 : 0);
            if (!shoot && !l.retaliated && !a.no_retaliation) score *= 0.85;  // ответный удар
        }
        if (score > best_score) {
            best_score = score;
            best = int(i);
        }
    }
    if (best < 0 && !attackable_only)
        for (size_t i = 0; i < live_[other].size(); ++i)
            if (live_[other][i].count > 0) return int(i);
    return best;
}

void Battle::auto_turn() {
    if (finished_) return;
    if (side_ == 0 && can_cast()) {
        auto list = castable();
        // Сначала снять чары с предводителя, потом жечь сильнейший отряд.
        int ward = enemy_.ward;
        bool shielded_left = std::ranges::any_of(shielded_, [](bool b) { return b; });
        if (shielded_left && ward >= 0 && std::ranges::find(list, ward) != list.end()) {
            for (size_t i = 0; i < shielded_.size(); ++i)
                if (shielded_[i]) {
                    cast(ward, int(i));
                    break;
                }
        } else if (std::ranges::find(list, 0) != list.end()) {
            int t = choose_target(0, -1, false);
            if (t >= 0) cast(0, t);
        }
        if (finished_) return;
    }
    // Стреляем или бьём того, до кого можно дойти в этот ход.
    int t = choose_target(side_, idx_, true);
    if (t >= 0) {
        // Отряд с натиском разгоняется: бьёт с самой дальней клетки рядом с целью.
        int from = -1;
        if ((side_ == 0 ? r_.ours : r_.theirs)[size_t(idx_)].charge && !can_shoot()) {
            std::vector<int> reach = reachable();
            for (int n : hex_neighbors(cell_[1 - side_][size_t(t)]))
                if (reach[size_t(n)] >= 0 && (from < 0 || reach[size_t(n)] > reach[size_t(from)])) from = n;
        }
        attack(t, from);
        return;
    }
    // Иначе идём к ближайшему врагу, насколько хватает скорости.
    std::vector<int> parent;
    std::vector<int> dist = bfs(side_, idx_, &parent);
    int other = 1 - side_;
    int goal = -1;
    for (size_t i = 0; i < cell_[other].size(); ++i) {
        if (live_[other][i].count <= 0) continue;
        for (int n : hex_neighbors(cell_[other][i])) {
            if (dist[size_t(n)] < 0 || occupied(n, side_, idx_)) continue;
            if (goal < 0 || dist[size_t(n)] < dist[size_t(goal)]) goal = n;
        }
    }
    if (goal < 0) {
        defend();
        return;
    }
    std::vector<int> path;
    for (int c = goal; c >= 0; c = parent[size_t(c)]) path.push_back(c);
    std::reverse(path.begin(), path.end());
    int speed = (side_ == 0 ? r_.ours : r_.theirs)[size_t(idx_)].speed;
    int stop = -1;
    for (size_t k = 1; k < path.size() && int(k) <= speed; ++k)
        if (!occupied(path[k])) stop = path[k];
    if (stop < 0) defend();
    else move(stop);
}

BattleResult simulate(const Army& army, const Enemy& enemy, uint32_t seed) {
    Battle b(army, enemy, seed);
    for (int guard = 0; guard < 5000 && !b.finished(); ++guard) b.auto_turn();
    return b.result();
}
// ---------------------------------------------------------------------------
// Мир

bool Frontier::ready() const {
    for (auto& r : requirements)
        if (!r.met && !r.advisory) return false;
    return true;
}

const Frontier* World::frontier(const std::string& id) const {
    for (auto& f : frontiers)
        if (f.id == id) return &f;
    return nullptr;
}

const Frontier* World::find(const std::string& s) const {
    for (auto& f : frontiers)
        if (f.label == s) return &f;
    return frontier(s);
}

const Frontier* World::next() const {
    for (auto& f : frontiers)
        if (f.status == Frontier::Open) return &f;
    return nullptr;
}

int World::army_count(int unit, bool elite) const {
    for (auto& s : army.stacks)
        if (s.unit == unit && s.elite == elite) return s.count;
    return 0;
}

World build_world(const Config& cfg, const State& st, const EventLog& game, const CampaignDef& campaign) {
    (void)cfg;
    World w;
    w.campaign_id = campaign.id;
    w.campaign_title = campaign.title;
    w.level = level_of(st.earned.xp).level;
    w.gold = st.earned.gold;
    w.army.spell_levels.assign(spells().size(), 0);

    auto frontier_def = [&](const std::string& id) -> const FrontierDef* {
        for (auto& d : campaign.frontiers)
            if (d.id == id) return &d;
        return nullptr;
    };

    // --- Игровой журнал. Золото, армия, кузня и магия общие; рубежи — только этой кампании.
    std::map<std::pair<int, bool>, int> army;
    std::set<std::string> won;
    struct Timed {
        std::string time;
        Chronicle c;
    };
    std::vector<Timed> items;
    auto add = [&](const std::string& time, std::string text, Chronicle::Tone tone, std::string title = "") {
        items.push_back({time, {time, std::move(text), tone, std::move(title)}});
    };
    std::string last_victory_time;
    std::set<std::string> legacy_won;

    auto spend = [&](const Event& e) { w.gold -= std::atoi(e.get("gold").c_str()); };
    for (auto& e : game.all()) {
        const std::string& t = e.type;
        if (t == "hire") {
            int u = unit_index(e.get("unit"));
            int n = std::atoi(e.get("count").c_str());
            if (u >= 0) {
                army[{u, false}] += n;
                add(e.time, std::format("Под знамёна встали новые бойцы: {} × {}.", unit_types()[size_t(u)].name, n), Chronicle::Story);
            }
            spend(e);
        } else if (t == "upgrade") {
            int u = unit_index(e.get("unit"));
            int n = std::atoi(e.get("count").c_str());
            if (u >= 0) {
                n = std::min(n, army[{u, false}]);
                army[{u, false}] -= n;
                army[{u, true}] += n;
                add(e.time, std::format("{} × {} прошли обучение: теперь это {}.", unit_types()[size_t(u)].name, n, unit_types()[size_t(u)].elite_name), Chronicle::Story);
            }
            spend(e);
        } else if (t == "forge") {
            int i = forge_index(e.get("item"));
            int lvl = std::atoi(e.get("level").c_str());
            if (i == 0) w.army.armor = std::max(w.army.armor, lvl);
            if (i == 1) w.army.weapon = std::max(w.army.weapon, lvl);
            if (i == 2) w.army.walls = std::max(w.army.walls, lvl);
            if (i >= 0) add(e.time, std::format("Кузня Цитадели: {} {}-го уровня.", forge_items()[size_t(i)].name, lvl), Chronicle::Story);
            spend(e);
        } else if (t == "learn") {
            int s = spell_index(e.get("spell"));
            int lvl = std::atoi(e.get("level").c_str());
            if (s >= 0) {
                w.army.spell_levels[size_t(s)] = std::max(w.army.spell_levels[size_t(s)], lvl);
                add(e.time, std::format("Башня магов: изучено заклинание «{}»{}.", spells()[size_t(s)].name, lvl > 1 ? std::format(", уровень {}", lvl) : ""), Chronicle::Story);
            }
            spend(e);
        } else if (t == "gift") {
            int g = std::atoi(e.get("gold").c_str());
            w.gold += g;
            std::string why = e.get("reason");
            add(e.time, std::format("Дар Цитадели: {:+} золота{}.", g, why.empty() ? "" : " — " + why), Chronicle::Story);
        } else if (t == "battle") {
            // Потери армии считаются в любой кампании: армия у героя одна.
            for (auto& item : split(e.get("losses"), ',')) {
                auto parts = split(item, ':');
                if (parts.size() != 3) continue;
                int u = unit_index(parts[0]);
                if (u < 0) continue;
                int& cnt = army[{u, parts[1] == "1"}];
                cnt = std::max(0, cnt - std::atoi(parts[2].c_str()));
            }
            // Добыча — тоже в любой кампании, иначе золото менялось бы при переключении.
            std::string where = e.get("campaign").empty() ? "shield" : e.get("campaign");
            bool victory = e.get("won") == "1";
            if (victory && !e.get("loot").empty()) {
                w.gold += std::atoi(e.get("loot").c_str());
            } else if (victory && legacy_won.insert(e.get("frontier")).second) {
                // Записи версии 0.3: ни кампании, ни добычи — это встроенная кампания, добыча из её описания.
                for (auto& d : builtin().frontiers)
                    if (d.id == e.get("frontier")) w.gold += d.gold;
            }
            if (where != campaign.id) continue;

            std::string id = e.get("frontier");
            const FrontierDef* def = frontier_def(id);
            if (!def) continue;
            if (victory && won.insert(id).second) {
                last_victory_time = e.time;
                // Взятый рубеж — сюжетная сцена летописи: заголовок рубежа, итог боя, продолжение истории.
                add(e.time, std::format("{}{}{} Добыча: {} золота.", def->victory, def->story.empty() ? "" : " ", def->story, def->gold), Chronicle::Good,
                    std::format("{} · {}", def->label, def->name));
                // Взят последний рубеж главы — начинается следующая.
                bool last_in_chapter = true;
                bool after = false;
                for (auto& d : campaign.frontiers) {
                    if (after && d.chapter == def->chapter) last_in_chapter = false;
                    if (d.id == id) after = true;
                }
                size_t ch = size_t(def->chapter) + 1;
                if (last_in_chapter && ch < campaign.chapters.size())
                    add(e.time, campaign.chapters[ch].intro, Chronicle::Chapter, std::format("Глава {}. {}", ch + 1, campaign.chapters[ch].name));
            } else if (!victory) {
                add(e.time, std::format("Рубеж {} «{}» не взят: войска отступили. Нужно больше сил.", id, def->name), Chronicle::Bad);
            }
        }
    }
    for (auto& [key, cnt] : army)
        if (cnt > 0) w.army.stacks.push_back(player_stack(key.first, key.second, cnt));

    // --- Бонусы героя: уровень, кузня, магия, трофеи.
    int art_atk = 0, art_def = 0, art_power = 0;
    for (auto& name : st.artifacts)
        if (auto* a = find_artifact(name)) {
            art_atk += a->atk;
            art_def += a->def;
            art_power += a->power;
        }
    // Уровень героя — сила армии: +1 к атаке и защите за каждые два уровня, +4% здоровья за уровень.
    w.army.atk_bonus = w.level / 2 + 2 * w.army.weapon + art_atk;
    w.army.def_bonus = w.level / 2 + 2 * w.army.spell_levels[1] + art_def;
    w.army.hp_pct = 4 * (w.level - 1);
    w.army.power = 1 + w.level / 4 + art_power;

    // --- Главы и рубежи.
    w.chapters = campaign.chapters;
    bool previous_cleared = true;
    for (auto& d : campaign.frontiers) {
        Frontier f;
        f.id = d.id;
        f.label = d.label;
        f.chapter = d.chapter;
        f.name = d.name;
        f.intro = d.intro;
        f.victory = d.victory;
        f.story = d.story;
        f.min_level = d.level;
        f.armor = d.armor;
        f.weapon = d.weapon;
        f.walls = d.walls;
        f.spell = d.spell;
        f.enemy.name = d.name;
        f.enemy.stacks = d.enemies;
        bool boss = false;
        for (auto& s : f.enemy.stacks) {
            s.color = w.chapters[size_t(d.chapter)].color;
            boss |= s.icon == "boss";
        }
        f.enemy.defense = d.defense;
        f.enemy.ward = boss ? d.spell : -1;  // требуемое заклинание снимает чары с предводителя
        f.loot.gold = d.gold;
        f.status = won.count(d.id) ? Frontier::Cleared : previous_cleared ? Frontier::Open : Frontier::Locked;
        previous_cleared = f.status == Frontier::Cleared;
        if (f.status != Frontier::Locked) w.chapters[size_t(d.chapter)].reached = true;

        f.requirements.push_back({std::format("Рекомендуемый уровень героя {}", d.level), w.level >= d.level, true});
        if (d.armor) f.requirements.push_back({std::format("Броня {}-го уровня", d.armor), w.army.armor >= d.armor, false});
        if (d.weapon) f.requirements.push_back({std::format("Оружие {}-го уровня", d.weapon), w.army.weapon >= d.weapon, false});
        if (d.walls) f.requirements.push_back({std::format("Стены Цитадели {}-го уровня", d.walls), w.army.walls >= d.walls, false});
        if (d.spell >= 0)
            f.requirements.push_back({std::format("Заклинание «{}»", spells()[size_t(d.spell)].name), w.army.spell_levels[size_t(d.spell)] > 0, false});
        f.x = d.x;
        f.y = d.y;
        if (f.status == Frontier::Cleared) ++w.cleared;
        w.frontiers.push_back(f);
    }

    // --- Летопись: пролог, работа и игра вперемешку по времени, эпилог.
    std::string first = st.tasks.empty() ? (game.all().empty() ? now_iso() : game.all().front().time) : st.tasks.front().created;
    std::string prologue = campaign.prologue.empty() ? "Начинается поход: " + campaign.title + "." : campaign.prologue;
    add(first.substr(0, 10) + "T00:00:00", prologue, Chronicle::Chapter, "Пролог");
    add(first.substr(0, 10) + "T00:00:01", campaign.chapters[0].intro, Chronicle::Chapter, "Глава 1. " + campaign.chapters[0].name);
    for (auto& t : st.tasks) {
        if (t.status != Task::Done) continue;
        add(t.finished, std::format("{} «{}» выполнен{}: {}.", t.side ? "Побочный квест" : "Миссия", t.title, t.side ? "" : "а", t.rewards.describe()),
            Chronicle::Good);
    }
    if (!campaign.frontiers.empty() && w.cleared == int(campaign.frontiers.size()))
        add(last_victory_time + "~", campaign.epilogue.empty() ? "Кампания пройдена." : campaign.epilogue, Chronicle::Chapter, "Эпилог");
    std::stable_sort(items.begin(), items.end(), [](const Timed& a, const Timed& b) { return a.time < b.time; });
    for (auto& it : items) w.chronicle.push_back(it.c);
    return w;
}
// ---------------------------------------------------------------------------
// Действия

namespace {

std::string write(EventLog& game, Event e) {
    return game.append(std::move(e)) ? "" : "Не удалось записать игровой журнал";
}

std::string not_enough(int need, int have) {
    return need > have ? std::format("Не хватает {} золота: нужно {}, в казне {}", need - have, need, have) : "";
}

}  // namespace

std::string hire(const World& w, EventLog& game, int unit, int count) {
    if (unit < 0 || unit >= int(unit_types().size()) || count <= 0) return "Неверный отряд";
    const UnitType& u = unit_types()[size_t(unit)];
    if (w.level < u.level_req) return std::format("{} доступны с {} уровня героя", u.name, u.level_req);
    int cost = u.cost * count;
    if (auto err = not_enough(cost, w.gold); !err.empty()) return err;
    Event e;
    e.type = "hire";
    e.set("unit", u.id).set("count", std::to_string(count)).set("gold", std::to_string(cost));
    return write(game, std::move(e));
}

std::string upgrade(const World& w, EventLog& game, int unit, int count) {
    if (unit < 0 || unit >= int(unit_types().size()) || count <= 0) return "Неверный отряд";
    if (w.army_count(unit, false) < count) return "Столько бойцов нет в строю";
    int cost = upgrade_cost(unit) * count;
    if (auto err = not_enough(cost, w.gold); !err.empty()) return err;
    Event e;
    e.type = "upgrade";
    e.set("unit", unit_types()[size_t(unit)].id).set("count", std::to_string(count)).set("gold", std::to_string(cost));
    return write(game, std::move(e));
}

std::string forge(const World& w, EventLog& game, int item) {
    if (item < 0 || item >= int(forge_items().size())) return "Неверный предмет";
    const ForgeItem& f = forge_items()[size_t(item)];
    int current = item == 0 ? w.army.armor : item == 1 ? w.army.weapon : w.army.walls;
    if (current >= int(f.costs.size())) return std::format("{}: уже наивысший уровень", f.name);
    int cost = f.costs[size_t(current)];
    if (auto err = not_enough(cost, w.gold); !err.empty()) return err;
    Event e;
    e.type = "forge";
    e.set("item", f.id).set("level", std::to_string(current + 1)).set("gold", std::to_string(cost));
    return write(game, std::move(e));
}

std::string learn(const World& w, EventLog& game, int spell) {
    if (spell < 0 || spell >= int(spells().size())) return "Неверное заклинание";
    const Spell& s = spells()[size_t(spell)];
    int current = w.army.spell_levels[size_t(spell)];
    if (current >= int(s.costs.size())) return std::format("«{}»: уже наивысший уровень", s.name);
    int cost = s.costs[size_t(current)];
    if (auto err = not_enough(cost, w.gold); !err.empty()) return err;
    Event e;
    e.type = "learn";
    e.set("spell", s.id).set("level", std::to_string(current + 1)).set("gold", std::to_string(cost));
    return write(game, std::move(e));
}

std::string battle_blocker(const World& w, const Frontier& f) {
    if (f.status == Frontier::Cleared) return "Рубеж уже взят";
    if (f.status == Frontier::Locked) return "Сначала возьмите предыдущие рубежи";
    if (w.army.stacks.empty()) return "Нужны войска: наймите их в городе";
    std::string missing;
    for (auto& r : f.requirements)
        if (!r.met && !r.advisory) missing += (missing.empty() ? "" : ", ") + r.text;
    return missing.empty() ? "" : "Не выполнены требования: " + missing;
}

std::string reset_game(const Config& cfg, EventLog& game) {
    fs::path folder;
    std::error_code ec;
    if (fs::exists(cfg.game_file(), ec) && !make_backup(cfg, "до сброса игры", &folder)) return "Не удалось сохранить копию перед сбросом";
    fs::remove(cfg.game_file(), ec);
    if (ec) return "Не удалось удалить game.log: " + ec.message();
    game.load();
    return {};
}

std::string gift(EventLog& game, int gold, const std::string& reason) {
    if (gold == 0 || gold < -1000000 || gold > 1000000) return "Сумма — целое число, не ноль, по модулю до миллиона";
    Event e;
    e.type = "gift";
    e.set("gold", std::to_string(gold));
    if (!trim(reason).empty()) e.set("reason", trim(reason));
    return write(game, std::move(e));
}

std::unique_ptr<Battle> start_battle(const World& w, const EventLog& game, const std::string& id) {
    const Frontier* f = w.frontier(id);
    if (!f || !battle_blocker(w, *f).empty()) return nullptr;
    return std::make_unique<Battle>(w.army, f->enemy, hash(id) ^ uint32_t(game.all().size() * 7919));
}

std::unique_ptr<Battle> training_battle(const World& w, uint32_t seed) {
    // Каждый род войск — отряд примерно на 600 золота, бонусы героя — как сейчас, «Огненная стрела» есть всегда.
    Army army = w.army;
    army.stacks.clear();
    auto& units = unit_types();
    std::vector<size_t> order(units.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::ranges::stable_sort(order, [&](size_t a, size_t b) { return units[a].level_req < units[b].level_req; });
    for (size_t i : order) army.stacks.push_back(player_stack(int(i), false, std::max(2, 600 / units[i].cost)));
    if (army.spell_levels.size() < spells().size()) army.spell_levels.resize(spells().size(), 0);
    army.spell_levels[0] = std::max(1, army.spell_levels[0]);
    army.walls = 0;

    Enemy enemy;
    enemy.name = "Ристалище";
    uint32_t c = rgb(200, 96, 70);
    enemy.stacks = {
        monster("Пепельный волк", 22, 14, 6, 3, 3, 5, 7),
        monster("Болотник-лучник", 16, 11, 6, 2, 3, 5, 4, true),
        monster("Гарпия", 12, 18, 7, 4, 4, 6, 8),
        monster("Пещерный тролль", 7, 60, 10, 8, 8, 12, 4),
        monster("Страж Разлома", 1, 420, 14, 12, 20, 30, 5, false, true),
    };
    enemy.stacks[2].flying = true;
    // Враги растут вместе с героем: учебный бой должен оставаться боем.
    double grow = 1.0 + 0.08 * (w.level - 1);
    for (auto& s : enemy.stacks) {
        s.color = c;
        if (s.count > 1) s.count = int(std::lround(s.count * grow));
        else s.hp = int(std::lround(s.hp * grow));
    }
    return std::make_unique<Battle>(army, enemy, seed);
}

void record_battle(const World& w, EventLog& game, const std::string& id, const BattleResult& r) {
    const Frontier* f = w.frontier(id);
    if (!f) return;
    Event e;
    e.type = "battle";
    e.set("campaign", w.campaign_id).set("frontier", id).set("won", r.won ? "1" : "0");
    e.set("loot", std::to_string(r.won && f->status == Frontier::Open ? f->loot.gold : 0)).set("losses", join_losses(r.ours, r.our_losses));
    write(game, std::move(e));
}

BattleResult fight(const World& w, EventLog& game, const std::string& id) {
    auto battle = start_battle(w, game, id);
    if (!battle) return {};
    BattleResult r = simulate(w.army, w.frontier(id)->enemy, hash(id) ^ uint32_t(game.all().size() * 7919));
    record_battle(w, game, id, r);
    return r;
}
int celebration_tier(const Task& task) {
    return task.side ? std::clamp(task.difficulty, 1, 3) : std::clamp(task.difficulty, 1, 5) + 1;
}

std::vector<Stack> task_monsters(const Task& task) {
    // Чем больше работа, тем больше тварей, а у миссий сложности 4–5 — ещё и предводитель.
    int tier = celebration_tier(task);
    uint32_t color = tier <= 2 ? rgb(210, 90, 180) : tier <= 4 ? rgb(130, 110, 235) : rgb(232, 72, 64);
    std::vector<Stack> m;
    switch (tier) {
        case 1: m = {monster("Бесёнок Разлома", 3, 10, 0, 0, 0, 0, 1)}; break;
        case 2: m = {monster("Тень Разлома", 6, 15, 0, 0, 0, 0, 1)}; break;
        case 3: m = {monster("Тень Разлома", 5, 15, 0, 0, 0, 0, 1), monster("Гончая Разлома", 4, 20, 0, 0, 0, 0, 1)}; break;
        case 4: m = {monster("Гончая Разлома", 6, 20, 0, 0, 0, 0, 1), monster("Страж Разлома", 3, 40, 0, 0, 0, 0, 1)}; break;
        case 5:
            m = {monster("Гончая Разлома", 6, 20, 0, 0, 0, 0, 1), monster("Страж Разлома", 4, 40, 0, 0, 0, 0, 1),
                 monster("Колосс Разлома", 1, 300, 0, 0, 0, 0, 1, false, true)};
            break;
        default:
            m = {monster("Тень Разлома", 8, 15, 0, 0, 0, 0, 1), monster("Страж Разлома", 5, 40, 0, 0, 0, 0, 1),
                 monster("Исполин Бездны", 1, 500, 0, 0, 0, 0, 1, false, true)};
    }
    for (auto& s : m) s.color = color;
    return m;
}

}  // namespace sq::quest
