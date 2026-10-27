#include "HudEditor.hpp"
#include <Features/Events/KeyEvent.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/MinecraftGame.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <Utils/MiscUtils/ImRenderUtils.hpp>
#include "ClickGui.hpp"

bool lastCscState = false;

void HudEditor::showAllElements()
{
    for (auto element : mElements) {
        element->mSampleMode = true;
    }
}

void HudEditor::hideAllElements()
{
    for (auto element : mElements) {
        element->mSampleMode = false;
    }
}

void HudEditor::registerElement(HudElement* element)
{
    // Record original source-code-defined positioning coordinates before overriding with cached data
    element->mDefaultPos = element->mPos;
    element->mDefaultAnchor = element->mAnchor;

    mElements.emplace_back(element);

    // Automatically apply configured coordinate layout parameters if they exist in the cache
    if (!mLoadedData.is_null() && mLoadedData.contains(element->mParentTypeIdentifier))
    {
        auto& data = mLoadedData[element->mParentTypeIdentifier];
        element->mPos = { data["pos"][0], data["pos"][1] };
        element->mSize = { data["size"][0], data["size"][1] };
        element->mAnchor = data["anchor"];
    }
}

void HudEditor::saveToFile()
{
    try
    {
        static std::string path = FileUtils::getSolsticeDir() + "hud.json";
        nlohmann::json j;
        for (auto element : mElements)
        {
            j[element->mParentTypeIdentifier] = {
                {"pos", {element->mPos.x, element->mPos.y}},
                {"size", {element->mSize.x, element->mSize.y}},
                {"anchor", element->mAnchor}
            };

            spdlog::info("Saved element: {}", element->mParentTypeIdentifier);
        }

        for (auto& element : mCustomElements)
        {
            j[element->mParentTypeIdentifier] = {
                {"pos", {element->mPos.x, element->mPos.y}},
                {"size", {element->mSize.x, element->mSize.y}},
                {"anchor", element->mAnchor},
                {"text", element->mText},
                {"fontSize", element->mFontSize},
                {"bold", element->mBold},
                {"useThemeColor", element->mUseThemeColor},
                {"color", {element->mColor.Value.x, element->mColor.Value.y, element->mColor.Value.z, element->mColor.Value.w}}
            };

            spdlog::info("Saved custom element: {}", element->mText);
        }

        j["snapDistance"] = mSnapDistance;

        std::ofstream file(path);
        file << j.dump(4);
        file.close();

        spdlog::info("Saved hud elements to file!");
    }
    catch (const std::exception& e)
    {
        spdlog::error("Failed to save hud elements to file: {}", e.what());
    }
    catch (const nlohmann::json::exception& e)
    {
        spdlog::error("Failed to save hud elements to file: {}", e.what());
    }
    catch (...)
    {
        spdlog::error("Failed to save hud elements to file: unknown error");
    }
}

void HudEditor::clampElements()
{
    auto display = ImGui::GetIO().DisplaySize;

    auto clampOne = [&](HudElement* element) {
        if (!element->mVisible) return;
        auto pos = element->getPos();
        auto size = element->mSize;

        if (element->mCentered)
        {
            pos.x -= size.x / 2;
            pos.y -= size.y / 2;
        }

        float maxX = std::max(0.f, display.x - element->mSize.x);
        float maxY = std::max(0.f, display.y - element->mSize.y);
        pos.x = std::clamp(pos.x, 0.f, maxX);
        pos.y = std::clamp(pos.y, 0.f, maxY);

        if (element->mSize.x > display.x) { element->mSize.x = display.x; pos.x = 0; }
        if (element->mSize.y > display.y) { element->mSize.y = display.y; pos.y = 0; }

        auto newPos = pos;

        if (element->mCentered)
        {
            newPos.x += size.x / 2;
            newPos.y += size.y / 2;
        }

        element->setFromPos(glm::vec2(newPos.x, newPos.y));
        };

    for (auto* element : mElements) clampOne(element);
    for (auto& element : mCustomElements) clampOne(element.get());

    mClampNeeded = false;
}

