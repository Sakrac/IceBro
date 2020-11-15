#pragma once
#include <stdint.h>
#include "struse\struse.h"
struct UserData;

struct GfxView
{
	enum System {
		Generic,
		C64,
		Apple2
	};

	enum Mode {
		Planar,
		Columns,
		C64_Bitmap,
		C64_ColBitmap,
		C64_Sprites,
		C64_Text,
		C64_ExtText,
		C64_Text_MC,
		C64_MCBM,
		C64_ColumnScreen_MC,
		C64_Current,
		Apl2_Text,
		Apl2_Hires,
		Apl2_HR_Col
	};

	enum Zoom {
		Zoom_1x1,
		Zoom_2x2,
		Zoom_4x4,
		Zoom_FitX,
		Zoom_FitY,
		Zoom_FitWindow
	};

	char address_screen[64];
	char address_gfx[64];
	char address_col[64];
	char columns_str[64];
	char rows_str[64];

	uint32_t addrScreenValue;
	uint32_t addrGfxValue;
	uint32_t addrColValue;
	uint32_t columns;
	uint32_t rows;

	int zoom;
	int displaySystem;
	int displayMode;
	int genericMode;
	int c64Mode;
	int apple2Mode;

	uint8_t* bitmap;
	size_t bitmapSize;

	ImTextureID texture;

	bool open;
	bool reeval;

	bool color;
	bool multicolor;

	GfxView();

	void WriteConfig( UserData & config );

	void ReadConfig( strref config );

	void Draw( int index );
	void Create8bppBitmap();

	void CreatePlanarBitmap(uint32_t* dst, int lines, uint32_t width, const uint32_t* palette);
	void CreateColumnsBitmap(uint32_t* dst, int lines, uint32_t width, const uint32_t* palette);

	void CreateC64BitmapBitmap(uint32_t* dst, const uint32_t* palette, uint16_t bitmap, uint32_t cl, uint32_t rw);
	void CreateC64ColorBitmapBitmap(uint32_t* dst, const uint32_t* palette, uint16_t bitmap, uint16_t screen, uint32_t cl, uint32_t rw);
	void CreateC64ExtBkgTextBitmap(uint32_t* dst, const uint32_t* palette, uint16_t bitmap, uint16_t screen, uint16_t cm, uint32_t cl, uint32_t rw);
	void CreateC64TextBitmap(uint32_t* dst, const uint32_t* palette, uint32_t cl, uint32_t rw);
	void CreateC64ColorTextBitmap(uint32_t * d, const uint32_t * pal, uint16_t bitmap, uint16_t screen, uint16_t cm, uint32_t cl, uint32_t rw);
	void CreateC64MulticolorTextBitmap(uint32_t* dst, const uint32_t* palette, uint16_t bitmap, uint16_t screen, uint16_t cm, uint32_t cl, uint32_t rw);
	void CreateC64MulticolorBitmapBitmap(uint32_t* dst, const uint32_t* palette, uint16_t bitmap, uint16_t screen, uint16_t cm, uint32_t cl, uint32_t rw);
	void CreateC64SpritesBitmap(uint32_t* dst, int lines, uint32_t width, const uint32_t* palette);
	void CreateC64ColorTextColumns(uint32_t* d, const uint32_t* pal, uint16_t bitmap, uint16_t screen, uint16_t colorAddr, uint32_t cl, uint32_t rw);
	void CreateC64CurrentBitmap(uint32_t* d, const uint32_t* pal);

	void CreateApple2TextBitmap(uint32_t* dst, int lines, uint32_t width, const uint32_t* palette);
	void CreateApple2HiresBitmap(uint32_t* dst, int lines, uint32_t width, const uint32_t* palette);
	void CreateApple2HiresColorBitmap(uint32_t* dst, int lines, uint32_t width, const uint32_t* palette);
};

