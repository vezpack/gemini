//
// Created by vastrakai on 7/1/2024.
//

#include <Features/Modules/Visual/Interface.hpp>
#include "ClickGui.hpp"

#include <cctype>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <Features/Events/ActorRenderEvent.hpp>
#include <Features/Events/BaseTickEvent.hpp>
#include <Features/Events/ModuleStateChangeEvent.hpp>
#include <Features/Events/DrawImageEvent.hpp>
#include <Features/Events/PreGameCheckEvent.hpp>
#include <Features/Events/LevelRenderEvent.hpp>
#include <Features/Events/PacketInEvent.hpp>
#include <Features/Events/PacketOutEvent.hpp>

#include <Hook/Hooks/RenderHooks/ActorRenderDispatcherHook.hpp>
#include <Hook/Hooks/RenderHooks/HoverTextRendererHook.hpp>

#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/mce.hpp>
#include <SDK/Minecraft/Options.hpp>
#include <SDK/Minecraft/Network/Packets/Packet.hpp>
#include <SDK/Minecraft/Network/Packets/PlayerAuthInputPacket.hpp>
#include <SDK/Minecraft/Actor/SyncedPlayerMovementSettings.hpp>
#include <SDK/Minecraft/Network/Packets/MovePlayerPacket.hpp>
#include <SDK/Minecraft/World/Level.hpp>

#ifdef __DEBUG__
std::vector<unsigned char> gFsBytes2 = {
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90
};

DEFINE_PATCH_FUNC(
    patchFullStack,
    SigManager::ResourcePackManager_composeFullStackBp,
    gFsBytes2
);
#endif

namespace
{
    struct MinecraftColorRegistry
    {
        static const std::unordered_map<char, ImColor>& getMap()
        {
            static const std::unordered_map<char, ImColor> colorMap = {
                {'0', ImColor(255, 255, 255)}, {'1', ImColor(255, 255, 255)},
                {'2', ImColor(255, 255, 255)}, {'3', ImColor(255, 255, 255)},
                {'4', ImColor(255, 255, 255)}, {'5', ImColor(255, 255, 255)},
                {'6', ImColor(255, 255, 255)}, {'7', ImColor(255, 255, 255)},
                {'8', ImColor(255, 255, 255)}, {'9', ImColor(255, 255, 255)},
                {'a', ImColor(255, 255, 255)}, {'b', ImColor(255, 255, 255)},
                {'c', ImColor(255, 255, 255)}, {'d', ImColor(255, 255, 255)},
                {'e', ImColor(255, 255, 255)}, {'f', ImColor(255, 255, 255)},
                {'r', ImColor(255, 255, 255)}
            };
            return colorMap;
        }
    };

    template <typename T>
    std::string combineStr(T value) { std::stringstream s; s << value; return s.str(); }
    template <typename T, typename... Args>
    std::string combineStr(T value, Args... args) { std::stringstream s; s << value << combineStr(args...); return s.str(); }
}

static float pYaw = 0.f, pOldYaw = 0.f, pHeadYaw = 0.f, pOldHeadYaw = 0.f;
static float pPitch = 0.f, pOldPitch = 0.f, pBodyYaw = 0.f, pOldBodyYaw = 0.f;
static float pLerpedYaw = 0.f, pLerpedHeadYaw = 0.f, pLerpedPitch = 0.f, pLerpedBodyYaw = 0.f;
static bool usingPaip = false;

Interface::Interface()
    : ModuleBase("Interface", "Customize the visuals!", ModuleCategory::Visual, 0, true)
{
    gFeatureManager->mDispatcher->listen<ModuleStateChangeEvent, &Interface::onModuleStateChange, nes::event_priority::FIRST>(this);
    gFeatureManager->mDispatcher->listen<RenderEvent, &Interface::onRenderEvent, nes::event_priority::NORMAL>(this);
    gFeatureManager->mDispatcher->listen<ActorRenderEvent, &Interface::onActorRenderEvent, nes::event_priority::NORMAL>(this);
    gFeatureManager->mDispatcher->listen<LevelRenderEvent, &Interface::onLevelRenderEvent, nes::event_priority::NORMAL>(this);
    gFeatureManager->mDispatcher->listen<BaseTickEvent, &Interface::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->listen<PacketOutEvent, &Interface::onPacketOutEvent, nes::event_priority::ABSOLUTE_LAST>(this);
    gFeatureManager->mDispatcher->listen<PreGameCheckEvent, &Interface::onPregameCheckEvent>(this);

    addSettings(
        &mNamingStyle
#ifdef __DEBUG__
        , &mForcePackSwitching
#endif
    );

    mNames = {
        {Lowercase, "interface"},
        {LowercaseSpaced, "interface"},
        {Normal, "Interface"},
        {NormalSpaced, "Interface"}
    };
}

