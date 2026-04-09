// PLUGIN_EXPORTS is defined in the project preprocessor definitions
#include "sdk/PluginHelpers.h"
#include "examples/ExampleBuffs.h"
#include "examples/ExampleEntities.h"
#include "examples/ExampleInventory.h"
#include "examples/ExampleMemory.h"
#include "examples/ExampleUiExplorer.h"

#include <fstream>
#include <filesystem>
#include <string>
#include <chrono>

using namespace PluginSDK;

// ============================================================================
// ExamplePlugin — demonstrates the full Plugin SDK v4 API
// ============================================================================
// Features:
//   - Area Info, Player Vitals, WorldToScreen
//   - Player Buffs with filtering and progress bars
//   - Nearby Entities with metadata, filtering, and components
//   - Inventories with names, grid view, items, and modifiers
//   - Memory Info with hex viewer and Read<T> demo
//   - Game UI Explorer (navigate UI element tree)
//   - Overlay mode support
// ============================================================================

class ExamplePlugin : public IPlugin {
public:
    void SetPluginDirectory(const char* dir) override {
        m_Directory = dir;
    }

    void SetContext(PluginContext* context) override {
        m_Context = context;
        if (m_Context && m_Context->ImGuiContext) {
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_Context->ImGuiContext));
        }
    }

    void OnEnable(bool isGameOpened) override {
        LoadSettings();
        if (m_Context) {
            m_Context->Log("Info", "ExamplePlugin v4 enabled");
        }
    }

    void OnDisable() override {
        if (m_Context) {
            m_Context->Log("Info", "ExamplePlugin v4 disabled");
        }
    }

    void DrawSettings() override {
        ImGui::Checkbox("Show Info Window", &m_ShowWindow);
        ImGui::Separator();
        ImGui::Text("Panels:");
        ImGui::Checkbox("Player Buffs", &m_ShowBuffs);
        ImGui::Checkbox("Nearby Entities", &m_ShowEntities);
        ImGui::Checkbox("Inventories", &m_ShowInventory);
        ImGui::Checkbox("Memory Info", &m_ShowMemoryInfo);
        ImGui::Checkbox("Game UI Explorer", &m_ShowUiExplorer);
        ImGui::Separator();
        ImGui::Checkbox("Enable Overlay Mode", &m_WantsOverlay);
        ImGui::SliderFloat("Window Opacity", &m_WindowAlpha, 0.3f, 1.0f, "%.1f");
    }

    void DrawUI() override {
        if (!m_ShowWindow || !m_Context) return;

        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(m_Context->ImGuiContext));

        auto snapshot = m_Context->GetSnapshot();
        if (!snapshot) return;

        ImGui::SetNextWindowSizeConstraints(ImVec2(400, 300), ImVec2(900, 1200));
        ImGui::SetNextWindowBgAlpha(m_WindowAlpha);

        if (!ImGui::Begin("Example Plugin v4##ExamplePlugin", &m_ShowWindow)) {
            ImGui::End();
            return;
        }

        // --- Connection Status ---
        if (!snapshot->IsAttached) {
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Game not attached");
            ImGui::End();
            return;
        }

        bool inGame = (snapshot->CurrentState == GameStateTypes::InGameState);

        // Overlay status indicator
        if (m_Context->IsOverlayMode && m_Context->IsOverlayMode()) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1), "[OVERLAY MODE]");
        }

        if (!inGame) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Not in game (state: %d)",
                (int)snapshot->CurrentState);
            ImGui::End();
            return;
        }

        // --- Tab Bar ---
        if (ImGui::BeginTabBar("##ExampleTabs")) {
            // ===== Area & Vitals Tab =====
            if (ImGui::BeginTabItem("Area & Vitals")) {
                DrawAreaAndVitals(snapshot);
                ImGui::EndTabItem();
            }

            // ===== Buffs Tab =====
            if (m_ShowBuffs && ImGui::BeginTabItem("Buffs")) {
                Examples::DrawBuffsPanel(snapshot);
                ImGui::EndTabItem();
            }

            // ===== Entities Tab =====
            if (m_ShowEntities && ImGui::BeginTabItem("Entities")) {
                Examples::DrawEntitiesPanel(m_Context);
                ImGui::EndTabItem();
            }

            // ===== Inventory Tab =====
            if (m_ShowInventory && ImGui::BeginTabItem("Inventories")) {
                Examples::DrawInventoryPanel(snapshot, m_Context, m_LastInventoryScan);
                ImGui::EndTabItem();
            }

            // ===== Memory Tab =====
            if (m_ShowMemoryInfo && ImGui::BeginTabItem("Memory")) {
                Examples::DrawMemoryPanel(m_Context);
                ImGui::EndTabItem();
            }

            // ===== UI Explorer Tab =====
            if (m_ShowUiExplorer && ImGui::BeginTabItem("UI Explorer")) {
                m_UiExplorer.Draw(m_Context);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    bool WantsOverlay() override {
        return m_WantsOverlay;
    }

    void SaveSettings() override {
        namespace fs = std::filesystem;
        fs::path configDir = fs::path(m_Directory) / "config";
        if (!fs::exists(configDir)) {
            fs::create_directories(configDir);
        }
        fs::path settingsPath = configDir / "settings.txt";
        std::ofstream file(settingsPath);
        if (file.is_open()) {
            file << "ShowWindow=" << (m_ShowWindow ? 1 : 0) << "\n";
            file << "ShowEntities=" << (m_ShowEntities ? 1 : 0) << "\n";
            file << "ShowBuffs=" << (m_ShowBuffs ? 1 : 0) << "\n";
            file << "ShowInventory=" << (m_ShowInventory ? 1 : 0) << "\n";
            file << "ShowMemoryInfo=" << (m_ShowMemoryInfo ? 1 : 0) << "\n";
            file << "ShowUiExplorer=" << (m_ShowUiExplorer ? 1 : 0) << "\n";
            file << "WantsOverlay=" << (m_WantsOverlay ? 1 : 0) << "\n";
            file << "WindowAlpha=" << m_WindowAlpha << "\n";
        }
    }

    const char* GetName() override { return "Example Plugin"; }

private:
    // Draw Area Info and Player Vitals
    void DrawAreaAndVitals(const std::shared_ptr<const PluginGameSnapshot>& snapshot) {
        // --- Area Info ---
        if (ImGui::CollapsingHeader("Area Info", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Area: %s", snapshot->CurrentAreaName.c_str());
            ImGui::Text("Level: %d", snapshot->CurrentAreaLevel);
            ImGui::Text("Hash: %s", snapshot->CurrentAreaHash.c_str());
            ImGui::Text("Town: %s  Hideout: %s",
                snapshot->IsTown ? "Yes" : "No",
                snapshot->IsHideout ? "Yes" : "No");
            ImGui::Text("Area Change #: %llu", snapshot->AreaChangeCounter);
            ImGui::Text("Screen: %dx%d", snapshot->ScreenWidth, snapshot->ScreenHeight);
        }

        // --- Player Vitals ---
        if (ImGui::CollapsingHeader("Player Vitals", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& v = snapshot->Vitals;

            // HP bar
            float hpFrac = v.MaxHP > 0 ? (float)v.CurrentHP / v.MaxHP : 0;
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            char hpOv[64]; snprintf(hpOv, sizeof(hpOv), "HP: %d / %d (%d%%)", v.CurrentHP, v.MaxHP, v.HPPercent);
            ImGui::ProgressBar(hpFrac, ImVec2(-1, 18), hpOv);
            ImGui::PopStyleColor();

            // ES bar
            if (v.MaxES > 0) {
                float esFrac = (float)v.CurrentES / v.MaxES;
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
                char esOv[64]; snprintf(esOv, sizeof(esOv), "ES: %d / %d (%d%%)", v.CurrentES, v.MaxES, v.ESPercent);
                ImGui::ProgressBar(esFrac, ImVec2(-1, 18), esOv);
                ImGui::PopStyleColor();
            }

            // MP bar
            float mpFrac = v.MaxMP > 0 ? (float)v.CurrentMP / v.MaxMP : 0;
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.2f, 0.8f, 1.0f));
            char mpOv[64]; snprintf(mpOv, sizeof(mpOv), "MP: %d / %d (%d%%)", v.CurrentMP, v.MaxMP, v.MPPercent);
            ImGui::ProgressBar(mpFrac, ImVec2(-1, 18), mpOv);
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // Position
            ImGui::Text("World: %.0f, %.0f, %.0f",
                snapshot->Player.WorldX, snapshot->Player.WorldY, snapshot->Player.WorldZ);
            ImGui::Text("Grid: %.1f, %.1f",
                snapshot->Player.GridPositionX, snapshot->Player.GridPositionY);

            // WorldToScreen demo
            if (m_Context->WorldToScreen) {
                float sx, sy;
                if (m_Context->WorldToScreen(snapshot->Player.WorldX, snapshot->Player.WorldY,
                    snapshot->Player.WorldZ, &sx, &sy)) {
                    ImGui::Text("Screen: %.0f, %.0f", sx, sy);
                }
            }

            // Player entity info
            auto& p = snapshot->Player;
            if (!p.Path.empty()) {
                std::string narrowPath = WideToNarrow(p.Path);
                ImGui::Text("Path: %s", narrowPath.c_str());
            }
            ImGui::Text("Entity ID: %u  Zone: %s",
                p.Id, GetNearbyZoneName(p.Zone));
        }
    }

    void LoadSettings() {
        namespace fs = std::filesystem;
        fs::path settingsPath = fs::path(m_Directory) / "config" / "settings.txt";
        if (!fs::exists(settingsPath)) return;

        std::ifstream file(settingsPath);
        std::string line;
        while (std::getline(file, line)) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);

            if (key == "ShowWindow") m_ShowWindow = (val == "1");
            else if (key == "ShowEntities") m_ShowEntities = (val == "1");
            else if (key == "ShowBuffs") m_ShowBuffs = (val == "1");
            else if (key == "ShowInventory") m_ShowInventory = (val == "1");
            else if (key == "ShowMemoryInfo") m_ShowMemoryInfo = (val == "1");
            else if (key == "ShowUiExplorer") m_ShowUiExplorer = (val == "1");
            else if (key == "WantsOverlay") m_WantsOverlay = (val == "1");
            else if (key == "WindowAlpha") m_WindowAlpha = std::stof(val);
        }
    }

    PluginContext* m_Context = nullptr;
    std::string m_Directory;
    bool m_ShowWindow = true;
    bool m_ShowEntities = true;
    bool m_ShowBuffs = true;
    bool m_ShowInventory = false;
    bool m_ShowMemoryInfo = false;
    bool m_ShowUiExplorer = false;
    bool m_WantsOverlay = false;
    float m_WindowAlpha = 0.9f;
    std::chrono::steady_clock::time_point m_LastInventoryScan;
    Examples::PluginUiExplorer m_UiExplorer;
};

// ============================================================================
// Factory exports
// ============================================================================

extern "C" PLUGIN_API IPlugin* CreatePlugin() {
    return new ExamplePlugin();
}

extern "C" PLUGIN_API void DestroyPlugin(IPlugin* plugin) {
    delete plugin;
}
