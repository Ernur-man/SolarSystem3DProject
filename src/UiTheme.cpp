#include "UiTheme.h"

#include <backends/imgui_impl_opengl3.h>
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace UiTheme {
ImFont *BodyFont = nullptr;
ImFont *LabelFont = nullptr;
ImFont *DataFont = nullptr;
ImFont *SmallDataFont = nullptr;
ImFont *HeroFont = nullptr;
ImFont *TitleFont = nullptr;
ImFont *IntroFont = nullptr;
float DpiScale = 1.0f;
float UiScale = kUiTextScale;

std::string teamCredit() {
    std::ostringstream text;
    text << "By: ";
    for (std::size_t i = 0; i < kTeamMembers.size(); ++i) {
        if (i) text << ", ";
        text << kTeamMembers[i];
    }
    return text.str();
}

namespace {
struct FontFiles {
    std::filesystem::path interRegular;
    std::filesystem::path interMedium;
    std::filesystem::path interSemiBold;
    std::filesystem::path monoRegular;
    std::filesystem::path monoMedium;
    bool foundVariable = false;
};

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

std::uint16_t read16(std::ifstream &file) {
    unsigned char bytes[2] = {};
    file.read(reinterpret_cast<char *>(bytes), 2);
    return static_cast<std::uint16_t>((bytes[0] << 8) | bytes[1]);
}

std::uint32_t read32(std::ifstream &file) {
    unsigned char bytes[4] = {};
    file.read(reinterpret_cast<char *>(bytes), 4);
    return (static_cast<std::uint32_t>(bytes[0]) << 24) |
           (static_cast<std::uint32_t>(bytes[1]) << 16) |
           (static_cast<std::uint32_t>(bytes[2]) << 8) | bytes[3];
}

bool isVariableFont(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    file.seekg(4);
    const std::uint16_t count = read16(file);
    for (std::uint16_t i = 0; i < count && file; ++i) {
        char tag[4] = {};
        file.read(tag, 4);
        (void)read32(file);
        (void)read32(file);
        (void)read32(file);
        if (std::string(tag, 4) == "fvar") return true;
    }
    return false;
}

FontFiles discoverFonts() {
    FontFiles result;
    std::vector<std::filesystem::path> files;
    std::error_code error;
    const std::filesystem::path root("assets/fonts");
    for (std::filesystem::recursive_directory_iterator it(root, error), end;
         !error && it != end; it.increment(error)) {
        if (it->is_regular_file() && lower(it->path().extension().string()) == ".ttf")
            files.push_back(it->path());
    }
    auto weight = [](const std::string &name) {
        if (name.find("semibold") != std::string::npos || name.find("semi-bold") != std::string::npos)
            return 3;
        if (name.find("medium") != std::string::npos) return 2;
        if (name.find("regular") != std::string::npos) return 1;
        return 0;
    };
    for (const auto &path : files) {
        std::string name = lower(path.filename().string());
        std::string compact = name;
        compact.erase(std::remove_if(compact.begin(), compact.end(), [](unsigned char c) {
            return std::isspace(c) || c == '-' || c == '_';
        }), compact.end());
        const bool inter = compact.find("inter") != std::string::npos;
        const bool mono = compact.find("jetbrainsmono") != std::string::npos;
        if (!inter && !mono) continue;
        if (isVariableFont(path)) {
            result.foundVariable = true;
            continue;
        }
        const int fileWeight = weight(name);
        if (fileWeight == 0) continue;
        auto assign = [&](std::filesystem::path &slot) {
            if (slot.empty()) slot = path;
        };
        if (inter && fileWeight == 1) assign(result.interRegular);
        if (inter && fileWeight == 2) assign(result.interMedium);
        if (inter && fileWeight == 3) assign(result.interSemiBold);
        if (mono && fileWeight == 1) assign(result.monoRegular);
        if (mono && fileWeight == 2) assign(result.monoMedium);
    }
    return result;
}

ImFont *addFont(const std::filesystem::path &path, float logicalSize) {
    if (path.empty()) return nullptr;
    ImFontConfig config;
    static const ImWchar glyphRanges[] = {0x0020, 0x00FF, 0x2014, 0x2014, 0};
    config.GlyphRanges = glyphRanges;
    config.PixelSnapH = false;
    config.OversampleH = 3;
    config.OversampleV = 2;
    return ImGui::GetIO().Fonts->AddFontFromFileTTF(path.string().c_str(),
                                                     logicalSize * UiScale, &config);
}

void buildFonts(GLFWwindow *window, bool replacing) {
    if (replacing) ImGui_ImplOpenGL3_DestroyFontsTexture();
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();
    io.FontGlobalScale = 1.0f;
    const FontFiles files = discoverFonts();
#ifdef _WIN32
    std::size_t windowsDirSize = 0;
    char *windowsDir = nullptr;
    _dupenv_s(&windowsDir, &windowsDirSize, "WINDIR");
    const std::filesystem::path systemFonts = (windowsDir && *windowsDir)
        ? std::filesystem::path(windowsDir) / "Fonts"
        : std::filesystem::path("C:/Windows/Fonts");
    std::free(windowsDir);
    const std::filesystem::path systemInterface = systemFonts / "segoeui.ttf";
    const std::filesystem::path systemReadout = systemFonts / "consola.ttf";
    const auto existingOrEmpty = [](const std::filesystem::path &path) {
        std::error_code error;
        return std::filesystem::is_regular_file(path, error) ? path : std::filesystem::path{};
    };
    const std::filesystem::path interfaceFallback = existingOrEmpty(systemInterface);
    const std::filesystem::path readoutFallback = existingOrEmpty(systemReadout);
#else
    const std::filesystem::path interfaceFallback;
    const std::filesystem::path readoutFallback;
#endif
    auto addDefault = [&](float logicalSize) {
        ImFontConfig config;
        config.SizePixels = logicalSize * UiScale;
        config.PixelSnapH = false;
        config.OversampleH = 3;
        config.OversampleV = 2;
        return io.Fonts->AddFontDefault(&config);
    };
    ImFont *fallback = addDefault(14.0f);
    ImFont *labelFallback = addDefault(11.0f);
    ImFont *titleFallback = addDefault(30.0f);
    ImFont *heroFallback = addDefault(22.0f);
    ImFont *introFallback = addDefault(60.0f);
    const auto useOrSystem = [](const std::filesystem::path &preferred,
                                const std::filesystem::path &system) {
        return preferred.empty() ? system : preferred;
    };
    const std::filesystem::path regularInter = useOrSystem(files.interRegular, interfaceFallback);
    const std::filesystem::path mediumInter = useOrSystem(
        files.interMedium.empty() ? files.interRegular : files.interMedium, interfaceFallback);
    const std::filesystem::path boldInter = useOrSystem(
        files.interSemiBold.empty() ? files.interRegular : files.interSemiBold, interfaceFallback);
    const std::filesystem::path regularMono = useOrSystem(files.monoRegular, readoutFallback);
    const std::filesystem::path mediumMono = useOrSystem(
        files.monoMedium.empty() ? files.monoRegular : files.monoMedium, readoutFallback);
    ImFont *inter = addFont(regularInter, 14.0f);
    ImFont *medium = addFont(mediumInter, 11.0f);
    ImFont *semibold = addFont(boldInter, 30.0f);
    ImFont *mono = addFont(regularMono, 14.0f);
    ImFont *monoMedium = addFont(mediumMono, 14.0f);
    ImFont *smallMono = addFont(mediumMono, 11.0f);
    ImFont *heroMono = addFont(mediumMono, 22.0f);
    ImFont *introInter = addFont(boldInter, 60.0f);
    BodyFont = inter ? inter : fallback;
    LabelFont = medium ? medium : (inter ? inter : labelFallback);
    TitleFont = semibold ? semibold : (inter ? inter : titleFallback);
    DataFont = monoMedium ? monoMedium : (mono ? mono : fallback);
    SmallDataFont = smallMono ? smallMono : labelFallback;
    HeroFont = heroMono ? heroMono : heroFallback;
    IntroFont = introInter ? introInter : introFallback;
    if (!replacing) {
        const bool defaultUsed = !inter || !medium || !semibold || !mono || !monoMedium;
        if (defaultUsed)
            std::cerr << "Warning: one or more requested UI fonts were unavailable; using ImGui default for affected text.\n";
        const auto nameFor = [&](const std::filesystem::path &path, ImFont *font) {
            if (path.empty()) return "ImGui default";
            if (!font) return "unavailable";
            if (path == interfaceFallback) return "Segoe UI";
            if (path == readoutFallback) return "Consolas";
            if (path == files.interRegular || path == files.interMedium ||
                path == files.interSemiBold) return "Inter";
            return "JetBrains Mono";
        };
        std::cout << "UI fonts loaded: interface=" << nameFor(regularInter, inter)
                  << ", labels=" << nameFor(mediumInter, medium)
                  << ", title=" << nameFor(boldInter, semibold)
                  << ", readouts=" << nameFor(mediumMono, monoMedium)
                  << ", hero=" << nameFor(mediumMono, heroMono)
                  << ", intro=" << nameFor(boldInter, introInter) << std::endl;
    }
    io.Fonts->Build();
    if (replacing && !ImGui_ImplOpenGL3_CreateFontsTexture())
        std::cerr << "Warning: could not rebuild the DPI-scaled UI font texture.\n";
    (void)window;
}

float pixel(float value) { return std::round(value * DpiScale) / DpiScale; }
ImU32 color(ImVec4 value) { return ImGui::ColorConvertFloat4ToU32(value); }

void applyStyle() {
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(16.0f * UiScale, 16.0f * UiScale);
    style.FramePadding = ImVec2(8.0f * UiScale, 6.0f * UiScale);
    style.ItemSpacing = ImVec2(8.0f * UiScale, 8.0f * UiScale);
    style.WindowRounding = 16.0f * UiScale;
    style.ChildRounding = 12.0f * UiScale;
    style.FrameRounding = 10.0f * UiScale;
    style.PopupRounding = style.ScrollbarRounding = style.GrabRounding = 8.0f * UiScale;
    style.WindowBorderSize = 1.0f * DpiScale;
    style.ChildBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.ScrollbarSize = 6.0f * DpiScale;
    style.Colors[ImGuiCol_WindowBg] = Background;
    style.Colors[ImGuiCol_ChildBg] = Background;
    style.Colors[ImGuiCol_Border] = Hairline;
    style.Colors[ImGuiCol_Text] = PanelText;
    style.Colors[ImGuiCol_TextDisabled] = Secondary;
    style.Colors[ImGuiCol_FrameBg] = Background;
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1, 1, 1, 0.06f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1, 1, 1, 0.08f);
    style.Colors[ImGuiCol_Button] = Background;
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1, 1, 1, 0.06f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(1, 1, 1, 0.08f);
    style.Colors[ImGuiCol_Header] = Background;
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1, 1, 1, 0.06f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(1, 1, 1, 0.08f);
    style.Colors[ImGuiCol_CheckMark] = Accent;
    style.Colors[ImGuiCol_SliderGrab] = Accent;
    style.Colors[ImGuiCol_SliderGrabActive] = Accent;
    style.Colors[ImGuiCol_Separator] = Hairline;
    style.Colors[ImGuiCol_PopupBg] = Background;
}
}

