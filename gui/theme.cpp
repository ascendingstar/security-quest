#include "theme.h"

#include "platform.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <vector>

namespace sq::gui {

Fonts fonts;
float ui_scale = 1.0f;

namespace {

ImFont* load_family(const fs::path& dir, const char* main_file) {
    ImGuiIO& io = ImGui::GetIO();
    std::error_code ec;
    fs::path main = dir / main_file;
    if (!fs::exists(main, ec)) return nullptr;
    ImFont* font = io.Fonts->AddFontFromFileTTF(from_path(main).c_str(), 18.0f);
    if (!font) return nullptr;

    // Символы (✓ ★ •) и иконки MDL2 подмешиваются в тот же шрифт.
    for (const char* extra : {"seguisym.ttf", "segmdl2.ttf"}) {
        fs::path p = dir / extra;
        if (!fs::exists(p, ec)) continue;
        ImFontConfig cfg;
        cfg.MergeMode = true;
        if (std::string(extra) == "segmdl2.ttf") cfg.GlyphOffset = ImVec2(0, 3);
        io.Fonts->AddFontFromFileTTF(from_path(p).c_str(), 18.0f, &cfg);
    }
    return font;
}

}  // namespace

void load_fonts() {
    fs::path dir = to_path(getenv_u8("WINDIR")) / "Fonts";
    fonts.regular = load_family(dir, "segoeui.ttf");
    fonts.semibold = load_family(dir, "seguisb.ttf");
    // Заголовки — Palatino, Georgia или Constantia: что найдётся (все с кириллицей, есть в Windows 10).
    for (const char* f : {"palab.ttf", "georgiab.ttf", "constanb.ttf"})
        if (!fonts.display) fonts.display = load_family(dir, f);
    if (!fonts.regular) fonts.regular = ImGui::GetIO().Fonts->AddFontDefault();
    if (!fonts.semibold) fonts.semibold = fonts.regular;
    if (!fonts.display) fonts.display = fonts.semibold;
}

ImU32 alpha(ImU32 c, float a) {
    ImVec4 v = ImGui::ColorConvertU32ToFloat4(c);
    v.w *= a;
    return ImGui::ColorConvertFloat4ToU32(v);
}

ImVec4 vec(ImU32 c) {
    return ImGui::ColorConvertU32ToFloat4(c);
}

void setup_style(float dpi_scale) {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowPadding = ImVec2(18, 16);
    s.FramePadding = ImVec2(10, 6);
    s.ItemSpacing = ImVec2(10, 8);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.CellPadding = ImVec2(8, 6);
    s.ScrollbarSize = 12;
    s.WindowRounding = 6;
    s.ChildRounding = 5;
    s.FrameRounding = 3;
    s.PopupRounding = 5;
    s.ScrollbarRounding = 3;
    s.GrabRounding = 3;
    s.TabRounding = 3;
    s.WindowBorderSize = 0;
    s.ChildBorderSize = 1;
    s.PopupBorderSize = 1.5f;
    s.FrameBorderSize = 1;
    s.FontSizeBase = 16.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = vec(col::text);
    c[ImGuiCol_TextDisabled] = vec(col::faint);
    c[ImGuiCol_WindowBg] = vec(col::bg);
    c[ImGuiCol_ChildBg] = vec(col::panel);
    c[ImGuiCol_PopupBg] = vec(IM_COL32(34, 26, 30, 252));
    c[ImGuiCol_Border] = vec(alpha(col::border, 0.85f));
    c[ImGuiCol_FrameBg] = vec(IM_COL32(14, 11, 14, 235));
    c[ImGuiCol_FrameBgHovered] = vec(IM_COL32(22, 17, 20, 240));
    c[ImGuiCol_FrameBgActive] = vec(IM_COL32(28, 22, 24, 245));
    c[ImGuiCol_TitleBg] = vec(IM_COL32(58, 26, 30, 255));
    c[ImGuiCol_TitleBgActive] = vec(IM_COL32(78, 32, 34, 255));
    c[ImGuiCol_ScrollbarBg] = vec(IM_COL32(0, 0, 0, 0));
    c[ImGuiCol_ScrollbarGrab] = vec(col::border);
    c[ImGuiCol_ScrollbarGrabHovered] = vec(col::bronze);
    c[ImGuiCol_ScrollbarGrabActive] = vec(col::gold_dark);
    c[ImGuiCol_CheckMark] = vec(col::gold);
    c[ImGuiCol_SliderGrab] = vec(col::gold);
    c[ImGuiCol_SliderGrabActive] = vec(col::gold_dark);
    c[ImGuiCol_Button] = vec(IM_COL32(60, 42, 38, 255));
    c[ImGuiCol_ButtonHovered] = vec(IM_COL32(82, 58, 48, 255));
    c[ImGuiCol_ButtonActive] = vec(IM_COL32(100, 70, 52, 255));
    c[ImGuiCol_Header] = vec(IM_COL32(72, 50, 42, 255));
    c[ImGuiCol_HeaderHovered] = vec(IM_COL32(86, 60, 48, 255));
    c[ImGuiCol_HeaderActive] = vec(IM_COL32(100, 70, 52, 255));
    c[ImGuiCol_Separator] = vec(alpha(col::border, 0.7f));
    c[ImGuiCol_TableHeaderBg] = vec(IM_COL32(56, 38, 36, 255));
    c[ImGuiCol_TableBorderStrong] = vec(col::border);
    c[ImGuiCol_TableBorderLight] = vec(IM_COL32(62, 50, 42, 255));
    c[ImGuiCol_TableRowBg] = vec(IM_COL32(0, 0, 0, 0));
    c[ImGuiCol_TableRowBgAlt] = vec(IM_COL32(255, 230, 190, 7));
    c[ImGuiCol_TextSelectedBg] = vec(alpha(col::gold, 0.35f));
    c[ImGuiCol_ModalWindowDimBg] = vec(IM_COL32(6, 4, 8, 180));
    c[ImGuiCol_NavCursor] = vec(col::gold);

    s.ScaleAllSizes(dpi_scale);
    s.FontScaleDpi = dpi_scale;
    ui_scale = dpi_scale;
}

void card_begin(const char* id, ImVec2 size, ImU32 bg) {
    // Панель полупрозрачна: сквозь неё чуть виден ночной пейзаж страницы.
    ImGui::PushStyleColor(ImGuiCol_ChildBg, alpha(bg, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, alpha(col::border, 0.9f));
    ImGuiChildFlags flags = ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding;
    if (size.y == 0) flags |= ImGuiChildFlags_AutoResizeY;
    ImGui::BeginChild(id, size, flags, ImGuiWindowFlags_NoScrollbar);
    // Каменный градиент рисуется первым, под содержимым. Размер — с прошлого кадра, этого достаточно.
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
    float top = std::min(s.y, px(56));
    dl->AddRectFilledMultiColor(ImVec2(p.x + 1, p.y + 1), ImVec2(p.x + s.x - 1, p.y + top), IM_COL32(255, 228, 180, 14),
                                IM_COL32(255, 228, 180, 14), IM_COL32(255, 228, 180, 0), IM_COL32(255, 228, 180, 0));
}

void card_end(ImU32 accent) {
    // Внутренняя кромка, бронзовые уголки и, если задан, цветной корешок слева — поверх содержимого.
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
    dl->PushClipRect(p, ImVec2(p.x + s.x, p.y + s.y), false);
    ornate_frame(dl, p, ImVec2(p.x + s.x, p.y + s.y), col::border, ImGui::GetStyle().ChildRounding);
    if (accent) {
        float y0 = p.y + px(6), y1 = p.y + s.y - px(6);
        dl->AddRectFilledMultiColor(ImVec2(p.x + 1, y0), ImVec2(p.x + px(5), y1), accent, alpha(accent, 0.2f), alpha(accent, 0.2f), accent);
    }
    dl->PopClipRect();
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

void heading(const char* text, float size) {
    ImGui::PushFont(fonts.display, size + 2.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, col::gold);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    // Справа от заголовка — золотой росчерк, который тает к краю.
    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;  // край колонки или окна
    float y = (a.y + b.y) * 0.5f + px(1);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float x0 = b.x + px(12), x1 = std::min(right, x0 + px(320));
    if (x1 > x0 + px(20)) {
        diamond(dl, ImVec2(x0, y), px(3.5f), col::gold);
        dl->AddRectFilledMultiColor(ImVec2(x0 + px(6), y - px(0.5f)), ImVec2(x1, y + px(0.5f)), alpha(col::gold, 0.7f), alpha(col::gold, 0),
                                    alpha(col::gold, 0), alpha(col::gold, 0.7f));
    }
    ImGui::Dummy(ImVec2(0, px(2)));
}

void subheading(const char* text) {
    ImGui::PushFont(fonts.display, 18.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopFont();
}

void dim_text(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ImGui::PushStyleColor(ImGuiCol_Text, col::dim);
    ImGui::TextV(fmt, args);
    ImGui::PopStyleColor();
    va_end(args);
}

void colored_text(ImU32 c, const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, c);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

void wrapped_dim(const std::string& text) {
    ImGui::PushStyleColor(ImGuiCol_Text, col::dim);
    ImGui::TextWrapped("%s", text.c_str());
    ImGui::PopStyleColor();
}

void bar(float fraction, ImVec2 size, ImU32 from, ImU32 to, ImU32 track) {
    if (size.x <= 0) size.x = ImGui::GetContentRegionAvail().x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float r = std::min(size.y * 0.5f, px(3));
    ImVec2 q(p.x + size.x, p.y + size.y);
    dl->AddRectFilled(p, q, IM_COL32(8, 6, 8, 230), r);
    dl->AddRectFilled(ImVec2(p.x + 1, p.y + 1), ImVec2(q.x - 1, q.y - 1), alpha(track, 0.6f), r);
    float w = size.x * (fraction < 0 ? 0 : fraction > 1 ? 1 : fraction);
    if (w > 2) {
        // Заливка: светлый верх цветом `to`, тёмный низ цветом `from`, блик по кромке.
        float x1 = p.x + std::max(w, size.y) - 1;
        dl->AddRectFilledMultiColor(ImVec2(p.x + 1, p.y + 1), ImVec2(x1, q.y - 1), to, to, from, from);
        dl->AddLine(ImVec2(p.x + 2, p.y + 1.5f), ImVec2(x1 - 1, p.y + 1.5f), IM_COL32(255, 255, 255, 70), 1.0f);
    }
    dl->AddRect(p, q, alpha(col::border, 0.9f), r, 1.0f);
    ImGui::Dummy(size);
}

void pill(const char* text, ImU32 color, bool filled) {
    ImVec2 ts = ImGui::CalcTextSize(text);
    ImVec2 pad(8, 2);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 size(ts.x + pad.x * 2, ts.y + pad.y * 2);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), filled ? color : alpha(color, 0.16f), size.y * 0.5f);
    dl->AddText(ImVec2(p.x + pad.x, p.y + pad.y), filled ? col::bg : color, text);
    ImGui::Dummy(size);
}

namespace {

ImU32 shade(ImU32 c, float t) {  // t > 0 светлее, t < 0 темнее
    ImVec4 v = ImGui::ColorConvertU32ToFloat4(c);
    float k = std::abs(t), to = t > 0 ? 1.0f : 0.0f;
    return ImGui::ColorConvertFloat4ToU32(ImVec4(v.x + (to - v.x) * k, v.y + (to - v.y) * k, v.z + (to - v.z) * k, v.w));
}

}  // namespace

float button_width(const char* label) {
    const char* end = label;
    while (*end && !(end[0] == '#' && end[1] == '#')) ++end;
    return fonts.display->CalcTextSizeA(ImGui::GetFontSize() * 1.06f, FLT_MAX, 0, label, end).x + ImGui::GetStyle().FramePadding.x * 2 + px(10);
}

bool accent_button(const char* label, ImVec2 size, ImU32 color) {
    // Литая кнопка: выпуклый градиент, тёмная кромка, блик сверху, надпись книжным шрифтом.
    const char* end = label;
    while (*end && !(end[0] == '#' && end[1] == '#')) ++end;
    ImFont* font = fonts.display;
    float fs = ImGui::GetFontSize() * 1.06f;
    ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0, label, end);
    float avail = ImGui::GetContentRegionAvail().x;
    if (size.x == 0) size.x = button_width(label);
    else if (size.x < 0) size.x = std::max(px(20), avail + size.x + 1);
    else size.x = std::max(size.x, button_width(label));  // заданная ширина не обрезает надпись
    if (size.y == 0) size.y = ImGui::GetFrameHeight();

    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(label, size);
    bool hovered = ImGui::IsItemHovered(), held = ImGui::IsItemActive();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 base = hovered ? shade(color, 0.12f) : color;
    if (held) base = shade(color, -0.12f);
    ImU32 top = ImGui::GetColorU32(shade(base, 0.32f)), low = ImGui::GetColorU32(shade(base, -0.28f));
    ImVec2 q(p.x + size.x, p.y + size.y);
    float r = px(4);
    dl->AddRectFilled(ImVec2(p.x, p.y + px(2)), ImVec2(q.x, q.y + px(2)), ImGui::GetColorU32(IM_COL32(0, 0, 0, 90)), r);
    dl->AddRectFilled(p, q, ImGui::GetColorU32(shade(base, -0.55f)), r);
    dl->AddRectFilledMultiColor(ImVec2(p.x + 1, p.y + 1), ImVec2(q.x - 1, q.y - 1), top, top, low, low);
    dl->AddLine(ImVec2(p.x + r, p.y + 1.5f), ImVec2(q.x - r, p.y + 1.5f), ImGui::GetColorU32(shade(base, 0.6f)), 1.0f);
    dl->AddRect(p, q, ImGui::GetColorU32(shade(base, -0.6f)), r, 1.0f);
    float luma = 0.3f * float(color & 0xFF) + 0.59f * float((color >> 8) & 0xFF) + 0.11f * float((color >> 16) & 0xFF);
    ImU32 ink = luma > 140 ? IM_COL32(44, 24, 10, 255) : IM_COL32(255, 244, 224, 255);
    ImVec2 tp(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f + (held ? 1.0f : 0.0f));
    if (luma > 140) dl->AddText(font, fs, ImVec2(tp.x, tp.y + 1), ImGui::GetColorU32(IM_COL32(255, 240, 200, 90)), label, end);
    else dl->AddText(font, fs, ImVec2(tp.x, tp.y + 1), ImGui::GetColorU32(IM_COL32(0, 0, 0, 140)), label, end);
    dl->AddText(font, fs, tp, ImGui::GetColorU32(ink), label, end);
    return clicked;
}

bool icon_button(const char* icon, const char* tip, ImU32 color) {
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    bool clicked = ImGui::Button(icon);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    if (tip) tooltip(tip);
    return clicked;
}

void ornate_frame(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 edge, float rounding) {
    // Вторая, тонкая кромка внутри и бронзовые уголки с заклёпкой.
    dl->AddRect(ImVec2(a.x + px(3), a.y + px(3)), ImVec2(b.x - px(3), b.y - px(3)), alpha(col::gold, 0.10f), std::max(0.0f, rounding - px(2)), 1.0f);
    float L = std::min({px(14), (b.x - a.x) * 0.25f, (b.y - a.y) * 0.3f});
    ImU32 c = alpha(shade(edge, 0.35f), 0.95f);
    float t = px(2);
    const ImVec2 corners[4] = {a, ImVec2(b.x, a.y), b, ImVec2(a.x, b.y)};
    const float sx[4] = {1, -1, -1, 1}, sy[4] = {1, 1, -1, -1};
    for (int i = 0; i < 4; ++i) {
        ImVec2 o(corners[i].x + sx[i] * px(1.5f), corners[i].y + sy[i] * px(1.5f));
        dl->AddLine(o, ImVec2(o.x + sx[i] * L, o.y), c, t);
        dl->AddLine(o, ImVec2(o.x, o.y + sy[i] * L), c, t);
        diamond(dl, ImVec2(o.x + sx[i] * px(4), o.y + sy[i] * px(4)), px(2.2f), alpha(col::gold, 0.8f));
    }
}

void diamond(ImDrawList* dl, ImVec2 c, float r, ImU32 fill) {
    dl->AddQuadFilled(ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y), ImVec2(c.x, c.y + r), ImVec2(c.x - r, c.y), fill);
}

