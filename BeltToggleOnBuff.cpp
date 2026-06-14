// ============================================================================
// BeltToggleOnBuff.cpp
// POE2Fixer SDK v6 Plugin
//
// ULTRA SAFE BUILD
//
// Fix:
// - DrawSettings() setzt den ImGui-Context direkt inline.
// - Kein SetImGuiContext()-Helper.
// - DrawSettings() enthält KEINE CollapsingHeader, KEINE Child-Windows,
//   KEINE Game-Memory-Lesezugriffe, KEINE Buff-Enumeration.
// - Alle gefährlichen Debug-/Scanner-Funktionen sind erst einmal entfernt.
// - Ziel: Plugin muss stabil aktivierbar sein.
// ============================================================================

#include "sdk/PluginSDK.h"
#include "imgui/imgui.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>

class BeltToggleOnBuffPlugin : public PluginSDK::Plugin
{
public:
    const char* GetName() const override
    {
        return "Belt Toggle On Buff";
    }

    void OnEnable(bool /*isGameAttached*/) override
    {
        LoadSettings();

        s_Instance = this;
        m_State = SequenceState::Idle;
        m_Status = "Enabled.";
        m_EnableTime = Clock::now();
        m_TrackedWalkers.clear();
        m_PendingWalkerDeathTrigger = false;

        ctx()->Log.Info("BeltToggleOnBuff enabled");
    }

    void OnDisable() override
    {
        StopSequence("Plugin disabled.");
        UninstallInputHooks();

        if (s_Instance == this)
            s_Instance = nullptr;

        ctx()->Log.Info("BeltToggleOnBuff disabled");
    }

    bool WantsOverlay() const override
    {
        return true;
    }

    void DrawSettings() override
    {
        // Wichtig für diese SDK/Host-Version:
        // Ohne gesetzten ImGui-Context crasht DrawSettings sofort.
        if (ctx() && ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));

        bool changed = false;

        ImGui::TextUnformatted("Belt Toggle On Buff - Walker Monster Death Build");
        ImGui::Separator();

        changed |= ImGui::Checkbox("Enable automatic trigger by outer buff", &m_EnableAutoTrigger);
        changed |= ImGui::Checkbox("Enable test mode with F6 hotkey", &m_EnableF6TestHotkey);
        changed |= ImGui::Checkbox("Do not run in town/hideout", &m_BlockTownHideout);
        changed |= ImGui::Checkbox("Require game window foreground", &m_RequireForeground);
        changed |= ImGui::Checkbox("Block human input while running", &m_BlockHumanInput);
        changed |= ImGui::Checkbox("Use automatic Belt1 detection", &m_UseAutomaticBeltDetection);

        ImGui::Separator();

        changed |= ImGui::Checkbox("Trigger on Walker monster death", &m_TriggerOnWalkerMonsterDeath);
        changed |= ImGui::Checkbox("Only rare/unique monsters", &m_OnlyRareOrUniqueWalkers);
        changed |= ImGui::Checkbox("Check monster OMP mods", &m_CheckMonsterMods);
        changed |= ImGui::Checkbox("Check monster buffs", &m_CheckMonsterBuffs);
        changed |= ImGui::Checkbox("Trigger if tracked Walker disappears", &m_TriggerOnTrackedWalkerDisappear);
        changed |= ImGui::SliderInt("Monster scan interval ms", &m_MonsterScanIntervalMs, 100, 2000);
        changed |= ImGui::SliderFloat("Max monster scan distance", &m_MaxMonsterScanDistance, 500.0f, 12000.0f, "%.0f");
        changed |= ImGui::InputText("Walker keyword A", m_WalkerKeywordA, sizeof(m_WalkerKeywordA));
        changed |= ImGui::InputText("Walker keyword B", m_WalkerKeywordB, sizeof(m_WalkerKeywordB));
        ImGui::Text("Tracked walkers: %d", static_cast<int>(m_TrackedWalkers.size()));
        ImGui::Text("Pending death trigger: %s", m_PendingWalkerDeathTrigger ? "yes" : "no");

        changed |= ImGui::SliderInt("Action delay ms", &m_DelayMs, 20, 1000);
        changed |= ImGui::SliderInt("Cooldown ms", &m_CooldownMs, 500, 30000);

        ImGui::Separator();

        ImGui::Text("Manual belt coordinate: X=%d Y=%d", m_ManualBeltX, m_ManualBeltY);

        if (ImGui::Button("Set manual belt coordinate from current mouse"))
        {
            POINT p{};
            if (GetCursorPos(&p))
            {
                m_ManualBeltX = p.x;
                m_ManualBeltY = p.y;
                m_Status = "Manual belt coordinate set.";
                changed = true;
            }
        }

        if (ImGui::Button("Refresh Belt1 detection"))
        {
            auto belt = ResolveBeltPoint(true);

            if (belt)
            {
                m_LastBeltInfo = belt->DebugInfo;
                m_Status = "Belt1 detected.";
            }
            else
            {
                m_Status = "Belt1 not detected. Use manual coordinate.";
            }
        }

        ImGui::Separator();

        if (ImGui::Button("Test sequence now"))
        {
            if (m_EnableF6TestHotkey)
                m_Status = "F6 test mode enabled. Press F6.";
            else
                TryStartSequence(StartReason::ManualTest);
        }

