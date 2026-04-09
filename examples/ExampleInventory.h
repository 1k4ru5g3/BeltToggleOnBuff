#pragma once
#include "../sdk/PluginHelpers.h"
#include <chrono>
#include <cmath>
#include <algorithm>

namespace Examples {

// Renders inventory panel matching Debug->ServerData/Inventory tab:
// ServerData header, player inventory list, inventory selector with WatchInventory,
// slot grid, per-item TreeNode with rarity coloring and inline mods.
inline void DrawInventoryPanel(
    const std::shared_ptr<const PluginSDK::PluginGameSnapshot>& snapshot,
    PluginContext* ctx,
    std::chrono::steady_clock::time_point& lastScan) {

    if (!ctx) return;

    // Request scan periodically (for currency totals in snapshot)
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastScan).count();
    if (ctx->RequestInventoryScan && elapsed > 2000) {
        ctx->RequestInventoryScan(-1);
        lastScan = now;
    }

    // --- ServerData header ---
    if (ctx->GetServerDataAddress && ctx->GetPlayerInventoryList) {
        uintptr_t serverDataAddr = ctx->GetServerDataAddress();
        auto playerInvList = ctx->GetPlayerInventoryList();

        if (ImGui::BeginTable("SrvData", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("ServerData Address");
            ImGui::TableNextColumn();
            char addrBuf[20]; snprintf(addrBuf, sizeof(addrBuf), "0x%llX", serverDataAddr);
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "%s", addrBuf);
            if (ImGui::IsItemHovered() && ImGui::IsItemClicked()) ImGui::SetClipboardText(addrBuf);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Player Inventories");
            ImGui::TableNextColumn(); ImGui::Text("%d", (int)playerInvList.size());

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Scanned Inventories");
            ImGui::TableNextColumn(); ImGui::Text("%d", (int)snapshot->Inventories.size());

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Currency Types");
            ImGui::TableNextColumn(); ImGui::Text("%d", (int)snapshot->CurrencyTotals.size());

            ImGui::EndTable();
        }

        // --- Player Inventories tree-table ---
        if (!playerInvList.empty() && ImGui::TreeNode("Player Inventories")) {
            if (ImGui::BeginTable("InvTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 160.0f);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                for (const auto& [id, addr] : playerInvList) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("%d", id);
                    ImGui::TableNextColumn();
                    char ab[20]; snprintf(ab, sizeof(ab), "0x%llX", addr);
                    ImGui::Text("%s", ab);
                    if (ImGui::IsItemHovered() && ImGui::IsItemClicked()) ImGui::SetClipboardText(ab);
                    ImGui::TableNextColumn();
                    if (ctx->GetInventoryName) {
                        const char* name = ctx->GetInventoryName(id);
                        ImGui::Text("%s", name ? name : "");
                    }
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }

        // --- Inventory Selector & Grid ---
        if (!playerInvList.empty() && ctx->WatchInventory && ctx->GetWatchedInventoryData) {
            ImGui::Separator();
            static int selectedInvIdx = 0;

            int invCount = static_cast<int>(playerInvList.size());
            if (selectedInvIdx >= invCount) selectedInvIdx = 0;

            char previewLabel[64] = "Select...";
            if (selectedInvIdx < invCount) {
                int pid = playerInvList[selectedInvIdx].first;
                const char* name = ctx->GetInventoryName ? ctx->GetInventoryName(pid) : "";
                snprintf(previewLabel, sizeof(previewLabel), "[%d] %s", pid, name ? name : "");
            }

            if (ImGui::BeginCombo("Inspect Inventory", previewLabel)) {
                for (int ci = 0; ci < invCount; ci++) {
                    int cid = playerInvList[ci].first;
                    const char* name = ctx->GetInventoryName ? ctx->GetInventoryName(cid) : "";
                    char itemLabel[64];
                    snprintf(itemLabel, sizeof(itemLabel), "[%d] %s", cid, name ? name : "");
                    bool isSelected = (ci == selectedInvIdx);
                    if (ImGui::Selectable(itemLabel, isSelected)) {
                        selectedInvIdx = ci;
                        ctx->WatchInventory(cid);
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            // Ensure we're watching the currently selected inventory
            if (selectedInvIdx < invCount) {
                ctx->WatchInventory(playerInvList[selectedInvIdx].first);
            }

            auto si = ctx->GetWatchedInventoryData();
            if (si.InventoryId >= 0 && si.TotalBoxesX > 0 && si.TotalBoxesY > 0) {
                if (ImGui::BeginTable("InvDetail", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Address");
                    ImGui::TableNextColumn();
                    char ab2[20]; snprintf(ab2, sizeof(ab2), "0x%llX", si.Address);
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "%s", ab2);
                    if (ImGui::IsItemHovered() && ImGui::IsItemClicked()) ImGui::SetClipboardText(ab2);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Total Boxes X");
                    ImGui::TableNextColumn(); ImGui::Text("%d", si.TotalBoxesX);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Total Boxes Y");
                    ImGui::TableNextColumn(); ImGui::Text("%d", si.TotalBoxesY);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("ServerRequestCounter");
                    ImGui::TableNextColumn(); ImGui::Text("%d", si.ServerRequestCounter);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Items");
                    ImGui::TableNextColumn(); ImGui::Text("%d", (int)si.Items.size());

                    ImGui::EndTable();
                }

                // Visual slot grid
                if (ImGui::TreeNode("Inventory Slots")) {
                    float cellSize = 18.0f;
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    ImVec2 origin = ImGui::GetCursorScreenPos();
                    for (int y = 0; y < si.TotalBoxesY; y++) {
                        for (int x = 0; x < si.TotalBoxesX; x++) {
                            int idx = y * si.TotalBoxesX + x;
                            bool occupied = (idx < static_cast<int>(si.SlotOccupied.size())) && si.SlotOccupied[idx];
                            ImVec2 p0(origin.x + x * cellSize, origin.y + y * cellSize);
                            ImVec2 p1(p0.x + cellSize - 1, p0.y + cellSize - 1);
                            ImU32 col = occupied
                                ? IM_COL32(80, 200, 80, 255)
                                : IM_COL32(60, 60, 60, 255);
                            drawList->AddRectFilled(p0, p1, col);
                            drawList->AddRect(p0, p1, IM_COL32(100, 100, 100, 255));
                        }
                    }
                    ImGui::Dummy(ImVec2(si.TotalBoxesX * cellSize, si.TotalBoxesY * cellSize));
                    ImGui::TreePop();
                }

                // Items list with rarity coloring and inline mods
                if (!si.Items.empty() && ImGui::TreeNode("Items")) {
                    auto renderModList = [](const char* label, const std::vector<PluginSDK::DebugModInfo>& mods) {
                        if (mods.empty()) return;
                        if (ImGui::TreeNode(label)) {
                            for (const auto& mod : mods) {
                                bool hasV0 = !std::isnan(mod.Value0);
                                bool hasV1 = !std::isnan(mod.Value1);
                                if (hasV0 && hasV1)
                                    ImGui::Text("%s: %.0f - %.0f", mod.Name.c_str(), mod.Value0, mod.Value1);
                                else if (hasV0)
                                    ImGui::Text("%s: %.0f", mod.Name.c_str(), mod.Value0);
                                else
                                    ImGui::Text("%s", mod.Name.c_str());
                                if (ImGui::IsItemHovered() && ImGui::IsItemClicked())
                                    ImGui::SetClipboardText(mod.Name.c_str());
                            }
                            ImGui::TreePop();
                        }
                    };

                    for (size_t i = 0; i < si.Items.size(); i++) {
                        const auto& item = si.Items[i];
                        const char* rarityNames[] = { "Normal", "Magic", "Rare", "Unique" };
                        int ri = (std::min)(item.Rarity, 3);
                        ImVec4 rarityColors[] = {
                            ImVec4(0.8f, 0.8f, 0.8f, 1.0f),
                            ImVec4(0.5f, 0.5f, 1.0f, 1.0f),
                            ImVec4(1.0f, 1.0f, 0.3f, 1.0f),
                            ImVec4(0.8f, 0.5f, 0.2f, 1.0f),
                        };

                        char itemNodeLabel[256];
                        snprintf(itemNodeLabel, sizeof(itemNodeLabel), "%s##item%zu", item.Path.c_str(), i);
                        ImGui::PushStyleColor(ImGuiCol_Text, rarityColors[ri]);
                        bool open = ImGui::TreeNode(itemNodeLabel);
                        ImGui::PopStyleColor();

                        if (open) {
                            if (ImGui::BeginTable("ItemT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn(); ImGui::Text("Address");
                                ImGui::TableNextColumn();
                                char ab3[20]; snprintf(ab3, sizeof(ab3), "0x%llX", item.Address);
                                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "%s", ab3);
                                if (ImGui::IsItemHovered() && ImGui::IsItemClicked()) ImGui::SetClipboardText(ab3);

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn(); ImGui::Text("Rarity");
                                ImGui::TableNextColumn(); ImGui::Text("%s", rarityNames[ri]);

                                ImGui::EndTable();
                            }
                            renderModList("Implicit Mods", item.ImplicitMods);
                            renderModList("Explicit Mods", item.ExplicitMods);
                            renderModList("Enchant Mods", item.EnchantMods);
                            renderModList("Hellscape Mods", item.HellscapeMods);

                            int totalMods = static_cast<int>(item.ImplicitMods.size() + item.ExplicitMods.size()
                                + item.EnchantMods.size() + item.HellscapeMods.size());
                            if (totalMods == 0) {
                                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(no mods)");
                            }
                            ImGui::TreePop();
                        }
                    }
                    ImGui::TreePop();
                }
            }
        }
    } else {
        // Fallback: old-style inventory display if SDK v4 not available
        if (snapshot->Inventories.empty()) {
            ImGui::TextDisabled("No inventory data (scanning...)");
            return;
        }
        ImGui::Text("Inventories: %d", (int)snapshot->Inventories.size());
    }

    // Currency totals (always available via snapshot)
    if (!snapshot->CurrencyTotals.empty() && ImGui::TreeNode("Currency Totals")) {
        if (ImGui::BeginTable("CurrTable", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0, 150))) {
            ImGui::TableSetupColumn("Currency", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableHeadersRow();
            for (auto& [name, count] : snapshot->CurrencyTotals) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(name.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%d", count);
            }
            ImGui::EndTable();
        }
        ImGui::TreePop();
    }
}

} // namespace Examples
