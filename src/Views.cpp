#include "imgui\Imgui.h"
#include "ViceView.h"
#include "MemView.h"
#include "CodeView.h"
#include "RegView.h"
#include "TimeView.h"
#include "GfxView.h"
#include "WatchView.h"
#include "BreakView.h"
#include "ToolBar.h"
#include "machine.h"
#include "Image.h"
#include "Config.h"
#include "CodeControl.h"
#include "ViceConnect.h"
#include "FileDialog.h"
#include "..\Data\C64_Pro_Mono-STYLE.ttf.h"

// font sizes
static const float sFontSizes[] = {
	8.0f, 10.0f, 12.0, 14.0f, 16.0f, 20.0f, 24.0f
};
static const int sNumFontSizes = sizeof(sFontSizes) / sizeof(sFontSizes[0]);

static const ImWchar C64CharRanges[] =
{
	0x0020, 0x00FF, // Basic Latin + Latin Supplement
	0xee00, 0xeeff,
	0
	//	0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
	//	0x2DE0, 0x2DFF, // Cyrillic Extended-A
	//	0xA640, 0xA69F, // Cyrillic Extended-B
	//	0,
};



static const int codeViewCount = 4;
static const int memViewCount = 4;
static const int gfxViewCount = 4;
static const int watchViewCount = 2;

struct IceBroViews {
	ViceConsole viceConsole; // vice monitor console
	MemView memView[memViewCount];
	CodeView disAsmView[codeViewCount];
	RegisterView regView;
	GfxView gfxView[gfxViewCount];
	WatchView watchView[watchViewCount];
	BreakView breakView;
	TimeView timeView;
	ToolBar toolBar;
	ImFont* aFonts[sNumFontSizes];

	IceBroViews() {}

	void FocusPC();
	void LoadViews();
	void SaveViews();
	void DrawViews();
};

int currFont = 1;
int windowWidth = 1280;
int windowHeight = 720;

float fontCharWidth = 10.0f;
float fontCharHeight = 10.0f;

static const char* sConfigFilename = "IceBro.cfg";
strref OnOff[] = { strref("Off"), strref("On") };

static IceBroViews* sViews = nullptr;



void InitViews()
{
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	assert(sViews == nullptr);

	sViews = new IceBroViews;

	for (int f = 0; f < sNumFontSizes; ++f) {
		sViews->aFonts[f] = io.Fonts->AddFontFromMemoryCompressedTTF(GetC64FontData(), GetC64FontSize(), sFontSizes[f], NULL, C64CharRanges);
		assert(sViews->aFonts[f] != NULL);
	}

	LoadIcons();
	sViews->viceConsole.Init();
}

void DrawViews()
{
	if (sViews) { sViews->DrawViews(); }
}

void LoadViews()
{
	if (sViews) { sViews->LoadViews(); }
}

void SaveViews()
{
	if (sViews) { sViews->SaveViews(); }
}

void FocusPC()
{
	if (sViews) { sViews->FocusPC(); }
}


void UpdateMainWindowWidthHeight(int width, int height)
{
	windowWidth = width;
	windowHeight = height;
}

int GetMainWindowWidthHeight(int *width)
{
	*width = windowWidth;
	return windowHeight;
}

void ViewsWriteConfig(UserData& config)
{
	config.AddValue(strref("fontSize"), currFont);
	config.AddValue(strref("width"), windowWidth);
	config.AddValue(strref("height"), windowHeight);
}

void ViewsReadConfig(strref config)
{
	ConfigParse conf(config);
	while (!conf.Empty()) {
		strref name, value;
		ConfigParseType type = conf.Next(&name, &value);
		if (name.same_str("fontSize") && type == CPT_Value) {
			currFont = (int)value.atoi();
		} else if (name.same_str("width") && type == CPT_Value) {
			windowWidth = (int)value.atoi();
		} else if (name.same_str("height") && type == CPT_Value) {
			windowHeight = (int)value.atoi();
		}
	}
}



void IceBroViews::FocusPC()
{
	for (int c = 0; c < codeViewCount; ++c) {
		if (!disAsmView[c].fixedAddress && disAsmView[c].open) {
			disAsmView[c].focusPC = true;
		}
	}
}

