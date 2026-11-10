#pragma once

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <array>
#include <string>

namespace UiTheme {
inline constexpr float kUiTextScale = 1.2f;
inline constexpr std::array<const char *, 3> kTeamMembers = {"Yernur", "Renat", "Azamat"};
inline constexpr ImVec4 Background{7.0f / 255.0f, 9.0f / 255.0f, 13.0f / 255.0f, 0.90f};
inline constexpr ImVec4 PanelText{232.0f / 255.0f, 236.0f / 255.0f, 242.0f / 255.0f, 1.0f};
inline constexpr ImVec4 Secondary{138.0f / 255.0f, 148.0f / 255.0f, 166.0f / 255.0f, 1.0f};
inline constexpr ImVec4 Tertiary{91.0f / 255.0f, 101.0f / 255.0f, 119.0f / 255.0f, 1.0f};
inline constexpr ImVec4 Hairline{1.0f, 1.0f, 1.0f, 0.14f};
inline constexpr ImVec4 Accent{245.0f / 255.0f, 184.0f / 255.0f, 46.0f / 255.0f, 1.0f};
inline constexpr ImVec4 Alert{1.0f, 90.0f / 255.0f, 95.0f / 255.0f, 1.0f};
inline constexpr ImVec4 NearBlack{7.0f / 255.0f, 9.0f / 255.0f, 13.0f / 255.0f, 1.0f};

extern ImFont *BodyFont;
extern ImFont *LabelFont;
extern ImFont *DataFont;
extern ImFont *SmallDataFont;
extern ImFont *HeroFont;
extern ImFont *TitleFont;
extern ImFont *IntroFont;
extern float DpiScale;
extern float UiScale;

std::string teamCredit();
void setup(GLFWwindow *window);
void updateDpi(GLFWwindow *window);
void beginFrame();
void textSecondary(const char *text);
void SectionHeader(const char *index, const char *title);
void LabelValueRow(const char *label, const char *value, const char *unit = "");
bool ToggleRow(const char *name, const char *key, bool state);
bool PrimaryButton(const char *label, ImVec2 size = ImVec2(0, 0));
bool GhostButton(const char *label, ImVec2 size = ImVec2(0, 0));
void HairlineRule();
void Tag(const char *text);

void header();
} // namespace UiTheme