void HudEditor::loadFromFile()
{
    try
    {
        static std::string path = FileUtils::getSolsticeDir() + "hud.json";
        if (!FileUtils::fileExists(path))
        {
            spdlog::warn("No hud elements file found, creating one!");
            saveToFile();
            return;
        }
        std::ifstream file(path);
        file >> mLoadedData;
        file.close();

        for (auto element : mElements)
        {
            if (mLoadedData.contains(element->mParentTypeIdentifier))
            {
                auto& data = mLoadedData[element->mParentTypeIdentifier];
                element->mPos = { data["pos"][0], data["pos"][1] };
                element->mSize = { data["size"][0], data["size"][1] };
                element->mAnchor = data["anchor"];
            }
        }

        if (mLoadedData.contains("snapDistance"))
        {
            mSnapDistance = mLoadedData["snapDistance"];
        }

        // Parse remaining custom elements from file configuration
        for (auto& [key, value] : mLoadedData.items())
        {
            if (key == "snapDistance") continue;

            bool isRegisteredNonCustom = false;
            for (auto element : mElements) {
                if (element->mParentTypeIdentifier == key && !element->mCustom) {
                    isRegisteredNonCustom = true;
                    break;
                }
            }
            if (isRegisteredNonCustom) continue;

            auto element = std::make_unique<CustomHudElement>(key.c_str(), value["text"], CustomHudElement::Type::Text, value["text"], value["useThemeColor"], ImColor(static_cast<float>(value["color"][0]), value["color"][1], value["color"][2], value["color"][3]));
            element->mPos = { value["pos"][0], value["pos"][1] };
            element->mSize = { value["size"][0], value["size"][1] };
            element->mAnchor = value["anchor"];
            element->mFontSize = value["fontSize"];
            element->mBold = value["bold"];
            element->mUseThemeColor = value.contains("useThemeColor") ? static_cast<bool>(value["useThemeColor"]) : false;

            auto colorArray = value["color"];
            element->mColor = ImColor(static_cast<float>(colorArray[0]), colorArray[1], colorArray[2], colorArray[3]);
            strcpy(element->mInputBuffer, element->mText.c_str());

            element->mDefaultPos = element->mPos;
            element->mDefaultAnchor = element->mAnchor;

            bool found = false;
            for (auto& customElement : mCustomElements)
            {
                if (customElement->mParentTypeIdentifier == key)
                {
                    customElement->mPos = { value["pos"][0], value["pos"][1] };
                    customElement->mSize = { value["size"][0], value["size"][1] };
                    customElement->mAnchor = value["anchor"];
                    customElement->mFontSize = value["fontSize"];
                    customElement->mText = value["text"];
                    customElement->mUseThemeColor = value["useThemeColor"];
                    customElement->mColor = ImColor(static_cast<float>(colorArray[0]), colorArray[1], colorArray[2], colorArray[3]);
                    customElement->mVisible = true;
                    customElement->mBold = value["bold"];
                    strcpy(customElement->mInputBuffer, customElement->mText.c_str());

                    customElement->mDefaultPos = customElement->mPos;
                    customElement->mDefaultAnchor = customElement->mAnchor;

                    found = true;
                }
            }

            if (!found)
                mCustomElements.push_back(std::move(element));
        }

        mClampNeeded = true;
        spdlog::info("Loaded hud elements from file!");
    }
    catch (const std::exception& e)
    {
        spdlog::error("Failed to load hud elements from file: {}", e.what());
    }
    catch (const nlohmann::json::exception& e)
    {
        spdlog::error("Failed to load hud elements from file: {}", e.what());
        saveToFile();
    }
    catch (...)
    {
        spdlog::error("Failed to load hud elements from file: unknown error");
    }
}

void HudEditor::onInit()
{
    loadFromFile();
}

void HudEditor::onEnable()
{
    gFeatureManager->mDispatcher->listen<KeyEvent, &HudEditor::onKeyEvent>(this);
    gFeatureManager->mDispatcher->listen<MouseEvent, &HudEditor::onMouseEvent>(this);

    auto ci = ClientInstance::get();
    lastCscState = ci->getMouseGrabbed();
    ci->releaseMouse();

    static auto clickGuiModule = gFeatureManager->mModuleManager->getModule<ClickGui>();
    if (clickGuiModule) clickGuiModule->setEnabled(false);

    mRightClickedElement = nullptr;
    showAllElements();
}

void HudEditor::onDisable()
{
    gFeatureManager->mDispatcher->deafen<KeyEvent, &HudEditor::onKeyEvent>(this);
    gFeatureManager->mDispatcher->deafen<MouseEvent, &HudEditor::onMouseEvent>(this);

    auto ci = ClientInstance::get();
    ci->grabMouse();

    mRightClickedElement = nullptr;
    saveToFile();
    hideAllElements();
}

ImVec2 HudEditor::getBoundingBoxTopLeft(HudElement* element)
{
    ImVec2 pos = element->getPos();
    if (element->mCentered)
    {
        pos.x -= element->mSize.x / 2.f;
        pos.y -= element->mSize.y / 2.f;
    }
    return pos;
}