void IceBroViews::LoadViews()
{
	FILE* f;
	if (fopen_s(&f, sConfigFilename, "r") == 0) {
		void* data;
		size_t size;

		fseek(f, 0, SEEK_END);
		size = ftell(f);
		fseek(f, 0, SEEK_SET);
		data = calloc(1, size);
		fread(data, size, 1, f);
		fclose(f);

		ConfigParse config(data, size);
		while (!config.Empty()) {
			strref name, value;
			ConfigParseType type = config.Next(&name, &value);
			if (name.same_str("BinFile") && type == CPT_Struct) {
				BinFileReadConfig(value);
			} else if (name.same_str("Views") && type == CPT_Struct) {
				ViewsReadConfig(value);
			} else if (name.same_str("ViceMonitor") && type == CPT_Struct) {
				viceConsole.ReadConfig(value);
			} else if (name.same_str("MemView") && type == CPT_Array) {
				ConfigParse elements(value);
				for (int m = 0; m < 4; ++m) {
					memView[m].ReadConfig(elements.ArrayElement());
				}
			} else if (name.same_str("CodeView") && type == CPT_Array) {
				ConfigParse elements(value);
				for (int c = 0; c < 4; ++c) {
					disAsmView[c].ReadConfig(elements.ArrayElement());
				}
			} else if (name.same_str("RegisterView") && type == CPT_Struct) {
				regView.ReadConfig(value);
			} else if (name.same_str("TimeView") && type == CPT_Struct) {
				timeView.ReadConfig(value);
			} else if (name.same_str("ScreenView") && type == CPT_Array) {
				ConfigParse elements(value);
				for (int s = 0; s < 4; ++s) {
					gfxView[s].ReadConfig(elements.ArrayElement());
				}
			} else if (name.same_str("WatchView") && type == CPT_Array) {
				ConfigParse elements(value);
				for (int w = 0; w < 2; ++w) {
					watchView[w].ReadConfig(elements.ArrayElement());
				}
			} else if (name.same_str("Breakpoints") && type == CPT_Struct) {
				breakView.ReadConfig(value);
			} else if (name.same_str("Toolbar") && type == CPT_Struct) {
				toolBar.ReadConfig(value);
			}
		}

		free(data);
	}
}

void IceBroViews::SaveViews()
{
	UserData conf;
	strown<128> arg;

	conf.BeginStruct(strref("Views"));
	ViewsWriteConfig(conf);
	conf.EndStruct();

	conf.BeginStruct(strref("BinFile"));
	BinFileWriteConfig(conf);
	conf.EndStruct();

	conf.BeginStruct(strref("ViceMonitor"));
	viceConsole.WriteConfig(conf);
	conf.EndStruct();

	conf.BeginArray(strref("MemView"));
	for (int v = 0; v < 4; ++v) {
		conf.BeginStruct();
		memView[v].WriteConfig(conf);
		conf.EndStruct();
	}
	conf.EndArray();

	conf.BeginArray(strref("CodeView"));
	for (int v = 0; v < 4; ++v) {
		conf.BeginStruct();
		disAsmView[v].WriteConfig(conf);
		conf.EndStruct();
	}
	conf.EndArray();

	conf.BeginStruct(strref("RegisterView"));
	regView.WriteConfig(conf);
	conf.EndStruct();

	conf.BeginStruct(strref("TimeView"));
	timeView.WriteConfig(conf);
	conf.EndStruct();

	conf.BeginArray(strref("ScreenView"));
	for (int v = 0; v < 4; ++v) {
		conf.BeginStruct();
		gfxView[v].WriteConfig(conf);
		conf.EndStruct();
	}
	conf.EndArray();

	conf.BeginArray(strref("WatchView"));
	for (int v = 0; v < 2; ++v) {
		conf.BeginStruct();
		watchView[v].WriteConfig(conf);
		conf.EndStruct();
	}
	conf.EndArray();

	conf.BeginStruct(strref("Breakpoints"));
	breakView.WriteConfig(conf);
	conf.EndStruct();

	conf.BeginStruct(strref("Toolbar"));
	toolBar.WriteConfig(conf);
	conf.EndStruct();

	FILE* f;
	if (fopen_s(&f, sConfigFilename, "w") == 0 && f != nullptr) {
		fwrite(conf.start, conf.curr - conf.start, 1, f);
		fclose(f);
	}
}

uint8_t InputHex()
{
	for (int num = 0; num < 9; ++num) { if (ImGui::IsKeyPressed(num + '0')) return num; }
	for (int num = 10; num < 16; ++num) { if (ImGui::IsKeyPressed(num + 'A' - 10)) return num; }
	return 0xff;
}

void SelectFont(int index)
{
	if (index >= 0 && index < sNumFontSizes) {
		currFont = index;
		fontCharWidth = sFontSizes[index];
		fontCharHeight = sFontSizes[index];
	}
}

