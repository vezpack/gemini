#pragma once

#include <Features/Modules/Module.hpp>
#include <Features/Modules/Setting.hpp>
#include <Features/Events/RenderEvent.hpp>
#include <Features/Events/ActorRenderEvent.hpp>
#include <Features/Events/BaseTickEvent.hpp>
#include <Features/Events/ModuleStateChangeEvent.hpp>
#include <Features/Events/DrawImageEvent.hpp>
#include <Features/Events/PreGameCheckEvent.hpp>
#include <Features/Events/LevelRenderEvent.hpp>
#include <Features/Events/PacketInEvent.hpp>
#include <Features/Events/PacketOutEvent.hpp>
#include <unordered_map>
#include <vector>

class Interface : public ModuleBase<Interface>
{
public:
    enum ColorTheme {
        Trans,
        Rainbow,
        Bubblegum,
        Watermelon,
        Sunset,
        Poison,
        Custom
    };

    enum class FontType {
        Mojangles,
        ProductSans,
        Lexend,
        AsapSharp
    };

    enum class GradientType {
        Vertical,
        Diagonal,
        Horizontal
    };

    EnumSettingT<NamingStyle> mNamingStyle =
        EnumSettingT<NamingStyle>(
            "Naming",
            "The style of the module names.",
            NamingStyle::NormalSpaced,
            "lowercase",
            "lower spaced",
            "Normal",
            "Spaced"
        );

    // Member variables restored to satisfy external files (ColorUtils, FontHelper, Watermark, Scaffold)
    EnumSettingT<ColorTheme> mMode = EnumSettingT<ColorTheme>("Theme", "Theme", ColorTheme::Rainbow, "Trans", "Rainbow", "Bubblegum", "Watermelon", "Sunset", "Poison", "Custom");
    EnumSettingT<GradientType> mGradientType = EnumSettingT<GradientType>("Gradient Type", "Type", GradientType::Diagonal, "Vertical", "Diagonal", "Horizontal");
    NumberSetting mGradientSeparation = NumberSetting("Gradient Separation", "Separation", 120.f, 10.f, 500.f, 1.f);
    EnumSettingT<FontType> mFont = EnumSettingT<FontType>("Font", "Font", FontType::Mojangles, "Mojangles", "Product Sans", "Lexend Medium", "Asap Sharp");
    NumberSetting mColors = NumberSetting("Colors", "Colors", 3, 1, 6, 1);
    ColorSetting mColor1 = ColorSetting("Color 1", "Color", 0xFFFF0000);
    ColorSetting mColor2 = ColorSetting("Color 2", "Color", 0xFFFF7F00);
    ColorSetting mColor3 = ColorSetting("Color 3", "Color", 0xFFFFD600);
    ColorSetting mColor4 = ColorSetting("Color 4", "Color", 0xFF00FF00);
    ColorSetting mColor5 = ColorSetting("Color 5", "Color", 0xFF0000FF);
    ColorSetting mColor6 = ColorSetting("Color 6", "Color", 0xFF8B00FF);
    NumberSetting mColorSpeed = NumberSetting("Color Speed", "Speed", 3.f, 0.01f, 20.f, 0.01f);
    NumberSetting mSaturation = NumberSetting("Saturation", "Saturation", 1.f, 0.f, 1.f, 0.01f);
    ColorSetting mListColorTop = ColorSetting("List Top Color", "Top", 0xFFB4FF32);
    ColorSetting mListColorMid = ColorSetting("List Middle Color", "Mid", 0xFF32FFAA);
    ColorSetting mListColorBottom = ColorSetting("List Bottom Color", "Bottom", 0xFF3264FF);
    BoolSetting mHoveredItem = BoolSetting("Custom Hover", "Hover", false);
    BoolSetting mSlotEasing = BoolSetting("Slot Easing", "Easing", true);
    NumberSetting mSlotEasingSpeed = NumberSetting("Easing Speed", "Speed", 20.f, 0.1f, 20.f, 0.01f);

#ifdef __DEBUG__
    BoolSetting mForcePackSwitching =
        BoolSetting("Force Pack Switching", "Allows pack switching in-game", false);
#endif

    Interface();

    static inline std::unordered_map<int, std::vector<ImColor>> ColorThemes = {
        {Trans, {ImColor(255, 255, 255, 255)}},
        {Rainbow, {}},
        {Bubblegum, {ImColor(255, 255, 255, 255)}},
        {Watermelon, {ImColor(255, 255, 255, 255)}},
        {Sunset, {ImColor(255, 255, 255, 255)}},
        {Poison, {ImColor(255, 255, 255, 255)}},
        {Custom, {}}
    };

    std::vector<ImColor> getCustomColors() { return { ImColor(255, 255, 255, 255) }; }

    void onEnable() override;
    void onDisable() override;
    void renderHoverText();
    void onModuleStateChange(ModuleStateChangeEvent& event);
    void onPregameCheckEvent(PreGameCheckEvent& event);
    void onRenderEvent(RenderEvent& event);
    void onActorRenderEvent(ActorRenderEvent& event);
    void onLevelRenderEvent(LevelRenderEvent& event);
    void onBaseTickEvent(BaseTickEvent& event);
    void onPacketOutEvent(PacketOutEvent& event);
};