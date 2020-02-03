#pragma once
// Manages breakpoint window and synchronizing vice in monitor mode
#include <stdint.h>

enum ViceBPType : unsigned int {
	VBP_Break,
	VBP_WatchStore,
	VBP_WatchRead
};

struct ViceBP {
	uint16_t address;
	uint16_t end;	// for ranged breaks
	int viceIndex : 13;
	ViceBPType type : 2;
	bool disabled : 1;
};

void ResetViceBP();
void SetViceBP( uint16_t address, uint16_t end, int index, bool add, ViceBPType type, bool disabled, bool updateFromVice = false );
int GetViceBPIndex( uint16_t addr, ViceBPType type );

bool HasViceBP( uint16_t addr, ViceBPType type );

// for breakpoint view
const ViceBP* GetBreakpoints();
int GetNumBreakpoints();
