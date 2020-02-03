#pragma once

#include "ViceView.h"

extern float fontCharWidth;
extern float fontCharHeight;

void FocusPC();
void UpdateMainWindowWidthHeight( int width, int height );
int GetMainWindowWidthHeight( int *width );
void ViewsWriteConfig( UserData& config );
void ViewsReadConfig( strref config );
void SelectFont( int index );
uint8_t InputHex();
void InitViews();
void DrawViews();
void SaveViews();
void LoadViews();
