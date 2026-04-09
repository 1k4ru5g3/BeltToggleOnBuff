#pragma once
#include "../sdk/PluginHelpers.h"

namespace Examples {

// Demonstrates advanced memory reading capabilities
inline void DrawMemoryPanel(PluginContext* ctx) {
    if (!ctx->GetBaseAddress) {
        ImGui::TextDisabled("Memory reading not available");
        return;
    }

    PluginSDK::MemoryReader mem(ctx);
    uintptr_t base = mem.GetBaseAddress();
    uintptr_t modSize = mem.GetModuleSize();

    // --- Basic Info ---
    ImGui::Text("Base Address: 0x%llX", base);
    ImGui::Text("Module Size: 0x%llX (%llu KB)", modSize, modSize / 1024);
    ImGui::Text("PID: %u", ctx->GetProcessId());

    // --- PE Header Verification ---
    if (base != 0 && ctx->ReadProcessMemory) {
        uint16_t dosSignature = mem.Read<uint16_t>(base);
        ImGui::Text("DOS Signature: 0x%04X (%s)", dosSignature,
            dosSignature == 0x5A4D ? "MZ - Valid PE" : "Unknown");

        // Read PE header offset and machine type
        uint32_t peOffset = mem.Read<uint32_t>(base + 0x3C);
        if (peOffset > 0 && peOffset < modSize) {
            uint32_t peSignature = mem.Read<uint32_t>(base + peOffset);
            ImGui::Text("PE Signature: 0x%08X (%s)", peSignature,
                peSignature == 0x00004550 ? "PE\\0\\0 - Valid" : "Invalid");

            uint16_t machine = mem.Read<uint16_t>(base + peOffset + 4);
            ImGui::Text("Machine: 0x%04X (%s)", machine,
                machine == 0x8664 ? "x64" : machine == 0x14C ? "x86" : "Other");
        }
    }

    // --- Pattern Addresses ---
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Pattern Addresses", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* patterns[] = {
            "Game States", "File Root", "AreaChangeCounter",
            "Terrain Rotator Helper", "Terrain Rotation Selector", "GameCullSize"
        };
        if (ImGui::BeginTable("PatternTable", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Pattern", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 140);
            ImGui::TableHeadersRow();

            for (auto* name : patterns) {
                uintptr_t addr = mem.GetPatternAddress(name);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", name);
                ImGui::TableNextColumn();
                if (addr != 0) {
                    char buf[20]; snprintf(buf, sizeof(buf), "0x%llX", addr);
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "%s", buf);
                    if (ImGui::IsItemHovered() && ImGui::IsItemClicked())
                        ImGui::SetClipboardText(buf);
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Not found");
                }
            }
            ImGui::EndTable();
        }
    }

    // --- Memory Hex Viewer ---
    if (ImGui::CollapsingHeader("Memory Hex Viewer")) {
        static char addrInput[20] = "";
        static uintptr_t viewAddr = 0;
        static int viewSize = 256;
        ImGui::InputText("Address (hex)", addrInput, sizeof(addrInput));
        ImGui::SameLine();
        if (ImGui::Button("Go")) {
            viewAddr = strtoull(addrInput, nullptr, 16);
        }
        ImGui::SameLine();
        if (ImGui::Button("Base")) {
            viewAddr = base;
            snprintf(addrInput, sizeof(addrInput), "%llX", base);
        }
        ImGui::SliderInt("Bytes", &viewSize, 16, 512);

        if (viewAddr != 0 && ctx->ReadProcessMemory) {
            std::vector<uint8_t> data(viewSize);
            if (ctx->ReadProcessMemory(viewAddr, data.data(), viewSize)) {
                // Hex dump
                for (int i = 0; i < viewSize; i += 16) {
                    ImGui::Text("%llX: ", viewAddr + i);
                    ImGui::SameLine();
                    for (int j = 0; j < 16 && (i + j) < viewSize; j++) {
                        if (j == 8) { ImGui::SameLine(); ImGui::TextDisabled("|"); }
                        ImGui::SameLine();
                        ImGui::Text("%02X", data[i + j]);
                    }
                    // ASCII
                    ImGui::SameLine(0, 20);
                    char ascii[17] = {};
                    for (int j = 0; j < 16 && (i + j) < viewSize; j++) {
                        uint8_t c = data[i + j];
                        ascii[j] = (c >= 32 && c < 127) ? (char)c : '.';
                    }
                    ImGui::TextDisabled("%s", ascii);
                }
            } else {
                ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Read failed at 0x%llX", viewAddr);
            }
        }
    }

    // --- Read Template Demo ---
    if (ImGui::CollapsingHeader("Read<T> Demo")) {
        ImGui::TextWrapped(
            "The PluginSDK::MemoryReader helper provides typed reading:\n"
            "  mem.Read<T>(addr)          - Read a single struct\n"
            "  mem.ReadArray<T>(addr, n)  - Read array of N structs\n"
            "  mem.ReadStdVector<T>(addr) - Read StdVector container\n"
            "  mem.ReadStdList<T>(addr)   - Read StdList container\n"
            "  mem.ReadStdBucket<T>(addr) - Read StdBucket container\n"
            "  mem.ReadStdMap<K,V>(addr)  - Read StdMap container\n"
            "  mem.ReadStdWString(addr)   - Read StdWString container"
        );
    }
}

} // namespace Examples
