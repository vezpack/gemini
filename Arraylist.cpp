#include "Arraylist.hpp"
#include <Features/FeatureManager.hpp>
#include <Features/Modules/Visual/Interface.hpp>
#include <Utils/MiscUtils/ColorUtils.hpp>
#include <algorithm>
#include <cmath>

void Arraylist::onEnable()
{
    gFeatureManager->mDispatcher->listen<RenderEvent, &Arraylist::onRenderEvent>(this);
}

void Arraylist::onDisable()
{
    gFeatureManager->mDispatcher->deafen<RenderEvent, &Arraylist::onRenderEvent>(this);

    for (auto& mod : gFeatureManager->mModuleManager->getModules())
    {
        mod->mArrayListAnim = 0.f;
    }
}

void Arraylist::onRenderEvent(RenderEvent& event)
{
    bool backgroundEnabled = mBackground.mValue;
    bool glowEnabled = mGlow.mValue;
    bool lineEnabled = mLine.mValue;
    bool rainbowEnabled = mRainbow.mValue;
    bool renderModeEnabled = mRenderMode.mValue;
    float fontSize = mFontSize.mValue;
    float spacingAdd = mModuleSpacing.mValue;

    // Line thickness value for the accent indicator
    float lineThickness = 2.5f;

    static std::vector<std::shared_ptr<Module>> module;
    auto drawList = ImGui::GetForegroundDrawList();

    static auto daInterface = gFeatureManager->mModuleManager->getModule<Interface>();
    if (!daInterface) return;

    FontHelper::pushPrefFont(true, false, false);

    if (module.empty())
    {
        module = gFeatureManager->mModuleManager->getModules();
    }

    std::ranges::sort(module, [fontSize, renderModeEnabled](const std::shared_ptr<Module>& a, const std::shared_ptr<Module>& b)
        {
            std::string aName = a->getName();
            std::string bName = b->getName();
            if (renderModeEnabled && !a->getSettingDisplayText().empty()) aName += " " + a->getSettingDisplayText();
            if (renderModeEnabled && !b->getSettingDisplayText().empty()) bName += " " + b->getSettingDisplayText();
            return ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0, aName.c_str()).x >
                ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0, bName.c_str()).x;
        });

    float maxWidth = 0.f;
    float totalHeight = 0.f;

    for (auto& mod : module)
    {
        if (!mod->mVisibleInArrayList.mValue) continue;

        mod->mArrayListAnim = MathUtils::lerp(mod->mArrayListAnim, mod->mEnabled ? 1.f : 0.f, ImGui::GetIO().DeltaTime * 14.f);
        mod->mArrayListAnim = MathUtils::clamp(mod->mArrayListAnim, 0.f, 1.f);
        if (mod->mArrayListAnim < 0.01f) continue;

        std::string name = mod->getName();
        std::string settingDisplay = renderModeEnabled ? mod->getSettingDisplayText() : "";
        std::string fullText = name + (settingDisplay.empty() ? "" : " " + settingDisplay);

        ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0, fullText.c_str());

        if (textSize.x > maxWidth) maxWidth = textSize.x;
        totalHeight += (textSize.y + spacingAdd) * mod->mArrayListAnim;
    }

    maxWidth += 16.f;

    if (totalHeight == 0.f) {
        if (HudEditor::gInstance && HudEditor::gInstance->mEnabled) {
            mHudElement.mSize = { 150.f, 25.f };
        }
        else {
            mHudElement.mSize = { 0.f, 0.f };
        }
    }
    else {
        mHudElement.mSize = { maxWidth, totalHeight };
    }

    ImVec2 basePos = mHudElement.getPos();
    float currentY = basePos.y;

    // Use ImU32 for global rainbow color to match IM_COL32 seamlessly
    ImU32 globalRainbowColor = IM_COL32(255, 255, 255, 255);
    if (rainbowEnabled)
    {
        float hue = fmodf(static_cast<float>(ImGui::GetTime()) * 0.2f, 1.0f);
        ImVec4 rainbowRgb;
        ImGui::ColorConvertHSVtoRGB(hue, 1.0f, 1.0f, rainbowRgb.x, rainbowRgb.y, rainbowRgb.z);
        rainbowRgb.w = 1.0f;
        globalRainbowColor = ImGui::ColorConvertFloat4ToU32(rainbowRgb);
    }

    for (auto& mod : module)
    {
        if (!mod->mVisibleInArrayList.mValue) continue;
        if (mod->mArrayListAnim < 0.01f) continue;

        std::string name = mod->getName();
        std::string settingDisplay = renderModeEnabled ? mod->getSettingDisplayText() : "";
        std::string fullText = name + (settingDisplay.empty() ? "" : " " + settingDisplay);

        ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0, fullText.c_str());

        float anim = mod->mArrayListAnim;
        float renderWidth = textSize.x * anim;
        float renderHeight = textSize.y;

        float rightBound = basePos.x + mHudElement.mSize.x;
        float rowLeft = rightBound - renderWidth - 12.f;
        float rowRight = rightBound;

        ImVec4 rect = {
            rowLeft - 4.f,
            currentY,
            rowRight,
            currentY + renderHeight + spacingAdd
        };

        if (glowEnabled && backgroundEnabled)
        {
            drawList->AddShadowRect(
                ImVec2(rect.x, rect.y),
                ImVec2(rect.z, rect.w),
                ImColor(0.f, 0.f, 0.f, 0.5f * anim),
                15.0f * anim,
                ImVec2(0.f, 0.f),
                0,
                8
            );
        }

        if (backgroundEnabled)
        {
            drawList->AddRectFilled(
                ImVec2(rect.x, rect.y),
                ImVec2(rect.z, rect.w),
                ImColor(10, 10, 12, static_cast<int>(160.f * anim))
            );
        }

        ImVec2 textPos = ImVec2(rect.x + 6.f, rect.y + (spacingAdd * 0.5f));

        // Both operands are now ImU32 types, fixing the ternary operator ambiguity
        ImU32 moduleColor = rainbowEnabled ? globalRainbowColor : IM_COL32(255, 255, 255, 255);

        if (lineEnabled)
        {
            float lineX = rect.z - lineThickness;
            drawList->AddRectFilled(
                ImVec2(lineX, rect.y),
                ImVec2(lineX + lineThickness, rect.w),
                moduleColor
            );
        }

        drawList->AddText(ImGui::GetFont(), fontSize, textPos, moduleColor, name.c_str());

        if (!settingDisplay.empty())
        {
            ImVec2 nameSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0, (name + " ").c_str());
            ImVec2 settingPos = ImVec2(textPos.x + nameSize.x, textPos.y);
            drawList->AddText(ImGui::GetFont(), fontSize, settingPos, IM_COL32(200, 200, 200, 255), settingDisplay.c_str());
        }

        currentY += (textSize.y + spacingAdd) * anim;
    }

    ImGui::PopFont();
}