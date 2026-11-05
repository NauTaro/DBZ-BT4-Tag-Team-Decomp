# 0.1.39.5 R3

- Restore Hit's original Time Skip backdrop and allow its HD replacement to load.
- Keep transient effect preparation running under the original pause flag during developer pauses, instead of redrawing stale aura/effect resources.
- Align the rim-light pass with its shading pass's exact interpolated bones, camera and light direction; preserve the rim's own colour and intensity.
- Remove cross-address HD replacement sharing and match native palette cache coordinates to the decoder.
- Replace periodic full-cache memory sweeps with tracked texture storage and a working-set budget that retains recent frame references. Keep a separate bounded CPU image cache.
- Invalidate artwork address aliases when cache entries move, and cancel software enlargement after an HD replacement arrives.
- Keep the native gauges suppressed throughout the Modern HUD exit animation, and upgrade only the exact unmodified bundled script.
- Respect the saved interpolation setting and allocate interpolation history only while enabled.
- Speed up indexed texture matching without adding another texture cache.
- Allocate water wakes for all Tag Team fighters and reject out-of-range object IDs instead of overwriting adjacent particle lists.
- Filter Tag Team RTT samples, ignore duplicate delay feedback, and reset adaptive timing between battles.
- Improve 1v1 room routing, legacy member descriptors and room admission diagnostics.
- Exchange Ginyu's body and current health with the struck opponent, retaining each player's controls and other roster members.
- Match BT4's tournament selection lock instead of disabling navigation in the former Yamcha slot.
- Preserve native ISO artwork and fonts; legacy generated-art/font settings no longer replace them, including after startup language selection.
- Offer the supplied BT4 HD v1.0 Light DDS archive (2.54 GB) as a separate optional GitHub download, verified and read directly from ZIP.
- Restore the ISO's Ragged Clothes Potara model selection, retain local Tag Team item sets, and block mechanical item bonuses on both online paths while retaining recognized visual components.
- Automatically migrate the managed v1.07 pack through Setup at an offline menu, with no ISO or language reinstallation; keep the previous pack until the replacement is verified.
- Receive synchronized development test rules for health, Ki, Blast Stock, pause and single-step. These rules apply equally to all fighters; only the separate Dev client exposes their controls.
- Use simulation family sim6 for the corrected gameplay and shared test-rule inputs. All players in a room need a compatible R3 build.

- Keep Modern Camera presentation consistent when a render frame is skipped; use stable view identities for alternating display buffers and freeze the world camera during native pause.
- Bound camera collision traversal to valid stage data and intersected zones.
- Apply one smooth fade across every piece of a character-selection artwork, preserving original ISO files and unrelated interface elements.
- Restore F1 font choices, including Original, without changing saved choices when selecting a language.
- Restore Modern Look energy lighting on DirectX 12 as well as Vulkan, sample effect colour with bounded work and avoid the lighting pass when no energy light is active.

<!-- bt4-update:en
- Restore Hit's Time Skip effect and correct Modern HUD transition overlap.
- Improve texture-cache lifetime and bounded memory use.
- Improve Tag Team timing feedback and 1v1 room admission.
-->
<!-- bt4-update:es
- Restaura el efecto de salto temporal de Hit y corrige la superposición durante la salida del HUD moderno.
- Mejora la duración de la caché de texturas y su control de memoria.
- Mejora el ajuste de tiempos de Tag Team y el acceso a salas 1 vs 1.
-->
<!-- bt4-update:pt
- Restaura o efeito de salto temporal do Hit e corrige a sobreposição na saída do HUD moderno.
- Melhora a duração do cache de texturas e o controle de memória.
- Melhora o ajuste de tempo do Tag Team e a entrada em salas 1 contra 1.
-->

# 0.1.39.5 R2

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

- Update the online simulation family to sim4 for the Tag Team effect changes. All players in a room need a compatible R2 build.

- Show localized update highlights beside Restart now throughout menus, excluding battles and character selection.

<!-- bt4-update:en
- Correct widescreen health-bar colors and HD texture stability.
- Reduce retained texture memory and queued GPU frames.
- Improve camera smoothing, lighting and Tag Team effects.
- Add stick rotation in clashes and maximum-power readiness.
- Show translated update highlights in menus before restarting.
-->
<!-- bt4-update:es
- Corrige los colores de las barras de vida en panorámica y la estabilidad de las texturas HD.
- Reduce la memoria retenida por texturas y los fotogramas pendientes de la GPU.
- Mejora el suavizado de cámara, la iluminación y los efectos de Tag Team.
- Añade giro de palanca en los choques e indicador de máximo poder.
- Muestra las mejoras de las actualizaciones en los menús antes de reiniciar.
-->
<!-- bt4-update:pt
- Corrige as cores das barras de vida em tela panorâmica e a estabilidade das texturas HD.
- Reduz a memória retida por texturas e os quadros pendentes da GPU.
- Melhora a suavização da câmera, a iluminação e os efeitos de Tag Team.
- Adiciona rotação do analógico nos confrontos e indicador de poder máximo.
- Mostra as novidades das atualizações nos menus antes de reiniciar.
-->

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
