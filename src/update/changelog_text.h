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
)";

static const char* s_changelogLatest = R"(
[Release 2.0]

Thank you for downloading or updating to version 2.0!

- Converted the render pipeline from OpenGL to DX11!
-- Linux users will now need to build using wine as Proton is not currently supported.
-- This improves performance and stability.

- Controls update:
-- Added vertical panning with Q/E.
-- Controls can be rebound in the Keybind settings.

- Additional formats:
-- Added .LVL file's to the list of files that can be loaded.
-- Added .RIM file's to the list of files that can be loaded.
-- Added terrain rendering in .LVL files (part of the single player resources pack)
-- Added level explorer -> load and explore the game's levels.

- UI Updates:
-- Updated the skeleton rendering to be cleaner.
-- Updated UI positions to be cleaner.
-- Added settings -> keybinds to the top toolbar.
-- Updated the audio playback to use a cleaner UI.
-- Rewrote the Gff viewer ~ much clearer and functional UI.
-- Added Gff Value editing ~ fast and efficient.
--- Default save location is in core/override.
-- Added Core/Override to the ERF browser to access mods, load mods, and edit mods.

- Bug fixes:
-- Fixed a bug with a rogue separator in the ERF browser.
-- Fixed a bug in audio banks where it spat out garbage data or cut off the file names.
-- Fixed a bug with Awakening .fsb files causing a crash.

)";