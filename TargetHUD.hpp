#pragma once
#include <SDK/Minecraft/Actor/EntityId.hpp>
#include "HudEditor.hpp"

class TargetHUD : public ModuleBase<TargetHUD> {
public:
    enum class Style {
        Swiss,
    };

    EnumSettingT<Style> mStyle = EnumSettingT("Style", "The style of the target HUD", Style::Swiss, "Swiss");
    NumberSetting mXOffset = NumberSetting("X Offset", "The X offset of the target HUD", 100, -400, 400, 1);
    NumberSetting mYOffset = NumberSetting("Y Offset", "The Y offset of the target HUD", 100, -400, 400, 1);
    NumberSetting mFontSize = NumberSetting("Font Size", "The size of the font", 16, 10, 30, 1);

    BoolSetting mShowHead = BoolSetting("Show Head", "Displays the target's avatar head icon", true);
    BoolSetting mShowStatus = BoolSetting("Show Status", "Displays winning/losing combat status text", true);
    BoolSetting mShowAbsorption = BoolSetting("Show Absorption", "Displays gold absorption overlay on health bar", true);
    NumberSetting mBackgroundAlpha = NumberSetting("Background Alpha", "Opacity of the backdrop panel", 0.35f, 0.05f, 1.0f, 0.05f);

    BoolSetting mHealthCalculation = BoolSetting("Health Calculation", "Calculate health", false);

    TargetHUD() : ModuleBase("TargetHUD", "Shows target information", ModuleCategory::Visual, 0, false)
    {
        addSettings(
            &mStyle,
            &mXOffset,
            &mYOffset,
            &mFontSize,
            &mShowHead,
            &mShowStatus,
            &mShowAbsorption,
            &mBackgroundAlpha,
            &mHealthCalculation
        );

        VISIBILITY_CONDITION(mShowHead, mStyle.mValue == Style::Swiss);
        VISIBILITY_CONDITION(mShowStatus, mStyle.mValue == Style::Swiss);
        VISIBILITY_CONDITION(mShowAbsorption, mStyle.mValue == Style::Swiss);
        VISIBILITY_CONDITION(mBackgroundAlpha, mStyle.mValue == Style::Swiss);

        mNames = {
            {Lowercase, "targethud"},
            {LowercaseSpaced, "target hud"},
            {Normal, "TargetHUD"},
            {NormalSpaced, "Target HUD"},
        };

        gFeatureManager->mDispatcher->listen<RenderEvent, &TargetHUD::onRenderEvent, nes::event_priority::LAST>(this);

        mElement = std::make_unique<HudElement>();
        mElement->mPos = { 500, 500 };
        const char* ModuleBaseType = ModuleBase<TargetHUD>::getTypeID();
        mElement->mParentTypeIdentifier = const_cast<char*>(ModuleBaseType);
        HudEditor::gInstance->registerElement(mElement.get());
    }

    struct TargetTextureHolder {
        ID3D11ShaderResourceView* texture = nullptr;
        bool loaded = false;
        EntityId associatedEntity = EntityId();
    };

    float mHealth = 0;
    float mMaxHealth = 0;
    float mLastHealth = 0;
    float mLastMaxHealth = 0;
    float mAbsorption = 0;
    float mMaxAbsorption = 0;
    float mLastAbsorption = 0;
    float mLastMaxAbsorption = 0;
    float mLerpedHealth = 0;
    float mLerpedAbsorption = 0;
    std::string mLastPlayerName = "";
    float mLastHurtTime = 0;
    float mHurtTime = 0;
    Actor* mLastTarget = nullptr;
    std::map<Actor*, TargetTextureHolder> mTargetTextures;
    constexpr static uint64_t cHurtTimeDuration = 500;

    struct HealthInfo {
        float health = 20;
        float lastAbsorption = 0;
        float damage = 1;
    };
    std::map<std::string, HealthInfo> mHealths;
    uint64_t mLastHealTime = 0;

    std::unique_ptr<HudElement> mElement;

    void onEnable() override;
    void onDisable() override;
    void onBaseTickEvent(class BaseTickEvent& event);
    void validateTextures();
    void calculateHealths();
    ID3D11ShaderResourceView* getActorSkinTex(Actor* actor);
    void onRenderEvent(class RenderEvent& event);
    void onPacketInEvent(class PacketInEvent& event);
};