//
// Created by lilycat on 23/07/2026
//

#include "Watermark.hpp"
#include "Interface.hpp"
#include <Utils/FontHelper.hpp>
#include <Utils/MiscUtils/ImRenderUtils.hpp>

#include <algorithm>
#include <cmath>

namespace
{
    float x = 20.0f;
    float y = 8.5f;
    const std::string ClientName = "aeolus v3 - vezpack";
}

void Watermark::onEnable()
{
    mElement->mVisible = true;
}

void Watermark::onDisable()
{
    mElement->mVisible = false;
}

void Watermark::onRenderEvent(RenderEvent& event)
{
    if (!ImGui::GetCurrentContext())
        return;

    if (!mElement->mVisible)
        return;

    mElement->mAnchor = HudElement::Anchor::TopLeft;
    mElement->mPos = { x, y };
    const ImVec2 position = { x, y };

    // Use true for crisp, anti-aliased font rendering (cleaner & smoother text)
    FontHelper::pushPrefFont(true, false);
    auto* font = ImGui::GetFont();
    if (!font)
    {
        FontHelper::popPrefFont();
        return;
    }

    const std::string text = ClientName;
    const float fontSize = 25.f;
    const float padX = 6.0f;
    const float padY = 3.0f;
    const ImVec2 position2 = { x - 11.5f, y };
    const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, text.c_str());
    const ImVec2 boxSize = { padX + textSize.x + padX, padY + textSize.y + padY };
    const ImVec2 boxPos = position2;
    const ImVec2 boxEnd = { boxPos.x + boxSize.x, boxPos.y + boxSize.y };
    mElement->mSize = glm::vec2(boxSize.x, boxSize.y);

    auto* drawList = ImGui::GetBackgroundDrawList();

    // Restored original background box style (sharp rect & shadow settings)
    drawList->AddShadowRect(boxPos, boxEnd, ImColor(0.f, 0.f, 0.f, 0.55f), 25, ImVec2(0, 0));
    drawList->AddRectFilled(boxPos, boxEnd, ImColor(0.f, 0.f, 0.f, 0.45f), 0, 0);

    const ImVec2 textPos = { boxPos.x + padX, boxPos.y + (boxSize.y - textSize.y) / 2.0f };

    // Hardcoded pure white color (bypasses all interface/rainbow theme options entirely)
    const ImColor color = ImColor(255, 255, 255, 255);

    auto* interfaceModule = gFeatureManager->mModuleManager->getModule<Interface>();

    if (interfaceModule && interfaceModule->mFont.as<int>() == 0)
        ImRenderUtils::drawShadowText(drawList, text, textPos, IM_COL32_BLACK, fontSize, false, false);

    // Draw text strictly in white
    ImRenderUtils::drawShadowText(drawList, text, textPos, color, fontSize, false, false, true);

    FontHelper::popPrefFont();
}