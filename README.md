# sadx-edit-mode-restore

This is the debug object editor seen in various prototypes (AutoDemo, SADX:Preview, SA2 May 7) decompiled from the AutoDemo's code and ported to the PC version.
Controls can be found on [TCRF](https://tcrf.net/Proto:Sonic_Adventure_(Dreamcast)/AutoDemo#SetEdit).

There are some bugs, mainly around placing objects in certain areas. I think changes the devs made after the AutoDemo are causing problems with where it writes new objects.</br>
Here are the places I know are crash-happy at the moment:
- Speed Highway 3
- Red Mountain 1
- Emerald Coast 2 (Sometimes)
- Twinkle Park (On act transition if you've caused the object count to go higher than it usually is)

The original code used Camera and Shadow code that differs to the final game. I used the final equivalents for them, but the camera can sometimes get stuck after leaving the editor and MULTI fires additional objects like a cannon instead of holding them above the cursor. (That part was a complete mess in the disassembly - I don't even think the devs even used it considering they disabled it in SA2.)

I also changed some stuff in other functions that the editor needed:
- njPrintH, used for the editor's angle display and a few objects' info, now prints correctly.
- OnEdit is no longer empty and behaves the same way as it does in the AutoDemo, checking if an object is the one selected by the editor.
- ProcessStatusTable now loads the editor-compatible ProcessStatusTable2P while using the editor, so objects can spawn when the camera moves away from the player.
