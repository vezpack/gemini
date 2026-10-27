#include "TargetHUD.hpp"

#include <Features/Events/BaseTickEvent.hpp>
#include <Features/Events/PacketInEvent.hpp>
#include <Features/Modules/Combat/Aura.hpp>
#include <Hook/Hooks/RenderHooks/D3DHook.hpp>
#include <SDK/Minecraft/Actor/SerializedSkin.hpp>
#include <SDK/Minecraft/Actor/Components/ActorOwnerComponent.hpp>
#include <SDK/Minecraft/Network/Packets/ActorEventPacket.hpp>

void TargetHUD::onEnable()
{
    gFeatureManager->mDispatcher->listen<BaseTickEvent, &TargetHUD::onBaseTickEvent, nes::event_priority::VERY_LAST>(this);
    gFeatureManager->mDispatcher->listen<PacketInEvent, &TargetHUD::onPacketInEvent>(this);

    mElement->mVisible = true;
}

void TargetHUD::onDisable()
{
    gFeatureManager->mDispatcher->deafen<BaseTickEvent, &TargetHUD::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->deafen<PacketInEvent, &TargetHUD::onPacketInEvent>(this);

    for (auto& [actor, textureHolder] : mTargetTextures)
    {
        if (textureHolder.texture) textureHolder.texture->Release();
    }
    mTargetTextures.clear();

    mElement->mVisible = false;
}

void TargetHUD::onBaseTickEvent(BaseTickEvent& event)
{
    validateTextures();
    if (mHealthCalculation.mValue) calculateHealths();

    // Validate that Aura target is alive and safe to read
    if (Aura::sHasTarget && Aura::sTarget)
    {
        if (!Aura::sTarget->isValid() || !Aura::sTarget->getMobHurtTimeComponent() || !Aura::sTarget->getActorTypeComponent())
        {
            Aura::sTarget = nullptr;
            Aura::sHasTarget = false;
            return;
        }

        mHealth = Aura::sTarget->getHealth();
        if (mHealthCalculation.mValue) mHealth = mHealths[Aura::sTarget->getRawName()].health;
        mMaxHealth = Aura::sTarget->getMaxHealth();
        mAbsorption = Aura::sTarget->getAbsorption();
        mMaxAbsorption = Aura::sTarget->getMaxAbsorption();
        if (!Aura::sTarget->isPlayer())
        {
            mLastPlayerName = "Mob";
            return;
        }
        mLastHurtTime = mHurtTime;
        mHurtTime = static_cast<float>(Aura::sTarget->getMobHurtTimeComponent()->mHurtTime);
        mLastHealth = mHealth;
        mLastAbsorption = mAbsorption;
        mLastMaxHealth = mMaxHealth;
        mLastMaxAbsorption = mMaxAbsorption;
        mLastPlayerName = Aura::sTarget->getRawName();

        if (mHurtTime > mLastHurtTime)
        {
            mLastHurtTime = mHurtTime;
        }
    }
}

void TargetHUD::calculateHealths() {
    auto player = ClientInstance::get()->getLocalPlayer();
    auto actors = ActorUtils::getActorList(true, true);

    bool heal = 4000 <= NOW - mLastHealTime;
    if (heal) mLastHealTime = NOW;

    for (auto actor : actors) {
        if (!actor || !actor->isValid() || actor == player) continue;
        if (!actor->getMobHurtTimeComponent() || !actor->getActorTypeComponent()) continue;
        auto info = &mHealths[actor->getRawName()];
        float absorption = actor->getAbsorption();
        int hurtTime = actor->getMobHurtTimeComponent()->mHurtTime;
        if (0 < hurtTime) {
            float damage = 0;
            if (absorption < info->lastAbsorption) {
                if (0 < absorption) {
                    info->damage = abs(info->lastAbsorption - absorption);
                    damage = 0;
                }
                else if (0 < info->lastAbsorption) {
                    damage = abs(info->damage - info->lastAbsorption);
                }
            }
            else if (hurtTime == 9)
            {
                damage = info->damage;
            }

            if (absorption == 0 && 0 < damage) {
                if (info->health - damage < 0) info->health = 0;
                else info->health -= damage;
            }
        }
        if (heal) {
            if (info->health + 1 > 20) info->health = 20;
            else info->health++;
        }
        info->lastAbsorption = absorption;
    }
}

