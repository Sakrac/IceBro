// example for creating a texture:
// static void ImGui_ImplDX11_CreateFontsTexture()
#include <stdio.h>
#include "imgui\ImGui.h"
#include "Views.h"
#include "machine.h"
#include "GfxView.h"
#include "Image.h"
#include "Expressions.h"
#include "Config.h"
#include "ViceConnect.h"

extern unsigned char _fruitFont[];
extern unsigned char _aStartupFont[];

static const char a2c_lookup[] = {
	0, 0, 0, 0, 2, 2, 3, 3,
	0, 0, 1, 1, 3, 3, 3, 3,
	0, 0, 0, 0, 2, 2, 3, 3,
	0, 0, 1, 0, 3, 3, 3, 3,
	0, 0, 0, 0, 1, 1, 3, 3,
	0, 0, 0, 0, 1, 1, 3, 3,
	0, 0, 2, 2, 3, 3, 3, 3,
	0, 0, 2, 0, 3, 3, 3, 3,
};

static const char a2c_colors[] = {
	3, 5, 6, 4, 3, 7, 8, 4
};

static GfxView::Mode sGenericModes[] = { GfxView::Planar, GfxView::Columns };
static GfxView::Mode sC64Modes[] = { GfxView::C64_Bitmap, GfxView::C64_ColBitmap,
									 GfxView::C64_Sprites, GfxView::C64_Text,
									 GfxView::C64_ExtText, GfxView::C64_Text_MC,
									 GfxView::C64_MCBM, GfxView::C64_ColumnScreen_MC,
									 GfxView::C64_Current };
static GfxView::Mode Apple2Modes[] = { GfxView::Apl2_Text, GfxView::Apl2_Hires, GfxView::Apl2_HR_Col };

#define ColRGBA( r, g, b, a ) uint32_t((a<<24)|(b<<16)|(g<<8)|(r))
uint32_t c64pal[16] = {
	ColRGBA(0,0,0,255),
	ColRGBA(255,255,255,255),
	ColRGBA(136,57,50,255),
	ColRGBA(103,182,189,255),
	ColRGBA(139,63,150,255),
	ColRGBA(85,160,73,255),
	ColRGBA(64,49,141,255),
	ColRGBA(191,206,114,255),
	ColRGBA(139,84,41,255),
	ColRGBA(87,66,0,255),
	ColRGBA(184,105,98,255),
	ColRGBA(80,80,80,255),
	ColRGBA(120,120,120,255),
	ColRGBA(148,224,137,255),
	ColRGBA(120,105,196,255),
	ColRGBA(159,159,159,255)
};

void GfxView::WriteConfig(UserData& config)
{
	config.AddValue(strref("open"), config.OnOff(open));
	config.AddValue(strref("addressScreen"), strref(address_screen));
	config.AddValue(strref("addressChars"), strref(address_gfx));
	config.AddValue(strref("columns_str"), strref(columns_str));
	config.AddValue(strref("rows_str"), strref(rows_str));
	config.AddValue(strref("mode"), displayMode);
	config.AddValue(strref("system"), displaySystem);
	config.AddValue(strref("genericMode"), genericMode);
	config.AddValue(strref("c64Mode"), c64Mode);
	config.AddValue(strref("apple2Mode"), apple2Mode);
	config.AddValue(strref("zoom"), zoom);
	config.AddValue(strref("columns"), columns);
	config.AddValue(strref("rows"), rows);
}

