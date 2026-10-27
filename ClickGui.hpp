#pragma once

//
// Created by vastrakai on 6/29/2024.
//

#include <Features/FeatureManager.hpp>
#include <Features/Modules/Setting.hpp>
#include <Features/GUI/ModernDropdown.hpp>

#include <string>

class ClickGui : public ModuleBase<ClickGui>
{
public:
    BoolSetting mTextShadow =
        BoolSetting(
            "Text Shadow",
            "Whether to draw text with a shadow in the ClickGUI.",
            true
        );

    NumberSetting mGuiScale =
        NumberSetting(
            "Gui Scale",
            "The scale of the GUI.",
            1.0f,
            0.5f,
            1.0f,
            0.01f
        );

    ColorSetting mColor1 =
        ColorSetting(
            "Color 1",
            "First color in the cycle.",
            1.0f, 0.0f, 0.65f, 1.0f // Default Pink
        );

    ColorSetting mColor2 =
        ColorSetting(
            "Color 2",
            "Second color in the cycle.",
            0.0f, 0.5f, 1.0f, 1.0f // Default Blue
        );

    ColorSetting mColor3 =
        ColorSetting(
            "Color 3",
            "Third color in the cycle.",
            1.0f, 0.2f, 0.2f, 1.0f // Default Red
        );

    ClickGui()
        : ModuleBase(
            "ClickGui",
            "A customizable GUI for toggling modules.",
            ModuleCategory::Visual,
            VK_TAB,
            false
        )
    {
        gFeatureManager->mDispatcher->listen<
            RenderEvent,
            &ClickGui::onRenderEvent,
            nes::event_priority::LAST
        >(this);

        gFeatureManager->mDispatcher->listen<
            WindowResizeEvent,
            &ClickGui::onWindowResizeEvent
        >(this);

        addSetting(&mTextShadow);
        addSetting(&mGuiScale);
        addSetting(&mColor1);
        addSetting(&mColor2);
        addSetting(&mColor3);

        mNames = {
            {Lowercase, "clickgui"},
            {LowercaseSpaced, "click gui"},
            {Normal, "ClickGui"},
            {NormalSpaced, "Click Gui"}
        };
    }

    void onEnable() override;
    void onDisable() override;

    void onWindowResizeEvent(class WindowResizeEvent& event);
    void onMouseEvent(class MouseEvent& event);
    void onKeyEvent(class KeyEvent& event);

    bool isBinding() const;
    float getEaseAnim(EasingUtil ease, int mode);

    void onRenderEvent(class RenderEvent& event);
};