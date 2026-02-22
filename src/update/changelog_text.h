#pragma once

static const char* s_changelogHistory = R"(
[Release 1.0]

Initial Release of Haven Tools:

Features include:

- Complete model browser (go to modelmeshdata.erf)

- Preview models, textures, collision, animations, materials, skeleton, etc.

- Character designer

- Design a character and dress them up. This is heavily WIP and buggy.

- Export models as GLB

- Export models with their animations from either the browser or character designer. Allows easy import to blender or other application. Great for 3d printing
**NOTE: Exporting from the character designer will export a model at ~300mb and can be slow due to it processing over 1000 animations. This is WIP and will be changed later.**

- Dump the contents of the .erf files quickly and easily.

- You do not need to dump anything to preview the models.

Use: Browse to your DAO installation and select the .exe.

[Release 1.1]

- Added support for audio file playback, and extraction to mp3.

- Only works on single-sample files that use mp3 encoding.
    Some, like tower_level_1.fsb, are multi-samples (in this case 121), and I believe are IMA ADPCM encoded and do not work yet.

[Release 1.2]

- Embedded MSVCP to prevent missing .dll errors.

[Release 1.3]

### Audio System

- Expanded Search Paths:

-- Added support for packages/core (Base Game) and packages/core_ep1 (Awakening expansion) directories.

-- The scanner now looks in both modules/... and packages/... for .fsb files.

- Header Markers:

-- Inserted header markers into the audio file lists to separate content visually.

- FSB4 Parsing:

-- Rewrote parseFSB4Samples to calculate header sizes dynamically based on the number of samples, rather than using fixed offsets. This fixes the "failed to play" errors on many voice-over files.

- MP3 Detection Fix:

-- Removed strict MP3 sync byte validation (0xFF 0xE0) which was rejecting valid game audio. Now strictly trusts the FSB header flags for format detection.

- Export Fixes:

-- Fixed extractFSB4toMP3 to use the parsed offsets.

-- Updated extractFSB4SampleToWav to include a fallback: if you try to "Export to WAV" on an MP3-encoded file, it now uses Windows Media Foundation to decode it to PCM first, ensuring the export works.

### UI

- Visual Headers:

-- Added logic to detect the header markers.

-- Renders them as separators with a red font.

-- Removed ImGuiListClipper: The clipper requires all items to be the exact same height. Since headers and files now differ in height, the clipper was causing empty black voids in the list. Removing it ensures everything renders correctly.

- Interaction Logic:

-- Double-clicking an audio file now opens the Sound Bank Browser for both single and multi-sample files.

### UI Layout

- Default Window Positions:

-- Added ImGui::SetNextWindowPos with ImGuiCond_FirstUseEver to all tool windows so they don't stack on top of each other when first opened.

-- Render Settings: Top-Left

-- Texture Preview: Top-Right

-- Audio Player: Bottom-Left

-- UV Viewer: Bottom-Right (below Texture Preview)

-- Animation Window: Far Right

-- MAO Viewer / Sound Bank Browser: Center Screen

[Release 1.4]

- Performance & Stability

- - Virtualization Implemented: Replaced standard loops with ImGuiListClipper across all long lists. This ensures only visible items are rendered, eliminating UI lag for folders with thousands of files.

[Release 1.5]

- Fixed: Resolved "Missing DLL" runtime errors for specifically MSVCP140D.dll and VCRUNTIME140D.dll.

- Changed: Switched MSVC runtime linkage from Dynamic to Static.

[Release 1.6]

- Morphs:

-- Implemented .mor file parsing to extract absolute vertex positions for heads.

-- Added Face Presets system.

- Beard Support:

-- Added beard mesh scanning and loading.

-- Added Beard Slider to the head tab.

-- Beards share the same color as hair.

### Fixes & Improvements

- Bald Head Fix:

-- Fixed issue where bald heads were incorrectly detected as hair, causing them to load as white/swizzled textures.

[Release 1.7]

- Shaders updated

--Updated shaders to correctly render diffuse, normal, specular, and tint maps.

