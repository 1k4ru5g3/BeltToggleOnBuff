#pragma once
#include "../sdk/PluginHelpers.h"
#include <deque>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace Examples {

// ============================================================================
// Game UI Explorer — plugin version matching host's GameUiExplorer
// ============================================================================
// Navigates the game's UI element tree via memory reading.
//
// UI Element layout offsets (POE2):
//   0x000: Vtable (ptr)          0x008: Self (ptr)
//   0x010: ChildrensPtr (StdVector of ptrs)
//   0x0B8: ParentPtr (ptr)       0x118: RelativePosition (float,float)
//   0x120: PositionModifier (float,float)
//   0x130: LocalScaleMultiplier (float)
//   0x138: TooltipPtr (ptr)      0x180: Flags (uint32)
//   0x188: ElementType (uint16)  0x18A: ScaleIndex (uint8)
//   0x228: LabelTextColor (uint32)
//   0x22C: LabelBorderColor (uint32) / IsHighlighted (uint8)
//   0x243: ShinyHighlightState (uint8)
//   0x258: TextureNamePtr (ptr)
//   0x288: UnscaledSize (float,float)
//   0x2A4: BackgroundColor (uint32)
//   0x448: StringIdPtr (StdWString, 32 bytes)
//   0x4D0: TextNoTagsPtr (StdWString, 32 bytes)
//   0x668: InventoryPtr (ptr)
// ============================================================================

class PluginUiExplorer {
public:
    void Draw(PluginContext* ctx) {
        if (!ctx || !ctx->ReadProcessMemory) {
            ImGui::TextDisabled("Memory reading not available");
            return;
        }

        m_Ctx = ctx;
        m_Mem = PluginSDK::MemoryReader(ctx);

        // Auto-initialize from game UI root
        if (m_CurrentAddress == 0 && ctx->GetGameUiRootAddress) {
            uintptr_t gameUiAddr = ctx->GetGameUiRootAddress();
            if (gameUiAddr != 0) {
                m_CurrentAddress = gameUiAddr;
                m_History.clear();
                RefreshElement();
            }
        }

        if (m_CurrentAddress == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
                "GameUI Address not found. Game may not be in InGameState.");
            return;
        }

        // Refresh current element each frame
        RefreshElement();
        if (m_ChildAddresses.empty() && m_CurrentAddress != 0) {
            // Re-read children in case the element became valid
        }

        // === Breadcrumbs ===
        DrawBreadcrumbs();
        ImGui::Spacing();