void setup(GLFWwindow *window) {
    float sx = 1.0f, sy = 1.0f;
    glfwGetWindowContentScale(window, &sx, &sy);
    DpiScale = (sx + sy) * 0.5f;
    UiScale = DpiScale * kUiTextScale;
    buildFonts(window, false);
    ImGui_ImplOpenGL3_CreateFontsTexture();
    applyStyle();
}

void updateDpi(GLFWwindow *window) {
    float sx = 1.0f, sy = 1.0f;
    glfwGetWindowContentScale(window, &sx, &sy);
    const float scale = (sx + sy) * 0.5f;
    if (std::abs(scale - DpiScale) < 0.01f) return;
    DpiScale = scale;
    UiScale = DpiScale * kUiTextScale;
    buildFonts(window, true);
    applyStyle();
}

void beginFrame() { ImGui::PushFont(BodyFont); }
void textSecondary(const char *text) { ImGui::TextColored(Secondary, "%s", text); }

void HairlineRule() {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float y = pixel(p.y) + 0.5f / DpiScale;
    ImGui::GetWindowDrawList()->AddLine(ImVec2(pixel(p.x), y),
        ImVec2(pixel(p.x + ImGui::GetContentRegionAvail().x), y), color(Hairline), 1.0f / DpiScale);
    ImGui::Dummy(ImVec2(0, 5.0f * UiScale));
}