void GfxView::ReadConfig(strref config)
{
	ConfigParse conf(config);
	while (!conf.Empty()) {
		strref name, value;
		ConfigParseType type = conf.Next(&name, &value);
		if (name.same_str("open") && type == CPT_Value) {
			open = !value.same_str("Off");
		} else if (name.same_str("addressScreen") && type == CPT_Value) {
			strovl addr_scrn_str(address_screen, sizeof(address_screen));
			addr_scrn_str.copy(value);
			addrScreenValue = ValueFromExpression(addr_scrn_str.c_str());
			reeval = true;
		} else if (name.same_str("addressChars") && type == CPT_Value) {
			strovl addr_gfx_str(address_gfx, sizeof(address_gfx));
			addr_gfx_str.copy(value);
			addrGfxValue = ValueFromExpression(addr_gfx_str.c_str());
			reeval = true;
		} else if (name.same_str("addressColor") && type == CPT_Value) {
			strovl addr_col_str(address_col, sizeof(address_col));
			addr_col_str.copy(value);
			addrColValue = ValueFromExpression(addr_col_str.c_str());
			reeval = true;
		} else if (name.same_str("columns_str") && type == CPT_Value) {
			strovl col_str(columns_str, sizeof(columns_str));
			col_str.copy(value);
			columns = ValueFromExpression(col_str.c_str());
			reeval = true;
		} else if (name.same_str("rows_str") && type == CPT_Value) {
			strovl row_str(rows_str, sizeof(rows_str));
			row_str.copy(value);
			rows = ValueFromExpression(row_str.c_str());
			reeval = true;
		} else if (name.same_str("mode") && type == CPT_Value) {
			displayMode = (int)value.atoi();
			reeval = true;
		} else if (name.same_str("system") && type == CPT_Value) {
			displaySystem = (int)value.atoi();
			reeval = true;
		} else if (name.same_str("genericMode") && type == CPT_Value) {
			genericMode = (int)value.atoi();
			reeval = true;
		} else if (name.same_str("c64Mode") && type == CPT_Value) {
			c64Mode = (int)value.atoi();
			reeval = true;
		} else if (name.same_str("apple2Mode") && type == CPT_Value) {
			apple2Mode = (int)value.atoi();
			reeval = true;
		} else if (name.same_str("zoom") && type == CPT_Value) {
			zoom = (int)value.atoi();
			reeval = true;
		} else if (name.same_str("columns") && type == CPT_Value) {
			columns = (int)value.atoi();
			reeval = true;
		} else if (name.same_str("rows") && type == CPT_Value) {
			rows = (int)value.atoi();
			reeval = true;
		}
	}
}

