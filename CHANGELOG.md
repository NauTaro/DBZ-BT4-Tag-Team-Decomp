# 0.1.39.5 R1.1

- Align health-bar alpha clearing with widescreen HUD scaling, so the remaining blue tier and the exposed green tier use the same mask on both sides.
- Reuse exact matching resident HD textures across GS writes and read every entry of native 16-color palettes when caching textures.
- Bound decoded texture memory and queued GPU frames, including hidden or minimized windows.
- Keep Lua camera smoothing relative to a moving fighter and preserve camera preferences during rollback. Apply the Tag Team camera mod once to the selected view.
- Interpolate rim lighting with the fighter model and sample colored HD ki effects for energy lighting on SDL GPU renderers.
- Track arena-boundary geometry by its own packets instead of reused texture addresses.
- Keep unrelated Tag Team effects running during another pair's cinematics and use external framing rules for clashes.
- Add an optional left-stick rotation control for beam and melee clashes under F1 > Controls. Button controls remain available.
- Show the ultimate technique and maximum-power readiness in Skill List.
- Align the native Team Battle stage reel and honor an explicitly selected installation data folder during setup validation.
- Upgrade unmodified bundled presentation scripts while preserving customized scripts and saved settings.

- Update the online simulation family to sim4 for the Tag Team effect changes. All players in a room need a compatible R1.1 build.

# 0.1.39.5 R1

- Integrate upstream 0.1.39.5 Skill List changes while retaining BT4 and Tag Team presentation mappings.
- Add F1 > Updates with manual checks, retry, download progress and a confirmed restart from the offline main menu.
- Read ADX version 3 and 4 loop points for combat music, restore the predictor at loop boundaries and correct shared music timing metadata.
- Skip Lua presentation work during rollback resimulation and discarded display frames.
- Add optional right-stick enemy targeting and a local special-move camera preference.
- Keep unrelated Tag Team fighters active during another pair's special-move preparation.
- Score both human players' correct button-clash answers independently.
- Restore scripted Parallel Quest setup and descriptions; load all BT4 roster chips in the remaining mission and ranking selectors.
- Center Modern HUD portraits with artwork-specific eye anchors and soften portrait borders.
- Fix the automatic GitHub download and installation flow for the existing DBZ BT4 HD v1.07 pack.
- Preserve installed language profiles, custom scripts, saves, setup ISO selection and signed update checks. The online simulation family advances to sim3; participants need compatible builds.

# 0.1.39.4 R1

- Integrate upstream 0.1.39.4 Lua presentation mods, animated matchmaking radar and widescreen message alignment.
- Include Modern HUD, Modern Camera, Modern Look, Skill List and two examples, disabled by default; deliver them through older BT4 updaters while preserving existing scripts.
- Adapt mod camera and fighter data to Tag Team targeting; keep free-camera priority and Hide HUD behavior.
- Add English, Spanish and Brazilian Portuguese controls for Mods and optional offline duel mechanics.
- Preserve BT4 language setup, HD textures, HUD colors, signed updates and native Windows, Linux and macOS packages.

# 0.1.39.3 R1.1

- Center installed-language choices and download optional packages directly from GitHub; setup only asks for the ISO.
- Fix Portuguese menu captions, restore high-resolution font rendering and keep localized lettering ahead of generic HD artwork.
- Recognize matching HD portraits across texture alpha modes while preserving palette and size checks.
- Align the Potara Shop scroll bar, restore the screen-position preview and correct full health-bar backing colors.
- Refresh the HUD editor preview and run Windows game and setup executables without requesting elevation.

# 0.1.39.3 R1

- Integrate upstream 0.1.39.3 widescreen menus, replay loading and screen reader support.
- Fix Portuguese selection and install its required English data automatically.
- Add Brazilian Portuguese installer text and keep the optional HD pack checkbox visible.
- Download public language and texture packs without GitHub sign-in.

# 0.1.39.2 R9

- Install Brazilian Portuguese through setup and select it from the native game language menu, using the English data profile.
- Download the optional HD texture pack (3.69 GB) with progress and archive verification.
- Isolate Portuguese text and textures from the Spanish and English installations; no command scripts or junctions are needed.
- Keep language packs and HD textures in their dedicated releases, separate from signed game updates.
- Rebuild the Windows RTC library from locked upstream sources and verify its build receipt during packaging.

# 0.1.39.2 R8 Hotfix

- Restore the One on One / native BT3 Team Battle selector in Quick Match and route both modes through the standard online lobby queues.

# 0.1.39.2 R8

- Separate Tag Team Quick Match (2 vs 2) from custom rooms, with 2 vs 2, 5 vs 5 and six-player Free for All presets.
- Restore configurable duel creation and use a four-frame rollback limit for Quick Match.
- Add an online-menu fighter count refreshed every five minutes.
- Expand native message panels without stretching their decorations; align menu groups and enlarge ultrawide labels.
- Synchronize Free for All rules, targeting and winner selection through the participant-hosted room protocol.
- Reset Tag Team input admission when a same-room quick-match roster fills, so the four-player match accepts its first synchronized frames.

# 0.1.39.2 R7 Hotfix

- Load all 250 BT4 portrait sheets in tournament, Hero and Time Nest selectors, including alternate forms.
- Restore BT4 menu music IDs, tournament fanfare and mode-specific Time Nest tracks.
- Preserve panoramic artwork proportions, align costume captions with their panels and hide fighter portraits during stage selection.
- Restore the full spectator controls: free camera, fighter tracking, fight overview, target, speed, zoom and reset.
- Restore keyboard and controller navigation with complete English, Spanish and Brazilian Portuguese labels.
- Keep camera motion outside rollback state; route camera controls before local input is recorded or transmitted.

# 0.1.39.2 R7

- Bundle nested macOS library dependencies, including Brotli, and reject packages with missing or external libraries.
- Expand menus and HUD across the full panoramic viewport, including character, costume, stage, settings and Dragon Wisdom screens.
- Align F1 help with the full-width layout and recognize ordered patch-revision updater tags.
- Align costume captions and Tag Team HUD editor widgets at ultrawide aspect ratios; hide selection portraits on stage previews.
- Expose the local free camera in every battle mode with English, Spanish and Brazilian Portuguese controls.
- Dismiss the Start pause menu immediately when toggling HUD visibility; Start opens it again.

# 0.1.39.2 R6

- Move room discovery and matchmaking to private Supabase signaling.
- Host Tag Team input coordination on the room owner through direct peer channels.
- Negotiate voice directly between participants, including manual-address sessions.
- Preserve room membership for voice across duel and tournament transitions.
- Pin Windows GitHub CLI packaging to its verified upstream archive; remove the deleted R26 bootstrap dependency.
- Retain R5 custom-content, music-tempo, rematch and renderer fixes.
