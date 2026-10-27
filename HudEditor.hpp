#pragma once
#include <Features/Modules/Module.hpp>
#include <glm/glm.hpp>
#include <imgui.h>
#include <limits>
#include <algorithm>
#include <string>
#include <memory>
#include <nlohmann/json.hpp>

class HudElement
{
public:
    enum class Anchor {
        TopLeft,
        TopMiddle,
        TopRight,
        MiddleLeft,
        Middle,
        MiddleRight,
        BottomLeft,
        BottomMiddle,
        BottomRight
    };

    static ImVec2 getAnchorPos(Anchor anchor)
    {
        auto display = ImGui::GetIO().DisplaySize;
        ImVec2 pos = { 0, 0 };

        switch (anchor)
        {
        case Anchor::TopLeft:
            break;
        case Anchor::TopMiddle:
            pos.x = display.x / 2;
            break;
        case Anchor::TopRight:
            pos.x = display.x;
            break;
        case Anchor::MiddleLeft:
            pos.y = display.y / 2;
            break;
        case Anchor::Middle:
            pos.x = display.x / 2;
            pos.y = display.y / 2;
            break;
        case Anchor::MiddleRight:
            pos.x = display.x;
            pos.y = display.y / 2;
            break;
        case Anchor::BottomLeft:
            pos.y = display.y;
            break;
        case Anchor::BottomMiddle:
            pos.x = display.x / 2;
            pos.y = display.y;
            break;
        case Anchor::BottomRight:
            pos.x = display.x;
            pos.y = display.y;
            break;
        }

        return pos;
    }

    glm::vec2 mPos = { 0, 0 };  // Relative position to the anchor point
    glm::vec2 mSize = { 0, 0 }; // Size of the element
    glm::vec2 mDefaultPos = { 0, 0 }; // Recorded default relative position
    Anchor mDefaultAnchor = Anchor::TopLeft; // Recorded default anchor
    bool mVisible = false;
    bool mSampleMode = false;   // Show the element in the editor
    char* mParentTypeIdentifier = nullptr;
    bool mCentered = false;
    Anchor mAnchor = Anchor::TopLeft;
    bool mCustom = false;

    HudElement() = default;
    HudElement(char* parentTypeIdentifier) : mParentTypeIdentifier(parentTypeIdentifier) {}

    // Maps the raw top-left position back to the correct boundary coordinate relative to the selected anchor
    void setFromPos(glm::vec2 normalPos) {
        Anchor closestAnchor = Anchor::TopLeft;
        float minDistance = std::numeric_limits<float>::max();

        // Calculate based on the center of the bounding box to match the closest anchor accurately
        glm::vec2 elementCenter = { normalPos.x + mSize.x / 2.f, normalPos.y + mSize.y / 2.f };

        for (int i = 0; i < 9; i++) {
            ImVec2 anchorPos = getAnchorPos(static_cast<Anchor>(i));
            float dist = glm::distance(glm::vec2(anchorPos.x, anchorPos.y), elementCenter);

            if (dist < minDistance) {
                minDistance = dist;
                closestAnchor = static_cast<Anchor>(i);
            }
        }

        mAnchor = closestAnchor;
        ImVec2 anchorPos = getAnchorPos(mAnchor);
        ImVec2 anchoredPointPos = { normalPos.x, normalPos.y };

        switch (mAnchor)
        {
        case Anchor::TopLeft:
        case Anchor::MiddleLeft:
        case Anchor::BottomLeft:
            break;
        case Anchor::TopMiddle:
        case Anchor::Middle:
        case Anchor::BottomMiddle:
            anchoredPointPos.x = normalPos.x + mSize.x / 2.f;
            break;
        case Anchor::TopRight:
        case Anchor::MiddleRight:
        case Anchor::BottomRight:
            anchoredPointPos.x = normalPos.x + mSize.x;
            break;
        }

        switch (mAnchor)
        {
        case Anchor::TopLeft:
        case Anchor::TopMiddle:
        case Anchor::TopRight:
            break;
        case Anchor::MiddleLeft:
        case Anchor::Middle:
        case Anchor::MiddleRight:
            anchoredPointPos.y = normalPos.y + mSize.y / 2.f;
            break;
        case Anchor::BottomLeft:
        case Anchor::BottomMiddle:
        case Anchor::BottomRight:
            anchoredPointPos.y = normalPos.y + mSize.y;
            break;
        }

        mPos = { anchoredPointPos.x - anchorPos.x, anchoredPointPos.y - anchorPos.y };
    }