void SectionHeader(const char *index, const char *title) {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImFont *font = LabelFont ? LabelFont : ImGui::GetFont();
    const float size = font->FontSize;
    ImDrawList *draw = ImGui::GetWindowDrawList();
    const ImU32 tint = color(Secondary);
    draw->AddText(font, size, p, tint, index);
    float x = p.x + font->CalcTextSizeA(size, FLT_MAX, 0.0f, index).x + 8.0f * UiScale;
    for (const char *c = title; *c; ++c) {
        const char glyph[2] = {*c, '\0'};
        const float glyphWidth = font->CalcTextSizeA(size, FLT_MAX, 0.0f, glyph).x;
        draw->AddText(font, size, ImVec2(pixel(x), pixel(p.y)), tint, glyph);
        x += glyphWidth + size * 0.12f;
    }
    ImGui::Dummy(ImVec2(0, size + 4.0f * UiScale));
    HairlineRule();
}

void LabelValueRow(const char *label, const char *value, const char *unit) {
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImFont *labelFont = LabelFont ? LabelFont : ImGui::GetFont();
    const float labelSize = labelFont->FontSize;
    ImFont *font = DataFont ? DataFont : ImGui::GetFont();
    const float valueSize = font->FontSize;
    const ImVec2 labelExtent = labelFont->CalcTextSizeA(labelSize, FLT_MAX, 0.0f, label);
    const ImVec2 valueExtent = font->CalcTextSizeA(valueSize, FLT_MAX, 0.0f, value);
    const ImVec2 unitExtent = font->CalcTextSizeA(valueSize, FLT_MAX, 0.0f, unit);
    const float gap = *unit ? 6.0f * UiScale : 0.0f;
    const bool stacked = labelExtent.x + valueExtent.x + unitExtent.x + gap + 10.0f * UiScale > width;
    ImVec2 valuePos(p.x + width - valueExtent.x - unitExtent.x - gap, p.y);
    if (stacked) valuePos.y += labelSize + 3.0f * UiScale;
    if (LabelFont) ImGui::PushFont(LabelFont);
    ImGui::GetWindowDrawList()->AddText(LabelFont ? LabelFont : ImGui::GetFont(),
        LabelFont ? LabelFont->FontSize : ImGui::GetFontSize(), p, color(Secondary), label);
    if (LabelFont) ImGui::PopFont();
    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->AddText(font, valueSize, valuePos, color(PanelText), value);
    draw->AddText(font, valueSize, ImVec2(valuePos.x + valueExtent.x + gap, valuePos.y), color(Secondary), unit);
    ImGui::Dummy(ImVec2(0, (stacked ? labelSize + 3.0f * UiScale : 0.0f) +
                           std::max(valueSize, labelSize) + 4.0f * UiScale));
}

