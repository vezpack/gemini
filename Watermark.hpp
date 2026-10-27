//
// Created by lilycat on 23/07/2026
//

#pragma once

#include <Features/Modules/Module.hpp>
#include <Features/Modules/Setting.hpp>
#include "HudEditor.hpp"

class Watermark : public ModuleBase<Watermark> {
public:
    Watermark() : ModuleBase("Watermark", "Displays The Watermark Of The Client", ModuleCategory::Visual, 0, true) {
        gFeatureManager->mDispatcher->listen<RenderEvent, &Watermark::onRenderEvent, nes::event_priority::FIRST>(this);

        mNames = {
            {Lowercase, "watermark"},
            {LowercaseSpaced, "watermark"},
            {Normal, "Watermark"},
            {NormalSpaced, "Watermark"}
        };

        mElement = std::make_unique<HudElement>();
        mElement->mPos = { 20.0, 7.5 };
        mElement->mParentTypeIdentifier = const_cast<char*>(ModuleBase<Watermark>::getTypeID());
        HudEditor::gInstance->registerElement(mElement.get());
    }

    std::unique_ptr<HudElement> mElement;

    void onEnable() override;
    void onDisable() override;
    void onRenderEvent(class RenderEvent& event);
};