        if (ImGui::Button("Abort sequence"))
            StopSequence("Aborted by UI button.");

        ImGui::Separator();

        if (ImGui::Button("Dump SDK-visible player buffs to log"))
            DumpSdkVisiblePlayerBuffs();

        ImGui::Text("State: %s", StateName(m_State));
        ImGui::Text("Status: %s", m_Status.c_str());
        ImGui::Text("Last belt: %s", m_LastBeltInfo.c_str());
        ImGui::Text("Last walker: %s", m_LastMatchedWalker.c_str());

        if (changed)
            SaveSettings();
    }

    void DrawUI() override
    {
        if (ctx() && ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));

        TickSequence();
        HandleF6TestHotkey();

        if (m_State != SequenceState::Idle)
            return;

        if (!m_EnableAutoTrigger)
            return;

        PluginSDK::Snapshot snapshot = ctx()->Game.GetSnapshot();

        if (!IsSnapshotSafeForAction(snapshot))
            return;

        if (m_TriggerOnWalkerMonsterDeath)
        {
            ScanWalkerMonsterDeaths(snapshot);

            if (!m_PendingWalkerDeathTrigger)
                return;
        }
        else
        {
            if (!HasOuterBuffActive(snapshot))
                return;
        }

        const auto now = Clock::now();
        const auto msSinceLast =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - m_LastTriggerTime).count();

        if (msSinceLast < m_CooldownMs)
            return;

        m_PendingWalkerDeathTrigger = false;
        TryStartSequence(StartReason::BuffDetected);
    }

    void SaveSettings() override
    {
        namespace fs = std::filesystem;

        fs::path dir = DirectoryPath() / "config";
        std::error_code ec;
        fs::create_directories(dir, ec);

        std::ofstream file(dir / "settings.txt");
        if (!file.is_open())
            return;

        file << "EnableAutoTrigger=" << (m_EnableAutoTrigger ? 1 : 0) << "\n";
        file << "EnableF6TestHotkey=" << (m_EnableF6TestHotkey ? 1 : 0) << "\n";
        file << "BlockTownHideout=" << (m_BlockTownHideout ? 1 : 0) << "\n";
        file << "RequireForeground=" << (m_RequireForeground ? 1 : 0) << "\n";
        file << "BlockHumanInput=" << (m_BlockHumanInput ? 1 : 0) << "\n";
        file << "UseAutomaticBeltDetection=" << (m_UseAutomaticBeltDetection ? 1 : 0) << "\n";
        file << "TriggerOnWalkerMonsterDeath=" << (m_TriggerOnWalkerMonsterDeath ? 1 : 0) << "\n";
        file << "OnlyRareOrUniqueWalkers=" << (m_OnlyRareOrUniqueWalkers ? 1 : 0) << "\n";
        file << "CheckMonsterMods=" << (m_CheckMonsterMods ? 1 : 0) << "\n";
        file << "CheckMonsterBuffs=" << (m_CheckMonsterBuffs ? 1 : 0) << "\n";
        file << "TriggerOnTrackedWalkerDisappear=" << (m_TriggerOnTrackedWalkerDisappear ? 1 : 0) << "\n";
        file << "MonsterScanIntervalMs=" << m_MonsterScanIntervalMs << "\n";
        file << "MaxMonsterScanDistance=" << m_MaxMonsterScanDistance << "\n";
        file << "WalkerKeywordA=" << m_WalkerKeywordA << "\n";
        file << "WalkerKeywordB=" << m_WalkerKeywordB << "\n";
        file << "OuterBuffBaseName=" << m_OuterBuffBaseName << "\n";
        file << "DelayMs=" << m_DelayMs << "\n";
        file << "CooldownMs=" << m_CooldownMs << "\n";
        file << "ManualBeltX=" << m_ManualBeltX << "\n";
        file << "ManualBeltY=" << m_ManualBeltY << "\n";
    }

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::steady_clock::time_point;

    enum class SequenceState
    {
        Idle,
        OpenInventory,
        WaitAfterOpenInventory,
        MoveToBelt,
        WaitAfterMoveToBelt,
        UnequipClick,
        WaitAfterUnequipClick,
        ReequipClick,
        WaitAfterReequipClick,
        CloseInventory,
        WaitAfterCloseInventory,
        RestoreMouse,
        Done
    };

    enum class StartReason
    {
        ManualTest,
        BuffDetected
    };

    struct BeltPoint
    {
        int X = 0;
        int Y = 0;
        std::string DebugInfo;
    };

