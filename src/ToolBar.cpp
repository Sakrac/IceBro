#include "imgui/imgui.h"
#include "struse\struse.h"
#include "Image.h"
#include "ToolBar.h"
#include "ViceConnect.h"
#include "machine.h"
#include "C64Colors.h"
#include "Config.h"
#include "FileDialog.h"

ToolBar::ToolBar() : open(true) {}

void ToolBar::WriteConfig(UserData& config)
{
	config.AddValue(strref("open"), config.OnOff(open));
}

void ToolBar::ReadConfig(strref config)
{
	ConfigParse conf(config);
	while (!conf.Empty()) {
		strref name, value;
		ConfigParseType type = conf.Next(&name, &value);
		if (name.same_str("open") && type == CPT_Value) {
			open = !value.same_str("Off");
		}
	}
}

bool CenterTextInColumn(const char* text)
{
	ImVec2 textSize = ImGui::CalcTextSize(text);
	textSize.x += 12.0f;
	textSize.y += 4.0f;
	ImGui::SetCursorPosX(0.5f * (ImGui::GetColumnWidth() - textSize.x) + ImGui::GetColumnOffset());
//	ImGui::Text( text );
	return ImGui::Button(text, textSize);
}

void ToolBar::Draw()
{
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(720, 64), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowDockID(1, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Toolbar", &open)) {
		ImGui::End();
		return;
	}

	ImGui::Columns(11, 0, false);

	bool pause = DrawTexturedIconCenter(VMI_Pause, false, -1.0f, ViceRunning() || IsCPURunning() ? C64_PINK : C64_LGRAY);
	pause = CenterTextInColumn("Pause") || pause;

	ImGui::NextColumn();

	bool play = DrawTexturedIconCenter(VMI_Play);
	play = CenterTextInColumn("Go") || play;

	ImGui::NextColumn();

	bool reverse = DrawTexturedIconCenter(VMI_Play, true);
	reverse = CenterTextInColumn("Rvs") || reverse;

	ImGui::NextColumn();

	bool step = DrawTexturedIconCenter(VMI_Step);
	step = CenterTextInColumn("Step") || step;

	ImGui::NextColumn();

	bool stepBack = DrawTexturedIconCenter(VMI_Step, true);
	stepBack = CenterTextInColumn("Back") || stepBack;

	ImGui::NextColumn();

	bool load = DrawTexturedIconCenter(VMI_Load);
	load = CenterTextInColumn("Load") || load;

	ImGui::NextColumn();

	bool reload = DrawTexturedIconCenter(VMI_Reload);
	reload = CenterTextInColumn("Reload") || reload;

	ImGui::NextColumn();

	bool reset = DrawTexturedIconCenter(VMI_Reset);
	reset = CenterTextInColumn("Reset") || reset;

	ImGui::NextColumn();

	bool NMI = DrawTexturedIconCenter(VMI_NMI);
	NMI = CenterTextInColumn("NMI") || NMI;

	ImGui::NextColumn();

	bool Interrupt = DrawTexturedIconCenter(VMI_Interrupt);
	Interrupt = CenterTextInColumn("Intrpt") || Interrupt;

	ImGui::NextColumn();

	bool connect = DrawTexturedIconCenter(ViceConnected() ? VMI_Connected : VMI_Disconnected);
	connect = CenterTextInColumn("Vice") || connect;

	ImGui::Columns(1);
	ImGui::End();

	if (pause) {
		if (ViceRunning()) { ViceBreak(); } else if (IsCPURunning()) { CPUStop(); }
	}

	if (play) {
		if (!ViceRunning() && !IsCPURunning()) { CPUGo(); }
	}

	if (reverse) {
		if (!ViceRunning() && !IsCPURunning()) { CPUReverse(); }
	}

	if (step && !ViceRunning()) { CPUStep(); }

	if (stepBack && !ViceRunning()) { CPUStepBack(); }

	if (connect) {
		if (ViceConnected()) {
			ViceConnectShutdown();
		} else {
			ViceOpen("127.0.0.1", 6510);
		}
	}

	if (load) {
		FileLoadThread();
	}

	if (reload) { ReloadBinary(); }

	if (NMI) { CPUNMI(); }

	if (Interrupt) { CPUIRQ(); }

	if (reset) { CPUReset(); }

	if (IsFileLoadReady()) {
		ImGui::OpenPopup("Load Binary");
		FileLoadReadyAck();
	}

	if (ImGui::BeginPopupModal("Load Binary", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
		static int loadFileType = 0;
		ImGui::RadioButton("C64 PRG File", &loadFileType, 0);
		ImGui::RadioButton("Apple II Dos 3.3 File", &loadFileType, 1);
		ImGui::RadioButton("Raw Binary File", &loadFileType, 2);

		static bool setPCToLoadAddress = true;
		static bool resetBacktrace = true;
		static bool forceLoadTo = false;
		static char forceLoadAddress[16] = {};
		ImGui::Checkbox("Force Load To", &forceLoadTo); ImGui::SameLine();
		ImGui::InputText("Address", forceLoadAddress, 16, ImGuiInputTextFlags_CharsHexadecimal);
		ImGui::Checkbox("Set PC to Load Addres", &setPCToLoadAddress);
		ImGui::Checkbox("Reset back trace on Load", &resetBacktrace);

		if (ImGui::Button("OK", ImVec2(120, 0))) {
			int addr = (int)strref(forceLoadAddress).ahextoui();
			if (!addr) { addr = 0x1000; }
			LoadBinary(loadFileType, addr, setPCToLoadAddress, forceLoadTo, resetBacktrace);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}
}