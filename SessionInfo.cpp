//
// Created by alteik on 02/10/2024.
//

#include "SessionInfo.hpp"
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Network/PacketID.hpp>
#include <Features/Events/PacketInEvent.hpp>
#include <Features/Events/BaseTickEvent.hpp>
#include <SDK/Minecraft/Network/Packets/TextPacket.hpp>
#include <Hook/Hooks/RenderHooks/D3DHook.hpp>
#include "NameProtect.hpp"

std::vector<std::string> gamemodesToGetStatsFor = { "sky", "sky-duos", "sky-squads", "sky-mega", "ctf", "bed", "bed-duos", "bed-squads", "sg", "sg-duos" };

static int mTotalKills = 0;
static int mTotalDeaths = 0;
static int mCompletedRequests = 0;
static bool mShouldUpdate = false;

void SessionInfo::resetStatistics() {
    mTotalKills = 0;
    mTotalDeaths = 0;
    mCompletedRequests = 0;
}

void SessionInfo::makeRequestsForAllGamemodes(const std::string& gamertag) {
    if (gamertag.empty()) return;
    resetStatistics();

    for (const auto& gamemode : gamemodesToGetStatsFor) {
        auto request = std::make_unique<HttpRequest>(
            HttpMethod::GET,
            "https://api.playhive.com/v0/game/all/" + gamemode + "/" + gamertag,
            "",
            "",
            &SessionInfo::onHttpResponse,
            this
        );
        int64_t now = NOW;
        mRequests.emplace_back(now, std::move(request));
    }
}

void SessionInfo::onHttpResponse(HttpResponseEvent event) {
    auto sender = static_cast<SessionInfo*>(event.mSender);
    if (!sender) return;

    if (event.mStatusCode == 200) {
        try {
            nlohmann::json json = nlohmann::json::parse(event.mResponse);

            int kills = 0, deaths = 0;

            if (json.contains("kills") && json["kills"].is_number()) {
                kills = json["kills"];
            }
            if (json.contains("deaths") && json["deaths"].is_number()) {
                deaths = json["deaths"];
            }

            mTotalKills += kills;
            mTotalDeaths += deaths;
        }
        catch (...) {
            // Catch JSON parsing errors safely
        }
    }
    else if (event.mStatusCode == 404) {
        spdlog::info("[SessionInfo] Stats not found for this gamemode!");
    }
    else {
        spdlog::error("[SessionInfo] Request failed with status code: {}", event.mStatusCode);
    }

    mCompletedRequests++;

    if (mCompletedRequests >= static_cast<int>(gamemodesToGetStatsFor.size())) {
        mShouldUpdate = false;
    }
}

void SessionInfo::onEnable() {
    resetStatistics();
    if (gFeatureManager && gFeatureManager->mDispatcher) {
        gFeatureManager->mDispatcher->listen<BaseTickEvent, &SessionInfo::onBaseTickEvent>(this);
        gFeatureManager->mDispatcher->listen<RenderEvent, &SessionInfo::onRenderEvent>(this);
    }
    if (mElement) {
        mElement->mVisible = true;
    }
}

void SessionInfo::onDisable() {
    if (gFeatureManager && gFeatureManager->mDispatcher) {
        gFeatureManager->mDispatcher->deafen<BaseTickEvent, &SessionInfo::onBaseTickEvent>(this);
        gFeatureManager->mDispatcher->deafen<RenderEvent, &SessionInfo::onRenderEvent>(this);
    }
    if (mElement) {
        mElement->mVisible = false;
    }
}

static int kills, deaths;

