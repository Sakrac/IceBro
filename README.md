# IceBro

## Todo
* Documentation
* Debuggable Example
* C64 Current Mode with Sprites
* Rework the draw call for code view
* Rework the draw call for memory view
* Replace text loading code with struse
* Vic 20 screen modes
* Label View
* Look at possibility of loading vice snapshots link: http://vice-emu.sourceforge.net/vice_9.html

![IceBro Toolbar](images/toolbar.png)

## What is IceBro
At the core IceBro consists of a 6502 simulator with a graphical debugger around it.
Programs can be loaded into the simulator memory and executed but no other hardware
is simulated within the debugger.

To work with VICE the debugger can connect to a running
instance of VICE (C64 or Vic20) and copy the machine state
(RAM, CPU registers, labels and breakpoints)
when its monitor mode is enabled.

This means that the debugger is primarily looking at
the copy of the machine state,
and most debugging happens in the copied state.
The vice console commands still works with the VICE machine state.

This differs from a debugger that is working
directly with the machine state and
IceBro is not intended as a replacement for such debuggers.

### Name

Ice normally stands for an in-circuit emulator, or replacement
CPU for a system to allow more debugging. This isn't exactly that
but it is a cool concept. Bro relates to the connection to VICE.

## Acknowledgements

---

This tool is built using the Docking Branch of Dear ImGui (https://github.com/ocornut/imgui)

---

VICE is a program that runs on a Unix, MS-DOS, Win32, OS/2, BeOS, QNX 4.x,
QNX 6.x, Amiga, Syllable or Mac OS X machine and executes programs intended for the old 8-bit computers. The current version emulates the C64, the C64DTV, the C128, the VIC20, practically all PET models, the PLUS4 and the CBM-II (aka C610/C510). An extra emulator is provided for C64 expanded with the CMD SuperCPU.

---

Fonts in this package are(c) 2010 - 2014 Style.

This license is applicable to each font file included in this package in all their variants(ttf, eot, woff, woff2, svg).

You MAY NOT :
* sell this font; include / redistribute the font in any font collection regardless of pricing;
* provide the font for direct download from any web site, modify or rename the font.

You MAY :
* link to "http://style64.org/c64-truetype" in order for others to download and install the font;
* embed the font(without any modification or file renaming) for display on any web site using @font-face rules;
* use this font in static images and vector art;
include this font(without any modification or file renaming) as part of a software package but ONLY if said software package is freely provided to end users.
* You may also contact us to negotiate a(possibly commercial) license for your use outside of these guidelines at "http://style64.org/contact-style".
* At all times the most recent version of this license can be found at "http://style64.org/c64-truetype/license".

----

## Features

* Visualize the machine state in various views
* Run, step over, step into code, issue interrupts, etc.
* Step and run code backwards in roughly 5 million undo steps.
* Edit code and memory and update VICE state if connected
* Sync breakpoints, watch points and trace points with VICE
* Vice Console with extra commands to control the connection
* Visualize graphics in memory in a variety of ways


# Getting Started

You should be familiar with VICE C64 and how to use the built-in monitor
to get the most out of IceBro. The idea is to visualize things rather than
changing how to do things, to make debugging more effective rather than
dumb it down.

### Running an example without connecting to VICE

* Start the program and click the LOAD icon  ![LOAD](images/toolbar_load.png)  in the toolbar, then load **IceBro.prg** from the **Example** folder.
* The "Load Binary" dialog should default to a **C64 PRG file** which is what you want,
so click **OK** to load it in.
* The debugger will identify that it is a basic program with a **SYS** command
and set the initial PC accordingly (if loading a **PRG** file to another memory location it will assume the load address is the start address).
* At this point you can step through the program with F10 (Step Over), F11 (Step Into) or just hit F5 to run it. Press the Pause Icon in the Toolbar to stop running.
* To check out source level debugging you can click **FILE** / **LOAD LISTING** from the drop down menu of the tool, select **Example/IceBro.lst** then in a code view enable the **SOURCE** checkbox.

### Running with VICE

* Start IceBro if you don't have it running already
* Run VICE with -remotemonitor and start IceBro.prg
* While VICE is starting click the VICE Connect icon once ![VICE](images/toolbar_vice.png) in IceBro
* At any time when the program has loaded press the Pause icon ![PAUSE](images/toolbar_pause.png) to enter monitor mode and IceBro will automatically sync the machine state from VICE.
* To return VICE to running mode just type X in the monitor, just like a VICE integrated monitor window.
* To exit VICE without exiting IceBro just type QUIT into the VICE Console window at any time it is connected.
* To disconnect IceBro from VICE press the VICE Connect icon again.


# Keyboard Reference
* Tab - Move current code window cursor to PC
* Shift+Tab - Set current code window cursor to PC
* F5 - Go
* F6 - Run to cursor in current code window
* F9 - Place breakpoint at cursor in current code window
* F10 - Step over
* F11 - Step into
* Shift+F5 Go reverse
* Shift+F10 Step over backwards
* Shift+F11 Step back
* Page up/down - Move several lines up/down in current code / memory window
* Cursor up/down - Move code cursor up/down in current code window
