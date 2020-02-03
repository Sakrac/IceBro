#pragma once

#include <inttypes.h>

struct block6502 {
	uint16_t address, size;
	uint8_t *data;
};

int GetBlocks6502(struct block6502 **ppBlocks);

extern unsigned char _fruitFont[];
extern unsigned char _aStartupScreen[];
extern char _aStartupScreenFruit[];