void SessionInfo::onRenderEvent(RenderEvent& event) {
    auto ci = ClientInstance::get();
    if (!ci) return;
    if (!ci->getLocalPlayer()) return;
    if (!ci->getLevelRenderer()) return;
    if (!mElement) return;

    if (!mShouldUpdate) {
        kills = mTotalKills;
        deaths = mTotalDeaths;
    }

    float kdRatio = (deaths > 0) ? (static_cast<float>(kills) / static_cast<float>(deaths)) : static_cast<float>(kills);

    std::string mKillsStr = "Kills: " + std::to_string(kills);
    std::string mDeathsStr = "Deaths: " + std::to_string(deaths);
    char kdBuffer[32];
    snprintf(kdBuffer, sizeof(kdBuffer), "K/D: %.2f", kdRatio);
    std::string mKdStr = std::string(kdBuffer);

    auto pos = mElement->getPos();
    auto drawList = ImGui::GetForegroundDrawList();

    ImVec2 size = ImVec2(210, 115);
    ImVec2 maxPos = ImVec2(pos.x + size.x, pos.y + size.y);

    // Premium Rounded Glass Window Background & Shadow
    ImRenderUtils::addBlur(ImVec4(pos.x, pos.y, maxPos.x, maxPos.y), 6.f, 6.f, drawList, false);
    drawList->AddRectFilled(pos, maxPos, ImColor(18, 18, 22, 160), 14.0f); // Deep modern dark translucent fill
    drawList->AddRect(pos, maxPos, ImColor(255, 255, 255, 40), 14.0f, 0, 1.2f); // Subtle rim border
    drawList->AddShadowRect(ImVec2(pos.x - 4, pos.y - 4), ImVec2(maxPos.x + 4, maxPos.y + 4), ImColor(0.f, 0.f, 0.f, 0.75f), 35.f, ImVec2(0, 0), 14.0f);

    FontHelper::pushPrefFont(true);

    // Centered Title Header
    std::string titleText = "Statistics";
    ImVec2 titleSize = ImGui::GetFont()->CalcTextSizeA(19.0f, FLT_MAX, 0, titleText.c_str());
    ImVec2 titlePos = ImVec2(pos.x + (size.x - titleSize.x) * 0.5f, pos.y + 10.0f);
    ImRenderUtils::drawShadowText(drawList, titleText, titlePos, ImColor(255, 255, 255, 255), 19.0f, false);

    // Accent Gradient Line under title
    ImVec2 lineStart = ImVec2(pos.x + 15, pos.y + 36);
    ImVec2 lineEnd = ImVec2(maxPos.x - 15, pos.y + 38);
    int lineSegments = 20;
    float segWidth = (lineEnd.x - lineStart.x) / static_cast<float>(lineSegments);
    for (int i = 0; i < lineSegments; i++) {
        ImVec2 segStart = ImVec2(lineStart.x + (i * segWidth), lineStart.y);
        ImVec2 segEnd = ImVec2(segStart.x + segWidth + 0.5f, lineEnd.y);
        drawList->AddRectFilled(segStart, segEnd, ColorUtils::getThemedColor(i));
    }

    // Centered Content Lines
    float textStartY = pos.y + 48.0f;
    float textSpacing = 20.0f;

    auto drawCenteredText = [&](const std::string& text, float yOffset, ImColor color) {
        ImVec2 tSize = ImGui::GetFont()->CalcTextSizeA(16.0f, FLT_MAX, 0, text.c_str());
        ImVec2 tPos = ImVec2(pos.x + (size.x - tSize.x) * 0.5f, yOffset);
        ImRenderUtils::drawShadowText(drawList, text, tPos, color, 16.0f, false);
        };

    drawCenteredText(mKillsStr, textStartY, ImColor(230, 230, 230, 255));
    drawCenteredText(mDeathsStr, textStartY + textSpacing, ImColor(230, 230, 230, 255));
    drawCenteredText(mKdStr, textStartY + (textSpacing * 2.0f), ImColor(100, 220, 255, 255)); // Highlighted K/D ratio in cyan

    FontHelper::popPrefFont();
}

void SessionInfo::onBaseTickEvent(BaseTickEvent& event) {
    auto player = event.mActor;
    if (!player) return;

    if (NOW < lastUpdate + 15000 && !mShouldUpdate) {
        mShouldUpdate = false;
        return;
    }
    else if (!mShouldUpdate && NOW > lastUpdate + 15000) {
        mShouldUpdate = true;
        if (!mPlayerName.empty()) {
            makeRequestsForAllGamemodes(mPlayerName);
        }
        lastUpdate = NOW;
    }

    if (gFeatureManager && gFeatureManager->mModuleManager) {
        auto nameProtect = gFeatureManager->mModuleManager->getModule<NameProtect>();
        if (nameProtect && nameProtect->mEnabled) {
            mPlayerName = nameProtect->mOldLocalName;
        }
        else if (player) {
            mPlayerName = player->getLocalName();
        }
    }
    else if (player) {
        mPlayerName = player->getLocalName();
    }

    for (auto it = mRequests.begin(); it != mRequests.end();) {
        if (it->second && it->second->isDone()) {
            it = mRequests.erase(it);
        }
        else {
            ++it;
        }
    }

    if (!mRequests.empty()) {
        auto& request = mRequests.front();
        if (request.second && !request.second->mRequestSent) {
            request.second->sendAsync();
        }
    }
}