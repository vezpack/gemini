#include "Notifications.hpp"

#include <Features/Events/ConnectionRequestEvent.hpp>
#include <Features/Events/NotifyEvent.hpp>
#include <Features/Events/RenderEvent.hpp>
#include <Features/Modules/Visual/Interface.hpp>
#include <Features/FeatureManager.hpp>

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>

// ------------------------------------------------------------
// Notification appearance
// ------------------------------------------------------------

namespace NotificationStyle
{
    constexpr float PADDING_X = 14.0f;
    constexpr float PADDING_Y = 12.0f;

    constexpr float MIN_WIDTH = 260.0f;
    constexpr float MAX_WIDTH = 380.0f;

    constexpr float TITLE_FONT_SIZE = 15.0f;
    constexpr float BODY_FONT_SIZE = 13.0f;

    constexpr float GAP = 8.0f;

    constexpr float RIGHT_MARGIN = 15.0f;
    constexpr float BOTTOM_MARGIN = 15.0f;

    constexpr float BACKGROUND_ALPHA = 245.0f;
    constexpr float CORNER_ROUNDING = 8.0f;

    constexpr float ANIMATION_SPEED = 10.0f;
    constexpr float EXIT_DELAY = 0.35f;

    static ImU32 background(float alpha)
    {
        return IM_COL32(22, 22, 26, static_cast<int>(BACKGROUND_ALPHA * alpha));
    }

    static ImU32 textPrimary(float alpha)
    {
        return IM_COL32(255, 255, 255, static_cast<int>(255.0f * alpha));
    }

    static ImU32 textSecondary(float alpha)
    {
        return IM_COL32(200, 200, 210, static_cast<int>(255.0f * alpha));
    }

    static ImU32 accent(Notification::Type type, float alpha)
    {
        switch (type)
        {
        case Notification::Type::Warning:
            return IM_COL32(225, 180, 65, static_cast<int>(255.0f * alpha));
        case Notification::Type::Error:
            return IM_COL32(220, 75, 75, static_cast<int>(255.0f * alpha));
        default:
            return IM_COL32(255, 255, 255, static_cast<int>(255.0f * alpha));
        }
    }
}

// ------------------------------------------------------------
// Event registration
// ------------------------------------------------------------

void Notifications::onEnable()
{
    gFeatureManager->mDispatcher->listen<
        NotifyEvent,
        &Notifications::onNotifyEvent
    >(this);

    gFeatureManager->mDispatcher->listen<
        ModuleStateChangeEvent,
        &Notifications::onModuleStateChange
    >(this);

    gFeatureManager->mDispatcher->listen<
        ConnectionRequestEvent,
        &Notifications::onConnectionRequestEvent
    >(this);
}

void Notifications::onDisable()
{
    gFeatureManager->mDispatcher->deafen<
        NotifyEvent,
        &Notifications::onNotifyEvent
    >(this);

    gFeatureManager->mDispatcher->deafen<
        ModuleStateChangeEvent,
        &Notifications::onModuleStateChange
    >(this);

    gFeatureManager->mDispatcher->deafen<
        ConnectionRequestEvent,
        &Notifications::onConnectionRequestEvent
    >(this);
}

// ------------------------------------------------------------
// Rendering
// ------------------------------------------------------------