bool ToggleRow(const char *name, const char *key, bool state) {
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 size(width, 25.0f * UiScale);
    const bool clicked = ImGui::InvisibleButton(name, size);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList *draw = ImGui::GetWindowDrawList();
    if (hovered) draw->AddRectFilled(ImVec2(pixel(p.x), pixel(p.y)),
        ImVec2(pixel(p.x + size.x), pixel(p.y + size.y)), IM_COL32(255, 255, 255, 15));
    const float midY = p.y + size.y * 0.5f;
    const std::string label = std::string(name) + (key && *key ? "  [" + std::string(key) + "]" : "");
    const float labelSize = LabelFont ? LabelFont->FontSize : ImGui::GetFontSize();
    const float dataSize = DataFont ? DataFont->FontSize : ImGui::GetFontSize();
    draw->AddText(LabelFont ? LabelFont : ImGui::GetFont(),
        labelSize, ImVec2(p.x, midY - labelSize * 0.5f),
        color(PanelText), label.c_str());
    const float box = 8.0f * DpiScale;
    const ImVec2 boxMin(pixel(p.x + size.x - box - 36.0f * UiScale), pixel(midY - box * 0.5f));
    if (state) draw->AddRectFilled(boxMin, ImVec2(boxMin.x + box, boxMin.y + box), color(Accent));
    else draw->AddRect(boxMin, ImVec2(boxMin.x + box, boxMin.y + box), color(Secondary), 0, 0, 1.0f / DpiScale);
    draw->AddText(DataFont ? DataFont : ImGui::GetFont(), DataFont ? DataFont->FontSize : ImGui::GetFontSize(),
        ImVec2(boxMin.x + box + 8.0f * UiScale, midY - dataSize * 0.5f),
        color(state ? Accent : Secondary), state ? "ON" : "OFF");
    return clicked;
}