void coin(ImDrawList* dl, ImVec2 c, float r) {
    dl->AddCircleFilled(ImVec2(c.x, c.y + r * 0.15f), r, IM_COL32(120, 70, 18, 255));
    dl->AddCircleFilled(c, r, col::gold_dark);
    dl->AddCircleFilled(ImVec2(c.x - r * 0.08f, c.y - r * 0.08f), r * 0.78f, col::gold);
    dl->AddCircle(c, r * 0.6f, col::gold_dark, 0, std::max(1.0f, r * 0.12f));
    dl->AddCircleFilled(ImVec2(c.x - r * 0.35f, c.y - r * 0.38f), r * 0.16f, IM_COL32(255, 246, 210, 220));
}

void xp_gem(ImDrawList* dl, ImVec2 c, float r) {
    // Ограненный кристалл опыта.
    ImVec2 t(c.x, c.y - r), rt(c.x + r * 0.8f, c.y - r * 0.2f), bt(c.x, c.y + r), lt(c.x - r * 0.8f, c.y - r * 0.2f);
    dl->AddQuadFilled(t, rt, bt, lt, IM_COL32(58, 150, 120, 255));
    dl->AddTriangleFilled(t, rt, ImVec2(c.x, c.y - r * 0.1f), IM_COL32(150, 240, 200, 255));
    dl->AddTriangleFilled(t, lt, ImVec2(c.x, c.y - r * 0.1f), IM_COL32(104, 214, 168, 255));
    dl->AddQuad(t, rt, bt, lt, IM_COL32(20, 70, 56, 255), std::max(1.0f, r * 0.12f));
}

