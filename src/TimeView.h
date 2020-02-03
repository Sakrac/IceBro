#pragma once
#include <stdint.h>
#include "struse\struse.h"
struct UserData;

struct TimeView {
	TimeView();
	void WriteConfig(UserData & config);
	void ReadConfig(strref config);
	uint32_t cursor;
	bool open;
	void Draw();
};