bool PrimaryButton(const char *label, ImVec2 size) {
    if (size.x <= 0) size.x = -1;
    if (size.y <= 0) size.y = 34.0f;
    ImGui::PushStyleColor(ImGuiCol_Button, Accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Accent);
    ImGui::PushStyleColor(ImGuiCol_Text, NearBlack);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f * UiScale);
    const bool clicked = ImGui::Button(label, ImVec2(size.x < 0 ? size.x : size.x * UiScale,
                                                      size.y * UiScale));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    return clicked;
}

bool GhostButton(const char *label, ImVec2 size) {
    if (size.x <= 0) size.x = -1;
    if (size.y <= 0) size.y = 32.0f;
    ImGui::PushStyleColor(ImGuiCol_Button, Background);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.06f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.08f));
    ImGui::PushStyleColor(ImGuiCol_Text, Accent);
    ImGui::PushStyleColor(ImGuiCol_Border, Hairline);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f * UiScale);
    const bool clicked = ImGui::Button(label, ImVec2(size.x < 0 ? size.x : size.x * UiScale,
                                                      size.y * UiScale));
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);
    return clicked;
}

void Tag(const char *text) {
    if (LabelFont) ImGui::PushFont(LabelFont);
    ImGui::PushStyleColor(ImGuiCol_Text, Accent);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
    if (LabelFont) ImGui::PopFont();
}

void header() {
    ImFont *font = LabelFont ? LabelFont : ImGui::GetFont();
    const float size = font->FontSize;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    float x = p.x;
    for (const char *c = "SOLAR SYSTEM 3D"; *c; ++c) {
        const char glyph[2] = {*c, '\0'};
        ImGui::GetWindowDrawList()->AddText(font, size, ImVec2(pixel(x), pixel(p.y)),
                                             color(PanelText), glyph);
        x += font->CalcTextSizeA(size, FLT_MAX, 0.0f, glyph).x + size * 0.12f;
    }
    ImGui::Dummy(ImVec2(0, size + 4.0f * UiScale));
    ImGui::TextColored(Secondary, "Computer Graphics · AITU");
    Tag("SIMULATION · STYLISED SCALE");
    HairlineRule();
}
} // namespace UiTheme
