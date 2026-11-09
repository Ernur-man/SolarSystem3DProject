#include "UiTheme.h"

#include <filesystem>
#include <iostream>

namespace UiTheme {
ImFont *BodyFont = nullptr;
ImFont *SmallFont = nullptr;
ImFont *TitleFont = nullptr;
float DpiScale = 1.0f;
float UiScale = 1.2f;

namespace {
ImFont *loadFont(const char *path, float size, ImFont *fallback) {
    if (!std::filesystem::exists(path)) {
        std::cerr << "Warning: font unavailable at " << path << "; using the default ImGui font.\n";
        return fallback;
    }
    ImFont *font = ImGui::GetIO().Fonts->AddFontFromFileTTF(path, size);
    if (!font) {
        std::cerr << "Warning: font could not be loaded at " << path
                  << "; using the default ImGui font.\n";
        return fallback;
    }
    return font;
}
}

void setup(GLFWwindow *window) {
    float scaleX = 1.0f, scaleY = 1.0f;
    glfwGetWindowContentScale(window, &scaleX, &scaleY);
    DpiScale = (scaleX + scaleY) * 0.5f;
    UiScale = DpiScale * kUiTextScale;
    ImGuiIO &io = ImGui::GetIO();
    ImFontConfig fallbackConfig;
    fallbackConfig.SizePixels = 13.0f * UiScale;
    ImFont *fallback = io.Fonts->AddFontDefault(&fallbackConfig);
    BodyFont = loadFont("assets/fonts/Inter-Regular.ttf", 15.0f * UiScale, fallback);
    SmallFont = loadFont("assets/fonts/Inter-Bold.ttf", 11.0f * UiScale, fallback);
    TitleFont = loadFont("assets/fonts/SpaceGrotesk-Bold.ttf", 34.0f * UiScale, fallback);

    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(20.0f * UiScale, 18.0f * UiScale);
    style.FramePadding = ImVec2(12.0f * UiScale, 8.0f * UiScale);
    style.ItemSpacing = ImVec2(10.0f * UiScale, 10.0f * UiScale);
    style.WindowRounding = 12.0f * UiScale;
    style.ChildRounding = 11.0f * UiScale;
    style.FrameRounding = 8.0f * UiScale;
    style.PopupRounding = 10.0f * UiScale;
    style.ScrollbarRounding = 8.0f * UiScale;
    style.GrabRounding = 8.0f * UiScale;
    style.WindowBorderSize = UiScale;
    style.ChildBorderSize = UiScale;
    style.FrameBorderSize = UiScale;
    style.Colors[ImGuiCol_WindowBg] = PanelBg;
    style.Colors[ImGuiCol_ChildBg] = CardBg;
    style.Colors[ImGuiCol_Border] = Border;
    style.Colors[ImGuiCol_Text] = White;
    style.Colors[ImGuiCol_TextDisabled] = Secondary;
    style.Colors[ImGuiCol_FrameBg] = CardBg;
    style.Colors[ImGuiCol_FrameBgHovered] = CardHover;
    style.Colors[ImGuiCol_FrameBgActive] = CardHover;
    style.Colors[ImGuiCol_Button] = CardBg;
    style.Colors[ImGuiCol_ButtonHovered] = CardHover;
    style.Colors[ImGuiCol_ButtonActive] = CardHover;
    style.Colors[ImGuiCol_Header] = CardBg;
    style.Colors[ImGuiCol_HeaderHovered] = CardHover;
    style.Colors[ImGuiCol_HeaderActive] = CardHover;
    style.Colors[ImGuiCol_CheckMark] = Accent;
    style.Colors[ImGuiCol_SliderGrab] = Accent;
    style.Colors[ImGuiCol_SliderGrabActive] = Accent;
    style.Colors[ImGuiCol_Separator] = Border;
    style.Colors[ImGuiCol_PopupBg] = PanelBg;
}

void beginFrame() {
    ImGui::PushFont(BodyFont);
}

void textSecondary(const char *text) { ImGui::TextColored(Secondary, "%s", text); }

bool chip(const char *label, bool active, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_Button, active ? Accent : CardBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? Accent : CardHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, active ? Accent : CardHover);
    ImGui::PushStyleColor(ImGuiCol_Text, active ? DarkText : Secondary);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 14.0f * UiScale);
    if (size.x > 0.0f) size.x *= UiScale;
    if (size.y > 0.0f) size.y *= UiScale;
    const bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    return clicked;
}