void GfxView::Draw(int index)
{
	if (!open) { return; }
	{
		strown<64> title("Screen");
		title.append_num(index + 1, 1, 10);

		ImGui::SetNextWindowPos(ImVec2(400, 150), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(520, 200), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin(title.c_str(), &open)) {
			ImGui::End();
			return;
		}
	}

	bool redraw = false;
	{
		strown<32> name("gfxSetupColumns");
		name.append_num(index + 1, 1, 10);
		ImGui::Columns(4, name.c_str(), true);  // 4-ways, no border
		name.copy("system##");
		name.append_num(index + 1, 1, 10);
		ImGui::Combo(name.c_str(), &displaySystem, "Generic\0C64\0Apple2\0\0");
		ImGui::NextColumn();

		int prevMode = displayMode;
		if (displayMode != C64_Current) {
			name.copy("screen##");
			name.append_num(index + 1, 1, 10);
			if (ImGui::InputText(name.c_str(), address_screen, sizeof(address_screen))) {
				addrScreenValue = ValueFromExpression(address_screen);
				redraw = true;
			}
			ImGui::NextColumn();
			name.copy("chars##");
			name.append_num(index + 1, 1, 10);
			if (ImGui::InputText(name.c_str(), address_gfx, sizeof(address_gfx))) {
				addrGfxValue = ValueFromExpression(address_gfx);
				redraw = true;
			}
			ImGui::NextColumn();
			name.copy("color##");
			name.append_num(index + 1, 1, 10);
			if (ImGui::InputText(name.c_str(), address_col, sizeof(address_col))) {
				addrColValue = ValueFromExpression(address_col);
				redraw = true;
			}
			ImGui::NextColumn();
		} else {
			ImGui::NextColumn();
			ImGui::NextColumn();
			ImGui::NextColumn();
		}

		name.copy("gfxSetupNext");
		name.append_num(index + 1, 1, 10);
		ImGui::Columns(4, name.c_str(), true);  // 4-ways, no border
		name.copy("mode##");
		name.append_num(index + 1, 1, 10);
		switch (displaySystem) {
		case Generic:
			ImGui::Combo(name.c_str(), &genericMode, "Planar\0Columns\0\0");
			displayMode = sGenericModes[genericMode];
			break;
		case C64:
			ImGui::Combo(name.c_str(), &c64Mode, "Bitmap\0Col Bitmap\0Sprites\0Text\0ExtText\0Text MC\0MCBM\0Colmn Text MC\0Current\0\0");
			displayMode = sC64Modes[c64Mode];
			break;
		case Apple2:
			ImGui::Combo(name.c_str(), &apple2Mode, "Apl2 Text\0Apl2 Hires\0Apl2 Col\0\0");
			displayMode = Apple2Modes[apple2Mode];
			break;
		}
		if (prevMode != displayMode) { redraw = true; }
		ImGui::NextColumn();
		name.copy("zoom##");
		name.append_num(index + 1, 1, 10);
		ImGui::Combo(name.c_str(), &zoom, "Pixel\0Double\0Quad\0Fit X\0Fit Y\0Fit Window\0\0");

		bool modeOpt = displayMode == C64_Bitmap || displayMode == C64_Text || displayMode == C64_Sprites;

		if (displayMode != C64_Current) {
			ImGui::NextColumn();
			//			ImGui::Columns(modeOpt ? 3 : 2);

			if (displayMode == C64_Bitmap || displayMode == C64_Text) {
				int colMode = color ? 1 : (multicolor ? 2 : 0), prevMode = colMode;
				name.copy("Color##");
				name.append_num(index + 1, 1, 10);
				ImGui::Combo(name.c_str(), &colMode, "Mono\0Color\0Multi\0\0");
				color = colMode == 1;
				multicolor = colMode == 2;
				if (prevMode != colMode) { redraw = true; }
				ImGui::NextColumn();
			} else if (displayMode == C64_Sprites) {
				int colMode = multicolor ? 1 : 0;
				name.copy("Color##");
				name.append_num(index + 1, 1, 10);
				ImGui::Combo(name.c_str(), &colMode, "Mono\0Multi\0\0");
				if (multicolor != !!colMode) { redraw = true; }
				multicolor = !!colMode;
				ImGui::NextColumn();
			}

			name.copy("cols##");
			name.append_num(index + 1, 1, 10);
			if (ImGui::InputText(name.c_str(), columns_str, sizeof(columns_str))) {
				columns = ValueFromExpression(columns_str);
				redraw = true;
			}
			ImGui::NextColumn();
			name.copy("rows##");
			name.append_num(index + 1, 1, 10);
			if (ImGui::InputText(name.c_str(), rows_str, sizeof(rows_str))) {
				rows = ValueFromExpression(rows_str);
				redraw = true;
			}
		}
		ImGui::Columns(1);
	}


	if (!bitmap || redraw || reeval || MemoryChange()) {
		if (ViceSyncing()) { reeval = true; }
		else {
			Create8bppBitmap();
			reeval = false;
		}
	}


	ImVec2 size(float(columns * 8), float(rows * 8));
	switch (zoom) {
		case Zoom_2x2: size.x *= 2; size.y *= 2; break;
		case Zoom_4x4: size.x *= 4; size.y *= 4; break;
		case Zoom_FitX:
		{
			float x = ImGui::GetWindowWidth();
			size.y *= x / size.x;
			size.x = x;
			break;
		}
		case Zoom_FitY:
		{
			float y = ImGui::GetWindowHeight();
			size.x *= y / size.y;
			size.y = y;
			break;
		}
		case Zoom_FitWindow:
		{
			float x = ImGui::GetWindowWidth();
			float y = ImGui::GetWindowHeight();
			if ((x * size.y) < (y * size.x)) {
				size.y *= x / size.x; size.x = x;
			} else {
				size.x *= y / size.y; size.y = y;
			}
			break;
		}
	}
	ImGui::Image(texture, size);

	ImGui::End();
}

