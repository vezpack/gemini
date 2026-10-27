//
// Created by alteik on 15/10/2024.
//

#include "Zoom.hpp"
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Rendering/LevelRenderer.hpp>
#include <SDK/Minecraft/Options.hpp>
/*ihate useless methot*/
void Zoom::onEnable()
{
    
    mCurrentValue = ClientInstance::get()->getOptions()->mGfxFieldOfView->mValue;
    /*ClientInstance::get()->getOptions()->mGfxFieldOfView->mMinValue = 10.f;
    */
    gFeatureManager->mDispatcher->listen<MouseEvent, &Zoom::onMouseEvent>(this);
    gFeatureManager->mDispatcher->listen<LevelRenderEvent, &Zoom::onLevelRenderEvent>(this);
}

void Zoom::onDisable()
{
    
    gFeatureManager->mDispatcher->deafen<MouseEvent, &Zoom::onMouseEvent>(this);
    gFeatureManager->mDispatcher->deafen<LevelRenderEvent, &Zoom::onLevelRenderEvent>(this);
    /*
    ClientInstance::get()->getOptions()->mGfxFieldOfView->mValue = mPastFov;
    ClientInstance::get()->getOptions()->mGfxFieldOfView->mMinValue = 30.f;*/
}

void Zoom::onMouseEvent(MouseEvent& event)
{ /* to do fi x  this shit */
    if (ClientInstance::get()->getMouseGrabbed()) return;
    if (!mScroll.mValue) return;

    if (event.mActionButtonId == 4)
    {
        if (event.mButtonData == 0x78 || event.mButtonData == 0x7F)
        {
            mZoomValue.mValue -= mScrollIncrement.mValue;
            event.cancel();
        }
        else if (event.mButtonData == 0x88 || event.mButtonData == 0x80 || event.mButtonData == -0x78)
        {
            mZoomValue.mValue += mScrollIncrement.mValue;
            event.cancel();
        }
    }
    mZoomValue.mValue = std::clamp(mZoomValue.mValue, 10.f, 120.f);
}

void Zoom::onLevelRenderEvent(LevelRenderEvent& event)
{
    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;
    auto lr = ClientInstance::get()->getLevelRenderer()->getRendererPlayer();
    if (!lr) return;
    if (!mSmooth.mValue) {
        mCurrentValue = mZoomValue.mValue;
    }
    else {
        mCurrentValue = MathUtils::lerp(mCurrentValue, mZoomValue.mValue, ImGui::GetIO().DeltaTime * 10.f);
    }
    lr->setFovX(*lr->getFovX() * (120.f / std::max(mCurrentValue, 10.f)));
    lr->setFovY(*lr->getFovY() * (120.f / std::max(mCurrentValue, 10.f)));

}