- Character Designer Changes

-- Added age slider
-- Added Stubble/ Eyebrow Slider
-- Makeup color picker
-- Eye color picker
-- Tattoo picker
-- Updated armor to include different styles
-- Color picker for armor

- Preset faces

-- Preset faces now correctly find eye colors and makeup. Some bugs with this right now.

[Release 1.8]

- UI Cleanup

-- Added loading progress for clarity.
-- Cleaned up the splash screen

- Auto Updated

-- Added autoupdater by checking version on github.

[Release 1.9]

- Update to test auto updating.

-- Checks for a new version on launch
-- Offers to update if a new version is available on launch.
-- Creates a .bak for old version after updating.

- Added Changelog and About

[Release 1.10]

- Updated UI
- Added Import option to File -> import

- Added importing
-- Importing is a WIP and buggy. It is not guaranteed to work. I have tried to automate fixing some of the necessary structure, such as GOB.
-- HavenTools embeds a variety of .dll's and .exe's to support this.

-- Please ensure that you report any issues to the issue tracker on GitHub, or in a comment on NexusMods.

[Release 1.11]

- Added progress bar to import/ exporting.
- Added export options. For now, only animations. More will come later,
- Added FBX binary as an export option. File -> Export -> to FBX.

Note: Importing does not currently work on Linux.

[Release 1.12]

- Fixed a bug with importing where models would not be added to the .erf
- Added support for skeletons when importing
- Imported models now correctly display their textures and rigs.
- Added filters between All/ Mods/ Core game/ and Awakening Expansion.
- Added Weapons tab to character browser (WIP!)

[Release 1.13]

- Fixed a bug where a models root node would unexpectedly rotated.
- Fixed a bug where models with submeshes would get merged when importing.
- Added collision importing/ exporting.
-- You must follow the naming convention UCX_MeshName_01 for collision.
- Added an option to delete models that have been imported from the .erf.
- Updated 2da parsing.

[Release 1.14]

- Fixed shaders on export
-- Now exports diffuse, specular, and normals. May need some adjustments to make it "look right."
- Export to FBX now correctly creates the armature in Blender.
-- 3DS max is still kind of buggy.
-- FBX Importing is coming soon.
- Added option to export a 1x, 10x, 100x, or 1000x scale for FBX.
- Added option to export without a skeleton.
-- You must follow the naming convention UCX_MeshName_01 for collision.

[Release 2.0]

Thank you for downloading or updating to version 2.0!

- Converted the render pipeline from OpenGL to DX11!
-- Linux users will now need to build using wine as Proton is not currently supported.
-- This improves performance and stability.

- Controls update:
-- Added vertical panning with Q/E.
-- Added bone selection with rotations and movement.
-- Controls can be rebound in the Keybind settings.
-- Added right click dialog for opening files GFF files in the GFF Viewer.

- Levels:
-- Added .LVL file's to the list of files that can be loaded.
-- Added .RIM file's to the list of files that can be loaded.
-- Added level loading, includes terrain, props, and foliage.
-- Added level exporting
--- Export levels as GLB or FBX. Use the havenarea_import.py to import the levels to blender.
-- No level editing/ importing (please don't ask, it won't happen!).

- Audio:
-- Wrote a new audio player UI.
-- Fixed a bug with audio banks spitting out garbage data.

- UI Updates:
-- Updated the skeleton rendering to be cleaner.
-- Updated UI positions to be cleaner.
-- Added settings -> keybinds to the top toolbar.
-- Rewrote the Gff viewer ~ much clearer and functional UI.
-- Added Gff Value editing ~ fast and efficient.
--- Default save location is in core/override.
-- Added Core/Override to the ERF browser to access mods, load mods, and edit mods.
-- Updated to skip the splashscreen on subsequent loads.
-- Updated the .exe selection to include DAOrigins for EA versions of the game.
-- Added "Add Ons" to the toolbar for extracting the blender importer for levels.

- Importing
-- Added import to override/ embedding in ERF as options when importing