void GfxView::Create8bppBitmap()
{
	// make sure generated bitmap fits in mem
	uint32_t cl = displayMode == C64_Current ? 40 : columns;
	uint32_t rw = displayMode == C64_Current ? 25 : rows;

	size_t bitmapMem = cl * rw * 64 * 4;
	if (!bitmap || bitmapMem > bitmapSize) {
		if (bitmap) { free(bitmap); }
		bitmap = (uint8_t*)calloc(1, bitmapMem);
		bitmapSize = bitmapMem;
	}

	int linesHigh = rows * 8;

	uint32_t *d = (uint32_t*)bitmap;
	uint32_t w = cl * 8;
	uint32_t cw = 8;
	const uint32_t* pal = c64pal;// (const uint32_t*)c64Cols;

	switch (displayMode) {
		case Planar: CreatePlanarBitmap(d, linesHigh, w, c64pal); break;
		case Columns: CreateColumnsBitmap(d, linesHigh, w, c64pal); break;

		case C64_Bitmap: 
			if (color) {
				CreateC64ColorBitmapBitmap(d, c64pal, addrGfxValue, addrScreenValue, cl, rw);
			} else if (multicolor) {
				CreateC64MulticolorBitmapBitmap(d, c64pal, addrGfxValue, addrScreenValue, addrColValue, cl, rw); break;
			} else {
				CreateC64BitmapBitmap(d, c64pal, addrGfxValue, cl, rw); break;
			}
			break;

		case C64_ColBitmap: CreateC64ColorBitmapBitmap(d, c64pal, addrGfxValue, addrScreenValue, cl, rw); break;
		case C64_ExtText: CreateC64ExtBkgTextBitmap(d, c64pal, addrGfxValue, addrScreenValue, addrColValue, cl, rw); break;

		case C64_Text:
			if (color) {
				CreateC64ColorTextBitmap(d, c64pal, addrGfxValue, addrScreenValue, addrColValue, cl, rw);
			} else if (multicolor) {
				CreateC64MulticolorTextBitmap(d, c64pal, addrGfxValue, addrScreenValue, addrColValue, cl, rw); break;
			} else {
				CreateC64TextBitmap(d, c64pal, cl, rw); break;
			}
			break;
		case C64_Text_MC: CreateC64MulticolorTextBitmap(d, c64pal, addrGfxValue, addrScreenValue, addrColValue, cl, rw); break;
		case C64_MCBM: CreateC64MulticolorBitmapBitmap(d, c64pal, addrGfxValue, addrScreenValue, addrColValue, cl, rw); break;
		case C64_Sprites: CreateC64SpritesBitmap(d, linesHigh, w, c64pal); break;
		case C64_ColumnScreen_MC: CreateC64ColorTextColumns(d, c64pal, addrGfxValue, addrScreenValue, addrColValue, cl, rw); break;
		case C64_Current: CreateC64CurrentBitmap(d, c64pal); break;

		case Apl2_Text: CreateApple2TextBitmap(d, linesHigh, w, c64pal); break;
		case Apl2_Hires: CreateApple2HiresBitmap(d, linesHigh, w, c64pal); break;
		case Apl2_HR_Col: CreateApple2HiresColorBitmap(d, linesHigh, w, c64pal); break;
	}

	if (!texture) { texture = CreateTexture(); }
	if (texture) {
		SelectTexture(texture);
		UpdateTextureData(cl * 8, rw * 8, bitmap);
	}
}

void GfxView::CreatePlanarBitmap(uint32_t* d, int linesHigh, uint32_t w, const uint32_t* pal)
{
	uint16_t a = addrGfxValue;
	for (int y = 0; y < linesHigh; y++) {
		uint16_t xp = 0;
		for (uint32_t x = 0; x < columns; x++) {
			uint8_t b = Get6502Byte(a++);
			uint8_t m = 0x80;
			for (int bit = 0; bit < 8; bit++) {
				d[(y)*w + (xp++)] = pal[(b&m) ? 14 : 6];
				m >>= 1;
			}
		}
	}
}

void GfxView::CreateColumnsBitmap(uint32_t* d, int linesHigh, uint32_t w, const uint32_t* pal)
{
	const uint32_t cw = 8;
	uint16_t a = addrGfxValue;
	for (uint32_t x = 0; x < columns; x++) {
		for (int y = 0; y < linesHigh; y++) {
			int xp = x*cw;
			uint8_t b = Get6502Byte(a++);
			uint8_t m = 0x80;
			for (uint32_t bit = 0; bit < cw; bit++) {
				d[(y)*w + (xp++)] = pal[(b&m) ? 14 : 6];
				m >>= 1;
			}
		}
	}
}

