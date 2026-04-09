# ExamplePlugin - Standalone Distribution

Self-contained buildable plugin for POE2Fixer. No dependency on the POE2Fixer host source tree.

## Requirements

- Visual Studio 2022 with MSVC v143 toolset
- Windows SDK 10.0
- C++20

## Build

Open `ExamplePlugin.sln` in Visual Studio 2022 and build **Release | x64**.

Or from command line:

```
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" ExamplePlugin.sln -p:Configuration=Release -p:Platform=x64
```

Output: `bin\Release\ExamplePlugin.dll`

## Install

Copy `ExamplePlugin.dll` to `Plugins/ExamplePlugin/` in your POE2Fixer installation directory, then enable it in the Plugins tab.

## Structure

```
sdk/            Plugin SDK headers (PluginAPI.h, PluginContext.h, PluginGameData.h, PluginHelpers.h)
imgui/          ImGui library (headers + sources, compiled into the DLL)
examples/       Example tab implementations (Buffs, Entities, Inventory, Memory, UI Explorer)
ExamplePlugin.cpp   Main plugin entry point
```