- Bug fixes:
-- Fixed a bug with a rogue separator in the ERF browser.
-- Fixed a bug in audio banks where it spat out garbage data or cut off the file names.
-- Fixed a bug with Awakening .fsb files causing a crash.

- Additional notes:
-- Many things here are WIP. I wanted to get v2.0 out, so expect ongoing updates as I continue to bugfix and refine features.

[Release 2.1]

Hotfix:
- Fixed the clouds in the renderer.
)";

static const char* s_changelogLatest = R"(
[Release 2.2]

- Crash & Stability Fixes
-- D3D Initialization Fallback — App no longer crashes if Graphics Tools aren't installed. Tries hardware+debug → hardware (no debug) → WARP software renderer.
-- Level Texture Loading OOM Fix — Fixed std::bad_alloc crash when loading levels. Stopped storing full RGBA texture copies in RAM alongside GPU textures during level material loading (~4.8GB savings on large levels).
-- Crashlog System Removed — Removed all crashlog() / crashlog_clear() infrastructure, calls, and crash popup MessageBox across all files. Top-level try/catch in main.cpp retained for silent crash prevention.

- Performance
-- Texture/Material Loading Optimization — Rewrote ERF lookup system from O(N×M) linear scans to O(1) hash-based indexing. Added s_erfIndex, s_erfIndexNoExt hash maps and s_texIdCache for texture deduplication. Expected 100–1000× speedup for typical levels.
-- GFF4 Tree Building — Added maxForceDepth limit (depth 3) to prevent full recursive tree expansion on load. Large DLG files (e.g. 4.8MB alistair_main.dlg) now open instantly instead of hanging. Deeper nodes load on-demand when expanded.

- Talktable Loading
-- Removed TLK loading from startup — No longer scans the entire game directory for .tlk files during the splash screen loading phase. TLK strings still load on-demand when opening GFF4 files in the viewer.
-- TLKString now lists the entire string, and doesn't truncate.

- Model Loading Fallback
-- Same-ERF asset resolution — readFromModelErfs, readFromMaterialErfs, and loadTextureByName now fall back to searching state.currentErf (the source ERF) when dedicated model/material/texture ERFs don't contain the needed MMH, MAO, PHY, or DDS files. Enables loading models from non-standard game versions where assets are bundled together.

- UI Changes
-- Override Folder Button — Replaced "Add Ons" menu button with "Override" button in the ERF Browser menu bar. Opens a folder chooser to set a custom override directory.
-- Override Folder Persistence — Selected override folder saved to haventools_settings.ini and restored on next launch.
-- Override Folder Integration — Override Folder selectable in the left pane now uses the user-configured path (falls back to packages/core/override if not set). GFF save dialog defaults to the override folder for modified files.

- Xbox 360 Support
-- Big-Endian GFF4 Parsing — GFF4 parser now detects X360/PS3 platform tags and automatically byte-swaps all numeric fields (uint16, uint32, float). ASCII tag fields (magic, version, platform, fileType) read raw without swapping. Field layout differences (flags/typeId positions) handled per-platform.
-- Big-Endian Model Loading — MSH vertex/index data (positions, normals, UVs, blend weights/indices) and MMH/PHY fields all byte-swap correctly when loading X360 models.
-- XDS Texture Decoding — Added full Xbox 360 .xds texture support (GPU-tiled textures with 52-byte footer). Handles 16-bit endian swap, Morton/Z-order untiling (8×8 block tiles), and decompression for DXT1, DXT3, DXT5, and DXN/BC5 (normal maps with Z reconstruction). Integrated into dds_loader alongside existing DDS/TGA paths without modifying them.
-- XDS-to-DDS Converter — Standalone Python tool (xds_to_dds.py) for batch converting .xds files to standard .dds. Supports directory input and all X360 GPU texture formats.
-- Same-ERF Texture Fallback — X360 bundles MSH, MMH, PHY, and XDS together in one ERF with MAOs in a separate materialobjects.erf. Existing same-ERF fallback from 2.2 covers this layout automatically.
-- No animations.
)";