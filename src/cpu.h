//============================================================================
// Name        : mos6502 
// Author      : Gianluca Ghettini
// Version     : 1.0
// Copyright   : 
// Description : A MOS 6502 CPU emulator written in C++
//============================================================================

#include <stdint.h>

#include "machine.h"

// read/write callbacks
typedef void(*CBSetByte)(uint16_t, uint8_t);
typedef uint8_t(*CBGetByte)(uint16_t);

Regs Step6502(const Regs &r, CBGetByte read, CBSetByte write);
Regs IRQ6502(const Regs &r, CBGetByte read, CBSetByte write);
Regs NMI6502(const Regs &r, CBGetByte read, CBSetByte write);
Regs Reset6502(const Regs &r, CBGetByte read, CBSetByte write);

