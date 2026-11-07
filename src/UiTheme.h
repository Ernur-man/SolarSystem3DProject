#pragma once

#include <GLFW/glfw3.h>
#include <imgui.h>

namespace UiTheme {
// One adjustment scales every UI font and its related layout measurements.
inline constexpr float kUiTextScale = 1.2f;
// Central palette keeps the interface easy to adjust or explain.
inline constexpr ImVec4 PanelBg{8.0f / 255.0f, 10.0f / 255.0f, 16.0f / 255.0f, 0.92f};
inline constexpr ImVec4 CardBg{15.0f / 255.0f, 19.0f / 255.0f, 29.0f / 255.0f, 0.96f};
inline constexpr ImVec4 CardHover{25.0f / 255.0f, 31.0f / 255.0f, 44.0f / 255.0f, 0.98f};
inline constexpr ImVec4 White{0.96f, 0.97f, 0.99f, 1.0f};
inline constexpr ImVec4 Secondary{154.0f / 255.0f, 163.0f / 255.0f, 178.0f / 255.0f, 1.0f};
inline constexpr ImVec4 Accent{245.0f / 255.0f, 184.0f / 255.0f, 46.0f / 255.0f, 1.0f};
inline constexpr ImVec4 Border{1.0f, 1.0f, 1.0f, 0.08f};
inline constexpr ImVec4 DarkText{16.0f / 255.0f, 18.0f / 255.0f, 24.0f / 255.0f, 1.0f};

extern ImFont *BodyFont;
extern ImFont *SmallFont;
extern ImFont *TitleFont;
extern float DpiScale;
extern float UiScale;

void setup(GLFWwindow *window);
void beginFrame();
void textSecondary(const char *text);
bool chip(const char *label, bool active, ImVec2 size = ImVec2(0, 0));
bool button(const char *label, bool primary, ImVec2 size = ImVec2(0, 0));
void header();
void hero();
bool planetCard(const char *name, const char *subtitle, ImVec4 color, bool selected);
void keyValue(const char *key, const char *value);
} // namespace UiTheme