void Interface::onEnable() {}
void Interface::onDisable() {
#ifdef __DEBUG__
    patchFullStack(false);
#endif
}

void Interface::renderHoverText()
{
    static EasingUtil inEase;
    const bool showHoverText = HoverTextRender::mTimeDisplayed != 0 && !gFeatureManager->mModuleManager->getModule<ClickGui>()->mEnabled;
    if (showHoverText) inEase.incrementPercentage(ImRenderUtils::getDeltaTime() * 2.f);
    else inEase.decrementPercentage(ImRenderUtils::getDeltaTime() * 4.f);

    float inScale = showHoverText ? inEase.easeOutExpo() : inEase.easeOutBack();
    if (inEase.isPercentageMax()) inScale = 1.f;
    if (inScale < 0.01f) return;

    const glm::vec2 mPos = HoverTextRender::mInfo.mPos;
    glm::vec2 mTextPos(mPos.x + 6.f, mPos.y + 6.f);
    const float mTextSize = 1.25f * inScale;
    const std::string mMessage = HoverTextRender::mInfo.mText;
    const std::string mNoneColoredText = ColorUtils::removeColorCodes(HoverTextRender::mInfo.mText);

    ImColor mCurrentColor(255, 255, 255);
    const float mMeasurementX = ImGui::GetFont()->CalcTextSizeA(mTextSize * 18.f, FLT_MAX, -1.f, mNoneColoredText.c_str()).x;
    const float mMeasurementY = ImGui::GetFont()->CalcTextSizeA(mTextSize * 18.f, FLT_MAX, -1.f, mNoneColoredText.c_str()).y;
    const ImVec4 mRect(mPos.x, mPos.y, mPos.x + mMeasurementX + 12.f, mPos.y + mMeasurementY + 12.f);

    ImRenderUtils::addBlur(mRect, 3.f * inScale, 10);
    ImRenderUtils::fillRectangle(mRect, ImColor(0, 0, 0), 0.78f * inScale, 10);

    const auto& colorMap = MinecraftColorRegistry::getMap();
    for (size_t j = 0; j < mMessage.length(); ++j)
    {
        const unsigned char c = static_cast<unsigned char>(mMessage[j]);
        if (c == 0xC2 && j + 2 < mMessage.length() && static_cast<unsigned char>(mMessage[j + 1]) == 0xA7)
        {
            j += 2; continue;
        }
        if (c == '\n') { mTextPos.x = mPos.x + 6.f; mTextPos.y += 18.f; continue; }
        if (!std::isprint(c)) continue;

        const std::string mString = combineStr(static_cast<char>(c));
        ImRenderUtils::drawText(mTextPos, mString, mCurrentColor, mTextSize, inScale, false);
        mTextPos.x += ImGui::GetFont()->CalcTextSizeA(mTextSize * 18.f, FLT_MAX, -1.f, mString.c_str()).x;
    }
    HoverTextRender::mTimeDisplayed = 0;
}

void Interface::onModuleStateChange(ModuleStateChangeEvent& event) { if (event.mModule == this) event.setCancelled(true); }
void Interface::onPregameCheckEvent(PreGameCheckEvent& event) {}
void Interface::onRenderEvent(RenderEvent& event) {}
void Interface::onActorRenderEvent(ActorRenderEvent& event) {}
void Interface::onLevelRenderEvent(LevelRenderEvent& event) {}
void Interface::onBaseTickEvent(BaseTickEvent& event)
{
    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;
    pOldBodyYaw = pBodyYaw;
    pBodyYaw = pYaw;
}
void Interface::onPacketOutEvent(PacketOutEvent& event) {}