void TargetHUD::validateTextures()
{
    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player || !player->mContext.mRegistry) return;

    std::vector<EntityId> foundEntities;
    for (auto&& [daId, moduleOwner, typeComponent] : player->mContext.mRegistry->view<ActorOwnerComponent, ActorTypeComponent>().each())
    {
        if (moduleOwner.mActor)
        {
            foundEntities.push_back(moduleOwner.mActor->mContext.mEntityId);
        }
    }

    for (auto it = mTargetTextures.begin(); it != mTargetTextures.end();)
    {
        if (!it->first || !it->first->isValid() || std::ranges::find(foundEntities, it->second.associatedEntity) == foundEntities.end())
        {
            if (it->second.texture) it->second.texture->Release();
            it = mTargetTextures.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

ID3D11ShaderResourceView* TargetHUD::getActorSkinTex(Actor* actor)
{
    if (!actor || !actor->isValid()) return nullptr;

    auto player = ClientInstance::get()->getLocalPlayer();

    if (!mTargetTextures.contains(actor)) mTargetTextures[actor] = TargetTextureHolder();

    auto& [texture, loaded, id] = mTargetTextures[actor];

    if (actor)
    {
        auto skin = actor->getSkin();
        if (skin)
        {
            if (!loaded) {
                bool isPlayer = true;
                if (actor->isValid() && !actor->isPlayer())
                {
                    isPlayer = false;
                    if (player) skin = player->getSkin();
                }
                if (!skin) return nullptr;

                int headSize = skin->skinWidth / 8;
                int headOffsetX = skin->skinWidth / 8;
                int headOffsetY = skin->skinHeight / 8;

                std::vector<uint8_t> headData(headSize * headSize * 4);

                for (int y = 0; y < headSize; y++) {
                    for (int x = 0; x < headSize; x++) {
                        int srcIndex = ((y + headOffsetY) * skin->skinWidth + (x + headOffsetX)) * 4;
                        int dstIndex = (y * headSize + x) * 4;
                        std::copy_n(skin->skinData + srcIndex, 4, headData.data() + dstIndex);
                    }
                }

                int scalingFactor = 8;
                std::vector<uint8_t> scaledHeadData(headSize * scalingFactor * headSize * scalingFactor * 4);

                for (int y = 0; y < headSize * scalingFactor; y++) {
                    for (int x = 0; x < headSize * scalingFactor; x++) {
                        int srcX = x / scalingFactor;
                        int srcY = y / scalingFactor;
                        int srcIndex = (srcY * headSize + srcX) * 4;
                        int dstIndex = (y * headSize * scalingFactor + x) * 4;
                        std::copy_n(headData.data() + srcIndex, 4, scaledHeadData.data() + dstIndex);
                    }
                }

                headSize *= scalingFactor;
                headData = std::move(scaledHeadData);
                D3DHook::createTextureFromData(headData.data(), headSize, headSize, &texture);
                loaded = true;
                id = actor->mContext.mEntityId;
            }
        }
    }
    return texture;
}

void TargetHUD::onRenderEvent(RenderEvent& event)
{
    auto localPlayer = ClientInstance::get()->getLocalPlayer();
    if (!localPlayer) return;
    if (!ClientInstance::get()->getLevelRenderer()) return;

    auto drawList = ImGui::GetBackgroundDrawList();
    if (mStyle.mValue != Style::Swiss)
    {
        return;
    }

    Actor* target = Aura::sTarget;
    bool hasTarget = Aura::sHasTarget;

    if (target && !target->isValid()) {
        target = nullptr;
        hasTarget = false;
        Aura::sTarget = nullptr;
        Aura::sHasTarget = false;
    }

    if (mElement->mSampleMode && !hasTarget) {
        target = localPlayer;
        hasTarget = true;
        mHealth = target->getHealth();
        mMaxHealth = target->getMaxHealth();
        mAbsorption = target->getAbsorption();
        mMaxAbsorption = target->getMaxAbsorption();
        mLastPlayerName = target->getRawName();
        mLastHurtTime = mHurtTime;
        mHurtTime = target->getMobHurtTimeComponent()->mHurtTime;
    }

    static float anim = 0.f;
    float delta = ImGui::GetIO().DeltaTime;

    float lerpedHurtTime = MathUtils::lerp(mLastHurtTime / 10.f, mHurtTime / 10.f, delta);

    static float hurtTimeAnimPerc = 0.f;
    static float healthAnimPerc = 0.f;
    static float absorptionAnimPerc = 0.f;

    if (mLastTarget != target)
    {
        mLastTarget = target;
        mLastHealth = mHealth;
        mLastAbsorption = mAbsorption;
        mLastMaxHealth = mMaxHealth;
        mLastMaxAbsorption = mMaxAbsorption;
        healthAnimPerc = mMaxHealth > 0.f ? mHealth / mMaxHealth : 0.f;
        absorptionAnimPerc = mAbsorption / 20.f;
        mLerpedHealth = mHealth;
        mLerpedAbsorption = mAbsorption;
    }

    mLastTarget = target;

    hurtTimeAnimPerc = MathUtils::lerp(hurtTimeAnimPerc, lerpedHurtTime, delta * 20.f);

    float perc = mLastMaxHealth > 0.f ? mLastHealth / mLastMaxHealth : 0.f;
    healthAnimPerc = MathUtils::lerp(healthAnimPerc, perc, delta * 6.f);
    float perc2 = mLastAbsorption / 20.f;
    absorptionAnimPerc = MathUtils::lerp(absorptionAnimPerc, perc2, delta * 6.f);

    mLerpedHealth = MathUtils::lerp(mLerpedHealth, mHealth, delta * 10.f);
    mLerpedAbsorption = MathUtils::lerp(mLerpedAbsorption, mAbsorption, delta * 10.f);

    bool showing = mEnabled && hasTarget && target && target->isValid();
    anim = MathUtils::lerp(anim, showing ? 1.f : 0.f, delta * 12.f);

    if (anim < 0.01f)
    {
        return;
    }

    FontHelper::pushPrefFont(true, true);

    float boxWidth = mShowHead.mValue ? 230.f : 200.f;
    auto boxSize = ImVec2(boxWidth * anim, 64.f * anim);
    auto boxPos = ImVec2(mElement->getPos().x, mElement->getPos().y);
    boxPos.x -= boxSize.x / 2;
    boxPos.y -= boxSize.y / 2;

    mElement->mSize = glm::vec2(boxSize.x, boxSize.y);
    mElement->mCentered = true;

    ImRenderUtils::addBlur(ImVec4(boxPos.x, boxPos.y, boxPos.x + boxSize.x, boxPos.y + boxSize.y), 6.f, 14.f * anim);
    ImRenderUtils::fillShadowRectangle(
        ImVec4(boxPos.x, boxPos.y, boxPos.x + boxSize.x, boxPos.y + boxSize.y),
        ImColor(15, 15, 15, static_cast<int>(220 * mBackgroundAlpha.mValue * anim)),
        anim, 25, ImDrawFlags_ShadowCutOutShapeBackground, 12.f * anim
    );

    ID3D11ShaderResourceView* texture = target ? getActorSkinTex(target) : nullptr;
    auto imageColor = ImColor(1.f, 1.f, 1.f, 1.f * anim);
    imageColor.Value.y = MathUtils::lerp(imageColor.Value.y, 0.2f, hurtTimeAnimPerc);
    imageColor.Value.z = MathUtils::lerp(imageColor.Value.z, 0.2f, hurtTimeAnimPerc);

    float currentX = boxPos.x + 10.f * anim;

    if (mShowHead.mValue)
    {
        auto headSize = ImVec2(44.f * anim, 44.f * anim);
        auto headPos = ImVec2(currentX, boxPos.y + (boxSize.y - headSize.y) / 2);
        if (texture)
            drawList->AddImageRounded(texture, headPos, headPos + headSize, ImVec2(0, 0), ImVec2(1, 1), imageColor, 8.f * anim);
        currentX += headSize.x + 10.f * anim;
    }

    float textStartX = currentX;
    float textY = boxPos.y + 10.f * anim;

    std::string name = mLastPlayerName.empty() ? "Target" : mLastPlayerName;
    drawList->AddText(ImGui::GetFont(), mFontSize.mValue * anim, ImVec2(textStartX, textY), ImColor(255, 255, 255, static_cast<int>(255 * anim)), name.c_str());

    if (mShowStatus.mValue)
    {
        std::string mStatus = localPlayer->getHealth() >= mHealth ? "Winning" : "Losing";
        ImColor statusColor = mStatus == "Winning" ? ImColor(46, 204, 113, static_cast<int>(220 * anim)) : ImColor(231, 76, 60, static_cast<int>(220 * anim));
        drawList->AddText(ImGui::GetFont(), (mFontSize.mValue - 4.f) * anim, ImVec2(textStartX, textY + 18.f * anim), statusColor, mStatus.c_str());
    }

    float barWidth = (boxPos.x + boxSize.x) - textStartX - 10.f * anim;
    float barHeight = 8.f * anim;
    float barY = boxPos.y + boxSize.y - 18.f * anim;
    auto healthBarStart = ImVec2(textStartX, barY);
    auto healthBarEnd = ImVec2(textStartX + barWidth, barY + barHeight);

    drawList->AddRectFilled(healthBarStart, healthBarEnd, ImColor(40, 40, 40, static_cast<int>(200 * anim)), 4.f);

    float healthPerc = mMaxHealth > 0.f ? MathUtils::clamp(mLerpedHealth / mMaxHealth, 0.f, 1.f) : 0.f;
    auto healthEnd = ImVec2(MathUtils::lerp(healthBarStart.x, healthBarEnd.x, healthPerc), healthBarEnd.y);

    ImColor startColor = ColorUtils::getThemedColor(0);
    ImColor endColor = ColorUtils::getThemedColor(100);
    startColor.Value.w *= anim;
    endColor.Value.w *= anim;

    if (healthPerc > 0.01f)
    {
        drawList->PushClipRect(healthBarStart, healthEnd, true);
        drawList->AddRectFilledMultiColor(healthBarStart, healthBarEnd, startColor, endColor, endColor, startColor, 4.f, ImDrawCornerFlags_All);
        drawList->PopClipRect();
    }

    if (mShowAbsorption.mValue && mAbsorption > 0.f)
    {
        float absorptionPerc = MathUtils::clamp(mLerpedAbsorption / 20.f, 0.f, 1.f);
        auto absorpEnd = ImVec2(MathUtils::lerp(healthBarStart.x, healthBarEnd.x, absorptionPerc), healthBarEnd.y);
        drawList->PushClipRect(healthBarStart, absorpEnd, true);
        drawList->AddRectFilled(healthBarStart, healthBarEnd, ImColor(244, 204, 0, static_cast<int>(240 * anim)), 4.f);
        drawList->PopClipRect();
    }

    FontHelper::popPrefFont();
}

void TargetHUD::onPacketInEvent(PacketInEvent& event)
{
    if (event.mPacket->getId() == PacketID::ActorEvent)
    {
        auto packet = event.getPacket<ActorEventPacket>();
        if (packet->mEvent != ActorEvent::HURT) return;
        Actor* target = ActorUtils::getActorFromRuntimeID(packet->mRuntimeID);
        if (!target) return;
    }
    else if (event.mPacket->getId() == PacketID::ChangeDimension) {
        for (auto& [actor, textureHolder] : mTargetTextures)
        {
            if (textureHolder.texture) textureHolder.texture->Release();
        }
        mTargetTextures.clear();
        mLastHealTime = NOW;
        mHealths.clear();
    }
}