#pragma once
#include "../sdk/PluginHelpers.h"
#include <algorithm>
#include <sstream>
#include <limits>

namespace Examples {

// --- Component detail renderers (must be defined before DrawEntitiesPanel) ---

inline void DrawAddressRow(const char* label, uintptr_t addr) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::Text("%s", label);
    ImGui::TableNextColumn();
    char buf[20]; snprintf(buf, sizeof(buf), "0x%llX", addr);
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "%s", buf);
    if (ImGui::IsItemHovered() && ImGui::IsItemClicked()) ImGui::SetClipboardText(buf);
}

inline void DrawLifeComp(const PluginSDK::DebugLifeComp& cl, uint32_t entityId) {
    if (ImGui::BeginTable("LifeT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        DrawAddressRow("Address", cl.Address);
        DrawAddressRow("Owner Address", cl.OwnerAddress);
        ImGui::EndTable();
    }

    auto renderVital = [](const char* vlabel, const PluginSDK::DebugVital& v) {
        if (ImGui::TreeNode(vlabel)) {
            if (ImGui::BeginTable("VitalT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Regeneration");
                ImGui::TableNextColumn(); ImGui::Text("%.4f", v.Regeneration);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Total");
                ImGui::TableNextColumn(); ImGui::Text("%d", v.Total);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("ReservedFlat");
                ImGui::TableNextColumn(); ImGui::Text("%d", v.ReservedFlat);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Current");
                ImGui::TableNextColumn(); ImGui::Text("%d", v.Current);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Reserved(%%)");
                ImGui::TableNextColumn(); ImGui::Text("%d", v.ReservedPercent);

                if (v.Total > 0) {
                    float pct = static_cast<float>(v.Current) / v.Total * 100.0f;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Current(%%)");
                    ImGui::TableNextColumn(); ImGui::Text("%.1f%%", pct);
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    };
    renderVital("Health", cl.Health);
    renderVital("Energy Shield", cl.EnergyShield);
    renderVital("Mana", cl.Mana);
}

inline void DrawRenderComp(const PluginSDK::DebugRenderComp& cr) {
    if (ImGui::BeginTable("RenderT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        DrawAddressRow("Address", cr.Address);
        DrawAddressRow("Owner Address", cr.OwnerAddress);
        ImGui::EndTable();
    }
    ImGui::Text("Grid Position: {%.2f, %.2f}", cr.GridX, cr.GridY);
    ImGui::Text("World Position: {%.2f, %.2f, %.2f}", cr.WorldX, cr.WorldY, cr.WorldZ);
    if (ImGui::BeginTable("RenderT2", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Terrain Height (Z-Axis)");
        ImGui::TableNextColumn(); ImGui::Text("%.4f", cr.TerrainHeight);
        ImGui::EndTable();
    }
    ImGui::Text("Model Bounds: {%.2f, %.2f, %.2f}", cr.ModelBoundsX, cr.ModelBoundsY, cr.ModelBoundsZ);
}

inline void DrawPositionedComp(const PluginSDK::DebugPositionedComp& cp) {
    if (ImGui::BeginTable("PosT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        DrawAddressRow("Address", cp.Address);
        DrawAddressRow("Owner Address", cp.OwnerAddress);
        ImGui::EndTable();
    }
    ImGui::Text("Flags: 0x%02X", cp.Reaction);
    if (ImGui::BeginTable("PosT2", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("IsFriendly");
        ImGui::TableNextColumn(); ImGui::TextColored(cp.IsFriendly ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1), cp.IsFriendly ? "true" : "false");
        ImGui::EndTable();
    }
}

inline void DrawTargetableComp(const PluginSDK::DebugTargetableComp& ct) {
    if (ImGui::BeginTable("TgtT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        DrawAddressRow("Address", ct.Address);
        DrawAddressRow("Owner Address", ct.OwnerAddress);

        auto boolRow = [](const char* name, bool val) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%s", name);
            ImGui::TableNextColumn(); ImGui::TextColored(val ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1), val ? "true" : "false");
        };
        boolRow("IsHighlightable", ct.IsHighlightable);
        boolRow("IsTargettedByPlayer", ct.IsTargettedByPlayer);
        boolRow("IsTargetable", ct.IsTargetable);
        boolRow("HiddenFromPlayer", ct.HiddenFromPlayer);
        boolRow("MeetsQuestState", ct.MeetsQuestState);
        boolRow("MeetsItemRequirements", ct.MeetsItemRequirements);
        ImGui::EndTable();
    }
}

inline void DrawAnimatedComp(const PluginSDK::DebugAnimatedComp& ca) {
    if (ImGui::BeginTable("AnimT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        DrawAddressRow("Address", ca.Address);
        DrawAddressRow("Owner Address", ca.OwnerAddress);
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Path");
        ImGui::TableNextColumn(); ImGui::TextWrapped("%s", ca.Path.c_str());
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Id");
        ImGui::TableNextColumn(); ImGui::Text("%u", ca.Id);
        ImGui::EndTable();
    }
}

inline void DrawStatsComp(const PluginSDK::DebugStatsComp& cs, uint32_t entityId) {
    if (ImGui::BeginTable("StatsT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        DrawAddressRow("Address", cs.Address);
        DrawAddressRow("Owner Address", cs.OwnerAddress);
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("CurrentWeaponIndex");
        ImGui::TableNextColumn(); ImGui::Text("%d", cs.CurrentWeaponIndex);
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("IsInShapeshiftedForm");
        ImGui::TableNextColumn(); ImGui::TextColored(cs.IsShapeshifted ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1), cs.IsShapeshifted ? "true" : "false");
        ImGui::EndTable();
    }
    char treeId[32];
    snprintf(treeId, sizeof(treeId), "Items##si%u", entityId);
    if (ImGui::TreeNode(treeId)) {
        for (const auto& [k, v] : cs.StatsItems)
            ImGui::Text("[%d]: %d", k, v);
        ImGui::TreePop();
    }
    snprintf(treeId, sizeof(treeId), "BuffAndActions##sb%u", entityId);
    if (ImGui::TreeNode(treeId)) {
        for (const auto& [k, v] : cs.StatsBuff)
            ImGui::Text("[%d]: %d", k, v);
        ImGui::TreePop();
    }
}

inline void DrawActorComp(const PluginSDK::DebugActorComp& cac, uint32_t entityId) {
    if (ImGui::BeginTable("ActorT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        DrawAddressRow("Address", cac.Address);
        DrawAddressRow("Owner Address", cac.OwnerAddress);
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Animation");
        ImGui::TableNextColumn(); ImGui::Text("%s", cac.AnimationName.c_str());
        ImGui::EndTable();
    }
    char treeId[32];
    snprintf(treeId, sizeof(treeId), "Skills##as%u", entityId);
    if (!cac.ActiveSkills.empty() && ImGui::TreeNode(treeId)) {
        for (const auto& skill : cac.ActiveSkills) {
            if (ImGui::TreeNode(skill.Name.c_str())) {
                if (ImGui::BeginTable("SkillT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Use Stage");
                    ImGui::TableNextColumn(); ImGui::Text("%d", skill.UseStage);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Cast Type");
                    ImGui::TableNextColumn(); ImGui::Text("%d", skill.CastType);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Total Uses");
                    ImGui::TableNextColumn(); ImGui::Text("%d", skill.TotalUses);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Cooldown Time (ms)");
                    ImGui::TableNextColumn(); ImGui::Text("%d", skill.TotalCooldownTimeInMs);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Can Be Used");
                    ImGui::TableNextColumn(); ImGui::TextColored(skill.CanBeUsed ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1), skill.CanBeUsed ? "true" : "false");
                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
    snprintf(treeId, sizeof(treeId), "Deployed##dp%u", entityId);
    if (ImGui::TreeNode(treeId)) {
        for (int i = 0; i < 256; i++) {
            if (cac.DeployedCounts[i] > 0)
                ImGui::Text("Object Type: %d, Total Count: %d", i, cac.DeployedCounts[i]);
        }
        ImGui::TreePop();
    }
}

inline void DrawBuffsComp(const std::vector<PluginSDK::DebugBuff>& buffs, uint32_t entityId) {
    ImGui::Text("Effects: %zu", buffs.size());
    char treeId[32];
    snprintf(treeId, sizeof(treeId), "Effects##bf%u", entityId);
    if (!buffs.empty() && ImGui::TreeNode(treeId)) {
        for (const auto& buff : buffs) {
            if (ImGui::TreeNode(buff.Name.c_str())) {
                if (ImGui::BeginTable("BuffT", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Name");
                    ImGui::TableNextColumn(); ImGui::Text("%s", buff.Name.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Total Time");
                    ImGui::TableNextColumn(); ImGui::Text("%.2f", buff.TotalTime > 0 ? buff.TotalTime : std::numeric_limits<float>::infinity());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Time Left");
                    ImGui::TableNextColumn(); ImGui::Text("%.2f", buff.TimeLeft > 0 ? buff.TimeLeft : std::numeric_limits<float>::infinity());
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Source Entity Id");
                    ImGui::TableNextColumn(); ImGui::Text("%u", buff.SourceEntityId);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Charges");
                    ImGui::TableNextColumn(); ImGui::Text("%d", buff.Charges);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Source FlaskSlot");
                    ImGui::TableNextColumn(); ImGui::Text("%d", buff.FlaskSlot);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Text("Source Effectiveness");
                    ImGui::TableNextColumn(); ImGui::Text("%d", 100 + buff.Effectiveness);
                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
}

// ============================================================================
// Main entity list panel — matches Debug->Entity List
// ============================================================================

inline void DrawEntitiesPanel(PluginContext* ctx) {
    using namespace PluginSDK;

    if (!ctx || !ctx->GetEntityDebugList) {
        ImGui::TextDisabled("Entity debug API not available (requires SDK v4)");
        return;
    }

    auto entityList = ctx->GetEntityDebugList();
    ImGui::Text("Total: %zu entities", entityList.size());

    // --- Filters ---
    static char idFilter[32] = "";
    static char pathFilter[128] = "";
    static int typeFilter = -1;
    ImGui::InputText("Filter by Id", idFilter, sizeof(idFilter));
    ImGui::InputText("Filter by Path", pathFilter, sizeof(pathFilter));

    const char* typeNames[] = { "All", "Monster", "NPC", "Chest", "Player", "Item",
        "Shrine", "AreaTransition", "Renderable", "DeliriumSpawner", "DeliriumBomb", "Unidentified" };
    ImGui::Combo("Filter by Type", &typeFilter, typeNames, IM_ARRAYSIZE(typeNames));

    int shown = 0;
    for (const auto& e : entityList) {
        // Apply filters
        if (idFilter[0] != '\0') {
            char idStr[16]; snprintf(idStr, sizeof(idStr), "%u", e.Id);
            if (strstr(idStr, idFilter) == nullptr) continue;
        }
        if (pathFilter[0] != '\0') {
            if (e.Path.find(pathFilter) == std::string::npos) continue;
        }
        if (typeFilter > 0 && e.EntityType != (typeFilter - 1)) continue;

        shown++;
        if (shown > 500) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "... truncated (>500 shown)");
            break;
        }

        char nodeLabel[256];
        snprintf(nodeLabel, sizeof(nodeLabel), "%u %s##ent%u", e.Id, e.Path.c_str(), e.Id);
        if (ImGui::TreeNode(nodeLabel)) {
            // Entity properties table
            if (ImGui::BeginTable("EntProps", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                DrawAddressRow("Address", e.Address);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Id");
                ImGui::TableNextColumn(); ImGui::Text("%u", e.Id);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Path");
                ImGui::TableNextColumn(); ImGui::TextWrapped("%s", e.Path.c_str());

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Entity Type");
                ImGui::TableNextColumn(); ImGui::Text("%d", e.EntityType);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Entity SubType");
                ImGui::TableNextColumn(); ImGui::Text("%d", e.EntitySubType);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Entity State");
                ImGui::TableNextColumn(); ImGui::Text("%d", e.EntityState);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Rarity");
                ImGui::TableNextColumn(); ImGui::Text("%d", e.Rarity);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Nearby Zone");
                ImGui::TableNextColumn();
                const char* zoneNames[] = { "None", "InnerCircle", "OuterCircle", "Far" };
                int zi = static_cast<int>(e.Zone);
                ImGui::Text("%s", (zi >= 0 && zi < 4) ? zoneNames[zi] : "Unknown");

                ImGui::EndTable();
            }

            // JSON dump button
            {
                char btnId[32];
                snprintf(btnId, sizeof(btnId), "Copy JSON##json%u", e.Id);
                if (ImGui::SmallButton(btnId)) {
                    std::ostringstream js;
                    js << "{\n";
                    js << "  \"Id\": " << e.Id << ",\n";
                    js << "  \"Address\": \"0x" << std::hex << e.Address << std::dec << "\",\n";
                    js << "  \"Path\": \"" << e.Path << "\",\n";
                    js << "  \"EntityType\": " << e.EntityType << ",\n";
                    js << "  \"EntitySubType\": " << e.EntitySubType << ",\n";
                    js << "  \"EntityState\": " << e.EntityState << ",\n";
                    js << "  \"Rarity\": " << e.Rarity << ",\n";
                    js << "  \"Components\": {\n";
                    for (size_t ci = 0; ci < e.ComponentAddresses.size(); ci++) {
                        js << "    \"" << e.ComponentAddresses[ci].first << "\": \"0x"
                           << std::hex << e.ComponentAddresses[ci].second << std::dec << "\"";
                        if (ci + 1 < e.ComponentAddresses.size()) js << ",";
                        js << "\n";
                    }
                    js << "  }\n}";
                    ImGui::SetClipboardText(js.str().c_str());
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Copy entity data as JSON to clipboard");
            }

            // Components tree (lazy loaded via watch mechanism)
            char compLabel[64];
            snprintf(compLabel, sizeof(compLabel), "Components (%zu)##comp%u", e.ComponentAddresses.size(), e.Id);
            bool compOpen = ImGui::TreeNode(compLabel);
            if (compOpen) {
                ctx->WatchEntity(e.Id);

                auto ec = ctx->GetWatchedEntityData(e.Id);
                bool hasData = ec.Valid;

                if (!hasData) {
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.3f, 1.0f), "Loading...");
                }

                for (const auto& [compName, compAddr] : e.ComponentAddresses) {
                    bool rendered = false;
                    if (hasData) {
                        if (compName == "Life" && ec.HasLife) {
                            if (ImGui::TreeNode("Life")) { DrawLifeComp(ec.Life, e.Id); ImGui::TreePop(); }
                            rendered = true;
                        }
                        else if (compName == "Render" && ec.HasRender) {
                            if (ImGui::TreeNode("Render")) { DrawRenderComp(ec.Render); ImGui::TreePop(); }
                            rendered = true;
                        }
                        else if (compName == "Positioned" && ec.HasPositioned) {
                            if (ImGui::TreeNode("Positioned")) { DrawPositionedComp(ec.Positioned); ImGui::TreePop(); }
                            rendered = true;
                        }
                        else if (compName == "Targetable" && ec.HasTargetable) {
                            if (ImGui::TreeNode("Targetable")) { DrawTargetableComp(ec.Targetable); ImGui::TreePop(); }
                            rendered = true;
                        }
                        else if (compName == "Animated" && ec.HasAnimated) {
                            if (ImGui::TreeNode("Animated")) { DrawAnimatedComp(ec.Animated); ImGui::TreePop(); }
                            rendered = true;
                        }
                        else if (compName == "Stats" && ec.HasStats) {
                            if (ImGui::TreeNode("Stats")) { DrawStatsComp(ec.Stats, e.Id); ImGui::TreePop(); }
                            rendered = true;
                        }
                        else if (compName == "Actor" && ec.HasActor) {
                            if (ImGui::TreeNode("Actor")) { DrawActorComp(ec.Actor, e.Id); ImGui::TreePop(); }
                            rendered = true;
                        }
                        else if (compName == "Buffs" && ec.HasBuffs) {
                            if (ImGui::TreeNode("Buffs")) { DrawBuffsComp(ec.Buffs, e.Id); ImGui::TreePop(); }
                            rendered = true;
                        }
                    }

                    if (!rendered) {
                        char unknownLabel[128];
                        snprintf(unknownLabel, sizeof(unknownLabel), "%s: 0x%llX", compName.c_str(), compAddr);
                        ImGui::TextDisabled("%s", unknownLabel);
                    }
                }

                ImGui::TreePop();
            } else {
                ctx->UnwatchEntity(e.Id);
            }

            ImGui::TreePop();
        }
    }
    if (shown == 0 && !entityList.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No entities match filter");
    }
}

} // namespace Examples