    // Calculates and returns the absolute top-left position of the element using the static boundary map
    ImVec2 getPos() const {
        ImVec2 anchorPos = getAnchorPos(mAnchor);
        ImVec2 anchoredPointPos = { mPos.x + anchorPos.x, mPos.y + anchorPos.y };
        ImVec2 topLeft = anchoredPointPos;

        switch (mAnchor)
        {
        case Anchor::TopLeft:
        case Anchor::MiddleLeft:
        case Anchor::BottomLeft:
            break;
        case Anchor::TopMiddle:
        case Anchor::Middle:
        case Anchor::BottomMiddle:
            topLeft.x = anchoredPointPos.x - mSize.x / 2.f;
            break;
        case Anchor::TopRight:
        case Anchor::MiddleRight:
        case Anchor::BottomRight:
            topLeft.x = anchoredPointPos.x - mSize.x;
            break;
        }

        switch (mAnchor)
        {
        case Anchor::TopLeft:
        case Anchor::TopMiddle:
        case Anchor::TopRight:
            break;
        case Anchor::MiddleLeft:
        case Anchor::Middle:
        case Anchor::MiddleRight:
            topLeft.y = anchoredPointPos.y - mSize.y / 2.f;
            break;
        case Anchor::BottomLeft:
        case Anchor::BottomMiddle:
        case Anchor::BottomRight:
            topLeft.y = anchoredPointPos.y - mSize.y;
            break;
        }

        return topLeft;
    }
};

class CustomHudElement : public HudElement
{
public:
    enum class Type {
        Text,
    };
    std::string mDisplayName;
    Type mType;
    std::string mText;
    char mInputBuffer[256] = { 0 };
    float mFontSize = 24.f;
    bool mBold = false;
    std::string mIdentifier;
    bool mUseThemeColor = false;
    ImColor mColor = ImColor(255, 255, 255, 255);

    CustomHudElement(std::string identifier, std::string displayName, Type type, std::string text, bool useThemeColor, ImColor color)
        : mDisplayName(displayName), mText(text), mType(type), mUseThemeColor(useThemeColor), mColor(color) {
        mCustom = true;
        mIdentifier = identifier;
        mParentTypeIdentifier = mIdentifier.data();
    }
};

class HudEditor : public ModuleBase<HudEditor> {
public:
    static inline HudEditor* gInstance = nullptr;

    HudEditor() : ModuleBase("HudEditor", "Shows the hud editor on screen", ModuleCategory::Visual, VK_HOME, false) {
        mNames = {
            {Lowercase, "hudeditor"},
            {LowercaseSpaced, "hud editor"},
            {Normal, "HudEditor"},
            {NormalSpaced, "Hud Editor"}
        };

        gInstance = this;

        gFeatureManager->mDispatcher->listen<RenderEvent, &HudEditor::onRenderEvent, nes::event_priority::FIRST>(this);
        gFeatureManager->mDispatcher->listen<RenderEvent, &HudEditor::onCustomRenderEvent, nes::event_priority::LAST>(this);
    }

    const float mSnapPointDist = 10.f;

    int mSnapDistance = 0.f;
    std::vector<HudElement*> mElements = {};
    std::vector<std::unique_ptr<CustomHudElement>> mCustomElements = {};

    nlohmann::json mLoadedData; // Stores layout values to safely load elements initialized later in the boot cycle

    bool mCustomGuiOpen = false;
    bool mClampNeeded = false;
    HudElement* mRightClickedElement = nullptr; // Track the current right-clicked element

    void registerElement(HudElement* element);

    void showAllElements();
    void hideAllElements();
    void saveToFile();
    void loadFromFile();
    void clampElements();

    void onInit() override;
    void onEnable() override;
    void onDisable() override;
    void renderEditor(bool fromClickGui = false);
    void onRenderEvent(class RenderEvent& event);
    void onCustomRenderEvent(class RenderEvent& event);
    void onKeyEvent(class KeyEvent& event);
    void onMouseEvent(MouseEvent& event);

private:
    ImVec2 getBoundingBoxTopLeft(HudElement* element);
    void setBoundingBoxTopLeft(HudElement* element, ImVec2 newTopLeft);
};