void HudEditor::setBoundingBoxTopLeft(HudElement* element, ImVec2 newTopLeft)
{
    if (element->mCentered)
    {
        newTopLeft.x += element->mSize.x / 2.f;
        newTopLeft.y += element->mSize.y / 2.f;
    }
    element->setFromPos({ newTopLeft.x, newTopLeft.y });
}

void HudEditor::renderEditor(bool fromClickGui)
{
    if (!mEnabled && !fromClickGui) return;

    auto drawList = ImGui::GetBackgroundDrawList();
    ImVec2 display = ImGui::GetIO().DisplaySize;

    auto ci = ClientInstance::get();

    if (!fromClickGui) ci->releaseMouse();

    drawList->AddRectFilled(ImVec2(0, 0), display, IM_COL32(0, 0, 0, 50));

    if (ImGui::IsKeyPressed(ImGuiKey_Tab, false))
    {
        if (ImGui::GetIO().KeyShift) mSnapDistance -= 5;
        else mSnapDistance += 5;

        if (mSnapDistance > 100) mSnapDistance = 0;
        if (mSnapDistance < 0) mSnapDistance = 100;

        NotifyUtils::notify("Snap distance: " + std::to_string(mSnapDistance), 2.f, Notification::Type::Info);
    }

    const int snapDistance = mSnapDistance;

    static bool dragging = false;
    static ImVec2 dragStart;
    static ImVec2 dragOffset;
    static HudElement* draggedElement = nullptr;

    if (snapDistance > 0 && !mCustomGuiOpen)
    {
        for (int i = 0; i < display.x; i += snapDistance)
            drawList->AddLine(ImVec2(i, 0), ImVec2(i, display.y), IM_COL32(255, 255, 255, 100));
        for (int i = 0; i < display.y; i += snapDistance)
            drawList->AddLine(ImVec2(0, i), ImVec2(display.x, i), IM_COL32(255, 255, 255, 100));
    }

    // Capture standard Left-Click
    if (ImGui::IsMouseClicked(0) && !mCustomGuiOpen)
    {
        dragStart = ImGui::GetMousePos();
        dragOffset = { 0, 0 };

        for (auto element : mElements)
        {
            if (!element->mVisible) continue;

            ImVec2 pos = element->getPos();
            ImVec2 size = element->mSize;

            if (element->mCentered)
            {
                pos.x -= size.x / 2;
                pos.y -= size.y / 2;
            }

            if (dragStart.x > pos.x && dragStart.x < pos.x + size.x &&
                dragStart.y > pos.y && dragStart.y < pos.y + size.y)
            {
                dragging = true;
                draggedElement = element;
                dragOffset = { dragStart.x - pos.x, dragStart.y - pos.y };
                break;
            }
        }

        for (auto& element : mCustomElements)
        {
            if (!element->mVisible) continue;

            ImVec2 pos = element->getPos();
            ImVec2 size = element->mSize;

            if (element->mCentered)
            {
                pos.x -= size.x / 2;
                pos.y -= size.y / 2;
            }

            if (dragStart.x > pos.x && dragStart.x < pos.x + size.x &&
                dragStart.y > pos.y && dragStart.y < pos.y + size.y)
            {
                dragging = true;
                draggedElement = element.get();
                dragOffset = { dragStart.x - pos.x, dragStart.y - pos.y };
                break;
            }
        }
    }

    // Capture Right-Click to open the context menu
    if (ImGui::IsMouseClicked(1) && !mCustomGuiOpen)
    {
        ImVec2 clickPos = ImGui::GetMousePos();
        mRightClickedElement = nullptr;

        for (auto element : mElements)
        {
            if (!element->mVisible) continue;

            ImVec2 pos = element->getPos();
            ImVec2 size = element->mSize;

            if (element->mCentered)
            {
                pos.x -= size.x / 2;
                pos.y -= size.y / 2;
            }

            if (clickPos.x > pos.x && clickPos.x < pos.x + size.x &&
                clickPos.y > pos.y && clickPos.y < pos.y + size.y)
            {
                mRightClickedElement = element;
                break;
            }
        }

        if (!mRightClickedElement)
        {
            for (auto& element : mCustomElements)
            {
                if (!element->mVisible) continue;

                ImVec2 pos = element->getPos();
                ImVec2 size = element->mSize;

                if (element->mCentered)
                {
                    pos.x -= size.x / 2;
                    pos.y -= size.y / 2;
                }

                if (clickPos.x > pos.x && clickPos.x < pos.x + size.x &&
                    clickPos.y > pos.y && clickPos.y < pos.y + size.y)
                {
                    mRightClickedElement = element.get();
                    break;
                }
            }
        }

        if (mRightClickedElement != nullptr)
        {
            ImGui::OpenPopup("HudElementContextMenu");
        }
    }

    if (dragging && draggedElement != nullptr)
    {
        ImVec2 dragEnd = ImGui::GetMousePos();

        glm::ivec2 delta = { dragEnd.x - dragStart.x, dragEnd.y - dragStart.y };
        glm::ivec2 newPos = { dragEnd.x - dragOffset.x, dragEnd.y - dragOffset.y };

        if (mSnapDistance != 0)
        {
            newPos.x = std::round(newPos.x / mSnapDistance) * mSnapDistance;
            newPos.y = std::round(newPos.y / mSnapDistance) * mSnapDistance;
        }

        if (draggedElement->mCentered)
        {
            newPos.x += draggedElement->mSize.x / 2.f;
            newPos.y += draggedElement->mSize.y / 2.f;
        }

        draggedElement->setFromPos(newPos);

        if (!ImGui::IsMouseDown(0))
        {
            dragging = false;
            draggedElement = nullptr;
        }
    }

    clampElements();

    for (auto element : mElements)
    {
        if (!element->mVisible) continue;

        ImVec2 pos = element->getPos();
        ImVec2 size = element->mSize;
        if (element->mCentered)
        {
            pos.x -= size.x / 2;
            pos.y -= size.y / 2;
        }

        drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(255, 255, 255, 125));
    }

    for (auto& element : mCustomElements)
    {
        if (!element->mVisible) continue;

        ImVec2 pos = element->getPos();
        ImVec2 size = element->mSize;

        if (element->mCentered)
        {
            pos.x -= size.x / 2;
            pos.y -= size.y / 2;
        }

        drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(255, 255, 255, 125));
    }

    float radius = 15;
    ImVec2 buttonPos = { 30, display.y - 30 };
    ImVec4 buttonRect = { buttonPos.x - radius, buttonPos.y - radius, buttonPos.x + radius, buttonPos.y + radius };

    static ImColor butColor = ImColor(255, 255, 255, 125);
    ImColor targetColor = mCustomGuiOpen ? ImColor(0, 255, 0, 200) : ImRenderUtils::isMouseOver(buttonRect) ? ImColor(255, 255, 255, 255) : ImColor(255, 255, 255, 125);
    butColor.Value = MathUtils::lerp(butColor.Value, targetColor.Value, ImGui::GetIO().DeltaTime * 10);

    drawList->AddCircleFilled(buttonPos, radius, butColor);
    drawList->AddLine(ImVec2(buttonRect.x + 5, buttonRect.y + radius), ImVec2(buttonRect.z - 5, buttonRect.y + radius), IM_COL32(0, 0, 0, 255), 2);
    drawList->AddLine(ImVec2(buttonRect.x + radius, buttonRect.y + 5), ImVec2(buttonRect.x + radius, buttonRect.w - 5), IM_COL32(0, 0, 0, 255), 2);

    if (ImGui::IsMouseClicked(0)) {
        if (ImRenderUtils::isMouseOver(buttonRect)) {
            mCustomGuiOpen = !mCustomGuiOpen;
        }
    }

    // Render Context Menu Popup
    if (ImGui::BeginPopup("HudElementContextMenu"))
    {
        if (mRightClickedElement != nullptr)
        {
            std::string label = mRightClickedElement->mParentTypeIdentifier ? mRightClickedElement->mParentTypeIdentifier : "Custom Element";
            ImGui::Text("Align: %s", label.c_str());
            ImGui::Separator();

            if (ImGui::MenuItem("Center Horizontally"))
            {
                ImVec2 display = ImGui::GetIO().DisplaySize;
                ImVec2 curTopLeft = getBoundingBoxTopLeft(mRightClickedElement);
                float targetTopLeftX = (display.x - mRightClickedElement->mSize.x) / 2.f;
                setBoundingBoxTopLeft(mRightClickedElement, { targetTopLeftX, curTopLeft.y });
                mClampNeeded = true;
            }

            if (ImGui::MenuItem("Center Vertically"))
            {
                ImVec2 display = ImGui::GetIO().DisplaySize;
                ImVec2 curTopLeft = getBoundingBoxTopLeft(mRightClickedElement);
                float targetTopLeftY = (display.y - mRightClickedElement->mSize.y) / 2.f;
                setBoundingBoxTopLeft(mRightClickedElement, { curTopLeft.x, targetTopLeftY });
                mClampNeeded = true;
            }

            if (ImGui::MenuItem("Reset to Default Position"))
            {
                mRightClickedElement->mPos = mRightClickedElement->mDefaultPos;
                mRightClickedElement->mAnchor = mRightClickedElement->mDefaultAnchor;
                mClampNeeded = true;
            }
        }
        ImGui::EndPopup();
    }

    if (!mCustomGuiOpen) return;

    ImVec2 center = { display.x / 2, display.y / 2 };
    ImVec2 size = { 700, 700 };
    ImVec2 pos = { center.x - size.x / 2, center.y - size.y / 2 };

    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Hud Editor", &mCustomGuiOpen, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

    ImGui::Text("Edit custom hud elements here!");

    if (ImGui::Button("Add Text"))
    {
        auto element = std::make_unique<CustomHudElement>(StringUtils::randomString(32).c_str(), "Sample Text", CustomHudElement::Type::Text, "Sample Text", false, ImColor(255, 255, 255, 255));
        element->mVisible = true;
        strcpy(element->mInputBuffer, element->mText.c_str());

        element->mDefaultPos = element->mPos;
        element->mDefaultAnchor = element->mAnchor;

        mCustomElements.push_back(std::move(element));
        NotifyUtils::notify("Added new text element!", 1.f, Notification::Type::Info);
    }

    for (int i = 0; i < mCustomElements.size(); i++)
    {
        auto& element = mCustomElements[i];
        if (!element->mVisible) continue;

        std::string inputLabel = "Text##" + std::to_string(i);
        ImGui::InputText(inputLabel.c_str(), element->mInputBuffer, 256);

        ImGui::SameLine();

        std::string fontSizeLabel = "Font Size##" + std::to_string(i);
        ImGui::InputFloat(fontSizeLabel.c_str(), &element->mFontSize);

        std::string boldLabel = "Bold##" + std::to_string(i);
        ImGui::Checkbox(boldLabel.c_str(), &element->mBold);

        ImGui::SameLine();

        ImGui::Checkbox(("Use Theme Color##" + std::to_string(i)).c_str(), &element->mUseThemeColor);

        if (!element->mUseThemeColor)
        {
            ImGui::ColorEdit4(("Color##" + std::to_string(i)).c_str(), &element->mColor.Value.x);
        }

        if (ImGui::Button(("Delete##" + std::to_string(i)).c_str()))
        {
            NotifyUtils::notify("Deleted element!", 1.f, Notification::Type::Info);

            mCustomElements.erase(mCustomElements.begin() + i);
            i--;
        }

        ImGui::SameLine();

        if (ImGui::Button(("Set##" + std::to_string(i)).c_str()))
        {
            element->mText = element->mInputBuffer;
            NotifyUtils::notify("Set text!", 1.f, Notification::Type::Info);
        }

        ImGui::SameLine();

        if (ImGui::Button(("Clear##" + std::to_string(i)).c_str()))
        {
            element->mText = "";
            strcpy(element->mInputBuffer, "");
            NotifyUtils::notify("Cleared text!", 1.f, Notification::Type::Info);
        }
    }

    ImGui::End();
}

