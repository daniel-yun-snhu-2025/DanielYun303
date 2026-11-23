# Sector Nine: playable slice

Start in the main menu and choose **Deploy**. The facility (`/Game/Maps/TestEnvironment`) is the playable map:

1. Look at the gun and press **E** to equip it (HUD ammo changes from "No gun" to "Unlimited").
2. Look at the pink key and press **E** to carry it. **E** again drops it; doors also open with **E**, so drop the key, open Door 1, pick the key back up and walk through.
3. Hold the key and aim: the blue arc and the ring show where it will land. A **green** ring with a cross means the matching pressure plate. Press **R** to throw.
4. A key that lands on its plate is seated there and unlocks the matching door. Open it with **E**.
5. Defeat the enemy with **Left Mouse**. Enemies move through patrol, alert, attack and search states. In the original Blueprint a defeated enemy spawns a physics key in its `RewardItemColor` for a later plate.

Controls: W/A/S/D move, mouse look, Space jump, E pick up / drop / interact, R throw, Left Mouse fire.

## GAM 495 refinements

| Plan item | Where |
|---|---|
| 1. Compact slice, imported content labelled | `Config/DefaultEngine.ini` (menu start), `Config/DefaultGame.ini` (`MapsToCook`), this file |
| 2. Enemy AI state machine with tuning data | `Plugins/SectorNineEnemyAI` (`ASectorNineSentinel`, parent of `BP_Turret` and its five variants) |
| 3. Throw arc, landing ring, consistent landing | `Plugins/SectorThrow` |
| 4. HUD, interaction prompts, menu | `Plugins/SectorFieldUI` |
| 5. Rebalance and traceability | `docs/evidence/item5-6/facility/traceability.csv` |
| 6. Lighting and audio pass, build, video | `Content/FacilityAudio`, facility lights in `TestEnvironment`, `docs/evidence/final` |

`Plugins/SectorDemo` is a development-only evidence driver. It does nothing unless the game is started with `-SectorNineDemo`, and its module is denied for Shipping targets.

## Asset credits and scope

`Content/ParagonGrux` and `Content/ParagonRampage` are imported Paragon sample content from Epic Games, not original character art. The sample animation-test, lighting-background and character showcase maps are not playable levels of Sector Nine. The cooking map list selects only the facility and the main menu; assets referenced by those maps are still included in a build.
