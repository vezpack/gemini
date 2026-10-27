//
// Created by vastrakai on 10/19/2024.
//

#include "Glint.hpp"

#include <Features/Events/SetupShaderParameterEvent.hpp>
#include <Hook/Hooks/RenderHooks/RenderItemInHandHook.hpp>


void Glint::onEnable()
{
    gFeatureManager->mDispatcher->listen<SetupShaderParameterEvent, &Glint::onSetupShaderParameterEvent>(this);
}

void Glint::onDisable()
{
    gFeatureManager->mDispatcher->deafen<SetupShaderParameterEvent, &Glint::onSetupShaderParameterEvent>(this);
}

void Glint::onSetupShaderParameterEvent(SetupShaderParameterEvent& event)
{
    float sat = mSaturation.mValue;

    ImColor color = ColorUtils::getThemedColor(0);
    event.mglintColor = glm::vec4(color.Value.x * sat, color.Value.y * sat, color.Value.z * sat, 1.f);
}
