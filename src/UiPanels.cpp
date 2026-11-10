#include "UiPanels.h"
#include "UiTheme.h"

#include <cmath>
#include <iterator>

namespace UiPanels {
void beginCenteredCard(const char *name, float windowWidth, float windowHeight,
                       float cardWidth, float cardHeight, bool *open) {
    ImGui::SetNextWindowPos(ImVec2(windowWidth * 0.5f, windowHeight * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(cardWidth * UiTheme::DpiScale,
                                    cardHeight * UiTheme::DpiScale), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, UiTheme::Background);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f * UiTheme::UiScale);
    ImGui::Begin(name, open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                                    ImGuiWindowFlags_NoSavedSettings);
    const ImVec2 p = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const float dpi = UiTheme::DpiScale;
    auto snap = [dpi](float value) { return std::round(value * dpi) / dpi + 0.5f / dpi; };
    ImGui::GetWindowDrawList()->AddRect(ImVec2(snap(p.x), snap(p.y)),
        ImVec2(snap(p.x + size.x), snap(p.y + size.y)),
        ImGui::ColorConvertFloat4ToU32(UiTheme::Hairline), 16.0f * UiTheme::UiScale, 0, 1.0f / UiTheme::DpiScale);
}

void endCenteredCard() {
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void controlsList() {
    static constexpr const char *keys[] = {"F1 / H", "SPACE", "K", "1 / 2 / 3", "G", "L", "I", "Q", "M", "N", "P", "Y", "B", "R", "F", "[ / ]", "ESC", "F12", "LMB", "RMB / WHEEL", "WASD / SHIFT"};
    static constexpr const char *actions[] = {"Toggle controls", "Pause simulation", "Mute ambience", "Set simulation speed", "Toggle bloom", "Toggle lensing", "Toggle instancing", "Toggle quality", "Toggle meteors", "Toggle city markers", "Toggle Earth map when Earth is selected", "Toggle Milky Way view", "Summon or return the black hole", "Return or restart", "Toggle free-fly", "Rotate Sun light", "Return to overview", "Save screenshot", "Select a world", "Orbit camera / zoom", "Move / move faster"};
    if (!ImGui::BeginTable("controls_keymap", 2, ImGuiTableFlags_SizingStretchProp)) return;
    ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 82.0f * UiTheme::UiScale);
    for (std::size_t i = 0; i < std::size(keys); ++i) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (UiTheme::DataFont) ImGui::PushFont(UiTheme::DataFont);
        ImGui::TextColored(UiTheme::Secondary, "%s", keys[i]);
        if (UiTheme::DataFont) ImGui::PopFont();
        ImGui::TableNextColumn();
        ImGui::TextWrapped("%s", actions[i]);
        ImGui::TableNextRow(ImGuiTableRowFlags_None, 1.0f);
        ImGui::TableNextColumn();
        UiTheme::HairlineRule();
        ImGui::TableNextColumn();
        UiTheme::HairlineRule();
    }
    ImGui::EndTable();
}
} // namespace UiPanels
