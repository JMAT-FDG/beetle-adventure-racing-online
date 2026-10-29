# Extended racing and remote play — 2026-10-28

Target: two local humans plus one remote human, then a human in every available grid position. Tests must leave the desktop usable. This is an unfinished engine experiment, not a released three-player mode.

## Reproducible observations

From `beetle-pc`, `Test course arriere-plan.ps1` launches the actual executable with a hidden SDL window, no audio device, simulated inputs, isolated settings/saves and bounded lifetime. Captures come from the game renderer, not the desktop. `BAR_BACKGROUND_TEST` requires an absolute `BAR_TEST_CONFIG_DIR`. The script seeds a copy of the existing Controller Pak, never the live file.

- `logs/background-20261028-085705-105`: invalid count experiment. The initial native diagnostic used zero-extended MIPS addresses and observed the wrong RDRAM alias. Its requested count did not establish three spawns. Fixed with `S32(address)`. Capture paths also initially broke at spaces; later runs use unique relative filenames moved to the run directory after exit.
- `logs/background-20261028-090105-621`: correctly addressed settings, three distinct spawn calls (0, 1, 2), course initialization returned. Frame at SI poll 6000 shows a damaged third view.
- `logs/background-20261028-090401-197`: explicit three-camera layout. State 5, track 5, previous humans 1, requested humans 3, AI disabled. Player pointers `0x803897E8`, `0x8039B7A0`, `0x8039C468`. Same damaged third view. **Selecting the three-view layout does not resolve the corruption.** Keep this negative result.
- `logs/network-three-20261028-090403-698`: three independent native processes; two local slots plus remote slot 2. Distinct buttons/sticks, cropped synthetic video and PCM checked. All exited 0. Transport evidence, not race physics or Internet evidence.
- `logs/background-20261028-091222-635`: integrated real engine and windowless remote client. Three spawns, all three update callbacks reached 120 ticks; the game's controller getter read `0x8000` on slot 2. Client received 1,620 actual rendered frames in 130 seconds. Third view still damaged. This establishes that network input reaches the real game, not that the remote car's physics/camera/results are correct.
- `logs/background-20261028-091631-641`: initial descriptors are plausible (`-0.2,5,0,0`; `0.2,15,0,0`; `-0.2,25,0,0`), camera ids 0/1/2 distinct. 1,835 frames received. The damage persists at poll 7500, so it was not just an early transient. Bad starting descriptors were not supported by this measurement.
- `logs/background-20261028-092308-536`: **third-view fix**. Set count after `func_selection_00402F30` returns (selection cleanup finished) but before the next scene is loaded. Race entry now observes previous count 3. Three spawns and three updates at 120 ticks; slot 2 controller getter reads accelerator. Client receives 1,856 frames in 150 seconds. Internal frame7500 shows three coherent views with cars/track. Earlier race-entry-only allocation was too late for scene setup. Complete laps, physics correctness, collisions and results remain unvalidated.
- `logs/background-20261028-092942-302`: final build with `-ProductionSwitch`, exercising `BAR_RACE_PLAYERS=3` used by the launcher while keeping the window hidden. All three spawn/update criteria and remote controller read pass; 1,364 game frames received in 110 seconds. Both CTest transport suites pass and launcher `-ValidateForm` validates host-three and join states without displaying it.

The default test script selects a single race through original menus. Timing uses SI polls and depends on the machine. Reports require initialization, all requested spawn/update indices and, with `-Network`, a successful peer plus accelerator read on slot 2. Process survival is insufficient. `BAR_RACE_PLAYERS=3` is now an explicit opt-in experiment exposed by the host launcher's checkbox. Unset means original behavior. Background tests use `BAR_TEST_RACE_PLAYERS`, or `-ProductionSwitch` to exercise the launcher's exact environment setting. The launcher forces two locals and grid layout; the friend must join before Start Game. No readiness lobby yet; new players initially inherit J1's car choice.

## Grid and capacity evidence

- `selection.c`, `func_selection_004032EC`: full-grid default sets seven AI cars. Eight total cars is inferred from one human plus seven AI; not measured eight-human support.
- `include/structs.h`, `UnkStruct_80025CF0`: human count +0x24, AI count +0x28, `CarInfo unk138[4]` at +0x138, six-byte stride. Fixed settings base `0x80025CF0`, human count `0x80025D14`, AI `0x80025D18`.
- `race/__entrypoint_func_race_400000.s`, 0x4002E8–0x4003BC: count at `D_race_004049F0 + 0x10`, player pointers at +4*i. A fifth pointer overwrites the count. Increasing only the human count beyond four is invalid.
- `race/func_race_004035C8.s`: applies choices to all humans using the same six-byte stride.
- `scrn/func_scrn_0040009C.s`: table stride 70 bytes, first halfword is camera count. `scrn.data.s`: layout 1=one, 2=two, 3=three, 4=four. Race requests layout 1 at 0x400850; test wrapper selects human count.
- `cam/func_cam_00400190.s`: explicit initialization for camera counts 0..3; later cameras get environment id -1. Eight cameras need changes here.
- `plyr/func_plyr_00400678.s`: constructor receives controller slot in a0, stores it at player +0x14, allocates camera/car. Spawn descriptors: scene export +0x2C, then pointer +0x24, 16-byte stride per slot. Capacity/correctness for additional humans needs measuring.
- `plyr/func_plyr_00401484.s`: reads buttons through controller export +0x14 using player +0x14 as slot. Three car objects alone do not prove their controls affect physics.
- `plyr/func_plyr_00402268`: calls `00402340` only for count <3 and mode 0/3. Effect on extended races unconfirmed; do not blindly remove the condition.
- Frontend, SI callbacks and network currently serve four N64 slots. Increasing only the network limit cannot create more human cars.

## Network tests

One host simulation, TCP commands/JPEG/PCM. Local monitor clients also use loopback. HMAC-SHA256 over fresh nonce and metadata authenticates sessions; contents are unencrypted. Queues are bounded, controls expire after 500 ms, disconnected ports clear. No NAT traversal/relay or two-PC Internet validation.

`Test reseau trois joueurs.ps1` launches `--transport-host-test`, `--transport-local-test`, `--transport-peer-test` before any SDL initialization, loopback TCP 28767. `--game-peer-test` uses 28768, sends accelerator on slot 2 and receives actual game frames in the integrated background experiment. Ephemeral codes are passed in child environments, omitted from reports.

## Required evidence next

Measure slot 2 buttons in game updates, isolate spawn/camera faults from rendering corruption, verify three independently moving cars and complete results. Integrate extended racing with a ready-player lobby, then validate two PCs. To reach eight humans, extend/relocate settings, race storage, inputs, cameras, HUD and results before relaxing bounds. Keep total AI plus humans within the actual scene grid capacity.