void GfxView::CreateApple2TextBitmap(uint32_t* d, int linesHigh, uint32_t w, const uint32_t* pal)
{
	const uint32_t cw = 8;
	for (int y = 0; y < (linesHigh >> 3); y++) {
		uint16_t a = (y & 7) * 128 + (y >> 3) * 40 + addrScreenValue;
		for (uint32_t x = 0; x < columns; x++) {
			uint8_t chr = y >= 24 ? 0 : Get6502Byte(a++);
			uint8_t *cs = _fruitFont + 8 * chr;
			for (int h = 0; h < 8; h++) {
				uint8_t b = *cs++;
				uint8_t m = 0x80;
				for (int bit = 0; bit < 8; bit++) {
					d[(y * 8 + h)*w + (x*cw + bit)] = pal[(b&m) ? 5 : 0];
					m >>= 1;
				}
			}
		}
	}
}

void GfxView::CreateC64CurrentBitmap(uint32_t* d, const uint32_t* pal)
{
	uint16_t vic = (3 ^ (Get6502Byte(0xdd00) & 3)) * 0x4000;
	uint8_t d018 = Get6502Byte(0xd018);
	uint8_t d011 = Get6502Byte(0xd011);
	uint8_t d016 = Get6502Byte(0xd016);
	uint16_t chars = ( d018 & 0xe) * 0x400 + vic;
	uint16_t screen = (d018 >> 4) * 0x400 + vic;

	if (chars == 0x1000 || chars == 0xb000) { chars = 0; }

	bool mc = (d016 & 0x10) ? true : false;

	if (d011 & 0x40) {
		CreateC64ExtBkgTextBitmap(d, pal, chars, screen, 0xd800, 40, 25);
	} else if (d011 & 0x20) {
		if (mc) {
			CreateC64MulticolorBitmapBitmap(d, pal, chars, screen, 0xd800, 40, 25);
		} else {
			CreateC64ColorBitmapBitmap(d, pal, chars & 0xe000, screen, 40, 25);
		}
	} else {
		if (mc) {
			CreateC64MulticolorTextBitmap(d, pal, chars, screen, 0xd800, 40, 25);
		} else {
			CreateC64ColorTextBitmap(d, pal, chars, screen, 0xd800, 40, 25);
		}
	}

	uint8_t d015 = Get6502Byte(0xd015); // enable
	uint8_t d010 = Get6502Byte(0xd010); // hi x
	uint8_t d017 = Get6502Byte(0xd017); // double width
	uint8_t d01d = Get6502Byte(0xd01d); // double height
	uint8_t d01c = Get6502Byte(0xd01c); // multicolor
	uint8_t mcol [3] = { (uint8_t)(Get6502Byte(0xd025)&0xf), (uint8_t)0, (uint8_t)(Get6502Byte(0xd026)&0xf) };
	int sw = columns * 8;
	int sh = rows * 8;
	int w = 40 * 8;
	for (int s = 7; s >= 0; --s) {
		uint8_t col = Get6502Byte(0xd027 + s)&0xf;
		mcol[1] = col;
		if (d015 & (1 << s)) {
			int x = Get6502Byte(0xd000 + 2 * s) + (d010 & (1 << s) ? 256 : 0) - 24;
			int y = Get6502Byte(0xd001 + 2 * s) - 50;
			int l = 0, r = 0, sy = 0, sx = 0;
			if (d017 & (1 << s)) { sy = 1; }
			if (d01d & (1 << s)) { sx = 1; }
			if (x < sw && x>(-(24<<sx)) && y < sh && y >(-(21<<sy))) {
				bool isMC = !!(d01c & (1 << s));
				uint8_t index = Get6502Byte(screen + 0x3f8 + s);
				uint16_t sprite = vic + index * 64;
				for (int dy = y, by = y + (21<<sy); dy < by; ++dy) {
					if (dy >= 0 && dy < sh) {
						uint32_t* ds = d + dy * w;
						uint16_t sr = sprite + 3 * ((dy - y) >> sy);
						const uint8_t *row = Get6502Mem(sr);
						for (int dx = x, rx = x + (24<<sx); dx < rx; ++dx) {
							if (dx >= 0 && dy < sw) {
								int ox = (dx - x) >> sx;
								uint8_t b = row[ox >> 3];
								if (isMC) {
									uint8_t ci = (b >> ((ox^6) & 6)) & 3;
									if (ci) {
										d[dy * w + dx] = pal[mcol[ci - 1]];
									}
								} else {
									if (b & (1 << ((ox^7) & 7))) {
										d[dy * w + dx] = pal[col];
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

void GfxView::CreateC64BitmapBitmap(uint32_t* d, const uint32_t* pal, uint16_t a, uint32_t cl, uint32_t rw)
{
	for (uint32_t y = 0; y < rw; y++) {
		for (uint32_t x = 0; x < cl; x++) {
			uint32_t* o = d + y * 64 * cl + x * 8;
			for (int h = 0; h < 8; h++) {
				uint8_t b = Get6502Byte(a++);
				uint8_t m = 0x80;
				for (int bit = 0; bit < 8; bit++) {
					*o = pal[(b&m) ? 14 : 6];
					o++;
					m >>= 1;
				}
				o += (cl-1) * 8;
			}
		}
	}
}

void GfxView::CreateC64ColorBitmapBitmap(uint32_t* d, const uint32_t* pal, uint16_t a, uint16_t c, uint32_t cl, uint32_t rw)
{
	for (uint32_t y = 0; y < rw; y++) {
		for (uint32_t x = 0; x < cl; x++) {
			uint32_t* o = d + y * 64 * cl + x * 8;
			uint8_t col = Get6502Byte(c++);
			for (int h = 0; h < 8; h++) {
				uint8_t b = Get6502Byte(a++);
				uint8_t m = 0x80;
				for (int bit = 0; bit < 8; bit++) {
					*o = pal[(b&m) ? (col >> 4) : (col & 0xf)];
					o++;
					m >>= 1;
				}
				o += cl*8 - 8;
			}
		}
	}
}

void GfxView::CreateC64ExtBkgTextBitmap(uint32_t* d, const uint32_t* pal, uint16_t g, uint16_t a, uint16_t cm, uint32_t cl, uint32_t rw)
{
	bool romFont = g == 0;
	for (uint32_t y = 0; y < rw; y++) {
		for (uint32_t x = 0; x < cl; x++) {
			uint8_t chr = Get6502Byte(a++);
			uint32_t bg = pal[Get6502Byte((chr >> 6) + 0xd021) & 0xf];
			uint32_t fg = pal[Get6502Byte(y * 40 + x + cm) & 0xf];
			chr &= 0x3f;
			uint16_t cs = g + 8 * chr;
			for (int h = 0; h < 8; h++) {
				uint8_t b = romFont ? _aStartupFont[cs++] : Get6502Byte(cs++);
				uint8_t m = 0x80;
				for (int bit = 0; bit < 8; bit++) {
					d[(y * 8 + h)*cl*8 + (x * 8 + bit)] = (b&m) ? fg : bg;
					m >>= 1;
				}
			}
		}
	}
}

void GfxView::CreateC64TextBitmap(uint32_t* d, const uint32_t* pal, uint32_t cl, uint32_t rw)
{
	uint16_t a = addrScreenValue;
	bool romFont = addrGfxValue == 0;
	for (uint32_t y = 0; y < rw; y++) {
		for (uint32_t x = 0; x < cl; x++) {
			uint8_t chr = Get6502Byte(a++);
			uint16_t cs = addrGfxValue + 8 * chr;
			for (int h = 0; h < 8; h++) {
				uint8_t b = romFont ? _aStartupFont[cs++] : Get6502Byte(cs++);
				uint8_t m = 0x80;
				for (int bit = 0; bit < 8; bit++) {
					d[(y * 8 + h)*cl*8 + (x * 8 + bit)] = pal[(b&m) ? 14 : 6];
					m >>= 1;
				}
			}
		}
	}
}

void GfxView::CreateC64ColorTextBitmap(uint32_t* d, const uint32_t* pal, uint16_t g, uint16_t a, uint16_t f, uint32_t cl, uint32_t rw)
{
	uint8_t k = Get6502Byte(0xd021) & 0xf;
	uint32_t *o = d;
	bool romFont = a == 0;
	for (int y = 0, ye = rw; y < ye; y++) {
		for (uint32_t x = 0; x < cl; x++) {
			uint8_t c = Get6502Byte(f++) & 0xf;
			uint8_t chr = Get6502Byte(a++);
			uint16_t cs = g + 8 * chr;
			for (int h = 0; h < 8; h++) {
				uint8_t b = romFont ? _aStartupFont[cs++] : Get6502Byte(cs++);
				for (int m = 0x80; m; m>>=1) {
					*o++ = pal[(m&b) ? c : k];
				}
				o += cl*8 - 8;
			}
			o -= (cl*8 - 1) * 8;
		}
		o += 56*cl;
	}

}

void GfxView::CreateC64MulticolorTextBitmap(uint32_t* d, const uint32_t* pal, uint16_t g, uint16_t a, uint16_t cm, uint32_t cl, uint32_t rw)
{
	uint8_t k[4] = { uint8_t(Get6502Byte(0xd021) & 0xf), uint8_t(Get6502Byte(0xd022) & 0xf), uint8_t(Get6502Byte(0xd023) & 0xf), 0 };
	uint32_t *o = d;
	for (uint32_t y = 0; y < rw; y++) {
		for (uint32_t x = 0; x < cl; x++) {
			k[3] = Get6502Byte(cm++) & 0xf;
			int mc = k[3] & 0x8;
			k[3] &= 7;
			uint8_t chr = Get6502Byte(a++);
			uint16_t cs = g + 8 * chr;
			for (int h = 0; h < 8; h++) {
				uint8_t b = Get6502Byte(cs++);
				if (mc) {
					for (int bit = 6; bit >= 0; bit -= 2) {
						uint8_t c = k[(b >> bit) & 0x3];
						*o++ = pal[c];
						*o++ = pal[c];
					}
				} else {
					for (int bit = 7; bit >= 0; bit--) {
						*o++ = pal[k[((b >> bit) & 1) ? 3 : 0]];
					}
				}
				o += (cl-1) * 8;
			}
			o -= cl * 64 - 8;
		}
		o += 56 * cl;
	}
}

void GfxView::CreateC64MulticolorBitmapBitmap(uint32_t* d, const uint32_t* pal, uint16_t a, uint16_t s, uint16_t cm, uint32_t cl, uint32_t rw)
{
	uint8_t k = Get6502Byte(0xd021) & 15;
	for (uint32_t y = 0; y < rw; y++) {
		for (uint32_t x = 0; x < cl; x++) {
			uint8_t sc = Get6502Byte(s++);
			uint8_t fc = Get6502Byte(cm++);
			for (int h = 0; h < 8; h++) {
				uint8_t b = Get6502Byte(a++);
				for (int p = 3; p >= 0; p--) {
					uint8_t c;
					switch ((b >> (p << 1)) & 3) {
						case 0: c = k; break;
						case 1: c = (sc >> 4); break;
						case 2: c = (sc & 15); break;
						case 3: c = (fc & 15); break;
					}
					*d++ = pal[c];
					*d++ = pal[c];
				}
				d += cl*8 - 8;
			}
			d -= cl*64 - 8;
		}
		d += cl * 56;
	}
}

void GfxView::CreateC64SpritesBitmap(uint32_t* d, int linesHigh, uint32_t w, const uint32_t* pal)
{
	uint16_t a = addrGfxValue;
	int sx = columns / 3;
	int sy = linesHigh / 21;
	for (int y = 0; y < sy; y++) {
		for (int x = 0; x < sx; x++) {
			for (int l = 0; l < 21; l++) {
				uint32_t *ds = d + (y * 21 + l)*w + x * 24;
				for (int s = 0; s < 3; s++) {
					uint8_t b = Get6502Byte(a++);
					uint8_t m = 0x80;
					for (int bit = 0; bit < 8; bit++) {
						*ds++ = pal[b&m ? 14 : 6];
						m >>= 1;
					}
				}
			}
			++a;
		}
	}
}

void GfxView::CreateC64ColorTextColumns(uint32_t* d, const uint32_t* pal, uint16_t g, uint16_t a, uint16_t f, uint32_t cl, uint32_t rw)
{
	uint8_t k[4] = { uint8_t(Get6502Byte(0xd021) & 0xf), uint8_t(Get6502Byte(0xd022) & 0xf), uint8_t(Get6502Byte(0xd023) & 0xf), 0 };
	for (uint32_t x = 0; x < cl; x++) {
		uint32_t* o = d + x * 8;
		for (uint32_t y = 0; y < rw; y++) {
			uint8_t chr = Get6502Byte(a++);
			uint8_t charCol = Get6502Byte(f++);
			k[3] = charCol & 7;
			uint8_t mc = charCol & 0x8;
			uint16_t cs = g + 8 * chr;
			for (int h = 0; h < 8; h++) {
				uint8_t b = Get6502Byte(cs++);
				if (mc) {
					for (int bit = 6; bit >= 0; bit -= 2) {
						uint8_t c = k[(b >> bit) & 0x3];
						*o++ = pal[c];
						*o++ = pal[c];
					}
				}
				else {
					for (int bit = 7; bit >= 0; bit--) {
						*o++ = pal[k[((b >> bit) & 1) ? 3 : 0]];
					}
				}
				o += (cl - 1) * 8;
			}
		}
	}
}

void GfxView::CreateApple2HiresBitmap(uint32_t* d, int linesHigh, uint32_t w, const uint32_t* pal)
{
	int sy = linesHigh > (8 * 24) ? (8 * 24) : linesHigh;
	int sx = columns < 40 ? columns : 40;
	for (int y = 0; y < sy; y++) {
		uint16_t a = addrGfxValue + (y & 7) * 0x400 + ((y >> 3) & 7) * 128 + (y >> 6) * 40;
		uint32_t *dl = d + y*w;
		for (int x = 0; x < sx; x++) {
			uint8_t b = Get6502Byte(a++);
			uint8_t m = 0x40;
			for (int bit = 0; bit < 7; bit++) {
				*dl++ = b&m ? 4 : 3;
				m >>= 1;
			}
		}
	}
}

void GfxView::CreateApple2HiresColorBitmap(uint32_t* d, int linesHigh, uint32_t w, const uint32_t* pal)
{
	int sx = columns < 40 ? columns : 40;
	int sy = linesHigh > (8 * 24) ? (8 * 24) : linesHigh;
	int sw = sx * 7;
	uint8_t pBits[40 * 7 + 2] = { 0 };
	uint8_t pCol[40 * 7] = { 0 };
	for (int y = 0; y < sy; y++) {
		uint16_t a = addrGfxValue + (y & 7) * 0x400 + ((y >> 3) & 7) * 128 + (y >> 6) * 40;
		uint16_t i = 0;
		for (int x = 0; x < sx; x++) {
			uint8_t b = Get6502Byte(a++);
			uint8_t m = 0x40;
			for (int bit = 0; bit < 7; bit++) {
				pCol[i] = !!(b & 0x80);
				pBits[i++] = !!(b&m);
				m >>= 1;
			}
		}
		uint8_t c = 0;
		uint32_t *dl = d + y*w;
		for (int x = 0; x < sw; x++) {
			uint8_t i = ((x & 1) << 5) | (pBits[x] << 2) | (pBits[x + 1] << 1) | pBits[x + 2];
			c = a2c_lookup[i | (c << 3)];
			*dl++ = a2c_colors[c + (pCol[x] << 2)];
		}
	}
}




GfxView::GfxView() : open(false), reeval(false)
{
	addrScreenValue = 0x0400;
	addrGfxValue = 0x0000;
	addrColValue = 0xd800;
	columns = 40;
	rows = 25;
	zoom = Zoom_FitWindow;
	displaySystem = 1;
	displayMode = C64_Text;
	genericMode = GfxView::Planar;
	c64Mode = GfxView::C64_Current;
	apple2Mode = GfxView::Apl2_Text;
	bitmap = nullptr;
	bitmapSize = 0;
	texture = 0;
	open = true;

	sprintf_s(address_screen, "$%04x", addrScreenValue);
	sprintf_s(address_gfx, "$%04x", addrGfxValue);
	sprintf_s(address_col, "$%04x", addrColValue);
	sprintf_s(columns_str, "%d", columns);
	sprintf_s(rows_str, "%d", rows);
}