        // === Search ===
        DrawSearchPanel();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // === Current element title with highlight on hover ===
        std::string currentSid = ReadStringId(m_CurrentAddress);
        ImGui::BeginGroup();
        if (!currentSid.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Current: \"%s\"", currentSid.c_str());
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Current: 0x%llX", m_CurrentAddress);
        }
        ImGui::EndGroup();

        if (ImGui::IsItemHovered()) {
            DrawHighlighter(m_CurrentAddress, IM_COL32(255, 255, 0, 255),
                currentSid.empty() ? "Current" : currentSid);
            ImGui::SetTooltip("Click to copy address");
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                char buf[20]; snprintf(buf, sizeof(buf), "0x%llX", m_CurrentAddress);
                ImGui::SetClipboardText(buf);
            }
        }

        ImGui::Spacing();

        // === Navigation Controls ===
        uintptr_t parentAddr = m_Mem.Read<uintptr_t>(m_CurrentAddress + 0xB8);
        bool hasParent = (parentAddr != 0);
        bool hasSelectedChild = (m_CurrentChildIndex >= 0 &&
            m_CurrentChildIndex < static_cast<int>(m_ChildAddresses.size()));

        if (!hasParent) ImGui::BeginDisabled();
        if (ImGui::Button("Parent")) { NavigateTo(parentAddr); }
        if (!hasParent) ImGui::EndDisabled();

        ImGui::SameLine();

        if (!hasSelectedChild) ImGui::BeginDisabled();
        if (ImGui::Button("Child")) { NavigateTo(m_ChildAddresses[m_CurrentChildIndex]); }
        if (!hasSelectedChild) ImGui::EndDisabled();

        ImGui::SameLine();

        if (m_History.empty()) ImGui::BeginDisabled();
        if (ImGui::Button("Back")) {
            if (!m_History.empty()) {
                uintptr_t prevAddr = m_History.back().Address;
                m_History.pop_back();
                m_CurrentAddress = prevAddr;
                m_CurrentChildIndex = -1;
                RefreshElement();
            }
        }
        if (m_History.empty()) ImGui::EndDisabled();

        ImGui::SameLine();

        if (ImGui::Button("Root") && m_Ctx->GetGameUiRootAddress) {
            uintptr_t gameUiAddr = m_Ctx->GetGameUiRootAddress();
            if (gameUiAddr != 0) {
                m_History.clear();
                m_CurrentAddress = gameUiAddr;
                m_CurrentChildIndex = -1;
                RefreshElement();
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("UiRoot") && m_Ctx->GetUiRootAddress) {
            uintptr_t uiRootAddr = m_Ctx->GetUiRootAddress();
            if (uiRootAddr != 0) {
                m_History.clear();
                m_CurrentAddress = uiRootAddr;
                m_CurrentChildIndex = -1;
                RefreshElement();
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Copy Info")) {
            CopyElementInfo(currentSid);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // === Children ===
        if (ImGui::CollapsingHeader("Children", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawChildrenList();
        }

        ImGui::Spacing();

        // === Properties ===
        if (ImGui::CollapsingHeader("Element Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawElementProperties();
        }

        // === Selected Child Preview ===
        if (hasSelectedChild) {
            ImGui::Spacing();
            if (ImGui::CollapsingHeader("Selected Child Preview")) {
                ImGui::Indent();
                DrawElementPropertiesFor(m_ChildAddresses[m_CurrentChildIndex]);
                ImGui::Unindent();
            }
        }
    }

private:
    // --- Scale calculation (matches host's GetScaleValue) ---
    std::pair<float, float> GetScaleValue(int index, float multiplier) {
        if (!m_Ctx || !m_Ctx->GetSnapshot) return { multiplier, multiplier };
        auto snap = m_Ctx->GetSnapshot();
        if (!snap) return { multiplier, multiplier };

        int gameCullValue = m_Ctx->GetGameCullValue ? m_Ctx->GetGameCullValue() : 0;
        float v1 = static_cast<float>((snap->ScreenWidth - gameCullValue * 2) / 2560.0f);
        float v2 = static_cast<float>(snap->ScreenHeight / 1600.0f);

        float widthScale = multiplier;
        float heightScale = multiplier;

        switch (index) {
        case 1: widthScale *= v1; heightScale *= v1; break;
        case 2: widthScale *= v2; heightScale *= v2; break;
        case 3: widthScale *= v1; heightScale *= v2; break;
        }
        return { widthScale, heightScale };
    }

    // --- Position calculation (matches host's recursive GetUnScaledPosition) ---
    ImVec2 GetUnScaledPosition(uintptr_t addr, int depth = 0) {
        if (addr == 0 || depth > 32) return ImVec2(0, 0);

        float relX = m_Mem.Read<float>(addr + 0x118);
        float relY = m_Mem.Read<float>(addr + 0x11C);
        uintptr_t parentAddr = m_Mem.Read<uintptr_t>(addr + 0xB8);

        if (parentAddr == 0) return ImVec2(relX, relY);

        ImVec2 parentPos = GetUnScaledPosition(parentAddr, depth + 1);

        uint32_t flags = m_Mem.Read<uint32_t>(addr + 0x180);
        bool shouldMod = (flags & (1 << 0x0A)) != 0;
        if (shouldMod) {
            float pmX = m_Mem.Read<float>(parentAddr + 0x120);
            float pmY = m_Mem.Read<float>(parentAddr + 0x124);
            parentPos.x += pmX;
            parentPos.y += pmY;
        }

        uint8_t parentScaleIdx = m_Mem.Read<uint8_t>(parentAddr + 0x18A);
        float parentScaleMul = m_Mem.Read<float>(parentAddr + 0x130);
        uint8_t myScaleIdx = m_Mem.Read<uint8_t>(addr + 0x18A);
        float myScaleMul = m_Mem.Read<float>(addr + 0x130);

        if (parentScaleIdx == myScaleIdx && parentScaleMul == myScaleMul) {
            return ImVec2(parentPos.x + relX, parentPos.y + relY);
        }

        auto [psW, psH] = GetScaleValue(parentScaleIdx, parentScaleMul);
        auto [msW, msH] = GetScaleValue(myScaleIdx, myScaleMul);

        ImVec2 myPos;
        myPos.x = (msW != 0) ? (parentPos.x * psW / msW + relX) : (parentPos.x + relX);
        myPos.y = (msH != 0) ? (parentPos.y * psH / msH + relY) : (parentPos.y + relY);
        return myPos;
    }

    ImVec2 GetPosition(uintptr_t addr) {
        uint8_t scaleIdx = m_Mem.Read<uint8_t>(addr + 0x18A);
        float scaleMul = m_Mem.Read<float>(addr + 0x130);
        auto [sw, sh] = GetScaleValue(scaleIdx, scaleMul);

        ImVec2 pos = GetUnScaledPosition(addr);
        pos.x *= sw;
        pos.y *= sh;

        int gameCull = m_Ctx->GetGameCullValue ? m_Ctx->GetGameCullValue() : 0;
        pos.x += static_cast<float>(gameCull);
        return pos;
    }

    ImVec2 GetSize(uintptr_t addr) {
        uint8_t scaleIdx = m_Mem.Read<uint8_t>(addr + 0x18A);
        float scaleMul = m_Mem.Read<float>(addr + 0x130);
        auto [sw, sh] = GetScaleValue(scaleIdx, scaleMul);

        float uw = m_Mem.Read<float>(addr + 0x288);
        float uh = m_Mem.Read<float>(addr + 0x28C);
        return ImVec2(uw * sw, uh * sh);
    }

    bool IsVisible(uintptr_t addr) {
        uint32_t flags = m_Mem.Read<uint32_t>(addr + 0x180);
        return (flags & (1 << 0x0B)) != 0;
    }

    std::string ReadStringId(uintptr_t addr) {
        if (addr == 0 || !m_Ctx->ReadStdWString) return "";
        std::wstring ws = m_Ctx->ReadStdWString(addr + 0x448);
        if (ws.empty()) return "";
        return PluginSDK::WideToNarrow(ws);
    }

    std::string ReadTextNoTags(uintptr_t addr) {
        if (addr == 0 || !m_Ctx->ReadStdWString) return "";
        std::wstring ws = m_Ctx->ReadStdWString(addr + 0x4D0);
        if (ws.empty()) return "";
        return PluginSDK::WideToNarrow(ws);
    }

    // --- Navigation ---
    void NavigateTo(uintptr_t addr) {
        if (addr == 0) return;

        // Save current to history
        if (m_CurrentAddress != 0) {
            BreadcrumbEntry entry;
            entry.Address = m_CurrentAddress;
            entry.Label = ReadStringId(m_CurrentAddress);
            if (entry.Label.empty()) {
                char buf[20]; snprintf(buf, sizeof(buf), "0x%llX", m_CurrentAddress);
                entry.Label = buf;
            }
            if (m_History.empty() || m_History.back().Address != entry.Address) {
                m_History.push_back(entry);
                if (m_History.size() > 32) m_History.pop_front();
            }
        }

        m_CurrentAddress = addr;
        m_CurrentChildIndex = -1;
        RefreshElement();
    }

    void RefreshElement() {
        m_ChildAddresses.clear();
        if (m_CurrentAddress == 0 || !m_Ctx->ReadStdVector) return;

        // Read children StdVector at offset 0x10
        int count = 0;
        void* buf = m_Ctx->ReadStdVector(m_CurrentAddress + 0x10, sizeof(uintptr_t), &count);
        if (buf && count > 0 && count < 10000) {
            auto* ptrs = static_cast<uintptr_t*>(buf);
            m_ChildAddresses.assign(ptrs, ptrs + count);
            free(buf);
        } else if (buf) {
            free(buf);
        }
    }

    // --- Drawing helpers ---
    void DrawHighlighter(uintptr_t addr, ImU32 color, const std::string& label = "") {
        ImVec2 pos = GetPosition(addr);
        ImVec2 size = GetSize(addr);
        if (size.x <= 0 || size.y <= 0) return;

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 maxPos(pos.x + size.x, pos.y + size.y);
        dl->AddRect(pos, maxPos, color, 0.0f, 0, 2.0f);
        ImU32 fillColor = (color & 0x00FFFFFF) | 0x18000000;
        dl->AddRectFilled(pos, maxPos, fillColor);

        if (!label.empty()) {
            ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
            ImVec2 textBgMax(pos.x + textSize.x + 4, pos.y + textSize.y + 2);
            dl->AddRectFilled(pos, textBgMax, IM_COL32(0, 0, 0, 180));
            dl->AddText(ImVec2(pos.x + 2, pos.y + 1), color, label.c_str());
        }
    }

    void DrawBreadcrumbs() {
        if (m_History.empty()) return;

        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Path:");
        ImGui::SameLine();

        int startIdx = 0;
        if (m_History.size() > 6) {
            startIdx = static_cast<int>(m_History.size()) - 6;
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "...");
            ImGui::SameLine();
        }

        for (int i = startIdx; i < static_cast<int>(m_History.size()); i++) {
            auto& entry = m_History[i];
            std::string displayLabel = entry.Label;
            if (displayLabel.length() > 20) displayLabel = displayLabel.substr(0, 17) + "...";

            ImGui::PushID(i);
            if (ImGui::SmallButton(displayLabel.c_str())) {
                uintptr_t targetAddr = entry.Address;
                while (m_History.size() > static_cast<size_t>(i))
                    m_History.pop_back();
                m_CurrentAddress = targetAddr;
                m_CurrentChildIndex = -1;
                RefreshElement();
                ImGui::PopID();
                return;
            }
            ImGui::PopID();

            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), ">");
            ImGui::SameLine();
        }

        std::string currentLabel = ReadStringId(m_CurrentAddress);
        if (currentLabel.empty()) {
            char buf[20]; snprintf(buf, sizeof(buf), "0x%llX", m_CurrentAddress);
            currentLabel = buf;
        }
        if (currentLabel.length() > 20) currentLabel = currentLabel.substr(0, 17) + "...";
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", currentLabel.c_str());
    }

    void DrawSearchPanel() {
        ImGui::SetNextItemWidth(200);
        bool searchTriggered = ImGui::InputTextWithHint("##UiSearch", "Search StringId...",
            m_SearchBuffer, sizeof(m_SearchBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::SameLine();
        if (ImGui::Button("Search") || searchTriggered) {
            if (strlen(m_SearchBuffer) > 0) {
                m_SearchResults.clear();
                std::string query(m_SearchBuffer);
                std::transform(query.begin(), query.end(), query.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                uintptr_t searchRoot = m_Ctx->GetGameUiRootAddress ? m_Ctx->GetGameUiRootAddress() : 0;
                if (searchRoot != 0) {
                    SearchTree(searchRoot, query, 0, 15);
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            m_SearchBuffer[0] = '\0';
            m_SearchResults.clear();
        }

        if (!m_SearchResults.empty()) {
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "Found %d result(s):",
                static_cast<int>(m_SearchResults.size()));

            float listHeight = (std::min)(120.0f, static_cast<float>(m_SearchResults.size()) * 20.0f + 4.0f);
            if (ImGui::BeginListBox("##SearchResults", ImVec2(-1, listHeight))) {
                for (auto& [addr, name] : m_SearchResults) {
                    char label[256];
                    snprintf(label, sizeof(label), "0x%llX - %s", addr, name.c_str());
                    if (ImGui::Selectable(label)) {
                        NavigateTo(addr);
                    }
                    if (ImGui::IsItemHovered()) {
                        DrawHighlighter(addr, IM_COL32(0, 255, 255, 255), name);
                    }
                }
                ImGui::EndListBox();
            }
        }
    }

    void SearchTree(uintptr_t addr, const std::string& query, int depth, int maxDepth) {
        if (depth > maxDepth || addr == 0 || m_SearchResults.size() >= 100) return;

        std::string stringId = ReadStringId(addr);
        if (!stringId.empty()) {
            std::string lower = stringId;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lower.find(query) != std::string::npos) {
                m_SearchResults.emplace_back(addr, stringId);
            }
        }

        // Read children and recurse
        if (!m_Ctx->ReadStdVector) return;
        int count = 0;
        void* buf = m_Ctx->ReadStdVector(addr + 0x10, sizeof(uintptr_t), &count);
        if (buf && count > 0) {
            auto* ptrs = static_cast<uintptr_t*>(buf);
            int maxChildren = (std::min)(count, 10000);
            for (int i = 0; i < maxChildren && m_SearchResults.size() < 100; i++) {
                SearchTree(ptrs[i], query, depth + 1, maxDepth);
            }
            free(buf);
        } else if (buf) {
            free(buf);
        }
    }

    void DrawChildrenList() {
        int childCount = static_cast<int>(m_ChildAddresses.size());
        ImGui::Text("Children: %d", childCount);
        if (childCount == 0) return;

        ImGui::SameLine(150);

        // Quick nav buttons
        if (ImGui::SmallButton("|<")) m_CurrentChildIndex = 0;
        ImGui::SameLine();
        if (ImGui::SmallButton("<")) {
            if (m_CurrentChildIndex > 0) m_CurrentChildIndex--;
            else m_CurrentChildIndex = childCount - 1;
        }
        ImGui::SameLine();
        ImGui::Text("%d/%d", m_CurrentChildIndex + 1, childCount);
        ImGui::SameLine();
        if (ImGui::SmallButton(">")) {
            if (m_CurrentChildIndex < childCount - 1) m_CurrentChildIndex++;
            else m_CurrentChildIndex = 0;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(">|")) m_CurrentChildIndex = childCount - 1;

        // Children combo
        std::string preview = (m_CurrentChildIndex >= 0)
            ? std::to_string(m_CurrentChildIndex)
            : "None";

        if (ImGui::BeginCombo("##Children", preview.c_str())) {
            for (int i = 0; i < childCount; ++i) {
                uintptr_t childAddr = m_ChildAddresses[i];
                bool isSelected = (m_CurrentChildIndex == i);
                bool isVis = IsVisible(childAddr);

                std::stringstream ss;
                ss << i;

                std::string childSid = ReadStringId(childAddr);
                if (!childSid.empty()) {
                    if (childSid.length() > 30) childSid = childSid.substr(0, 27) + "...";
                    ss << " \"" << childSid << "\"";
                }

                // Grandchild count
                if (m_Ctx->ReadStdVector) {
                    int gc = 0;
                    void* gcBuf = m_Ctx->ReadStdVector(childAddr + 0x10, sizeof(uintptr_t), &gc);
                    if (gcBuf) { free(gcBuf); }
                    if (gc > 0) ss << " [" << gc << "]";
                }

                ImVec2 sz = GetSize(childAddr);
                if (sz.x > 0 && sz.y > 0) {
                    ss << " (" << static_cast<int>(sz.x) << "x" << static_cast<int>(sz.y) << ")";
                }

                ImVec4 textColor = isVis
                    ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f)
                    : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);

                ImGui::PushStyleColor(ImGuiCol_Text, textColor);
                if (ImGui::Selectable(ss.str().c_str(), isSelected)) {
                    m_CurrentChildIndex = i;
                }
                ImGui::PopStyleColor();

                if (isSelected) ImGui::SetItemDefaultFocus();

                if (ImGui::IsItemHovered()) {
                    std::string hoverLabel = std::to_string(i);
                    std::string cs = ReadStringId(childAddr);
                    if (!cs.empty()) hoverLabel += " " + cs;
                    DrawHighlighter(childAddr, IM_COL32(255, 255, 0, 255), hoverLabel);
                }
            }
            ImGui::EndCombo();
        }

        // Highlight selected child
        if (m_CurrentChildIndex >= 0 && m_CurrentChildIndex < childCount) {
            uintptr_t childAddr = m_ChildAddresses[m_CurrentChildIndex];
            std::string label = std::to_string(m_CurrentChildIndex);
            std::string cs = ReadStringId(childAddr);
            if (!cs.empty()) label += " " + cs;
            DrawHighlighter(childAddr, IM_COL32(255, 80, 80, 255), label);
        }
    }

    void DrawElementProperties() {
        DrawElementPropertiesFor(m_CurrentAddress);
    }

    void DrawElementPropertiesFor(uintptr_t addr) {
        if (addr == 0) return;

        std::string stringId = ReadStringId(addr);
        ImVec2 pos = GetPosition(addr);
        ImVec2 size = GetSize(addr);
        ImVec2 unscaledPos = GetUnScaledPosition(addr);

        // StringId
        if (!stringId.empty()) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "StringId: \"%s\"", stringId.c_str());
            if (ImGui::IsItemHovered() && ImGui::IsItemClicked())
                ImGui::SetClipboardText(stringId.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "StringId: (empty)");
        }
        ImGui::Spacing();

        if (ImGui::BeginTable("UiProps", 2,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

            // Address
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Address");
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "0x%llX", addr);
            if (ImGui::IsItemHovered() && ImGui::IsItemClicked()) {
                char buf[20]; snprintf(buf, sizeof(buf), "0x%llX", addr);
                ImGui::SetClipboardText(buf);
            }

            // Parent
            uintptr_t parentA = m_Mem.Read<uintptr_t>(addr + 0xB8);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Parent");
            ImGui::TableNextColumn();
            if (parentA != 0)
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "0x%llX", parentA);
            else
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(root)");

            // Visible
            bool vis = IsVisible(addr);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Visible (local)");
            ImGui::TableNextColumn();
            ImGui::TextColored(vis ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1), vis ? "TRUE" : "FALSE");

            // Children
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Children");
            ImGui::TableNextColumn();
            if (addr == m_CurrentAddress)
                ImGui::Text("%d", static_cast<int>(m_ChildAddresses.size()));
            else {
                int gc = 0;
                void* gcBuf = m_Ctx->ReadStdVector ? m_Ctx->ReadStdVector(addr + 0x10, sizeof(uintptr_t), &gc) : nullptr;
                if (gcBuf) free(gcBuf);
                ImGui::Text("%d", gc);
            }

            // --- Position ---
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "--- Position ---");
            ImGui::TableNextColumn();

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Screen Pos");
            ImGui::TableNextColumn(); ImGui::Text("%.1f, %.1f", pos.x, pos.y);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Unscaled Pos");
            ImGui::TableNextColumn(); ImGui::Text("%.1f, %.1f", unscaledPos.x, unscaledPos.y);

            float relX = m_Mem.Read<float>(addr + 0x118);
            float relY = m_Mem.Read<float>(addr + 0x11C);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Relative Pos");
            ImGui::TableNextColumn(); ImGui::Text("%.1f, %.1f", relX, relY);

            uint32_t flags = m_Mem.Read<uint32_t>(addr + 0x180);
            bool shouldMod = (flags & (1 << 0x0A)) != 0;
            float pmX = m_Mem.Read<float>(addr + 0x120);
            float pmY = m_Mem.Read<float>(addr + 0x124);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Pos Modifier");
            ImGui::TableNextColumn();
            if (shouldMod) ImGui::Text("%.1f, %.1f", pmX, pmY);
            else ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "disabled");

            // --- Size ---
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "--- Size ---");
            ImGui::TableNextColumn();

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Screen Size");
            ImGui::TableNextColumn(); ImGui::Text("%.1f x %.1f", size.x, size.y);

            float uw = m_Mem.Read<float>(addr + 0x288);
            float uh = m_Mem.Read<float>(addr + 0x28C);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Unscaled Size");
            ImGui::TableNextColumn(); ImGui::Text("%.1f x %.1f", uw, uh);

            // --- Scale ---
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "--- Scale ---");
            ImGui::TableNextColumn();

            uint8_t scaleIdx = m_Mem.Read<uint8_t>(addr + 0x18A);
            float scaleMul = m_Mem.Read<float>(addr + 0x130);
            const char* scaleNames[] = { "None (Multiply)", "Width/Width", "Height/Height", "Width/Height" };
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Scale Index");
            ImGui::TableNextColumn();
            ImGui::Text("%d (%s)", scaleIdx, (scaleIdx <= 3) ? scaleNames[scaleIdx] : "Unknown");

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Scale Multiplier");
            ImGui::TableNextColumn(); ImGui::Text("%.4f", scaleMul);

            auto [sw, sh] = GetScaleValue(scaleIdx, scaleMul);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Computed Scale");
            ImGui::TableNextColumn(); ImGui::Text("W: %.4f  H: %.4f", sw, sh);

            // --- Flags ---
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "--- Flags ---");
            ImGui::TableNextColumn();

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Flags (raw)");
            ImGui::TableNextColumn(); ImGui::Text("0x%08X", flags);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("IsVisible bit");
            ImGui::TableNextColumn();
            ImGui::TextColored(vis ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1), vis ? "SET" : "CLEAR");

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("ShouldModifyPos bit");
            ImGui::TableNextColumn();
            ImGui::TextColored(shouldMod ? ImVec4(0.3f,1,0.3f,1) : ImVec4(0.5f,0.5f,0.5f,1), shouldMod ? "SET" : "CLEAR");

            // Colors
            auto drawColor = [](const char* label, uint32_t color) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%s", label);
                ImGui::TableNextColumn();
                if (color != 0) {
                    float r = ((color >> 0) & 0xFF) / 255.0f;
                    float g = ((color >> 8) & 0xFF) / 255.0f;
                    float b = ((color >> 16) & 0xFF) / 255.0f;
                    float a = ((color >> 24) & 0xFF) / 255.0f;
                    ImGui::ColorButton("##c", ImVec4(r,g,b,a), ImGuiColorEditFlags_NoTooltip, ImVec2(16,16));
                    ImGui::SameLine();
                    ImGui::Text("0x%08X (%d,%d,%d,%d)", color,
                        (color>>0)&0xFF, (color>>8)&0xFF, (color>>16)&0xFF, (color>>24)&0xFF);
                } else {
                    ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "0x00000000 (transparent)");
                }
            };

            uint32_t bgColor = m_Mem.Read<uint32_t>(addr + 0x2A4);
            uint32_t txtColor = m_Mem.Read<uint32_t>(addr + 0x228);
            uint32_t brdColor = m_Mem.Read<uint32_t>(addr + 0x22C);
            drawColor("Background Color", bgColor);
            drawColor("Text Color", txtColor);
            drawColor("Border Color", brdColor);

            // --- Element Type ---
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "--- Element Type ---");
            ImGui::TableNextColumn();

            uint16_t elType = m_Mem.Read<uint16_t>(addr + 0x188);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Type");
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "0x%04X (%u)", elType, elType);

            // --- Extra ---
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "--- Extra ---");
            ImGui::TableNextColumn();

            // Tooltip
            uintptr_t tooltipAddr = m_Mem.Read<uintptr_t>(addr + 0x138);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Tooltip Ptr");
            ImGui::TableNextColumn();
            if (tooltipAddr != 0)
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "0x%llX", tooltipAddr);
            else
                ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "NULL");

            if (tooltipAddr != 0) {
                std::string tooltipSid = ReadStringId(tooltipAddr);
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Tooltip StringId");
                ImGui::TableNextColumn();
                if (!tooltipSid.empty())
                    ImGui::TextColored(ImVec4(0.8f,0.9f,1,1), "\"%s\"", tooltipSid.c_str());
                else
                    ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "(empty)");

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                char navBtnId[32]; snprintf(navBtnId, sizeof(navBtnId), "Navigate to Tooltip##tt%llX", addr);
                if (ImGui::SmallButton(navBtnId)) NavigateTo(tooltipAddr);
            }

            // IsHighlighted
            uint8_t isHl = m_Mem.Read<uint8_t>(addr + 0x22C);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("IsHighlighted");
            ImGui::TableNextColumn();
            ImGui::TextColored(isHl ? ImVec4(1,1,0.3f,1) : ImVec4(0.5f,0.5f,0.5f,1),
                "%s (%u)", isHl ? "YES" : "NO", isHl);

            // ShinyHighlight
            uint8_t shiny = m_Mem.Read<uint8_t>(addr + 0x243);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Shiny Highlight");
            ImGui::TableNextColumn(); ImGui::Text("%u", shiny);

            // TextNoTags
            std::string textNoTags = ReadTextNoTags(addr);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("TextNoTags");
            ImGui::TableNextColumn();
            if (!textNoTags.empty())
                ImGui::TextColored(ImVec4(0.8f,0.9f,1,1), "\"%s\"", textNoTags.c_str());
            else
                ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "(empty)");

            // Texture
            uintptr_t texPtr = m_Mem.Read<uintptr_t>(addr + 0x258);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Texture Ptr");
            ImGui::TableNextColumn();
            if (texPtr != 0)
                ImGui::TextColored(ImVec4(0.6f,0.8f,1,1), "0x%llX", texPtr);
            else
                ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "NULL");

            // Vtable
            uintptr_t vtable = m_Mem.Read<uintptr_t>(addr + 0x00);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Vtable");
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1), "0x%llX", vtable);

            // Inventory
            uintptr_t invPtr = m_Mem.Read<uintptr_t>(addr + 0x668);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Inventory Ptr");
            ImGui::TableNextColumn();
            if (invPtr != 0)
                ImGui::TextColored(ImVec4(1,0.8f,0.2f,1), "0x%llX", invPtr);
            else
                ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "NULL");

            ImGui::EndTable();
        }
    }

    void CopyElementInfo(const std::string& currentSid) {
        ImVec2 p = GetPosition(m_CurrentAddress);
        ImVec2 s = GetSize(m_CurrentAddress);
        float uw = m_Mem.Read<float>(m_CurrentAddress + 0x288);
        float uh = m_Mem.Read<float>(m_CurrentAddress + 0x28C);
        uint32_t flags = m_Mem.Read<uint32_t>(m_CurrentAddress + 0x180);
        uint8_t scaleIdx = m_Mem.Read<uint8_t>(m_CurrentAddress + 0x18A);
        float scaleMul = m_Mem.Read<float>(m_CurrentAddress + 0x130);
        float relX = m_Mem.Read<float>(m_CurrentAddress + 0x118);
        float relY = m_Mem.Read<float>(m_CurrentAddress + 0x11C);
        uintptr_t parentA = m_Mem.Read<uintptr_t>(m_CurrentAddress + 0xB8);
        uint16_t elType = m_Mem.Read<uint16_t>(m_CurrentAddress + 0x188);
        float pmX = m_Mem.Read<float>(m_CurrentAddress + 0x120);
        float pmY = m_Mem.Read<float>(m_CurrentAddress + 0x124);
        uint8_t isHl = m_Mem.Read<uint8_t>(m_CurrentAddress + 0x22C);
        uint32_t txtColor = m_Mem.Read<uint32_t>(m_CurrentAddress + 0x228);
        uint32_t brdColor = m_Mem.Read<uint32_t>(m_CurrentAddress + 0x22C);

        std::ostringstream ss;
        ss << "=== UI Element Debug ===\n";
        ss << "Address: 0x" << std::hex << m_CurrentAddress << "\n";
        ss << "StringId: " << currentSid << "\n";
        ss << std::dec << std::fixed << std::setprecision(1);
        ss << "Screen Pos: " << p.x << ", " << p.y << "\n";
        ss << "Screen Size: " << s.x << " x " << s.y << "\n";
        ss << "Unscaled Size: " << uw << " x " << uh << "\n";
        ss << "Visible: " << (IsVisible(m_CurrentAddress) ? "true" : "false") << "\n";
        ss << "Children: " << m_ChildAddresses.size() << "\n";
        ss << "Flags: 0x" << std::hex << flags << "\n";
        ss << "ScaleIndex: " << std::dec << (int)scaleIdx << "\n";
        ss << "ScaleMultiplier: " << scaleMul << "\n";
        ss << "RelPos: " << relX << ", " << relY << "\n";
        ss << "Parent: 0x" << std::hex << parentA << "\n";
        ss << "ElementType: 0x" << std::hex << elType << " (" << std::dec << elType << ")\n";
        ss << std::dec;
        ss << "PositionModifier: " << pmX << ", " << pmY << "\n";
        ss << "IsHighlighted: " << (int)isHl << "\n";
        ss << "TextColor: 0x" << std::hex << txtColor << "\n";
        ss << "BorderColor: 0x" << brdColor << "\n";
        ImGui::SetClipboardText(ss.str().c_str());
    }

    PluginContext* m_Ctx = nullptr;
    PluginSDK::MemoryReader m_Mem{ nullptr };
    uintptr_t m_CurrentAddress = 0;
    int m_CurrentChildIndex = -1;

    struct BreadcrumbEntry {
        uintptr_t Address = 0;
        std::string Label;
    };
    std::deque<BreadcrumbEntry> m_History;
    std::vector<uintptr_t> m_ChildAddresses;

    char m_SearchBuffer[128] = {};
    std::vector<std::pair<uintptr_t, std::string>> m_SearchResults;
};

} // namespace Examples
