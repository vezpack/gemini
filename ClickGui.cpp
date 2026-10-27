//
// Created by vastrakai on 6/29/2024.
//

#include "ClickGui.hpp"
#include "HudEditor.hpp"

#include <Features/Events/MouseEvent.hpp>
#include <Features/Events/KeyEvent.hpp>
#include <Features/Modules/ModuleManager.hpp>
#include <Features/Modules/Module.hpp>
#include <Features/Modules/Setting.hpp>

#include <SDK/Minecraft/ClientInstance.hpp>
#include <Utils/MiscUtils/ColorUtils.hpp>
#include <Utils/Keyboard.hpp>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

// Global state flag to sync instantly with MouseHook (eliminates frame lag and camera drift)
bool g_ClickGuiEnabled = false;

namespace
{
    bool gPreviousMouseGrabbed = false;
    bool gMouseStateSaved = false;

    ImU32 ToU32(ImVec4 color, float alpha = 1.0f)
    {
        color.w *= std::clamp<float>(alpha, 0.0f, 1.0f);
        return ImGui::ColorConvertFloat4ToU32(color);
    }

    std::string LowerString(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            }
        );

        return value;
    }

    bool PushClickGuiFont()
    {
        const auto regularIt = FontHelper::Fonts.find("mojangles");

        if (regularIt != FontHelper::Fonts.end() && regularIt->second)
        {
            ImGui::PushFont(regularIt->second);
            return true;
        }

        return false;
    }

    ImVec2 MeasureText(const char* text, float size)
    {
        ImFont* font = ImGui::GetFont();

        if (!text || !font)
            return ImVec2(0.0f, 0.0f);

        return font->CalcTextSizeA(
            size,
            FLT_MAX,
            0.0f,
            text
        );
    }

    void DrawText(
        ImDrawList* draw,
        const ImVec2& pos,
        float size,
        ImU32 color,
        const char* text,
        bool shadow)
    {
        if (!draw || !text)
            return;

        ImFont* font = ImGui::GetFont();

        if (!font)
            return;

        if (shadow)
        {
            draw->AddText(
                font,
                size,
                ImVec2(pos.x + 1.0f, pos.y + 1.0f),
                IM_COL32(0, 0, 0, 200),
                text
            );
        }

        draw->AddText(
            font,
            size,
            pos,
            color,
            text
        );
    }

    ImVec4 GetAccentColor(float verticalPos = 0.0f)
    {
        auto clickGui = gFeatureManager->mModuleManager->getModule<ClickGui>();
        if (!clickGui)
            return ImVec4(1.0f, 0.0f, 0.65f, 1.0f);

        ImVec4 c1 = clickGui->mColor1.getAsImColor().Value;
        ImVec4 c2 = clickGui->mColor2.getAsImColor().Value;
        ImVec4 c3 = clickGui->mColor3.getAsImColor().Value;

        float h1, s1, v1, h2, s2, v2, h3, s3, v3;
        ImGui::ColorConvertRGBtoHSV(c1.x, c1.y, c1.z, h1, s1, v1);
        ImGui::ColorConvertRGBtoHSV(c2.x, c2.y, c2.z, h2, s2, v2);
        ImGui::ColorConvertRGBtoHSV(c3.x, c3.y, c3.z, h3, s3, v3);

        float time = static_cast<float>(ImGui::GetTime()) * 0.4f;
        float cycle = fmodf(time, 3.0f);
        if (cycle < 0.0f) cycle += 3.0f;

        float h = h1, s = s1, v = v1;
        if (cycle < 1.0f) {
            float t = cycle;
            h = t * (h2 - h1) + h1;
            s = t * (s2 - s1) + s1;
            v = t * (v2 - v1) + v1;
        }
        else if (cycle < 2.0f) {
            float t = cycle - 1.0f;
            h = t * (h3 - h2) + h2;
            s = t * (s3 - s2) + s2;
            v = t * (v3 - v2) + v2;
        }
        else {
            float t = cycle - 2.0f;
            h = t * (h1 - h3) + h3;
            s = t * (s1 - s3) + s3;
            v = t * (v1 - v3) + v3;
        }

        float heightFactor = std::clamp((verticalPos - 45.0f) / 1200.0f, 0.0f, 0.45f);
        v = std::clamp(v - heightFactor, 0.2f, 1.0f);

        ImVec4 finalRgb;
        ImGui::ColorConvertHSVtoRGB(h, s, v, finalRgb.x, finalRgb.y, finalRgb.z);
        finalRgb.w = 1.0f;
        return finalRgb;
    }

    const std::string& GetDisplayName(Module* module)
    {
        static std::unordered_map<Module*, std::string> cache;

        auto found = cache.find(module);

        if (found != cache.end())
            return found->second;

        const std::string& source = module->mName;
        std::string result;

        for (size_t i = 0; i < source.size(); ++i)
        {
            const unsigned char current =
                static_cast<unsigned char>(source[i]);

            if (i > 0 && std::isupper(current))
            {
                const unsigned char previous =
                    static_cast<unsigned char>(source[i - 1]);

                const bool nextLower =
                    i + 1 < source.size() &&
                    std::islower(
                        static_cast<unsigned char>(source[i + 1])
                    );

                if (std::islower(previous) ||
                    std::isdigit(previous) ||
                    (std::isupper(previous) && nextLower))
                {
                    result.push_back(' ');
                }
            }

            result.push_back(source[i]);
        }

        return cache.emplace(module, result).first->second;
    }

    bool IsVisibleSetting(Setting* setting)
    {
        if (!setting)
            return false;

        if (setting->mIsVisible && !setting->mIsVisible())
            return false;

        if (setting->mHideOnClickGUI && *setting->mHideOnClickGUI)
            return false;

        return true;
    }

    std::vector<Setting*> GetSettings(Module* module)
    {
        std::vector<Setting*> result;

        if (!module)
            return result;

        for (Setting* setting : module->mSettings)
        {
            if (IsVisibleSetting(setting))
                result.push_back(setting);
        }

        return result;
    }

    namespace PraxStyle
    {
        constexpr int CategoryCount = 5;

        constexpr ModuleCategory Categories[CategoryCount] = {
            ModuleCategory::Combat,
            ModuleCategory::Visual,
            ModuleCategory::Movement,
            ModuleCategory::Player,
            ModuleCategory::Misc
        };

        constexpr const char* CategoryNames[CategoryCount] = {
            "Combat",
            "Visual",
            "Movement",
            "Player",
            "Misc"
        };

        constexpr float PanelWidth = 145.0f;
        constexpr float HeaderHeight = 24.0f;
        constexpr float ModuleHeight = 20.0f;
        constexpr float SettingHeight = 20.0f;
        constexpr float NumberHeight = 24.0f;
        constexpr float PanelGap = 8.0f;
        constexpr float PanelTop = 40.0f;
        constexpr float CornerRadius = 0.0f;

        float Scale = 1.0f;
        float Alpha = 1.0f;

        bool LayoutInitialized = false;
        ImVec2 LastScreenSize(0.0f, 0.0f);
        float LastScale = 0.0f;

        ImVec2 Positions[CategoryCount] = {};

        struct ModuleState
        {
            bool expanded = false;
            float expandedHeight = 0.0f;
            float enabledAnimation = 0.0f;
            ImVec4 textColor = ImVec4(0.85f, 0.85f, 0.88f, 1.0f);
        };

        std::unordered_map<Module*, ModuleState> ModuleStates;
        std::unordered_map<Setting*, float> ToggleAnimations;

        float S(float value)
        {
            return value * Scale;
        }

        float AnimationStep(float speed)
        {
            return std::clamp<float>(ImGui::GetIO().DeltaTime * speed, 0.0f, 1.0f);
        }

        ImU32 Color(int r, int g, int b, int a = 255)
        {
            return IM_COL32(r, g, b, a);
        }

        void ResetLayout()
        {
            LayoutInitialized = false;
        }

        void InitializeLayout(const ImVec2& screen)
        {
            const float width = S(PanelWidth);
            float gap = S(PanelGap);

            const float startX = 45.0f;

            for (int i = 0; i < CategoryCount; ++i)
            {
                Positions[i] = ImVec2(
                    std::round(startX + i * (width + gap)),
                    S(PanelTop)
                );
            }

            LastScreenSize = screen;
            LastScale = Scale;
            LayoutInitialized = true;
        }

        void DrawSetting(Setting* setting, float width, bool textShadow)
        {
            if (!IsVisibleSetting(setting))
                return;

            ImGui::PushID(setting);

            const bool numberSetting =
                setting->mType == SettingType::Number;

            const float height = S(
                numberSetting ? NumberHeight : SettingHeight
            );

            const ImVec2 pos = ImGui::GetCursorScreenPos();

            ImGui::InvisibleButton("##setting", ImVec2(width, height));

            const bool hovered = ImGui::IsItemHovered();
            const bool clicked =
                ImGui::IsItemClicked(ImGuiMouseButton_Left);
            const bool rightClicked =
                ImGui::IsItemClicked(ImGuiMouseButton_Right);

            ImDrawList* draw = ImGui::GetWindowDrawList();

            const ImVec2 end(pos.x + width, pos.y + height);

            if (hovered)
            {
                draw->AddRectFilled(
                    pos,
                    end,
                    Color(255, 255, 255, 25)
                );
            }

            const float pad = S(8.0f);
            const char* name = setting->mName.c_str();

            if (setting->mType == SettingType::Bool)
            {
                auto* value = static_cast<BoolSetting*>(setting);

                if (clicked)
                    value->mValue = !value->mValue;

                const ImVec2 textSize = MeasureText(name, S(13.0f));

                DrawText(
                    draw,
                    ImVec2(
                        pos.x + pad,
                        pos.y + (height - textSize.y) * 0.5f
                    ),
                    S(13.0f),
                    Color(220, 220, 230),
                    name,
                    textShadow
                );

                const float boxSize = S(10.0f);
                ImVec2 boxMin(end.x - pad - boxSize, pos.y + (height - boxSize) * 0.5f);
                ImVec2 boxMax(boxMin.x + boxSize, boxMin.y + boxSize);

                draw->AddRectFilled(boxMin, boxMax, Color(20, 20, 24, 255), S(2.0f));
                draw->AddRect(boxMin, boxMax, Color(80, 80, 90, 255), S(2.0f));

                if (value->mValue)
                {
                    ImVec2 checkCenter = ImVec2((boxMin.x + boxMax.x) * 0.5f, (boxMin.y + boxMax.y) * 0.5f);
                    ImU32 accentCol = ImGui::ColorConvertFloat4ToU32(GetAccentColor(pos.y));

                    draw->PathLineTo(ImVec2(checkCenter.x - S(3.0f), checkCenter.y));
                    draw->PathLineTo(ImVec2(checkCenter.x - S(1.0f), checkCenter.y + S(2.5f)));
                    draw->PathLineTo(ImVec2(checkCenter.x + S(3.5f), checkCenter.y - S(2.5f)));
                    draw->PathStroke(accentCol, 0, S(1.5f));
                }
            }
            else if (setting->mType == SettingType::Number)
            {
                auto* value = static_cast<NumberSetting*>(setting);

                const float minimum = static_cast<float>(value->mMin);
                const float maximum = static_cast<float>(value->mMax);
                const float range = maximum - minimum;

                const ImVec2 trackMin(
                    pos.x + pad,
                    pos.y + height - S(5.0f)
                );

                const ImVec2 trackMax(
                    end.x - pad,
                    pos.y + height - S(3.0f)
                );

                const float trackWidth = trackMax.x - trackMin.x;

                if (range > 0.0f &&
                    trackWidth > 0.0f &&
                    ImGui::IsItemActive() &&
                    ImGui::IsMouseDown(ImGuiMouseButton_Left))
                {
                    const float ratio = std::clamp<float>(
                        (ImGui::GetIO().MousePos.x - trackMin.x) /
                        trackWidth,
                        0.0f, 1.0f
                    );

                    value->setValue(minimum + ratio * range);
                }

                const float current =
                    static_cast<float>(value->mValue);

                const float ratio = range > 0.0f
                    ? std::clamp<float>((current - minimum) / range, 0.0f, 1.0f)
                    : 0.0f;

                char buffer[32];
                if (value->mStep >= 1.0)
                    std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(current));
                else
                    std::snprintf(buffer, sizeof(buffer), "%.1f", current);

                const ImVec2 valueSize = MeasureText(buffer, S(13.0f));

                DrawText(
                    draw,
                    ImVec2(pos.x + pad, pos.y + S(1.0f)),
                    S(13.0f),
                    Color(220, 220, 230),
                    name,
                    textShadow
                );

                DrawText(
                    draw,
                    ImVec2(end.x - pad - valueSize.x, pos.y + S(1.0f)),
                    S(13.0f),
                    Color(170, 170, 185),
                    buffer,
                    textShadow
                );

                if (ratio > 0.0f && trackWidth > 0.0f)
                {
                    draw->AddRectFilled(
                        trackMin,
                        ImVec2(
                            trackMin.x + trackWidth * ratio,
                            trackMax.y
                        ),
                        ImGui::ColorConvertFloat4ToU32(
                            GetAccentColor(pos.y)
                        )
                    );
                }
            }
            else if (setting->mType == SettingType::Enum)
            {
                auto* value = dynamic_cast<EnumSettingBase*>(setting);

                const ImVec2 textSize = MeasureText(name, S(13.0f));

                DrawText(
                    draw,
                    ImVec2(
                        pos.x + pad,
                        pos.y + (height - textSize.y) * 0.5f
                    ),
                    S(13.0f),
                    Color(220, 220, 230),
                    name,
                    textShadow
                );

                if (value && value->getOptionCount() > 0)
                {
                    const int count = value->getOptionCount();

                    const int current = std::clamp<int>(
                        value->getIndex(),
                        0,
                        count - 1
                    );

                    if (clicked)
                        value->setIndex((current + 1) % count);
                    else if (rightClicked)
                        value->setIndex((current - 1 + count) % count);

                    const std::string option =
                        value->getOptionName(value->getIndex());

                    const ImVec2 optionSize =
                        MeasureText(option.c_str(), S(12.0f));

                    DrawText(
                        draw,
                        ImVec2(
                            end.x - pad - optionSize.x,
                            pos.y + (height - optionSize.y) * 0.5f
                        ),
                        S(12.0f),
                        ImGui::ColorConvertFloat4ToU32(
                            GetAccentColor(pos.y)
                        ),
                        option.c_str(),
                        textShadow
                    );
                }
            }
            else if (setting->mType == SettingType::Color)
            {
                auto* value = static_cast<ColorSetting*>(setting);

                const ImVec2 textSize = MeasureText(name, S(13.0f));

                DrawText(
                    draw,
                    ImVec2(
                        pos.x + pad,
                        pos.y + (height - textSize.y) * 0.5f
                    ),
                    S(13.0f),
                    Color(220, 220, 230),
                    name,
                    textShadow
                );

                ImColor selected = value->getAsImColor();
                ImVec4 color = selected.Value;

                const float swatchHeight = S(12.0f);

                const ImVec2 swatchMin(
                    end.x - pad - S(20.0f),
                    pos.y + (height - swatchHeight) * 0.5f
                );

                const ImVec2 swatchMax(
                    end.x - pad,
                    swatchMin.y + swatchHeight
                );

                draw->AddRectFilled(
                    swatchMin,
                    swatchMax,
                    selected,
                    S(2.0f)
                );

                if (clicked)
                    ImGui::OpenPopup("##ColorPicker");

                if (ImGui::BeginPopup("##ColorPicker"))
                {
                    if (ImGui::ColorPicker4(
                        "##Picker",
                        &color.x,
                        ImGuiColorEditFlags_NoSidePreview
                    ))
                    {
                        value->setValue(
                            color.x,
                            color.y,
                            color.z,
                            color.w
                        );
                    }

                    ImGui::EndPopup();
                }
            }
            else
            {
                const ImVec2 textSize = MeasureText(name, S(13.0f));

                DrawText(
                    draw,
                    ImVec2(
                        pos.x + pad,
                        pos.y + (height - textSize.y) * 0.5f
                    ),
                    S(13.0f),
                    Color(220, 220, 230),
                    name,
                    textShadow
                );
            }

            ImGui::PopID();
        }

        void DrawModule(Module* module, float width, bool textShadow)
        {
            if (!module)
                return;

            ImGui::PushID(module);

            ModuleState& state = ModuleStates[module];
            std::vector<Setting*> settings = GetSettings(module);

            if (settings.empty())
                state.expanded = false;

            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const float height = S(ModuleHeight);

            ImGui::InvisibleButton("##module", ImVec2(width, height));

            const bool hovered = ImGui::IsItemHovered();
            const bool leftClicked =
                ImGui::IsItemClicked(ImGuiMouseButton_Left);
            const bool rightClicked =
                ImGui::IsItemClicked(ImGuiMouseButton_Right);

            const ImVec2 end(pos.x + width, pos.y + height);

            if (leftClicked)
                module->toggle();

            if (rightClicked && !settings.empty())
                state.expanded = !state.expanded;

            state.enabledAnimation = std::clamp<float>(
                state.enabledAnimation + (ImGui::GetIO().DeltaTime * 16.0f) * ((module->mEnabled ? 1.0f : 0.0f) - state.enabledAnimation),
                0.0f, 1.0f
            );

            ImDrawList* draw = ImGui::GetWindowDrawList();

            if (state.enabledAnimation > 0.01f)
            {
                const ImVec4 activeBg = GetAccentColor(pos.y);

                draw->AddRectFilled(
                    pos,
                    end,
                    ToU32(activeBg, state.enabledAnimation)
                );
            }
            else if (hovered)
            {
                draw->AddRectFilled(
                    pos,
                    end,
                    Color(255, 255, 255, 25)
                );
            }

            const ImVec4 targetText = module->mEnabled
                ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
                : (hovered
                    ? ImVec4(0.95f, 0.95f, 0.98f, 1.0f)
                    : ImVec4(0.85f, 0.85f, 0.90f, 1.0f));

            state.textColor = ImVec4(
                state.textColor.x + (ImGui::GetIO().DeltaTime * 14.0f) * (targetText.x - state.textColor.x),
                state.textColor.y + (ImGui::GetIO().DeltaTime * 14.0f) * (targetText.y - state.textColor.y),
                state.textColor.z + (ImGui::GetIO().DeltaTime * 14.0f) * (targetText.z - state.textColor.z),
                state.textColor.w + (ImGui::GetIO().DeltaTime * 14.0f) * (targetText.w - state.textColor.w)
            );

            const std::string& label = GetDisplayName(module);
            const ImVec2 textSize = MeasureText(label.c_str(), S(13.0f));

            DrawText(
                draw,
                ImVec2(
                    pos.x + (width - textSize.x) * 0.5f,
                    pos.y + (height - textSize.y) * 0.5f
                ),
                S(13.0f),
                ImGui::ColorConvertFloat4ToU32(state.textColor),
                label.c_str(),
                textShadow
            );

            float targetHeight = 0.0f;

            if (state.expanded)
            {
                targetHeight = S(1.5f);

                for (Setting* setting : settings)
                {
                    targetHeight += S(
                        setting->mType == SettingType::Number
                        ? NumberHeight
                        : SettingHeight
                    );
                }
                targetHeight += S(1.5f);
            }

            state.expandedHeight = std::clamp<float>(
                state.expandedHeight + (ImGui::GetIO().DeltaTime * 14.0f) * (targetHeight - state.expandedHeight),
                0.0f, 1000.0f
            );

            if (std::fabs(state.expandedHeight - targetHeight) < 0.5f)
                state.expandedHeight = targetHeight;

            if (state.expandedHeight > 0.5f)
            {
                ImGui::BeginChild(
                    "##Settings",
                    ImVec2(width, state.expandedHeight),
                    false,
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoBackground
                );

                const ImVec2 childPos = ImGui::GetCursorScreenPos();

                ImDrawList* childDraw = ImGui::GetWindowDrawList();
                const ImVec2 childMax(childPos.x + width, childPos.y + state.expandedHeight);

                childDraw->AddRectFilled(
                    childPos,
                    childMax,
                    Color(35, 35, 40, 255)
                );

                ImGui::Dummy(ImVec2(width, S(1.0f)));

                for (Setting* setting : settings)
                    DrawSetting(setting, width, textShadow);

                ImGui::Dummy(ImVec2(width, S(1.0f)));

                ImGui::EndChild();
            }

            ImGui::PopID();
        }

        void DrawCategory(
            int index,
            const ImVec2& screen,
            bool textShadow)
        {
            std::vector<Module*> modules;

            if (!gFeatureManager || !gFeatureManager->mModuleManager)
                return;

            for (const auto& owned :
                gFeatureManager->mModuleManager->getModules())
            {
                Module* module = owned.get();

                if (module &&
                    module->mCategory == Categories[index])
                {
                    modules.push_back(module);
                }
            }

            std::ranges::sort(modules, [](Module* a, Module* b) {
                return a->mName < b->mName;
                });

            const float width = S(PanelWidth);

            float contentHeight = 0.0f;

            for (Module* module : modules)
            {
                contentHeight += S(ModuleHeight);

                auto found = ModuleStates.find(module);

                if (found != ModuleStates.end())
                    contentHeight += found->second.expandedHeight;
            }

            const float panelHeight = std::min<float>(
                S(HeaderHeight) + contentHeight + S(2.0f),
                screen.y * 0.88f
            );

            ImGui::SetNextWindowPos(
                Positions[index],
                ImGuiCond_Always
            );

            ImGui::SetNextWindowSize(
                ImVec2(width, std::max<float>(S(HeaderHeight), panelHeight)),
                ImGuiCond_Always
            );

            ImGui::PushStyleVar(
                ImGuiStyleVar_WindowPadding,
                ImVec2(0.0f, 0.0f)
            );

            ImGui::PushStyleVar(
                ImGuiStyleVar_WindowBorderSize,
                0.0f
            );

            ImGui::PushStyleVar(
                ImGuiStyleVar_WindowRounding,
                S(CornerRadius)
            );

            ImGui::PushStyleVar(
                ImGuiStyleVar_ItemSpacing,
                ImVec2(0.0f, 0.0f)
            );

            const std::string title =
                std::string(CategoryNames[index]) + "##SwissPanel";

            if (ImGui::Begin(
                title.c_str(),
                nullptr,
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoBackground |
                ImGuiWindowFlags_NoSavedSettings
            ))
            {
                ImDrawList* draw = ImGui::GetWindowDrawList();
                const ImVec2 windowPos = ImGui::GetWindowPos();

                const ImVec2 windowMax(
                    windowPos.x + width,
                    windowPos.y + panelHeight
                );

                draw->AddRectFilled(
                    windowPos,
                    windowMax,
                    Color(45, 45, 50, 255),
                    S(CornerRadius)
                );

                draw->AddRectFilled(
                    windowPos,
                    ImVec2(
                        windowMax.x,
                        windowPos.y + S(HeaderHeight)
                    ),
                    Color(0, 0, 0, 255),
                    S(CornerRadius)
                );

                ImGui::Dummy(ImVec2(width, S(HeaderHeight)));

                const float headerTextSize = S(14.0f);

                const ImVec2 titleSize = MeasureText(
                    CategoryNames[index],
                    headerTextSize
                );

                DrawText(
                    draw,
                    ImVec2(
                        windowPos.x + (width - titleSize.x) * 0.5f,
                        windowPos.y +
                        (S(HeaderHeight) - titleSize.y) * 0.5f
                    ),
                    headerTextSize,
                    Color(255, 255, 255),
                    CategoryNames[index],
                    textShadow
                );

                ImGui::BeginChild(
                    "##Modules",
                    ImVec2(width, 0.0f),
                    false,
                    ImGuiWindowFlags_NoBackground |
                    ImGuiWindowFlags_NoScrollbar
                );

                for (Module* module : modules)
                    DrawModule(module, width, textShadow);

                if (modules.empty())
                    ImGui::Dummy(ImVec2(width, S(4.0f)));

                ImGui::EndChild();
            }

            ImGui::End();
            ImGui::PopStyleVar(4);
        }

        void Render(
            float animation,
            float scale,
            bool textShadow)
        {
            if (!gFeatureManager || !gFeatureManager->mModuleManager)
                return;

            ImGuiIO& io = ImGui::GetIO();
            const ImVec2 screen = io.DisplaySize;

            if (screen.x <= 0.0f || screen.y <= 0.0f)
                return;

            Scale = std::clamp<float>(scale, 0.5f, 2.5f);
            Alpha = std::clamp<float>(animation, 0.0f, 1.0f);

            if (!LayoutInitialized ||
                screen.x != LastScreenSize.x ||
                screen.y != LastScreenSize.y ||
                Scale != LastScale)
            {
                InitializeLayout(screen);
            }

            ImGui::GetBackgroundDrawList()->AddRectFilled(
                ImVec2(0.0f, 0.0f),
                screen,
                Color(0, 0, 0, static_cast<int>(120.0f * Alpha))
            );

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, Alpha);

            for (int i = 0; i < CategoryCount; ++i)
                DrawCategory(i, screen, textShadow);

            ImGui::PopStyleVar();
        }
    }
}

