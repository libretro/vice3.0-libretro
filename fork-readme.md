## The goal of this fork's changes and additions:

Provide a way to fit/fill any monitor of any aspect ratio, size or resolution, with perfectly scaled, non-distorted emulator screens at each platform's correctly viewed aspect ratio - the resulting output should be close to the proportion/dimensions you'd see on a CRT/TV.

Output and features must work with all displays/monitors: 16:9, 16:10, 5:4, 4:3, LCDs, CRTs, etc. Including portrait-rotated displays.

## Features ##
* Preset-based Horizontal & Vertical cropping for C64, C128, Vic-20 and Plus4 cores
  * 3 crop settings for each dimension, including complete border removal
  * Supports PAL & NTSC for every platform with appropriately-selected cropping presets for both Horizontal and Vertical
  * No buggy side-effects of the Display Border Off option

* Manual Vertical Cropping Option for C64 and C128 cores - make the output perfectly fit any game
  * With support for Top and Bottom over-cropping beyond the border to work with widest-possible range of games/titles

* Hot Key to toggle (ON/OFF) the currently set cropping for both Horizontal and Vertical at the same time (all above cores)

* Hot Keys to cycle through all Horizontal and Vertical modes inpendently (All above cores) - includes the Manual cropping option.


![Screenshot](https://github.com/HVR88/Project_Screenshots/blob/master/vice-libretro-cropping_small.png?raw=true)

## Additions & Changes to Codebase ##

* UI & Cropping code added in **libretro-core.c**
* Hot Key toggle & cycling code in **/vice/src/arch/libretro/retrostubs.c**
* UI: Moved Aspect Ratio setting above “Display Border” for a more hierarchical order for the Video Options
* UI: add description to Aspect Ratio
* UI: changed description for Display Borders to warn of performance issues with this feature & recommend Border crop features instead
* UI: Renamed Zoom Mode to "Crop Vertical Borders" for accuracy and to fit with the new border cropping features (internal variable/option names unchanged for compatibility with existing opt files)
* UI: added "Crop Horizontal Borders" above the vertical setting
* UI: The option labels have been renamed to make them clearer: OFF, Small Crop, Medium Crop, Full Crop
* UI: Added “Manual Crop” option value (# 4) to "Crop Vertical Borders" which will take values from two following core options
* UI: added "Manual Top Cropping" option - values in pixels 0-60 - NTSC and PAL “Full” border sizes specially marked
* UI: added "Manual Bottom Cropping”  option - values in pixels 0-60 - NTSC and PAL “Full” border sizes specially marked
* UI: added 2 new hot keys, one for Horizontal crop mode Cycling and one for Vertical crop mode (zoom mode) cycling
* UI: Relabeled Zoom Mode toggle hot key to include “(Horizontal + Vertical)” note
* Shifted the mapper numerical array indexes for the datasette hot keys and increase the mapper array size by 2
* Changed Zoom Mode toggle in retrostubs to remove redundant conditions & include horizontal mode so it toggles H+V at the same time - now it’s possible to do a 2-dimensional zoom on any display
* Retrostubs includes the new cycling code and additions for the cycle hot keys as necessary
* Fixed existing crop dimensions & offsets - borders pixel-exact now when cropped for all platforms
  * Pre-set crop dimensions for c64/128 selected & tested against large library of game content & for acceptable fit into standard monitor dimensions/orientations
  * Pre-set crop dimensions for xplus4 selected to match results of c64
  * Pre-set crop dimensions for Vic-20 selected based on previous values from Zoom Mode

Requirements: **Retroarch settings: Video settings -> Scaling -> Aspect Ratio: Core Provided + Integer Scaling: OFF**

***
The new options have already been adopted by the emulation community and a large collection of games have already been profiled to provide the optimal crop values to work on any display in the *C64 Dreams Curated Collection:* https://forums.launchbox-app.com/topic/49324-c64-dreams-massive-curated-c64-collection

**What others have said**

*"now 100% of the cropping is handled in the core itself, [RetroArch] video settings are untouched. This is a godsend because now it should "just work" for everyone. Equally important is that now that I can adjust these crops by single pixels, they're more exact than the previous pre-defined zoom levels."*

*"I'll be honest, I wish every core had this option. I would use it. At the very least Beetle PSX and Beetle Saturn could really use it.Thank you for doing this! It resolves probably the single biggest issue with this project, so that's a major boon."*

