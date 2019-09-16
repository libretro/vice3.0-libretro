# Vice 3.3 LIBRETRO

Port of VICE 3.3 for libretro with nuklear GUI virtual keyboard

Supported platforms : Linux Win64 Android emscripten Switch Vita Apple

Source base: 
https://sourceforge.net/projects/vice-emu/files/releases/vice-3.3.tar.gz

## Recent improvements

- Savestates (disabled during netplay and rerecording because of temp file creation)
- Vice 3.3 update
- x64sc / xpet
- Virtual keyboard revamped: more responsive, cleaner design, much easier to control
- Other GUI removed
- Autostart PRG mode to inject
- System-subdir renamed to vice and created automatically
- Keyrah joystick maps 
- C64 joystick disabled while using virtual keyboard
- Cursor keys disabled while using C64 joystick
- New settings:
  - Drive Sound Emulation + Volume
  - Reset type (autostart, soft, hard)
  - Customizable keymaps for essential functions (virtual keyboard, statusbar, joyport switch, reset, datasette controls)
  - Analog sticks mappable
  - Virtual keyboard themes

## To Do List

- Proper savestates that work without requiring temp file creation in Retroarch save dir.

## Joyport control via Filename

C64 games sometimes use joystick port 1 and sometimes joystick port 2 for player 1. There are several ways to switch ports in this core:
- Use the core option for it: `Quickmenu->Options->Controller 0` 
- Bring up the virtual keyboard with L shoulder button, and press the key labeled `Joy` there 
- Re-name your games, eg. `Bruce_Lee_j1.tap` or `Bruce_Lee_(j1).tap` for port 1, and similarly `Bruce_Lee_j2.tap` or `Bruce_Lee_(j2).tap` for port 2. Note this only works with extracted files (`.PRG`, `.D64` etc.) not with zip files. 

## M3U Support and Disk control
When you have a multi disk game, you can use a m3u file to specify each disk of the game and change them from the RetroArch Disk control interface.

