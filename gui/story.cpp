// Страница летописи после взятого рубежа: на пергаменте проявляется сюжетная сцена рубежа,
// под ней — что дальше: следующий рубеж, новая глава или эпилог кампании.

#include "app.h"

#include "draw.h"
#include "theme.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace sq::gui {

namespace {

constexpr ImU32 kInk = IM_COL32(58, 36, 20, 255);
constexpr ImU32 kInkDim = IM_COL32(112, 82, 52, 255);
constexpr ImU32 kInkRed = IM_COL32(140, 42, 30, 255);
constexpr float kLettersPerSecond = 60;

// Начало k-й буквы в UTF-8: текст проявляется по буквам, а не по байтам.
const char* nth_letter(const std::string& s, size_t k) {
    const char* p = s.c_str();
    const char* end = p + s.size();
    while (p < end && k > 0) {
        ++p;
        while (p < end && (static_cast<unsigned char>(*p) & 0xC0) == 0x80) ++p;
        --k;
    }
    return p;
}

size_t letters(const std::string& s) {
    return size_t(std::count_if(s.begin(), s.end(), [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
}

float text_height(ImFont* font, float size, float wrap, const std::string& text) {
    return text.empty() ? 0.0f : font->CalcTextSizeA(size, FLT_MAX, wrap, text.c_str()).y;
}

}  // namespace

void App::open_story(const std::string& frontier) {
    story_ = StoryPage{true, frontier, clock()};
}

void App::story_view() {
    StoryPage& sp = story_;
    const quest::Frontier* f = world_.frontier(sp.frontier);
    if (!f) {
        sp.active = false;
        return;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos(), size = ImGui::GetContentRegionAvail();
    ImGui::Dummy(size);
    float W = size.x, H = size.y;
    float time = float(clock());
    float t = sp.frozen >= 0 ? sp.frozen : float(clock() - sp.born);
    const quest::Chapter& chapter = world_.chapters[size_t(f->chapter)];

    // Что дальше: следующий рубеж той же главы, новая глава или эпилог.
    auto it = std::ranges::find_if(world_.frontiers, [&](const quest::Frontier& x) { return x.id == f->id; });
    const quest::Frontier* next = it + 1 < world_.frontiers.end() ? &*(it + 1) : nullptr;
    std::string ahead_title, ahead_text;
    if (!next) {
        ahead_title = "Эпилог";
        ahead_text = campaign().epilogue.empty() ? "Кампания пройдена." : campaign().epilogue;
    } else if (next->chapter != f->chapter) {
        const quest::Chapter& nc = world_.chapters[size_t(next->chapter)];
        ahead_title = std::format("Глава {}. {}", next->chapter + 1, nc.name);
        ahead_text = nc.intro;
    } else {
        ahead_title = std::format("Впереди: {} · {}", next->label, next->name);
        ahead_text = next->intro;
    }
    std::string body = f->victory + (f->story.empty() ? "" : " " + f->story);

    // Размеры пергамента считаются по полному тексту, чтобы он не прыгал, пока текст проявляется.
    // На низком окне шрифт пергамента мельчает, пока всё не влезет вместе с кнопкой.
    float pw = std::min(W * 0.8f, px(860)), wrap = pw - px(96);
    float k = 1.0f, body_size = 0, ahead_size = 0, body_h = 0, ahead_h = 0, ph = 0;
    for (int pass = 0; pass < 6; ++pass, k *= 0.9f) {
        body_size = 19.0f * ui_scale * k;
        ahead_size = 16.5f * ui_scale * k;
        body_h = text_height(fonts.regular, body_size, wrap, body);
        ahead_h = text_height(fonts.regular, ahead_size, wrap, ahead_text);
        ph = px(40) + px(34 * k) + px(44 * k) + px(22) + body_h + px(30) + px(30 * k) + ahead_h + px(80);
        if (ph <= H - px(48)) break;
    }
    ImVec2 c(origin.x + W * 0.5f, origin.y + std::max(H * 0.5f, ph * 0.5f + px(36)));
    ImVec2 lo(c.x - pw * 0.5f, c.y - ph * 0.5f), hi(c.x + pw * 0.5f, c.y + ph * 0.5f);

    dl->PushClipRect(origin, add(origin, size), true);
    draw_battlefield(dl, origin, size, chapter.color, time);
    float appear = ease(t / 0.4f);
    dl->AddRectFilled(origin, add(origin, size), alpha(IM_COL32(8, 6, 12, 255), 0.55f * appear));

    // Пергамент выезжает снизу, под ним тень.
    float slide = (1.0f - appear) * px(40);
    lo.y += slide;
    hi.y += slide;
    dl->AddRectFilled(ImVec2(lo.x + 6, lo.y + 10), ImVec2(hi.x + 6, hi.y + 10), alpha(IM_COL32(0, 0, 0, 255), 0.4f * appear), 8);
    dl->AddRectFilledMultiColor(lo, hi, alpha(IM_COL32(240, 225, 188, 255), appear), alpha(IM_COL32(236, 219, 180, 255), appear),
                                alpha(IM_COL32(220, 196, 150, 255), appear), alpha(IM_COL32(226, 204, 160, 255), appear));
    dl->AddRect(lo, hi, alpha(IM_COL32(150, 108, 60, 255), appear), 6, 3.0f);
    dl->AddRect(ImVec2(lo.x + px(8), lo.y + px(8)), ImVec2(hi.x - px(8), hi.y - px(8)), alpha(IM_COL32(170, 128, 76, 255), 0.7f * appear), 4, 1.0f);
    // Сургучная печать с эмблемой Цитадели наверху.
    ImVec2 seal(c.x, lo.y + px(4));
    dl->AddCircleFilled(seal, px(30), alpha(IM_COL32(126, 26, 30, 255), appear));
    dl->AddCircle(seal, px(30), alpha(IM_COL32(90, 14, 18, 255), appear), 0, 2.0f);
    if (appear > 0.5f) draw_emblem(dl, ImVec2(seal.x, seal.y + px(2)), px(42));

    float x = lo.x + px(48), y = lo.y + px(40);
    auto ink = [&](ImU32 col, float a) { return alpha(col, a * appear); };
    std::string header = std::format("Глава {} · {}", f->chapter + 1, chapter.name);
    text_center(dl, fonts.semibold, 15.0f * ui_scale * k, ImVec2(c.x, y + px(10 * k)), ink(kInkRed, 1), header.c_str());
    y += px(34 * k);
    // Длинное название ужимается по ширине пергамента.
    std::string title = std::format("{} · {}", f->label, f->name);
    float ts = 30.0f * ui_scale * k, tw = fonts.display->CalcTextSizeA(ts, FLT_MAX, 0, title.c_str()).x;
    if (tw > pw - px(64)) ts *= (pw - px(64)) / tw;
    text_center(dl, fonts.display, ts, ImVec2(c.x, y + px(16 * k)), ink(kInk, 1), title.c_str());
    y += px(44 * k);
    flourish(dl, ImVec2(c.x - px(130), y), ImVec2(c.x + px(130), y + px(6)), ink(kInkDim, 0.8f));
    y += px(22);

    // Сцена проявляется по буквам, после неё — что дальше.
    size_t total = letters(body);
    size_t shown = size_t(std::clamp((t - 0.5f) * kLettersPerSecond, 0.0f, float(total)));
    dl->AddText(fonts.regular, body_size, ImVec2(x, y), ink(kInk, 1), body.c_str(), nth_letter(body, shown), wrap);
    y += body_h + px(30);
    float typed_at = 0.5f + float(total) / kLettersPerSecond;
    bool typing = t < typed_at;
    float ahead = ease((t - typed_at) / 0.5f);
    if (ahead > 0) {
        dl->AddLine(ImVec2(x, y - px(14)), ImVec2(hi.x - px(48), y - px(14)), ink(kInkDim, 0.35f * ahead), 1.0f);
        dl->AddText(fonts.display, 21.0f * ui_scale * k, ImVec2(x, y), ink(next && next->chapter != f->chapter ? kInkRed : kInk, ahead), ahead_title.c_str());
        dl->AddText(fonts.regular, ahead_size, ImVec2(x, y + px(30 * k)), ink(kInkDim, ahead), ahead_text.c_str(), nullptr, wrap);
    }
    dl->PopClipRect();

    // Щелчок по пергаменту — показать текст сразу. Кнопка — внутри пергамента, внизу.
    ImGui::SetCursorScreenPos(lo);
    if (ImGui::InvisibleButton("parchment", ImVec2(pw, ph - px(64))) && typing && sp.frozen < 0) sp.born = clock() - typed_at;
    ImGui::SetCursorScreenPos(ImVec2(c.x - px(100), hi.y - px(54)));
    if (typing) {
        if (ImGui::Button("Показать всё", ImVec2(px(200), px(36))) && sp.frozen < 0) sp.born = clock() - typed_at;
    } else if (accent_button("К карте", ImVec2(px(200), px(38)))) {
        sp.active = false;
        page = Page::Campaign;
        campaign_tab_ = 0;
        selected_frontier_ = next ? next->id : f->id;
    }
}

}  // namespace sq::gui