private:

    float SecondsSinceEnable() const
    {
        return std::chrono::duration<float>(Clock::now() - m_EnableTime).count();
    }

    struct WalkerScanResult
    {
        bool Found = false;
        std::string Reason;
    };

    bool IsMonsterCandidateForWalker(const PluginSDK::Entity& e) const
    {
        if (!e.IsValid)
            return false;

        if (e.EntityType != PluginSDK::EntityType::Monster)
            return false;

        if (e.EntityState == PluginSDK::EntityState::MonsterFriendly)
            return false;

        if (m_OnlyRareOrUniqueWalkers && e.Rarity < 2)
            return false;

        if (!e.Components.HasOMP() && !e.Components.HasBuffs())
            return false;

        return true;
    }

    bool IsAliveMonster(const PluginSDK::Entity& e) const
    {
        if (!e.IsValid)
            return false;

        if (e.MaxHP > 0 && e.CurrentHP <= 0)
            return false;

        return true;
    }

    bool StringLooksLikeWalker(const std::string& s) const
    {
        if (s.empty())
            return false;

        if (ContainsCaseInsensitive(s, m_WalkerKeywordA))
            return true;

        if (ContainsCaseInsensitive(s, m_WalkerKeywordB))
            return true;

        // Extra hardcoded fallbacks for Dat/OMP ids.
        if (ContainsCaseInsensitive(s, "MonsterShroudWalker"))
            return true;

        if (ContainsCaseInsensitive(s, "MonsterShadeWalker"))
            return true;

        if (ContainsCaseInsensitive(s, "Shadewalker"))
            return true;

        if (ContainsCaseInsensitive(s, "ShadeWalker"))
            return true;

        return false;
    }

    WalkerScanResult HasWalkerMarker(const PluginSDK::Entity& e)
    {
        WalkerScanResult result;

        if (m_CheckMonsterMods && e.Components.HasOMP())
        {
            std::vector<PluginSDK::MonsterMod> mods =
                ctx()->Components.EnumerateMonsterMods(e.Components.OMP);

            for (const PluginSDK::MonsterMod& mod : mods)
            {
                if (StringLooksLikeWalker(mod.Id)
                    || StringLooksLikeWalker(mod.Name)
                    || StringLooksLikeWalker(mod.Metadata))
                {
                    std::ostringstream oss;
                    oss << "OMP: "
                        << mod.Id << " | "
                        << mod.Name << " | "
                        << mod.Metadata
                        << " h16=0x" << std::hex << static_cast<unsigned int>(mod.Hash16)
                        << " h32=0x" << static_cast<unsigned int>(mod.Hash32);

                    result.Found = true;
                    result.Reason = oss.str();
                    return result;
                }
            }
        }

        if (m_CheckMonsterBuffs && e.Components.HasBuffs())
        {
            std::vector<PluginSDK::Buff> buffs =
                ctx()->Components.EnumerateBuffs(e.Components.Buffs);

            for (const PluginSDK::Buff& buff : buffs)
            {
                if (StringLooksLikeWalker(buff.Name))
                {
                    std::ostringstream oss;
                    oss << "Buff: " << buff.Name
                        << " SourceEntityId=" << buff.SourceEntityId
                        << " TimeLeft=" << buff.TimeLeft
                        << " Charges=" << buff.Charges;

                    result.Found = true;
                    result.Reason = oss.str();
                    return result;
                }
            }
        }

        return result;
    }

    void MarkTrackedWalkersUnseen()
    {
        for (auto& [id, tracked] : m_TrackedWalkers)
            tracked.SeenThisScan = false;
    }

    void ScanWalkerMonsterDeaths(const PluginSDK::Snapshot& snapshot)
    {
        const auto nowTime = Clock::now();
        const auto msSinceScan =
            std::chrono::duration_cast<std::chrono::milliseconds>(nowTime - m_LastMonsterScanAt).count();

        if (msSinceScan < m_MonsterScanIntervalMs)
            return;

        m_LastMonsterScanAt = nowTime;

        const float now = SecondsSinceEnable();

        MarkTrackedWalkersUnseen();

        for (const PluginSDK::Entity& e : snapshot.Entities)
        {
            if (!IsMonsterCandidateForWalker(e))
                continue;

            const float dx = e.WorldX - snapshot.Player.WorldX;
            const float dy = e.WorldY - snapshot.Player.WorldY;
            const float distance = std::sqrt(dx * dx + dy * dy);

            if (distance > m_MaxMonsterScanDistance)
                continue;

            WalkerScanResult marker = HasWalkerMarker(e);

            auto existingIt = m_TrackedWalkers.find(e.Id);
            const bool alreadyTracked = existingIt != m_TrackedWalkers.end();

            if (!marker.Found && !alreadyTracked)
                continue;

            WalkerMonsterTrack& tracked = m_TrackedWalkers[e.Id];
            tracked.Id = e.Id;
            tracked.Address = e.Address;
            tracked.SeenThisScan = true;
            tracked.LastSeenSeconds = now;
            tracked.LastWorldX = e.WorldX;
            tracked.LastWorldY = e.WorldY;
            tracked.LastWorldZ = e.WorldZ;

            if (marker.Found)
            {
                tracked.HasWalkerMarker = true;
                tracked.LastMarker = marker.Reason;
                m_LastMatchedWalker = marker.Reason;
            }

            const bool aliveNow = IsAliveMonster(e);

            if (tracked.HasWalkerMarker && tracked.WasAlive && !aliveNow && !tracked.Triggered)
            {
                tracked.Triggered = true;
                m_PendingWalkerDeathTrigger = true;

                std::ostringstream oss;
                oss << "Walker monster died: id=" << e.Id
                    << " marker=" << tracked.LastMarker;

                m_Status = oss.str();
                ctx()->Log.Warn(oss.str().c_str());
                return;
            }

            if (aliveNow)
                tracked.WasAlive = true;
        }

        // Optional fallback: if a tracked live Walker vanishes from snapshot shortly after being seen,
        // treat it like a kill/despawn. Keep this off by default if it causes false positives.
        if (m_TriggerOnTrackedWalkerDisappear)
        {
            for (auto& [id, tracked] : m_TrackedWalkers)
            {
                if (!tracked.HasWalkerMarker || tracked.Triggered || !tracked.WasAlive)
                    continue;

                if (tracked.SeenThisScan)
                    continue;

                const float missingFor = now - tracked.LastSeenSeconds;

                if (missingFor >= m_TrackedWalkerDisappearSeconds)
                {
                    tracked.Triggered = true;
                    m_PendingWalkerDeathTrigger = true;

                    std::ostringstream oss;
                    oss << "Tracked Walker disappeared after being alive: id=" << id
                        << " marker=" << tracked.LastMarker;

                    m_Status = oss.str();
                    ctx()->Log.Warn(oss.str().c_str());
                    return;
                }
            }
        }

        // Prune old tracked entries to keep the map small.
        std::vector<uint32_t> eraseIds;

        for (const auto& [id, tracked] : m_TrackedWalkers)
        {
            if ((now - tracked.LastSeenSeconds) > m_ForgetTrackedWalkerAfterSeconds)
                eraseIds.push_back(id);
        }

        for (uint32_t id : eraseIds)
            m_TrackedWalkers.erase(id);

        if (!m_PendingWalkerDeathTrigger)
        {
            std::ostringstream oss;
            oss << "Tracking Walker monsters: " << m_TrackedWalkers.size();
            m_Status = oss.str();
        }
    }

    bool HasOuterBuffActive(const PluginSDK::Snapshot& snapshot)
    {
        const std::string outerBase = Trim(m_OuterBuffBaseName);

        if (outerBase.empty())
        {
            m_Status = "Outer buff base empty.";
            return false;
        }

        if (!snapshot.Player.Components.HasBuffs())
        {
            m_Status = "Player has no Buffs component.";
            return false;
        }

        std::vector<PluginSDK::Buff> buffs =
            ctx()->Components.EnumerateBuffs(snapshot.Player.Components.Buffs);

        for (const PluginSDK::Buff& buff : buffs)
        {
            if (!IsOuterBuffArrayName(buff.Name, outerBase))
                continue;

            m_LastMatchedOuterBuff = buff.Name;

            std::ostringstream oss;
            oss << "Outer buff active: " << buff.Name
                << " SourceEntityId=" << buff.SourceEntityId
                << " TimeLeft=" << buff.TimeLeft
                << " Charges=" << buff.Charges;
            m_Status = oss.str();

            return true;
        }

        m_Status = "Waiting for outer buff.";
        return false;
    }

    void DumpSdkVisiblePlayerBuffs()
    {
        PluginSDK::Snapshot snapshot = ctx()->Game.GetSnapshot();

        ctx()->Log.Warn("========== SDK PLAYER BUFF DUMP START ==========");

        if (!snapshot.IsAttached || snapshot.State != PluginSDK::GameState::InGame)
        {
            ctx()->Log.Warn("Not attached or not in game.");
            m_Status = "Buff dump failed: not in game.";
            return;
        }

        if (!snapshot.Player.Components.HasBuffs())
        {
            ctx()->Log.Warn("Player has no Buffs component.");
            m_Status = "Buff dump failed: no Buffs component.";
            return;
        }

        std::vector<PluginSDK::Buff> buffs =
            ctx()->Components.EnumerateBuffs(snapshot.Player.Components.Buffs);

        {
            std::ostringstream oss;
            oss << "Player.Components.Buffs=0x" << std::hex << snapshot.Player.Components.Buffs
                << " count=" << std::dec << buffs.size();
            ctx()->Log.Warn(oss.str().c_str());
        }

        for (size_t i = 0; i < buffs.size(); ++i)
        {
            const PluginSDK::Buff& b = buffs[i];

            std::ostringstream oss;
            oss << "SDK_BUFF[" << i << "] "
                << "Name='" << b.Name << "' "
                << "Base='" << StripTrailingNumericSuffix(b.Name) << "' "
                << "Total=" << b.TotalTime << " "
                << "Left=" << b.TimeLeft << " "
                << "Charges=" << b.Charges << " "
                << "SourceEntityId=" << b.SourceEntityId << " "
                << "FlaskSlot=" << b.FlaskSlot << " "
                << "Effectiveness=" << b.Effectiveness;

            if (ContainsCaseInsensitive(b.Name, m_OuterBuffBaseName))
                ctx()->Log.Warn(oss.str().c_str());
            else
                ctx()->Log.Info(oss.str().c_str());
        }

        ctx()->Log.Warn("========== SDK PLAYER BUFF DUMP END ==========");
        m_Status = "Dumped SDK-visible player buffs.";
    }

    std::optional<BeltPoint> ResolveBeltPoint(bool forceScan)
    {
        if (m_UseAutomaticBeltDetection)
        {
            auto autoPoint = ResolveBeltPointFromInventory(forceScan);
            if (autoPoint)
                return autoPoint;
        }

        if (m_ManualBeltX > 0 && m_ManualBeltY > 0)
        {
            BeltPoint p;
            p.X = m_ManualBeltX;
            p.Y = m_ManualBeltY;

            std::ostringstream oss;
            oss << "Manual coordinate: " << p.X << ", " << p.Y;
            p.DebugInfo = oss.str();

            return p;
        }

        return std::nullopt;
    }

    std::optional<BeltPoint> ResolveBeltPointFromInventory(bool forceScan)
    {
        std::vector<PluginSDK::Inventory> inventories = ctx()->Inventory.GetAll();

        std::optional<PluginSDK::Inventory> beltInventory;
        std::string beltName;

        for (const PluginSDK::Inventory& inv : inventories)
        {
            std::string name = ctx()->Inventory.GetName(inv.InventoryId);

            if (EqualsCaseInsensitive(name, "Belt1"))
            {
                beltInventory = inv;
                beltName = name;
                break;
            }
        }

        if (!beltInventory)
        {
            for (const PluginSDK::Inventory& inv : inventories)
            {
                std::string name = ctx()->Inventory.GetName(inv.InventoryId);

                if (ContainsCaseInsensitive(name, "belt"))
                {
                    beltInventory = inv;
                    beltName = name;
                    break;
                }
            }
        }

        if (!beltInventory)
            return std::nullopt;

        PluginSDK::Inventory inv = *beltInventory;

        if (forceScan)
        {
            ctx()->Inventory.Scan(inv.InventoryId);
            inv = ctx()->Inventory.Get(inv.InventoryId);
        }

        BeltPoint result;

        for (const PluginSDK::InventoryItem& item : inv.Items)
        {
            if (item.ScreenValid && item.ScreenW > 0.0f && item.ScreenH > 0.0f)
            {
                result.X = static_cast<int>(std::round(item.ScreenX + item.ScreenW * 0.5f));
                result.Y = static_cast<int>(std::round(item.ScreenY + item.ScreenH * 0.5f));

                std::ostringstream oss;
                oss << "Auto item rect | name=" << beltName
                    << " id=" << inv.InventoryId
                    << " item=" << item.BaseTypeName
                    << " x=" << result.X
                    << " y=" << result.Y;
                result.DebugInfo = oss.str();

                return result;
            }
        }

        if (inv.Grid.Valid && inv.Grid.CellSize > 0.0f && inv.TotalBoxesX > 0 && inv.TotalBoxesY > 0)
        {
            const float centerX =
                inv.Grid.GridScreenX + (static_cast<float>(inv.TotalBoxesX) * inv.Grid.CellSize * 0.5f);

            const float centerY =
                inv.Grid.GridScreenY + (static_cast<float>(inv.TotalBoxesY) * inv.Grid.CellSize * 0.5f);

            result.X = static_cast<int>(std::round(centerX));
            result.Y = static_cast<int>(std::round(centerY));

            std::ostringstream oss;
            oss << "Auto grid center | name=" << beltName
                << " id=" << inv.InventoryId
                << " x=" << result.X
                << " y=" << result.Y;
            result.DebugInfo = oss.str();

            return result;
        }

        return std::nullopt;
    }

    bool TryStartSequence(StartReason reason)
    {
        if (m_State != SequenceState::Idle)
        {
            m_Status = "Sequence already running.";
            return false;
        }

        PluginSDK::Snapshot snapshot = ctx()->Game.GetSnapshot();

        if (!IsSnapshotSafeForAction(snapshot))
            return false;

        POINT original{};
        if (!GetCursorPos(&original))
        {
            m_Status = "Could not read mouse position.";
            return false;
        }

        auto belt = ResolveBeltPoint(true);

        if (!belt)
        {
            m_Status = "No belt point found. Use manual coordinate.";
            return false;
        }

        m_OriginalMouseX = original.x;
        m_OriginalMouseY = original.y;
        m_BeltTargetX = belt->X;
        m_BeltTargetY = belt->Y;
        m_LastBeltInfo = belt->DebugInfo;

        m_LastTriggerTime = Clock::now();

        if (m_BlockHumanInput)
            InstallInputHooks();

        ReleaseAllCommonInputs();

        m_State = SequenceState::OpenInventory;
        m_NextActionAt = Clock::now() + std::chrono::milliseconds(30);

        std::ostringstream oss;
        oss << "Starting sequence. Belt target: "
            << m_BeltTargetX << ", " << m_BeltTargetY
            << " reason=" << (reason == StartReason::ManualTest ? "ManualTest" : "BuffDetected");
        m_Status = oss.str();

        return true;
    }

    void TickSequence()
    {
        if (m_State == SequenceState::Idle)
            return;

        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
        {
            StopSequence("Aborted by ESC.");
            return;
        }

        const TimePoint now = Clock::now();

        if (now < m_NextActionAt)
            return;

        switch (m_State)
        {
        case SequenceState::OpenInventory:
            PressKey('I');
            GoTo(SequenceState::WaitAfterOpenInventory);
            break;
        case SequenceState::WaitAfterOpenInventory:
            GoTo(SequenceState::MoveToBelt);
            break;
        case SequenceState::MoveToBelt:
            SetCursorPos(m_BeltTargetX, m_BeltTargetY);
            GoTo(SequenceState::WaitAfterMoveToBelt);
            break;
        case SequenceState::WaitAfterMoveToBelt:
            GoTo(SequenceState::UnequipClick);
            break;
        case SequenceState::UnequipClick:
            LeftClick();
            GoTo(SequenceState::WaitAfterUnequipClick);
            break;
        case SequenceState::WaitAfterUnequipClick:
            GoTo(SequenceState::ReequipClick);
            break;
        case SequenceState::ReequipClick:
            LeftClick();
            GoTo(SequenceState::WaitAfterReequipClick);
            break;
        case SequenceState::WaitAfterReequipClick:
            GoTo(SequenceState::CloseInventory);
            break;
        case SequenceState::CloseInventory:
            PressKey('I');
            GoTo(SequenceState::WaitAfterCloseInventory);
            break;
        case SequenceState::WaitAfterCloseInventory:
            GoTo(SequenceState::RestoreMouse);
            break;
        case SequenceState::RestoreMouse:
            SetCursorPos(m_OriginalMouseX, m_OriginalMouseY);
            GoTo(SequenceState::Done);
            break;
        case SequenceState::Done:
            StopSequence("Sequence finished.");
            break;
        case SequenceState::Idle:
        default:
            break;
        }
    }

    void GoTo(SequenceState next)
    {
        m_State = next;
        m_NextActionAt = Clock::now() + std::chrono::milliseconds(m_DelayMs);
    }

    void StopSequence(const std::string& reason)
    {
        if (m_State != SequenceState::Idle)
        {
            ReleaseAllCommonInputs();
            SetCursorPos(m_OriginalMouseX, m_OriginalMouseY);
        }

        m_State = SequenceState::Idle;
        m_Status = reason;

        ReleaseAllCommonInputs();
        UninstallInputHooks();
    }

    void HandleF6TestHotkey()
    {
        if (!m_EnableF6TestHotkey)
        {
            m_F6WasDown = false;
            return;
        }

        PluginSDK::Snapshot snapshot = ctx()->Game.GetSnapshot();

        if (m_RequireForeground && snapshot.IsAttached && !snapshot.GameWindowForeground)
        {
            m_F6WasDown = false;
            return;
        }

        const bool f6Down = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;

        if (f6Down && !m_F6WasDown)
        {
            if (m_State == SequenceState::Idle)
                TryStartSequence(StartReason::ManualTest);
            else
                m_Status = "F6 ignored. Sequence already running.";
        }

        m_F6WasDown = f6Down;
    }

    static void PressKey(WORD vk)
    {
        INPUT inputs[2]{};

        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = vk;

        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = vk;
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

        SendInput(2, inputs, sizeof(INPUT));
    }

    static void LeftClick()
    {
        INPUT inputs[2]{};

        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

        SendInput(2, inputs, sizeof(INPUT));
    }

    static void ReleaseAllCommonInputs()
    {
        std::vector<INPUT> inputs;
        inputs.reserve(96);

        auto addKeyUp = [&inputs](WORD vk)
        {
            INPUT input{};
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = vk;
            input.ki.dwFlags = KEYEVENTF_KEYUP;
            inputs.push_back(input);
        };

        auto addMouseUp = [&inputs](DWORD flag, DWORD mouseData = 0)
        {
            INPUT input{};
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = flag;
            input.mi.mouseData = mouseData;
            inputs.push_back(input);
        };

        addMouseUp(MOUSEEVENTF_LEFTUP);
        addMouseUp(MOUSEEVENTF_RIGHTUP);
        addMouseUp(MOUSEEVENTF_MIDDLEUP);
        addMouseUp(MOUSEEVENTF_XUP, XBUTTON1);
        addMouseUp(MOUSEEVENTF_XUP, XBUTTON2);

        addKeyUp(VK_SHIFT);
        addKeyUp(VK_CONTROL);
        addKeyUp(VK_MENU);

        for (WORD vk = 'A'; vk <= 'Z'; ++vk)
            addKeyUp(vk);

        for (WORD vk = '0'; vk <= '9'; ++vk)
            addKeyUp(vk);

        addKeyUp(VK_SPACE);
        addKeyUp(VK_TAB);
        addKeyUp(VK_RETURN);
        addKeyUp(VK_ESCAPE);

        addKeyUp(VK_LBUTTON);
        addKeyUp(VK_RBUTTON);
        addKeyUp(VK_MBUTTON);
        addKeyUp(VK_XBUTTON1);
        addKeyUp(VK_XBUTTON2);

        if (!inputs.empty())
            SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    }

    void InstallInputHooks()
    {
        if (m_InputHooksInstalled)
            return;

        s_Instance = this;

        m_MouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, GetModuleHandleW(nullptr), 0);
        m_KeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, GetModuleHandleW(nullptr), 0);

        m_InputHooksInstalled = (m_MouseHook != nullptr || m_KeyboardHook != nullptr);
    }

    void UninstallInputHooks()
    {
        if (m_MouseHook)
        {
            UnhookWindowsHookEx(m_MouseHook);
            m_MouseHook = nullptr;
        }

        if (m_KeyboardHook)
        {
            UnhookWindowsHookEx(m_KeyboardHook);
            m_KeyboardHook = nullptr;
        }

        m_InputHooksInstalled = false;
    }

    bool ShouldBlockHumanInput() const
    {
        return m_BlockHumanInput && m_State != SequenceState::Idle;
    }

    static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode >= 0 && s_Instance && s_Instance->ShouldBlockHumanInput())
        {
            const MSLLHOOKSTRUCT* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
            const bool injected = info && ((info->flags & LLMHF_INJECTED) != 0);

            if (!injected)
                return 1;
        }

        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    static LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode >= 0 && s_Instance && s_Instance->ShouldBlockHumanInput())
        {
            const KBDLLHOOKSTRUCT* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);

            if (info)
            {
                const bool injected = (info->flags & LLKHF_INJECTED) != 0;

                if (info->vkCode == VK_ESCAPE)
                    return CallNextHookEx(nullptr, nCode, wParam, lParam);

                if (!injected)
                    return 1;
            }
        }

        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    void LoadSettings()
    {
        namespace fs = std::filesystem;

        fs::path settingsPath = DirectoryPath() / "config" / "settings.txt";

        if (!fs::exists(settingsPath))
            return;

        std::ifstream file(settingsPath);
        if (!file.is_open())
            return;

        std::string line;

        while (std::getline(file, line))
        {
            const auto eq = line.find('=');
            if (eq == std::string::npos)
                continue;

            const std::string key = line.substr(0, eq);
            const std::string val = line.substr(eq + 1);

            try
            {
                if (key == "EnableAutoTrigger")
                    m_EnableAutoTrigger = (val == "1");
                else if (key == "EnableF6TestHotkey")
                    m_EnableF6TestHotkey = (val == "1");
                else if (key == "BlockTownHideout")
                    m_BlockTownHideout = (val == "1");
                else if (key == "RequireForeground")
                    m_RequireForeground = (val == "1");
                else if (key == "BlockHumanInput")
                    m_BlockHumanInput = (val == "1");
                else if (key == "UseAutomaticBeltDetection")
                    m_UseAutomaticBeltDetection = (val == "1");
                else if (key == "TriggerOnWalkerMonsterDeath")
                    m_TriggerOnWalkerMonsterDeath = (val == "1");
                else if (key == "OnlyRareOrUniqueWalkers")
                    m_OnlyRareOrUniqueWalkers = (val == "1");
                else if (key == "CheckMonsterMods")
                    m_CheckMonsterMods = (val == "1");
                else if (key == "CheckMonsterBuffs")
                    m_CheckMonsterBuffs = (val == "1");
                else if (key == "TriggerOnTrackedWalkerDisappear")
                    m_TriggerOnTrackedWalkerDisappear = (val == "1");
                else if (key == "MonsterScanIntervalMs")
                    m_MonsterScanIntervalMs = std::clamp(std::stoi(val), 100, 2000);
                else if (key == "MaxMonsterScanDistance")
                    m_MaxMonsterScanDistance = std::clamp(std::stof(val), 500.0f, 12000.0f);
                else if (key == "WalkerKeywordA")
                {
                    strncpy_s(m_WalkerKeywordA, val.c_str(), sizeof(m_WalkerKeywordA) - 1);
                    m_WalkerKeywordA[sizeof(m_WalkerKeywordA) - 1] = '\0';
                }
                else if (key == "WalkerKeywordB")
                {
                    strncpy_s(m_WalkerKeywordB, val.c_str(), sizeof(m_WalkerKeywordB) - 1);
                    m_WalkerKeywordB[sizeof(m_WalkerKeywordB) - 1] = '\0';
                }
                else if (key == "OuterBuffBaseName")
                {
                    strncpy_s(m_OuterBuffBaseName, val.c_str(), sizeof(m_OuterBuffBaseName) - 1);
                    m_OuterBuffBaseName[sizeof(m_OuterBuffBaseName) - 1] = '\0';
                }
                else if (key == "DelayMs")
                    m_DelayMs = std::clamp(std::stoi(val), 20, 1000);
                else if (key == "CooldownMs")
                    m_CooldownMs = std::clamp(std::stoi(val), 500, 30000);
                else if (key == "ManualBeltX")
                    m_ManualBeltX = std::stoi(val);
                else if (key == "ManualBeltY")
                    m_ManualBeltY = std::stoi(val);
            }
            catch (...)
            {
            }
        }
    }

    bool IsSnapshotSafeForAction(const PluginSDK::Snapshot& snapshot)
    {
        if (!snapshot.IsAttached)
        {
            m_Status = "Game not attached.";
            return false;
        }

        if (snapshot.State != PluginSDK::GameState::InGame)
        {
            m_Status = "Not in game.";
            return false;
        }

        if (m_RequireForeground && !snapshot.GameWindowForeground)
        {
            m_Status = "Game window is not foreground.";
            return false;
        }

        if (m_BlockTownHideout && (snapshot.IsTown || snapshot.IsHideout))
        {
            m_Status = "Blocked in town/hideout.";
            return false;
        }

        if (snapshot.IsPaused || snapshot.Vitals.IsPaused)
        {
            m_Status = "Game is paused.";
            return false;
        }

        return true;
    }

    static bool IsOuterBuffArrayName(const std::string& buffName, const std::string& baseName)
    {
        if (buffName.size() <= baseName.size() + 1)
            return false;

        if (!StartsWithCaseInsensitive(buffName, baseName + "_"))
            return false;

        for (size_t i = baseName.size() + 1; i < buffName.size(); ++i)
        {
            if (!std::isdigit(static_cast<unsigned char>(buffName[i])))
                return false;
        }

        return true;
    }

    static std::string StripTrailingNumericSuffix(const std::string& value)
    {
        if (value.empty())
            return value;

        size_t pos = value.size();

        while (pos > 0 && std::isdigit(static_cast<unsigned char>(value[pos - 1])))
            --pos;

        if (pos == value.size())
            return value;

        if (pos > 0 && value[pos - 1] == '_')
            --pos;

        return value.substr(0, pos);
    }

    static const char* StateName(SequenceState state)
    {
        switch (state)
        {
        case SequenceState::Idle: return "Idle";
        case SequenceState::OpenInventory: return "OpenInventory";
        case SequenceState::WaitAfterOpenInventory: return "WaitAfterOpenInventory";
        case SequenceState::MoveToBelt: return "MoveToBelt";
        case SequenceState::WaitAfterMoveToBelt: return "WaitAfterMoveToBelt";
        case SequenceState::UnequipClick: return "UnequipClick";
        case SequenceState::WaitAfterUnequipClick: return "WaitAfterUnequipClick";
        case SequenceState::ReequipClick: return "ReequipClick";
        case SequenceState::WaitAfterReequipClick: return "WaitAfterReequipClick";
        case SequenceState::CloseInventory: return "CloseInventory";
        case SequenceState::WaitAfterCloseInventory: return "WaitAfterCloseInventory";
        case SequenceState::RestoreMouse: return "RestoreMouse";
        case SequenceState::Done: return "Done";
        default: return "Unknown";
        }
    }

    static std::string Trim(const std::string& s)
    {
        const auto begin = std::find_if_not(s.begin(), s.end(), [](unsigned char c)
        {
            return std::isspace(c) != 0;
        });

        const auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c)
        {
            return std::isspace(c) != 0;
        }).base();

        if (begin >= end)
            return {};

        return std::string(begin, end);
    }

    static std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });

        return s;
    }

    static bool EqualsCaseInsensitive(const std::string& a, const std::string& b)
    {
        return ToLower(a) == ToLower(b);
    }

    static bool ContainsCaseInsensitive(const std::string& haystack, const std::string& needle)
    {
        if (needle.empty())
            return true;

        return ToLower(haystack).find(ToLower(needle)) != std::string::npos;
    }

    static bool StartsWithCaseInsensitive(const std::string& value, const std::string& prefix)
    {
        if (prefix.size() > value.size())
            return false;

        return ToLower(value.substr(0, prefix.size())) == ToLower(prefix);
    }

