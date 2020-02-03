#include "RegView.h"
#include "imgui/imgui.h"
#include "struse/struse.h"
#include <malloc.h>
#include "Views.h"
#include "machine.h"
#include "Expressions.h"
#include "ImGui_Helper.h"
#include "Config.h"
#include "GLFW/include/GLFW/glfw3.h"
#include "ViceConnect.h"
#include "Image.h"

constexpr auto CursorFlashPeriod = 64.0f/50.0f;

RegisterView::RegisterView() : open(false), cursorTime(0.0f)
{
	cursor = -1;
}

void RegisterView::WriteConfig(UserData& config)
{
	config.AddValue(strref("open"), config.OnOff(open));
}

void RegisterView::ReadConfig(strref config)
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

void RegisterView::Draw()
{
	if (!open) { return; }
	ImGui::SetNextWindowPos(ImVec2(600, 160), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(480, 40), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Registers", &open)) {
		ImGui::End();
		return;
	}

	ImGui::BeginChild(ImGui::GetID("regEdit"));

	bool active = KeyboardCanvas("RegisterView");// IsItemActive();


	ImVec2 topPos = ImGui::GetCursorPos();

	ImGui::Text("ADDR A  X  Y  SP 00 01 NV-BDIZC");
	cursorTime += ImGui::GetIO().DeltaTime;
	if (cursorTime >= CursorFlashPeriod) { cursorTime -= CursorFlashPeriod; }

	if (ImGui::IsMouseClicked(0)) {
		ImVec2 mousePos = ImGui::GetMousePos();
		ImVec2 winPos = ImGui::GetWindowPos();
		ImVec2 winSize = ImGui::GetWindowSize();
		if (mousePos.x >= winPos.x && mousePos.y >= winPos.y &&
			mousePos.x < (winPos.x + winSize.x) && mousePos.y < (winPos.y + winSize.y)) {
			int clickPos = int((mousePos.x - winPos.x) / fontCharWidth);
			if (clickPos < 31) { cursor = clickPos; }
		}
	}

	Regs& r = GetRegs();
	strown<64> regs;
	regs.append_num(r.PC, 4, 16);
	regs.append(' ');
	regs.append_num(r.A, 2, 16);
	regs.append(' ');
	regs.append_num(r.X, 2, 16);
	regs.append(' ');
	regs.append_num(r.Y, 2, 16);
	regs.append(' ');
	regs.append_num(r.S, 2, 16);
	regs.append(' ');
	regs.append_num(Get6502Byte(0), 2, 16);
	regs.append(' ');
	regs.append_num(Get6502Byte(1), 2, 16);
	regs.append(' ');
	regs.append_num(r.P, 8, 2);
	ImVec2 curPos = ImGui::GetCursorPos();
	ImGui::Text(regs.c_str());

	if (active && cursor >= 0) {
		int o = cursor;
		uint8_t b = InputHex();
		if (b <= 0xf) {
			if (o < 4) {	// PC
				int bt = 4 * (3 - o);
				r.PC = (r.PC & (~(0xf << bt))) | (b << bt);
				++cursor; if (cursor == 4) { ++cursor; }
				if (!IsSandboxContext()) {
					strown<32> setPC("r pc=");
					setPC.append_num(r.PC, 4, 16).append('\n');
					ViceSend(setPC.get(), setPC.len());
				}
			} else if (o >= 5 && o < 7) { // A
				int bt = 4 * (6 - o);
				r.A = (r.A & (~(0xf << bt))) | (b << bt);
				++cursor; if (cursor == 7) { ++cursor; }
				if (!IsSandboxContext()) {
					strown<32> setA("r a=");
					setA.append_num(r.A, 2, 16).append('\n');
					ViceSend(setA.get(), setA.len());
				}
			} else if (o >= 8 && o < 10) { // X
				int bt = 4 * (9 - o);
				r.X = (r.X & (~(0xf << bt))) | (b << bt);
				++cursor; if (cursor == 10) { ++cursor; }
				if (!IsSandboxContext()) {
					strown<32> setX("r x=");
					setX.append_num(r.X, 2, 16).append('\n');
					ViceSend(setX.get(), setX.len());
				}
			} else if (o >= 11 && o < 13) { // Y
				int bt = 4 * (12 - o);
				r.Y = (r.Y & (~(0xf << bt))) | (b << bt);
				++cursor; if (cursor == 13) { ++cursor; }
				if (!IsSandboxContext()) {
					strown<32> setY("r y=");
					setY.append_num(r.Y, 2, 16).append('\n');
					ViceSend(setY.get(), setY.len());
				}
			} else if (o >= 14 && o < 16) { // S
				int bt = 4 * (15 - o);
				r.S = (r.S & (~(0xf << bt))) | (b << bt);
				++cursor; if (cursor == 16) { ++cursor; }
				if (!IsSandboxContext()) {
					strown<32> setS("r sp=");
					setS.append_num(r.S, 2, 16).append('\n');
					ViceSend(setS.get(), setS.len());
				}
			} else if (o >= 17 && o < 19) { // 0
				int bt = 4 * (18 - o);
				Set6502Byte( 0, (Get6502Byte(0) & (~(0xf << bt))) | (b << bt) );
				++cursor; if (cursor == 19) { ++cursor; }
				if (!IsSandboxContext()) {
					strown<32> set0("> 0 ");
					set0.append_num(Get6502Byte(0), 2, 16).append('\n');
					ViceSend(set0.get(), set0.len());
				}
			} else if (o >= 20 && o < 22) { // 1
				int bt = 4 * (21 - o);
				Set6502Byte(1, (Get6502Byte(1) & (~(0xf << bt))) | (b << bt));
				++cursor; if (cursor == 22) { ++cursor; }
				if (!IsSandboxContext()) {
					strown<32> set1("> 1 ");
					set1.append_num(Get6502Byte(1), 2, 16).append('\n');
					ViceSend(set1.get(), set1.len());
				}
			} else if (b < 2 && o >= 23 && o < 31) {
				int bt = 30 - o;
				r.P = (r.P & (~(1 << bt))) | (b << bt);
				if (cursor < 30) { ++cursor; }
				if (!IsSandboxContext()) {
					strown<32> setFL("r fl=");
					setFL.append_num(r.P, 2, 16).append('\n');
					ViceSend(setFL.get(), setFL.len());
				}
			}
		}

		if (cursor && ImGui::IsKeyPressed(GLFW_KEY_LEFT)) { cursor--; }
		if (ImGui::IsKeyPressed(GLFW_KEY_RIGHT)) { cursor++; }
	}

	if (active && cursor >= 0 && cursorTime > (0.5f * CursorFlashPeriod)) {
		if ((uint32_t)cursor > regs.len()) {
			cursor = regs.len() - 1;
		}

		const ImGuiStyle style = ImGui::GetStyle();
		ImGui::SetCursorPos(ImVec2(curPos.x + fontCharWidth * cursor, curPos.y));
		const ImVec2 p = ImGui::GetCursorScreenPos();
		ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + fontCharWidth, p.y + fontCharHeight),
			ImColor(255, 255, 255));
		strown<16> curChr;
		curChr.append(regs[cursor]);
		ImGui::TextColored(style.Colors[ImGuiCol_ChildBg], curChr.c_str());
	}

	if (ViceConnected()) {
		ImGui::SetCursorPos(ImVec2(topPos.x + fontCharWidth * 32, topPos.y));
		if (DrawTexturedIcon(!IsSandboxContext() ? VMI_SendToVICE : VMI_DontSendToVICE, false)) {
			SetSandboxContext(!IsSandboxContext());
		}
	}
	ImGui::EndChild();
	ImGui::End();
}
