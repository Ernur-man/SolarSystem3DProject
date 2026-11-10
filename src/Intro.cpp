#include "Intro.h"
#include "UiTheme.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace {
constexpr float kSceneFadeSeconds = 0.8f; // Scene fades from black during the first 0.8 seconds.
constexpr float kTitleFadeStart = 0.8f; // Title fade and scale start at 0.8 seconds.
constexpr float kTitleFadeEnd = 2.2f; // Title reaches full opacity at 2.2 seconds.
constexpr float kLineGrowStart = 1.4f; // Yellow line begins growing at 1.4 seconds.
constexpr float kLineGrowEnd = 2.4f; // Yellow line reaches full width at 2.4 seconds.
constexpr float kCreditsFadeStart = 2.0f; // Credits begin appearing at 2.0 seconds.
constexpr float kCreditsFadeEnd = 3.0f; // Credits reach full opacity at 3.0 seconds.
constexpr float kHoldEnd = 4.4f; // Intro holds until 4.4 seconds.
constexpr float kFadeOutStart = 4.4f; // Text and overlay fade out from 4.4 seconds.
constexpr float kFadeOutEnd = 5.2f; // Intro ends at 5.2 seconds (below the 6 second maximum).
constexpr float kSkipSeconds = 0.4f; // Input skip fades over 0.4 seconds.

float smootherstep(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float phase(float time, float start, float end) {
    return smootherstep((time - start) / (end - start));
}

void spacedText(ImDrawList *draw, ImFont *font, float fontSize, ImVec2 center,
                const char *text, ImU32 color, float spacing) {
    float width = 0.0f;
    for (const char *p = text; *p; ++p) {
        char glyph[2] = {*p, '\0'};
        width += font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, glyph).x;
        if (p[1]) width += spacing;
    }
    ImVec2 pos(center.x - width * 0.5f, center.y - fontSize * 0.5f);
    for (const char *p = text; *p; ++p) {
        char glyph[2] = {*p, '\0'};
        draw->AddText(font, fontSize, pos, color, glyph);
        pos.x += font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, glyph).x + spacing;
    }
}

float spacedWidth(ImFont *font, float fontSize, const char *text, float spacing) {
    float width = 0.0f;
    for (const char *p = text; *p; ++p) {
        char glyph[2] = {*p, '\0'};
        width += font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, glyph).x;
        if (p[1]) width += spacing;
    }
    return width;
}
}

void Intro::start(bool enabled) {
    running = kShowIntro && enabled;
    elapsed = 0.0f;
    skipping = false;
    skipElapsed = 0.0f;
}

void Intro::update(float deltaTime, bool skipRequested) {
    if (!running) return;
    const float dt = std::clamp(deltaTime, 0.0f, 0.05f);
    if (skipRequested && !skipping) skipping = true;
    if (skipping) {
        skipElapsed = std::min(skipElapsed + dt, kSkipSeconds);
        if (skipElapsed >= kSkipSeconds) running = false;
        return;
    }
    elapsed = std::min(elapsed + dt, kIntroSeconds);
    if (elapsed >= kIntroSeconds) running = false;
}

void Intro::draw(float width, float height, float dpiScale) const {
    if (!running) return;
    const float time = elapsed;
    const float sceneIn = phase(time, 0.0f, kSceneFadeSeconds);
    const float titleIn = phase(time, kTitleFadeStart, kTitleFadeEnd);
    const float lineIn = phase(time, kLineGrowStart, kLineGrowEnd);
    const float creditsIn = phase(time, kCreditsFadeStart, kCreditsFadeEnd);
    const float fadeOut = time < kHoldEnd ? 1.0f : 1.0f - phase(time, kFadeOutStart, kFadeOutEnd);
    const float skipFactor = skipping ? 1.0f - smootherstep(skipElapsed / kSkipSeconds) : 1.0f;
    const float overlay = (time < kSceneFadeSeconds ? 1.0f - 0.45f * sceneIn
                                                    : 0.55f * fadeOut) * skipFactor;
    const float titleAlpha = titleIn * fadeOut * skipFactor;
    const ImU32 black = ImGui::ColorConvertFloat4ToU32(
        ImVec4(UiTheme::NearBlack.x, UiTheme::NearBlack.y, UiTheme::NearBlack.z, overlay));
    ImDrawList *draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(ImVec2(0, 0), ImVec2(width, height), black);

    ImFont *font = UiTheme::IntroFont ? UiTheme::IntroFont : ImGui::GetFont();
    float titleSize = std::clamp(height * 0.10f, 48.0f, 110.0f) * dpiScale;
    const float titleAvailable = std::max(32.0f * dpiScale, width - 36.0f * dpiScale);
    const float requestedTitleWidth = spacedWidth(font, titleSize, "SOLAR SYSTEM 3D", titleSize * 0.04f);
    if (requestedTitleWidth > titleAvailable) titleSize *= titleAvailable / requestedTitleWidth;
    const float scale = 0.94f + 0.06f * titleIn;
    const float actualSize = titleSize * scale;
    const ImVec2 center(width * 0.5f, height * 0.47f);
    spacedText(draw, font, actualSize, center, "SOLAR SYSTEM 3D",
               ImGui::ColorConvertFloat4ToU32(ImVec4(UiTheme::PanelText.x, UiTheme::PanelText.y,
                   UiTheme::PanelText.z, titleAlpha)), actualSize * 0.04f);

    const float lineWidth = std::min(220.0f * dpiScale, std::max(24.0f * dpiScale, width - 32.0f * dpiScale)) *
                            lineIn * fadeOut * skipFactor;
    const ImVec2 lineCenter(width * 0.5f, center.y + 0.5f * titleSize + 18.0f * dpiScale);
    const ImU32 lineColor = ImGui::ColorConvertFloat4ToU32(
        ImVec4(UiTheme::Accent.x, UiTheme::Accent.y, UiTheme::Accent.z, fadeOut * skipFactor));
    const float pixelY = std::round(lineCenter.y * dpiScale) / dpiScale + 0.5f / dpiScale;
    draw->AddLine(ImVec2(lineCenter.x - lineWidth * 0.5f, pixelY),
                  ImVec2(lineCenter.x + lineWidth * 0.5f, pixelY), lineColor, 1.0f / dpiScale);
    const ImU32 creditColor = ImGui::ColorConvertFloat4ToU32(
        ImVec4(UiTheme::Secondary.x, UiTheme::Secondary.y, UiTheme::Secondary.z,
               creditsIn * fadeOut * skipFactor));
    const std::string credits = UiTheme::teamCredit();
    ImFont *creditFont = UiTheme::BodyFont ? UiTheme::BodyFont : ImGui::GetFont();
    float creditSize = 22.0f * dpiScale;
    const float creditWidth = spacedWidth(creditFont, creditSize, credits.c_str(), 0.6f * dpiScale);
    const float creditAvailable = std::max(24.0f * dpiScale, width - 32.0f * dpiScale);
    if (creditWidth > creditAvailable) creditSize *= creditAvailable / creditWidth;
    spacedText(draw, creditFont, creditSize, ImVec2(width * 0.5f, lineCenter.y + 38.0f * dpiScale),
               credits.c_str(), creditColor, 0.6f * dpiScale);
}
