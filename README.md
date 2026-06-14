# Belt Toggle On Buff

A POE2Fixer SDK v6 plugin that automatically toggles the player's belt when a tracked Walker monster dies.

The plugin is intended for setups where a temporary stolen monster modifier, such as **Shade Walker** or **Shroud Walker**, should trigger an automatic belt unequip/equip sequence.

## What it does

The plugin watches nearby rare or unique monsters for specific Walker-related monster modifiers. When a tracked monster changes from alive to dead, the plugin assumes that the stolen modifier has been applied to the player and starts the configured belt toggle sequence.

The sequence can:

* save the current mouse position
* optionally block human mouse/keyboard input while running
* open the inventory
* click the configured belt slot
* close the inventory
* restore the original mouse position

A manual F6 test hotkey is also available.

## Detection method

This version does **not** rely on scanning the player's `stolen_mods_buff` memory structure.

Instead, it tracks monsters directly:

1. Scan nearby monsters from the game snapshot.
2. Check rare/unique monsters for Walker-related monster mods.
3. Track matching monsters while they are alive.
4. Trigger the belt sequence when a tracked monster dies.

This approach is designed to avoid expensive player-buff deep scans and should be more reliable for modifiers such as:

* Shade Walker
* Shroud Walker
* MonsterShadeWalker
* MonsterShroudWalker

## Recommended settings

```text
Enable automatic trigger by outer buff = on
Trigger on Walker monster death = on
Only rare/unique monsters = on
Check monster OMP mods = on
Check monster buffs = off
Trigger if tracked Walker disappears = off
Monster scan interval ms = 200
Max monster scan distance = 8000
Walker keyword A = Shroud Walker
Walker keyword B = Shade Walker
Cooldown ms = 5000
```

Start with `Check monster buffs` disabled. Monster OMP/mod detection should usually be enough and is expected to be cheaper than scanning monster buffs.

## Belt position

The plugin can either use automatic Belt1 detection or a manually configured belt coordinate.

To set a manual coordinate:

1. Move the mouse over the belt slot.
2. Click **Set manual belt coordinate from current mouse**.
3. Disable automatic belt detection if needed.

If automatic belt detection works correctly, leaving it enabled is recommended.

## Test mode

The plugin includes an optional F6 test hotkey.

When enabled, pressing F6 starts the belt toggle sequence manually. This is useful for verifying that the belt position, inventory timing, and input blocking behave correctly before enabling automatic detection.

## Safety options

The plugin includes several safety-related settings:

```text
Do not run in town/hideout
Require game window foreground
Block human input while running
Cooldown ms
```

`Require game window foreground` prevents the sequence from running while Path of Exile 2 is not the active window.

`Block human input while running` can help prevent accidental mouse movement or key presses from interfering with the belt toggle sequence.

## Troubleshooting

### The plugin does not trigger

Check the following:

* `Enable automatic trigger by outer buff` is enabled.
* `Trigger on Walker monster death` is enabled.
* `Check monster OMP mods` is enabled.
* The target monster is rare or unique.
* The monster is within the configured scan distance.
* The Walker keywords match the actual monster mod names.

Try increasing:

```text
Max monster scan distance = 12000
```

If it still does not detect anything, enable:

```text
Check monster buffs = on
```

for testing.

### The plugin triggers too often

Increase the cooldown:

```text
Cooldown ms = 8000
```

Also keep this disabled unless needed:

```text
Trigger if tracked Walker disappears = off
```

The disappear fallback can be useful in some cases, but it may cause false positives if monsters leave the snapshot without dying.

### The overlay lags

Use the monster-death based version instead of player-buff deep scan versions.

Recommended performance settings:

```text
Check monster OMP mods = on
Check monster buffs = off
Monster scan interval ms = 200
Max monster scan distance = 8000
```

If needed, increase:

```text
Monster scan interval ms = 500
```

## Build requirements

* Visual Studio 2022
* Release | x64
* C++20
* POE2Fixer SDK v6
* `PLUGIN_EXPORTS` defined

## Notes

This plugin is experimental and depends on POE2Fixer SDK structures remaining compatible with the current game version. Game updates may require adjustments to entity, component, or monster modifier handling.

Use at your own risk.
