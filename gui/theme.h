#pragma once

// Внешний вид: палитра, шрифты, иконки и небольшие виджеты поверх ImGui.

#include "imgui.h"

#include <string>

namespace sq::gui {

// Палитра «ночная крепость»: тёмный камень и кожа, пергаментный текст, золото и бронза.
namespace col {
constexpr ImU32 bg = IM_COL32(15, 13, 18, 255);
constexpr ImU32 sidebar = IM_COL32(30, 19, 22, 255);
constexpr ImU32 panel = IM_COL32(31, 26, 31, 255);
constexpr ImU32 panel_hi = IM_COL32(50, 40, 44, 255);
constexpr ImU32 border = IM_COL32(106, 82, 52, 255);
constexpr ImU32 text = IM_COL32(242, 230, 206, 255);
constexpr ImU32 dim = IM_COL32(184, 168, 142, 255);
constexpr ImU32 faint = IM_COL32(118, 104, 90, 255);
constexpr ImU32 gold = IM_COL32(242, 192, 92, 255);
constexpr ImU32 gold_dark = IM_COL32(184, 122, 42, 255);
constexpr ImU32 teal = IM_COL32(96, 204, 142, 255);
constexpr ImU32 blue = IM_COL32(112, 164, 244, 255);
constexpr ImU32 violet = IM_COL32(176, 132, 250, 255);
constexpr ImU32 red = IM_COL32(228, 90, 74, 255);
constexpr ImU32 orange = IM_COL32(240, 148, 66, 255);
constexpr ImU32 bronze = IM_COL32(150, 112, 62, 255);
}  // namespace col

// Иконки Segoe MDL2 Assets (есть в Windows 10 и новее), подмешаны в основной шрифт.
namespace icon {
constexpr const char* home = "";
constexpr const char* flag = "";
constexpr const char* shield = "";
constexpr const char* bug = "";
constexpr const char* history = "";
constexpr const char* settings = "";
constexpr const char* sync = "";
constexpr const char* add = "";
constexpr const char* check = "";
constexpr const char* copy = "";
constexpr const char* folder = "";
constexpr const char* warning = "";
constexpr const char* bulb = "";
constexpr const char* document = "";
constexpr const char* code = "";
constexpr const char* people = "";
constexpr const char* calendar = "";
constexpr const char* star = "";
constexpr const char* edit = "";
constexpr const char* info = "";
constexpr const char* close = "";
constexpr const char* game = "";
constexpr const char* lock = "";
constexpr const char* play = "";
constexpr const char* map = "";
constexpr const char* town = "";
constexpr const char* trash = "";
constexpr const char* book = "";
}  // namespace icon

// Масштаб интерфейса по DPI монитора: все размеры в пикселях проходят через px().
extern float ui_scale;
inline float px(float v) { return v * ui_scale; }

struct Fonts {
    ImFont* regular = nullptr;
    ImFont* semibold = nullptr;
    ImFont* display = nullptr;  // книжный шрифт с засечками для заголовков, кнопок и чисел
};
extern Fonts fonts;

void load_fonts();
void setup_style(float dpi_scale);

ImU32 alpha(ImU32 c, float a);
ImVec4 vec(ImU32 c);

// Карточка с фоном панели; высота подстраивается под содержимое, если size.y == 0.
void card_begin(const char* id, ImVec2 size = ImVec2(0, 0), ImU32 bg = col::panel);
void card_end(ImU32 accent = 0);  // accent — цветная полоса слева

void heading(const char* text, float size = 22.0f);
void subheading(const char* text);
void dim_text(const char* fmt, ...) IM_FMTARGS(1);
void colored_text(ImU32 c, const char* text);
void wrapped_dim(const std::string& text);

void bar(float fraction, ImVec2 size, ImU32 from, ImU32 to, ImU32 track = col::panel_hi);
void pill(const char* text, ImU32 color, bool filled = false);
bool accent_button(const char* label, ImVec2 size = ImVec2(0, 0), ImU32 color = col::gold);
float button_width(const char* label);  // ширина accent_button по надписи
bool icon_button(const char* icon, const char* tooltip, ImU32 color = col::dim);
void tooltip(const char* text);

// Квадратная плитка с подписью (трофеи).
void tile(const char* big, const char* small, ImVec2 size, bool lit, ImU32 color, const char* tip = nullptr);

// Игровые украшения, рисуются прямо в ImDrawList.
void ornate_frame(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 edge, float rounding);  // двойная кромка и уголки
void diamond(ImDrawList* dl, ImVec2 c, float r, ImU32 fill);
void coin(ImDrawList* dl, ImVec2 c, float r);
void xp_gem(ImDrawList* dl, ImVec2 c, float r);
void flourish(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 color);  // разделитель —◆—
void page_backdrop(ImDrawList* dl, ImVec2 a, ImVec2 b);         // ночное небо, горы и крепость за страницами
// Число с иконкой монеты или кристалла опыта в строке текста.
void stat_inline(bool gold, const char* text, ImU32 color = 0);

}  // namespace sq::gui
