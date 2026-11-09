#include "UiPanels.h"
#include "UiTheme.h"

namespace UiPanels {
void beginCenteredCard(const char *name, float windowWidth, float windowHeight,
                       float cardWidth, float cardHeight, bool *open) {
    ImGui::SetNextWindowPos(ImVec2(windowWidth * 0.5f, windowHeight * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(cardWidth * UiTheme::UiScale,
                                    cardHeight * UiTheme::UiScale), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, UiTheme::PanelBg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f * UiTheme::UiScale);
    ImGui::Begin(name, open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoCollapse);
}

void endCenteredCard() {
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void controlsList() {
    ImGui::TextWrapped("F1 or H: toggle this help overlay");
    ImGui::TextWrapped("Space: pause simulation");
    ImGui::TextWrapped("K: mute or unmute synthesized world ambience");
    ImGui::TextWrapped("1 / 2 / 3: simulation speed 1x / 5x / 10x");
    ImGui::TextWrapped("G: toggle bloom state | L: toggle lensing state");
    ImGui::TextWrapped("I: toggle asteroid instancing | Q: toggle high / low quality");
    ImGui::TextWrapped("M: toggle meteors");
    ImGui::TextWrapped("N: toggle Earth city markers and labels | P: toggle the Earth Map panel when Earth is selected");
    ImGui::TextWrapped("Y: toggle Milky Way view in Free Explore (black hole Idle)");
    ImGui::TextWrapped("Planet maps are optional; procedural surfaces and Earth's Moon work without files");
    ImGui::TextWrapped("B: summon / return black hole | R: return or restart after swallow in Free Explore");
    ImGui::TextWrapped("R restarts an active or finished Mission; Free Explore ending offers Restart / Keep watching");
    ImGui::TextWrapped("F: toggle free-fly | [ / ]: rotate Sun light");
    ImGui::TextWrapped("Esc: clear selection / return to overview");
    ImGui::TextWrapped("F12: save screenshot");
    ImGui::TextWrapped("Left click: select a body | Right drag: orbit camera | Wheel: zoom");
    ImGui::TextWrapped("WASD: move in free-fly | Shift: move faster");
}
} // namespace UiPanels