bool button(const char *label, bool primary, ImVec2 size) {
    if (size.x > 0.0f) size.x *= UiScale;
    if (size.y > 0.0f) size.y *= UiScale;
    if (primary) {
        ImGui::PushStyleColor(ImGuiCol_Button, Accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.79f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.88f, 0.62f, 0.08f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, DarkText);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, CardBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, CardHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, CardHover);
    }
    const bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(primary ? 4 : 3);
    return clicked;
}

void header() {
    ImDrawList *draw = ImGui::GetWindowDrawList();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float y = start.y + 13.0f * UiScale;
    const float x = start.x + 5.0f * UiScale;
    draw->AddCircleFilled(ImVec2(x, y), 9.0f * UiScale, ImGui::ColorConvertFloat4ToU32(ImVec4(Accent.x, Accent.y, Accent.z, 0.16f)));
    draw->AddCircleFilled(ImVec2(x, y), 4.0f * UiScale, ImGui::ColorConvertFloat4ToU32(Accent));
    ImGui::Dummy(ImVec2(0.0f, 28.0f * UiScale));
    const char *word = "SOLAR";
    ImVec2 pos = ImGui::GetCursorScreenPos();
    if (TitleFont) ImGui::PushFont(TitleFont);
    for (const char *c = word; *c; ++c) {
        char letter[2] = {*c, '\0'};
        ImGui::GetWindowDrawList()->AddText(pos, ImGui::ColorConvertFloat4ToU32(White), letter);
        pos.x += ImGui::CalcTextSize(letter).x + 2.0f * UiScale;
    }
    if (TitleFont) ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.0f, (TitleFont ? TitleFont->FontSize : 22.0f) + 4.0f * UiScale));
    if (SmallFont) ImGui::PushFont(SmallFont);
    ImGui::TextColored(Secondary, "E X P L O R E   T H E   S O L A R   S Y S T E M");
    if (SmallFont) ImGui::PopFont();
    ImGui::Spacing();
}

void hero() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, CardBg);
    ImGui::BeginChild("hero_card", ImVec2(0, 175.0f * UiScale), true);
    ImGui::TextColored(Accent, "● SIMULATION");
    ImGui::Spacing();
    if (TitleFont) ImGui::PushFont(TitleFont);
    ImGui::TextColored(White, "The Solar System");
    if (TitleFont) ImGui::PopFont();
    ImGui::PushTextWrapPos();
    ImGui::TextColored(Secondary, "Eight worlds on a stylised scale. Pick a world to fly there.");
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

bool planetCard(const char *name, const char *subtitle, ImVec4 color, bool selected) {
    ImGui::PushID(name);
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 size(width, 72.0f * UiScale);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton("world_card", size);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList *draw = ImGui::GetWindowDrawList();
    const ImVec2 end(pos.x + size.x, pos.y + size.y);
    const ImU32 background = ImGui::ColorConvertFloat4ToU32(
        selected ? CardHover : (hovered ? CardHover : CardBg));
    draw->AddRectFilled(pos, end, background, 9.0f * UiScale);
    draw->AddRect(pos, end, ImGui::ColorConvertFloat4ToU32(Border), 9.0f * UiScale);
    if (selected)
        draw->AddRectFilled(pos, ImVec2(pos.x + 3.0f * UiScale, end.y),
                            ImGui::ColorConvertFloat4ToU32(Accent), 2.0f * UiScale);
    draw->AddCircleFilled(ImVec2(pos.x + 24.0f * UiScale, pos.y + size.y * 0.5f),
                          6.0f * UiScale, ImGui::ColorConvertFloat4ToU32(color));
    const ImVec2 textPos(pos.x + 43.0f * UiScale, pos.y + 10.0f * UiScale);
    draw->AddText(textPos, ImGui::ColorConvertFloat4ToU32(White), name);
    const ImVec2 subPos(pos.x + 43.0f * UiScale, pos.y + 37.0f * UiScale);
    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), subPos,
                  ImGui::ColorConvertFloat4ToU32(Secondary), subtitle, nullptr,
                  width - 50.0f * UiScale);
    ImGui::PopID();
    return clicked;
}

void keyValue(const char *key, const char *value) {
    const float keyWidth = ImGui::GetContentRegionAvail().x * 0.43f;
    ImGui::TextColored(Secondary, "%s", key);
    ImGui::SameLine(keyWidth);
    ImGui::TextWrapped("%s", value);
}
} // namespace UiTheme