private:

    struct WalkerMonsterTrack
    {
        uint32_t Id = 0;
        uintptr_t Address = 0;
        bool SeenThisScan = false;
        bool HasWalkerMarker = false;
        bool WasAlive = false;
        bool Triggered = false;
        float LastSeenSeconds = 0.0f;
        float LastWorldX = 0.0f;
        float LastWorldY = 0.0f;
        float LastWorldZ = 0.0f;
        std::string LastMarker;
    };

    bool m_EnableAutoTrigger = false;
    bool m_EnableF6TestHotkey = false;
    bool m_BlockTownHideout = true;
    bool m_RequireForeground = true;
    bool m_BlockHumanInput = true;
    bool m_UseAutomaticBeltDetection = true;
    bool m_F6WasDown = false;

    bool m_TriggerOnWalkerMonsterDeath = true;
    bool m_OnlyRareOrUniqueWalkers = true;
    bool m_CheckMonsterMods = true;
    bool m_CheckMonsterBuffs = false;
    bool m_TriggerOnTrackedWalkerDisappear = false;
    bool m_PendingWalkerDeathTrigger = false;

    char m_OuterBuffBaseName[128] = "stolen_mods_buff";
    char m_WalkerKeywordA[128] = "Shroud Walker";
    char m_WalkerKeywordB[128] = "Shade Walker";

    int m_MonsterScanIntervalMs = 200;
    float m_MaxMonsterScanDistance = 8000.0f;
    float m_TrackedWalkerDisappearSeconds = 0.60f;
    float m_ForgetTrackedWalkerAfterSeconds = 8.0f;

    int m_DelayMs = 120;
    int m_CooldownMs = 5000;

    int m_ManualBeltX = 0;
    int m_ManualBeltY = 0;

    int m_BeltTargetX = 0;
    int m_BeltTargetY = 0;

    int m_OriginalMouseX = 0;
    int m_OriginalMouseY = 0;

    SequenceState m_State = SequenceState::Idle;
    TimePoint m_NextActionAt = Clock::now();
    TimePoint m_LastTriggerTime = Clock::now() - std::chrono::seconds(30);
    TimePoint m_EnableTime = Clock::now();
    TimePoint m_LastMonsterScanAt = Clock::now() - std::chrono::seconds(30);

    std::string m_Status = "Idle.";
    std::string m_LastBeltInfo = "No Belt1 detection yet.";
    std::string m_LastMatchedOuterBuff;
    std::string m_LastMatchedWalker;

    std::unordered_map<uint32_t, WalkerMonsterTrack> m_TrackedWalkers;

    HHOOK m_MouseHook = nullptr;
    HHOOK m_KeyboardHook = nullptr;
    bool m_InputHooksInstalled = false;

    static inline BeltToggleOnBuffPlugin* s_Instance = nullptr;
};

extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin()
{
    return new BeltToggleOnBuffPlugin();
}

extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* plugin)
{
    delete plugin;
}