void flourish(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 color) {
    float y = (a.y + b.y) * 0.5f, mid = (a.x + b.x) * 0.5f;
    dl->AddRectFilledMultiColor(ImVec2(a.x, y - 0.5f), ImVec2(mid - px(8), y + 0.5f), alpha(color, 0), color, color, alpha(color, 0));
    dl->AddRectFilledMultiColor(ImVec2(mid + px(8), y - 0.5f), ImVec2(b.x, y + 0.5f), color, alpha(color, 0), alpha(color, 0), color);
    diamond(dl, ImVec2(mid, y), px(4), color);
    diamond(dl, ImVec2(mid - px(8), y), px(2), color);
    diamond(dl, ImVec2(mid + px(8), y), px(2), color);
}

void page_backdrop(ImDrawList* dl, ImVec2 a, ImVec2 b) {
    float w = b.x - a.x, h = b.y - a.y;
    // Небо: глубокая ночь сверху, тёплое зарево у горизонта.
    float hz = a.y + h * 0.72f;
    ImU32 top = IM_COL32(14, 13, 26, 255), mid = IM_COL32(30, 22, 40, 255), low = IM_COL32(58, 32, 38, 255);
    dl->AddRectFilledMultiColor(a, ImVec2(b.x, a.y + h * 0.45f), top, top, mid, mid);
    dl->AddRectFilledMultiColor(ImVec2(a.x, a.y + h * 0.45f), ImVec2(b.x, hz), mid, mid, low, low);
    dl->AddRectFilled(ImVec2(a.x, hz), b, low);
    // Звёзды: место и яркость из хеша, рисунок не меняется между кадрами.
    uint32_t s = 2166136261u;
    auto rnd = [&] {
        s = s * 1664525u + 1013904223u;
        return float(s >> 8) / float(1 << 24);
    };
    for (int i = 0; i < 140; ++i) {
        float x = a.x + rnd() * w, y = a.y + rnd() * rnd() * h * 0.6f, br = rnd();
        dl->AddCircleFilled(ImVec2(x, y), px(br > 0.93f ? 1.6f : 0.9f), IM_COL32(255, 244, 220, int(50 + 150 * br)), 6);
    }

    // Хребты: дальний и ближний.
    auto ridge = [&](float base, float amp, float freq, float phase, ImU32 c) {
        std::vector<ImVec2> pts;
        int n = 48;
        for (int k = 0; k <= n; ++k) {
            float x = float(k) / float(n);
            float y = base - amp * (0.55f * std::abs(std::sin(x * freq + phase)) + 0.3f * std::sin(x * freq * 2.7f + phase * 1.7f) * 0.5f + 0.25f);
            pts.push_back(ImVec2(a.x + x * w, a.y + y * h));
        }
        pts.push_back(b);
        pts.push_back(ImVec2(a.x, b.y));
        dl->AddConcavePolyFilled(pts.data(), int(pts.size()), c);
    };
    ridge(0.80f, 0.20f, 5.0f, 0.7f, IM_COL32(38, 26, 40, 255));
    ridge(0.90f, 0.14f, 3.3f, 2.1f, IM_COL32(24, 17, 26, 255));

    // Крепость Хранителя на холме слева: силуэт с горящими окнами.
    float u = std::clamp(h * 0.0016f, 0.6f, 1.4f) * px(1);
    float cx = a.x + w * 0.16f, gy = a.y + h * 0.86f;
    ImU32 stone = IM_COL32(18, 13, 20, 255);
    dl->AddRectFilled(ImVec2(cx - 70 * u, gy - 46 * u), ImVec2(cx + 70 * u, b.y), stone);
    for (float tx : {-70.0f, -24.0f, 24.0f, 58.0f}) {
        float tw = tx == -24.0f ? 26.0f : 18.0f, th = tx == -24.0f ? 100.0f : 72.0f;
        dl->AddRectFilled(ImVec2(cx + tx * u, gy - th * u), ImVec2(cx + (tx + tw) * u, b.y), stone);
        dl->AddTriangleFilled(ImVec2(cx + (tx - 3) * u, gy - th * u), ImVec2(cx + (tx + tw + 3) * u, gy - th * u),
                              ImVec2(cx + (tx + tw * 0.5f) * u, gy - (th + 26) * u), stone);
        dl->AddRectFilled(ImVec2(cx + (tx + tw * 0.38f) * u, gy - (th - 18) * u), ImVec2(cx + (tx + tw * 0.62f) * u, gy - (th - 28) * u),
                          IM_COL32(255, 190, 90, 210));
    }
    for (int k = 0; k < 9; ++k) dl->AddRectFilled(ImVec2(cx + (-66 + k * 15.5f) * u, gy - 54 * u), ImVec2(cx + (-58 + k * 15.5f) * u, gy - 46 * u), stone);
    // Флаг на главной башне.
    float fx = cx - 11 * u, fy = gy - 126 * u;
    dl->AddLine(ImVec2(fx, fy), ImVec2(fx, fy - 22 * u), stone, 2 * u);
    dl->AddTriangleFilled(ImVec2(fx, fy - 22 * u), ImVec2(fx + 18 * u, fy - 17 * u), ImVec2(fx, fy - 12 * u), IM_COL32(150, 40, 40, 255));

    // Затемнение сверху вниз, чтобы текст на фоне читался.
    dl->AddRectFilledMultiColor(a, ImVec2(b.x, a.y + h * 0.35f), IM_COL32(8, 6, 12, 120), IM_COL32(8, 6, 12, 120), IM_COL32(8, 6, 12, 0),
                                IM_COL32(8, 6, 12, 0));
}

