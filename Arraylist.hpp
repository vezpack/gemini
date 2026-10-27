#pragma once
#include <Features/Modules/Module.hpp>
#include "HudEditor.hpp"

class Arraylist : public ModuleBase<Arraylist>
{
public:
    HudElement mHudElement;

    BoolSetting mBackground = BoolSetting("Background", "Enables the background fill", true);
    BoolSetting mGlow = BoolSetting("Glow", "Enables a soft black glow around the background", true);
    BoolSetting mLine = BoolSetting("Line", "Enables a basic accent line next to the text", true);
    BoolSetting mRainbow = BoolSetting("Rainbow", "Cycles colors smoothly through a rainbow spectrum", false);
    BoolSetting mRenderMode = BoolSetting("Render Mode", "Renders the module mode next to the module name", true);
    NumberSetting mFontSize = NumberSetting("Font Size", "The size of the font", 25.f, 10.f, 40.f, 0.01f);
    NumberSetting mModuleSpacing = NumberSetting("Module Spacing", "Spacing between modules", 2.f, 0.f, 15.f, 0.5f);

    Arraylist() : ModuleBase("Arraylist", "Displays a list of modules", ModuleCategory::Visual, 0, true), mHudElement((char*)"Arraylist") {
        addSettings(
            &mBackground,
            &mGlow,
            &mLine,
            &mRainbow,
            &mRenderMode,
            &mFontSize,
            &mModuleSpacing
        );

        mNames = {
            {Lowercase, "arraylist"},
            {LowercaseSpaced, "array list"},
            {Normal, "Arraylist"},
            {NormalSpaced, "Array List"}
        };

        mHudElement.mParentTypeIdentifier = (char*)"Arraylist";
        mHudElement.mVisible = true;
        mHudElement.mAnchor = HudElement::Anchor::TopRight;
        mHudElement.mPos = { -20.f, 20.f };
        if (HudEditor::gInstance) {
            HudEditor::gInstance->registerElement(&mHudElement);
        }
    }

    void onEnable() override;
    void onDisable() override;

    void onRenderEvent(class RenderEvent& event);
};