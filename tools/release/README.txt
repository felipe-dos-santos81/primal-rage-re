Primal Rage (DOS, 1995) — native port
=====================================

This download does NOT contain the game. You need your own copy of
Primal Rage for DOS: the port runs the game from your files.

How to play
-----------
1. Unzip this download anywhere.
2. Run the game:
     Windows:  prageport.exe
     macOS:    Primal Rage.app
     Linux:    ./prageport
3. The first time, a folder chooser opens. Choose the folder where
   Primal Rage is installed (the one that holds PRAGE.EXE).
   If your copy is a DOSBox bundle, choosing the bundle's folder is
   fine: the game's C folder inside it is found.
   If you only have the CD, copy the contents of the CD's RAGE.S16
   folder, and FAT.OPL from its RAGE.SND folder, into one folder and
   choose that.
4. The folder is remembered; next time the game starts straight away.
   If the folder moves, the chooser opens again.

The chooser checks the folder. It tells you if a game file is missing,
if PRAGE.EXE and INDEX need renaming to upper case, or if your
PRAGE.EXE is not the version this port supports.

Controls
--------
The original game's keyboard layout:
  Player 1:  S X Z C (up, down, left, right), U I (punch/bite), N M (kick/tail)
  Player 2:  the arrow keys, Home PgUp (punch/bite), End PgDn (kick/tail)
  Enter starts/selects, Esc goes back; F1/F2 are the start buttons.
Press F12 to let the arrow keys move Player 1 instead (the window title says
"arrows: Player 1"); press F12 again to go back. The choice is remembered.
Keys can be changed on the MODIFY CONTROLS screen of the OPTIONS menu.

Warnings on first launch (the app is not signed)
------------------------------------------------
macOS:    open "Primal Rage.app" once; when macOS blocks it, go to
          System Settings > Privacy & Security and click "Open Anyway".
          (macOS 14 and earlier: right-click the app and choose Open.)
          Or, in Terminal: xattr -dr com.apple.quarantine "Primal Rage.app"
Windows:  if SmartScreen appears, click "More info", then "Run anyway".

Saves and settings
------------------
Saves and the remembered folder live in your per-user folder:
  macOS:    ~/Library/Application Support/primal-rage/
  Windows:  %APPDATA%\primal-rage\
  Linux:    ~/.local/share/primal-rage/

Command line
------------
  prageport --game-dir <folder>   use this folder (skips the chooser)
  prageport --cmos <file>         use this save file

Licence
-------
The port is MIT licensed (LICENSE). It includes the Opal OPL emulator
(MIT, LICENSE.opal.txt) and SDL3 (zlib, LICENSE.SDL3.txt).
Primal Rage itself is not included and remains the property of its
rights holders.