void stat_inline(bool gold, const char* text, ImU32 color) {
    float h = ImGui::GetTextLineHeight();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (gold) coin(dl, ImVec2(p.x + h * 0.4f, p.y + h * 0.52f), h * 0.36f);
    else xp_gem(dl, ImVec2(p.x + h * 0.4f, p.y + h * 0.5f), h * 0.4f);
    ImGui::Dummy(ImVec2(h * 0.85f, h));
    ImGui::SameLine(0, px(3));
    colored_text(color ? color : gold ? col::gold : col::teal, text);
}

void tooltip(const char* text) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void tile(const char* big, const char* small, ImVec2 size, bool lit, ImU32 color, const char* tip) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 fill = lit ? alpha(color, 0.18f) : IM_COL32(255, 255, 255, 6);
    ImU32 edge = lit ? alpha(color, 0.75f) : col::border;
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), fill, 8);
    dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), edge, 8, lit ? 1.5f : 1.0f);

    // Подпись переносится по словам максимум на две строки, каждая по центру; лишнее — многоточие.
    float ss = ImGui::GetFontSize() * 0.8f;
    float max_w = size.x - px(10);
    auto width = [&](const std::string& s) { return fonts.regular->CalcTextSizeA(ss, FLT_MAX, 0, s.c_str()).x; };
    std::vector<std::string> lines;
    if (small) {
        std::string line, word;
        std::string text = small;
        text += ' ';
        for (char ch : text) {
            if (ch != ' ') {
                word += ch;
                continue;
            }
            if (word.empty()) continue;
            std::string candidate = line.empty() ? word : line + " " + word;
            if (width(candidate) <= max_w || line.empty()) line = candidate;
            else {
                lines.push_back(line);
                line = word;
            }
            word.clear();
        }
        if (!line.empty()) lines.push_back(line);
        if (lines.size() > 2) {
            lines.resize(2);
            lines[1] += "…";
        }
        for (auto& l : lines) {
            while (width(l) > max_w && l.size() > 4) {
                size_t cut = l.size() - (l.ends_with("…") ? 4 : 1);
                while (cut > 0 && (static_cast<unsigned char>(l[cut]) & 0xC0) == 0x80) --cut;
                l = l.substr(0, cut) + "…";
            }
        }
    }

    float big_size = ImGui::GetFontSize() * 1.25f;
    ImVec2 bs = fonts.semibold->CalcTextSizeA(big_size, FLT_MAX, 0, big);
    float line_h = ss + 1;
    float total = bs.y + (lines.empty() ? 0 : 2 + line_h * float(lines.size()));
    float y = p.y + (size.y - total) * 0.5f;
    dl->AddText(fonts.semibold, big_size, ImVec2(p.x + (size.x - bs.x) * 0.5f, y), lit ? color : col::faint, big);
    y += bs.y + 2;
    for (auto& l : lines) {
        dl->AddText(fonts.regular, ss, ImVec2(p.x + (size.x - width(l)) * 0.5f, y), lit ? col::text : col::faint, l.c_str());
        y += line_h;
    }
    ImGui::Dummy(size);
    if (tip) tooltip(tip);
}

}  // namespace sq::gui