void Notifications::onRenderEvent(RenderEvent& event)
{
    (void)event;
    if (mNotifications.empty())
        return;

    const ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    const float delta = ImGui::GetIO().DeltaTime;

    if (screenSize.x <= 0.0f || screenSize.y <= 0.0f)
        return;

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    if (!draw)
        return;

    FontHelper::pushPrefFont(true);

    // Remove notifications that have finished their exit animation.
    std::erase_if(
        mNotifications,
        [](const Notification& notification)
        {
            return notification.mIsTimeUp &&
                notification.mTimeShown >
                notification.mDuration +
                NotificationStyle::EXIT_DELAY;
        }
    );

    const int maxNotifications = mLimitNotifications.mValue
        ? std::max(1, static_cast<int>(mMaxNotifications.mValue))
        : static_cast<int>(mNotifications.size());

    int displayed = 0;
    float y = screenSize.y - NotificationStyle::BOTTOM_MARGIN;

    // Newest notifications appear closest to the bottom.
    for (auto it = mNotifications.rbegin(); it != mNotifications.rend(); ++it)
    {
        Notification& notification = *it;

        if (displayed >= maxNotifications)
            break;

        notification.mTimeShown += delta;

        const float duration = std::max(notification.mDuration, 0.01f);
        notification.mIsTimeUp = notification.mTimeShown >= duration;

        const float targetProgress = notification.mIsTimeUp ? 0.0f : 1.0f;

        notification.mCurrentDuration = MathUtils::lerp(
            notification.mCurrentDuration,
            targetProgress,
            std::clamp(delta * NotificationStyle::ANIMATION_SPEED, 0.0f, 1.0f)
        );

        const float animation = std::clamp(notification.mCurrentDuration, 0.0f, 1.0f);

        std::string titleText = "Information";
        std::string bodyText = notification.mMessage;

        if (notification.mType == Notification::Type::Warning) titleText = "Warning";
        if (notification.mType == Notification::Type::Error) titleText = "Error";

        const ImVec2 titleSize = ImGui::GetFont()->CalcTextSizeA(NotificationStyle::TITLE_FONT_SIZE, FLT_MAX, 0.0f, titleText.c_str());
        const ImVec2 bodySize = ImGui::GetFont()->CalcTextSizeA(NotificationStyle::BODY_FONT_SIZE, FLT_MAX, 0.0f, bodyText.c_str());

        const float iconRadius = 13.0f;
        const float leftContentOffset = NotificationStyle::PADDING_X + (iconRadius * 2.0f) + 12.0f;

        const float contentWidth = std::max(titleSize.x, bodySize.x);
        const float width = std::clamp(
            contentWidth + leftContentOffset + NotificationStyle::PADDING_X,
            NotificationStyle::MIN_WIDTH,
            NotificationStyle::MAX_WIDTH
        );

        const float height = NotificationStyle::PADDING_Y * 2.0f + titleSize.y + bodySize.y + 4.0f;

        const float targetX = screenSize.x - width - NotificationStyle::RIGHT_MARGIN;
        const float startX = screenSize.x + 10.0f;

        const float x = MathUtils::lerp(startX, targetX, animation);
        const float topY = y - height;

        const ImVec2 pos(x, topY);
        const ImVec2 max(x + width, topY + height);

        const ImU32 bgColor = NotificationStyle::background(animation);
        const ImU32 accentColor = NotificationStyle::accent(notification.mType, animation);

        // Layered soft black glow for a smooth, higher-radius shadow effect
        for (int i = 1; i <= 4; ++i)
        {
            float alphaMult = (1.0f - (static_cast<float>(i) / 5.0f)) * 0.35f * animation;
            ImVec2 expand(static_cast<float>(i) * 2.5f, static_cast<float>(i) * 2.5f);
            draw->AddRectFilled(
                ImVec2(pos.x - expand.x, pos.y - expand.y),
                ImVec2(max.x + expand.x, max.y + expand.y),
                IM_COL32(0, 0, 0, static_cast<int>(255.0f * alphaMult)),
                NotificationStyle::CORNER_ROUNDING + static_cast<float>(i)
            );
        }

        // Main background with rounded corners (No outline border)
        draw->AddRectFilled(pos, max, bgColor, NotificationStyle::CORNER_ROUNDING);

        // Render larger Information Icon on the left (Circle with perfectly centered 'i')
        ImVec2 iconCenter(pos.x + NotificationStyle::PADDING_X + iconRadius, pos.y + (height * 0.5f));
        draw->AddCircle(iconCenter, iconRadius, accentColor, 0, 1.8f);

        const char* iconText = "i";
        const ImVec2 iconTextSize = ImGui::GetFont()->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, iconText);
        draw->AddText(
            ImGui::GetFont(),
            14.0f,
            ImVec2(iconCenter.x - (iconTextSize.x * 0.5f), iconCenter.y - (iconTextSize.y * 0.5f)),
            accentColor,
            iconText
        );

        // Render Title ("Information") with clean clarity
        draw->AddText(
            ImGui::GetFont(),
            NotificationStyle::TITLE_FONT_SIZE,
            ImVec2(pos.x + leftContentOffset, pos.y + NotificationStyle::PADDING_Y),
            NotificationStyle::textPrimary(animation),
            titleText.c_str()
        );

        // Render Body Text (e.g. "ChestStealer was enabled") with crisp clarity
        draw->AddText(
            ImGui::GetFont(),
            NotificationStyle::BODY_FONT_SIZE,
            ImVec2(pos.x + leftContentOffset, pos.y + NotificationStyle::PADDING_Y + titleSize.y + 2.0f),
            NotificationStyle::textSecondary(animation),
            bodyText.c_str()
        );

        y = topY - NotificationStyle::GAP;
        ++displayed;
    }

    FontHelper::popPrefFont();
}

// ------------------------------------------------------------
// Notification events
// ------------------------------------------------------------

void Notifications::onModuleStateChange(ModuleStateChangeEvent& event)
{
    if (event.isCancelled() || !mShowOnToggle.mValue)
        return;

    if (!event.mModule)
        return;

    std::string moduleName = event.mModule->getName();
    std::string message = moduleName + (event.mEnabled ? " was enabled" : " was disabled");

    mNotifications.emplace_back(
        message,
        Notification::Type::Info,
        3.0f
    );
}

void Notifications::onConnectionRequestEvent(ConnectionRequestEvent& event)
{
    if (!mShowOnJoin.mValue)
        return;

    mNotifications.emplace_back(
        "Connecting to DA SERVER (get ready to get dtc)" + event.mServerAddress + "...",
        Notification::Type::Info,
        6.0f
    );
}

void Notifications::onNotifyEvent(NotifyEvent& event)
{
    mNotifications.push_back(event.mNotification);
}