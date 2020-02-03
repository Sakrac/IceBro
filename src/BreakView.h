#pragma once
#include <stdint.h>
struct UserData;

struct BreakView
{
	BreakView();
	void WriteConfig(UserData & config);
	void ReadConfig(strref config);
	void Draw();

	bool open;
};