void IceBroViews::DrawViews()
{
	{
		static bool p_open = true;
		static bool opt_fullscreen_persistant = true;
		bool opt_fullscreen = opt_fullscreen_persistant;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		if (opt_fullscreen) {
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->Pos);
			ImGui::SetNextWindowSize(viewport->Size);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}

		// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background 
		// and handle the pass-thru hole, so we ask Begin() to not render a background.
		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		ImGui::PushFont(aFonts[currFont]);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("IceBro DockSpace", &p_open, window_flags);
		ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		UpdateCodeControl();
		CheckLoadListing();
		CheckLoadKickDebug();
		CheckSymFileLoad();
		CheckLoadViceCommand();

		{
			if (ImGui::BeginMainMenuBar()) {
				if (ImGui::BeginMenu("File")) {
					bool updatingViceSymbls = ViceUpdatingSymbols();
					if (ImGui::MenuItem("Load KickAsm Debug")) { KickDebugFileDialog(); }
					if (ImGui::MenuItem("Load Listing")) { ListingFileDialog(); }
					if (ImGui::MenuItem("Load Sym File")) { SymFileDialog(); }
					if (ImGui::MenuItem("Load Vice Command Symbols")) { LoadViceCommandFileDialog(); }
					if (ImGui::MenuItem("Update Symbols With Vice Sync", nullptr, updatingViceSymbls)) { ViceSetUpdateSymbols(!updatingViceSymbls); }

					if (ImGui::MenuItem("Quit", "Alt+F4")) {}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Edit")) {
					if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
					if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
					ImGui::Separator();
					if (ImGui::MenuItem("Cut", "CTRL+X")) {}
					if (ImGui::MenuItem("Copy", "CTRL+C")) {}
					if (ImGui::MenuItem("Paste", "CTRL+V")) {}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Windows")) {
					if (ImGui::MenuItem("Vice Console", NULL, viceConsole.open)) { viceConsole.open = !viceConsole.open; }
					if (ImGui::BeginMenu("Memory")) {
						for (int i = 0; i < 4; ++i) {
							strown<64> name("Mem");
							name.append_num(i + 1, 1, 10).append(' ').append(memView[i].address);
							if (ImGui::MenuItem(name.c_str(), NULL, memView[i].open)) { memView[i].open = !memView[i].open; }
						}
						ImGui::EndMenu();
					}
					if (ImGui::BeginMenu("Code")) {
						for (int i = 0; i < 4; ++i) {
							strown<64> name("Code");
							name.append_num(i + 1, 1, 10).append(' ').append(disAsmView[i].address);
							if (ImGui::MenuItem(name.c_str(), NULL, disAsmView[i].open)) { disAsmView[i].open = !disAsmView[i].open; }
						}
						ImGui::EndMenu();
					}
					if (ImGui::MenuItem("Registers", NULL, regView.open)) { regView.open = !regView.open; }
					if (ImGui::BeginMenu("Screen")) {
						for (int i = 0; i < 4; ++i) {
							strown<64> name("Screen");
							name.append_num(i + 1, 1, 10).append(' ').append(gfxView[i].addrScreenValue).append('/').append(gfxView[i].addrGfxValue);
							if (ImGui::MenuItem(name.c_str(), NULL, gfxView[i].open)) { gfxView[i].open = !gfxView[i].open; }
						}
						ImGui::EndMenu();
					}
					if (ImGui::BeginMenu("Watch")) {
						for (int i = 0; i < 2; ++i) {
							strown<64> name("Watch");
							name.append_num(i + 1, 1, 10);
							if (ImGui::MenuItem(name.c_str(), NULL, gfxView[i].open)) { watchView[i].open = !watchView[i].open; }
						}
						ImGui::EndMenu();
					}
					if (ImGui::MenuItem("Breakpoints", NULL, breakView.open)) { breakView.open = !breakView.open; }
					if (ImGui::MenuItem("TimeView", NULL, timeView.open)) { timeView.open = !timeView.open; }
					if (ImGui::MenuItem("Toolbar", NULL, toolBar.open)) { toolBar.open = !toolBar.open; }
					ImGui::EndMenu();
				}

				ImGui::EndMainMenuBar();

			}
		}


		// Dockspace
		ImGuiIO& io = ImGui::GetIO();
		ImGuiID dockspace_id = ImGui::GetID("MyDockspace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		ImGui::End();
	}

	toolBar.Draw();

	viceConsole.Draw();

	for (int m = 0; m < 4; ++m) { memView[m].Draw(m); }

	for (int d = 0; d < 4; ++d) { disAsmView[d].Draw(d); }

	regView.Draw();

	timeView.Draw();

	for (int g = 0; g < 4; ++g) { gfxView[g].Draw(g); }

	for (int w = 0; w < 2; ++w) { watchView[w].Draw(w); }

	breakView.Draw();

	ImGui::PopFont();
}