A M3U file is a simple text file with one disk per line (see https://en.wikipedia.org/wiki/M3U).

Example :

Ultima VI - The False Prophet (1990)(Origin Systems).m3u
```
Ultima VI - The False Prophet (1990)(Origin Systems)(Disk 1 of 3 Side A)(Game).d64
Ultima VI - The False Prophet (1990)(Origin Systems)(Disk 1 of 3 Side B)(Surface).d64
Ultima VI - The False Prophet (1990)(Origin Systems)(Disk 2 of 3 Side A)(Dungeon).d64
Ultima VI - The False Prophet (1990)(Origin Systems)(Disk 2 of 3 Side B)(Populace A).d64
Ultima VI - The False Prophet (1990)(Origin Systems)(Disk 3 of 3 Side A)(Populace A).d64
Ultima VI - The False Prophet (1990)(Origin Systems)(Disk 3 of 3 Side B)(Populace A).d64
```
Path can be absolute or relative to the location of the M3U file.

When a game ask for it, you can change the current disk in the RetroArch 'Disk Control' menu :
- Eject the current disk with 'Disk Cycle Tray Status'.
- Select the right disk index.
- Insert the new disk with 'Disk Cycle Tray Status'.

Note : zip support is provided by RetroArch and is done before passing the game to the core. So, when using a m3u file, the specified disk image must be uncompressed (.d64 file formats).

## Command file operation

Vice command line options are supported with Retroarch by by placing the desired command line in a text file with a .cmd file extension. The command line format is as documented in the Vice documentation (see http://vice-emu.sourceforge.net/vice_6.html).

Using this you can overcome limitations of the gui and set advanced configurations required for running problematic files. Here are a couple of examples for the xvic core  ...

Vic20 megacart support:
```
xvic -cartmega /path/to/rom/mega-cart-name.rom
```
Vic20 games which require specific memory configs such as 8k expansion:
```
xvic -memory 1 /path/to/rom/some-8k-game.d64
```

## Build instructions

LIBDIR set to retro_system_dir

Add WANT_RGB565=1 to make for RGB565 16 bits depth (32 bits depth default)

### Linux
```
CC=gcc make -f Makefile.libretro -j10
```
### Win64 (crossbuild)
```
make -f Makefile.libretro platform=wincross64 -j4
```
### Android standalone toolchain Build
```
export path to your standalone toolchain like

export PATH=$PATH:/opt/standtc/bin

make -f Makefile.libretro platform=androidstc -j4
```
### Android ndk Build
```
cd jni
ndk-build
```
## Original readme

 ----------------------------------------------------------------------------

  VICE  3.3                                                        Dec 2018


                 _______________
                |      ||      ||
                |      ||      ||____________________________
                |      ||      ||    ||         ||          ||
                |      ||      ||    ||    |    ||    ______||
                |      ||      ||    ||    |----||          ||
                |      ||      ||    ||    |    ||    ------||
                |              ||____||_________||__________||
                 \            // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                  \          //
                   \________//   Versatile Commodore 8-bit Emulator



 ----------------------------------------------------------------------------

 This release stabilizes and introduces the GTK3 UI as the primary user 
 interface for all major ports. For "lesser" platforms we still have the SDL 
 based interface. All other user interfaces have been removed. RIP

 We are still looking for dedicated maintainers for the Windows- and macOS 
 ports, if you want to help improving these and/or want to provide binaries, 
 please get in touch.

 ----------------------------------------------------------------------------

 This is version 3.3 of VICE, the multi-platform C64, C128, VIC20,
 PET, PLUS4 and CBM-II emulator.  This version can be compiled for
 MSDOS, Win32, OS/2, BeOS, QNX 4.x, QNX 6.x, AmigaOS, Dingoo,
 Syllable, SkyOS and for most Unix systems provided with
 the X Window System version 11, R5 or later.

 The following programs are included:

   - x64, a C64 emulator;

   - x64sc, a C64 emulator focused on accuracy;

   - xscpu64, a C64 emulator with a SuperCPU cart;

   - x64dtv, a C64 Direct-to-TV (DTV) emulator;

   - x128, a C128 emulator;

   - xvic, a VIC20 emulator;

   - xpet, a PET emulator;

   - xplus4, a PLUS4 emulator;

   - xcbm2, a CBM-6x0/7x0 emulator;

   - xcbm5x0, a CBM-5x0 emulator;

   - vsid, a SID player;

   - c1541, a stand-alone disk image maintenance utility;

   - petcat, a CBM BASIC de-tokenizer;

   - cartconv, a C64/C128 cartridge conversion program.

 Moreover, the following documents are provided in both source and
 binary distributions:

   - README, this file;

   - COPYING, the GNU General Public License under which VICE is
     distributed -- *please read it before using the program*;

   - INSTALL, installation instructions;

   - NEWS, list of user-visible changes between this and older versions of VICE;

   - FEEDBACK, something you should read before reporting bugs or sending mail 
     to any of the authors;

 The following documents are provided in the source distribution:

   - in the doc directory:

     - vice.texi, the complete documentation; documentation in various different
       formats may be generated from it (vice.txt, vice.pdf, vice.chm, vice.hlp, 
       vice.guide, vice.inf, vice.info)
     - iec-bus.txt, overview of the VICE IEC bus emulation;
     - CIA-README.txt, overview of the VICE CIA emulation;
     - coding-guidelines.txt, description of VICE coding style
     - Documentation-Howto.txt, information on how to edit the documentation
     - Doxygen-Howto.txt, how to generate doxygen documentation
     - Release-Howto.txt, some hints and reminders on how to create a release
       tarball (for maintainers)
     - ffmpeg-support.txt, this document details the results of test compiles 
       with regards to the included ffmpeg library.
     - SDL-support.txt, this document details the results of test compiles with 
       regards to the SDL 1 and SDL 2 support.
     - hardware-sids.txt, this document details the hardware SIDs and how they 
       are supported on the various platforms.

   - in the doc/readmes directory:

     - Readme-GTK3.txt, GTK3 specific documentation;
     - Readme-SDL.txt, SDL specific documentation;
     - Readme-SDL2.txt, SDL2 specific documentation;

   - in the doc/building directory:

     - Windows-MinGW-GTK3-Howto.txt, how to compile the Windows port using MingGW/MSYS2
     - Linux-GTK3-Howto.txt, how to compile the GTK3 port on Linux
     - GTK3-macOS.txt, how to compile the Mac OSX port with GTK3 UI
     - SDL-Howto.txt, how to compile the SDL port on various platforms
     - Android-Howto, how to compile the Android SDL port
     - FreeBSD-GTK3-Howto.txt, hints on compiling the GTK3 port on FreeBSD
     - NetBSD-howto.txt, hints on compiling for NetBSD

 For the latest news, have a look at the VICE home page:

     http://vice-emu.sourceforge.net/

 Also some information that was previously included in the distribution has been
 moved into our wiki at sourceforge:

   - The TODO list,
     http://vice-emu.sourceforge.net/wiki/index.php/Todo#New_Features

   - The list of known BUGS,
     http://vice-emu.sourceforge.net/wiki/index.php/Todo#Known_Issues

 New versions are made available quite often, so please stay tuned.


 Have fun!


 Copyright notice
 ----------------

 VICE, the Versatile Commodore Emulator

    Core Team Members:
    2005-2018 Marco van den Heuvel
    2007-2018 Fabrizio Gennari
    2009-2018 Groepaz
    2010-2018 Olaf Seibert
    2011-2018 Marcus Sutton
    2011-2018 Kajtar Zsolt
    2016-2018 AreaScout
    2016-2018 Bas Wassink
    2017-2018 Michael C. Martin

    Inactive/Ex Team Members:
    1999-2017 Martin Pottendorfer
    2011-2016 Stefan Haubenthal
    2015-2016 BSzili
    1999-2016 Andreas Matthies
    2007-2015 Daniel Kahlin
    2009-2015 Errol Smith
    2012-2014 Benjamin 'BeRo' Rosseaux
    2011-2014 Ulrich Schulz
    2011-2014 Thomas Giesel
    2009-2014 Ingo Korb
    2008-2014 Antti S. Lankila
    2006-2014 Christian Vogelgsang
    1998-2014 Dag Lem
    2000-2011 Spiro Trikaliotis
    2007-2011 Hannu Nuotio
    1998-2010 Andreas Boose
    1998-2010 Tibor Biczo
    2007-2010 M. Kiesel
    1999-2007 Andreas Dehmel
    2003-2005 David Hansel
    2000-2004 Markus Brenner
    1999-2004 Thomas Bretz
    1997-2001 Daniel Sladic
    1996-2001 André Fachat
    1996-1999 Ettore Perazzoli
    1993-1994, 1997-1999 Teemu Rantanen
    1993-1996 Jouko Valta
    1993-1994 Jarkko Sonninen

    Translation Team Members:
    2009-2017 Mikkel Holm Olsen
    2000-2017 Martin Pottendorfer
    2011-2017 Manuel Antonio Rodriguez Bas
    2004-2017 Paul Dubé
    2006-2017 Czirkos Zoltan
    2006-2017 Karai Csaba
    2001-2017 Andrea Musuruane
    2011-2016 Jesse Lee
    2005-2017 Marco van den Heuvel
    2011-2017 Jarek Sobolewski
    2010-2017 Michael Litvinov
    2000-2017 Peter Krefting
    2008-2017 Emir Akaydin

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
  02111-1307  USA

 The ROM files in the `C128', `C64', `CBM-II', `DRIVES', `PET', `PLUS4'
 `PRINTER' and `VIC20' directories are Copyright C by Commodore
 Business Machines.

 The ROM files in the `C64DTV` directory are Copyright C by Commodore
 Business Machines, as well as Mammoth Toys, a division of nsi ltd.,
 Digital Concepts DC studios inc., Ironstone Partners ltd., and
 Toy:Lobster company ltd.