void ClickGui::onEnable()
{
    g_ClickGuiEnabled = true;

    auto ci = ClientInstance::get();
    if (!ci)
        return;

    gPreviousMouseGrabbed = ci->getMouseGrabbed();
    gMouseStateSaved = true;

    ci->releaseMouse();

    ImGui::GetIO().MouseDrawCursor = false;
}

void ClickGui::onDisable()
{
    g_ClickGuiEnabled = false;

    ImGui::GetIO().MouseDrawCursor = false;

    auto ci = ClientInstance::get();
    if (ci)
    {
        if (gMouseStateSaved)
        {
            if (gPreviousMouseGrabbed)
                ci->grabMouse();
            else
                ci->releaseMouse();
        }
    }

    if (ImGui::GetCurrentContext() != nullptr)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.MouseDown[0] = false;
        io.MouseDown[1] = false;
        io.MouseDown[2] = false;
        io.MouseWheel = 0.0f;
        io.MouseWheelH = 0.0f;
    }

    gMouseStateSaved = false;
}

void ClickGui::onWindowResizeEvent(WindowResizeEvent& event)
{
    PraxStyle::ResetLayout();
}

void ClickGui::onMouseEvent(MouseEvent& event)
{
}

float ClickGui::getEaseAnim(EasingUtil ease, int mode)
{
    switch (mode)
    {
    case 0:
        return ease.easeOutExpo();

    case 1:
        return mEnabled ? ease.easeOutElastic() : ease.easeOutBack();

    case 2:
        return ease.linear();

    default:
        return ease.easeOutExpo();
    }
}

void ClickGui::onRenderEvent(RenderEvent& event)
{
    static float animation = 0.0f;
    static EasingUtil inEase = EasingUtil();

    static float guiScale = 1.0f;

    if (!(ImGui::IsMouseDown(0) || ImGui::IsMouseDown(2)))
        guiScale = mGuiScale.mValue;

    const float delta = ImGui::GetIO().DeltaTime;
    const float speed = delta * 15.0f;

    if (this->mEnabled)
        inEase.incrementPercentage(speed / 10.0f);
    else
        inEase.decrementPercentage(speed / 5.0f);

    animation = std::clamp<float>(inEase.easeOutExpo(), 0.0f, 1.0f);

    if (animation < 0.0001f && !mEnabled)
        return;

    const bool clickGuiFontPushed = PushClickGuiFont();

    PraxStyle::Render(
        animation,
        guiScale,
        mTextShadow.mValue
    );

    if (clickGuiFontPushed)
        ImGui::PopFont();
}