void HudEditor::onRenderEvent(RenderEvent& event)
{
    renderEditor(false);
}

void HudEditor::onCustomRenderEvent(RenderEvent& event)
{
    if (mClampNeeded) clampElements();

    for (auto& element : mCustomElements)
    {
        FontHelper::pushPrefFont(element->mFontSize > 30, element->mBold);

        ImVec2 pos = element->getPos();

        auto drawList = ImGui::GetBackgroundDrawList();

        ImVec2 size = ImGui::GetFont()->CalcTextSizeA(element->mFontSize, FLT_MAX, 0, element->mText.c_str());
        ImColor color = element->mUseThemeColor ? ColorUtils::getGlobalGradientColor(ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f)) : element->mColor;

        if (element->mUseThemeColor) ImRenderUtils::drawShadowText(drawList, element->mText, pos, color, element->mFontSize, false, false, true);
        else drawList->AddText(ImGui::GetFont(), element->mFontSize, pos, color, element->mText.c_str());
        element->mSize = { size.x, size.y };

        FontHelper::popPrefFont();
    }
}

bool mShiftHeld = false;

void HudEditor::onKeyEvent(KeyEvent& event)
{
    if (ImGui::GetIO().WantCaptureKeyboard)
    {
        return;
    }

    if (event.mKey == mKey && event.mPressed)
    {
        setEnabled(false);
        return;
    }

    if (event.mKey == VK_ESCAPE && event.mPressed)
    {
        event.cancel();
        if (mCustomGuiOpen) mCustomGuiOpen = false;
        else setEnabled(false);
    }
}

void HudEditor::onMouseEvent(MouseEvent& event)
{
    event.cancel();
}