#pragma once
#include "imgui\imgui.h"
#include "Breakpoints.h"
#include "struse\struse.h"
#include "BreakView.h"
#include "machine.h"
#include "Config.h"

BreakView::BreakView() : open(false)
{
}

void BreakView::WriteConfig(UserData& config)
{
	config.AddValue(strref("open"), config.OnOff(open));
}

void BreakView::ReadConfig(strref config)
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

void BreakView::Draw()
{
	if (!open) { return; }
	ImGui::SetNextWindowPos(ImVec2(0, 150), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(384, 56), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Breakpoints", &open)) {
		ImGui::End();
		return;
	}

	int numBP = GetNumBreakpoints();
	const ViceBP* bp = GetBreakpoints();

	if (numBP == 0) {
		uint16_t nSB_BP = GetNumPCBreakpoints();
		uint16_t* BPAddr = GetPCBreakpoints();
		for (uint16_t b = 0; b < nSB_BP; ++b) {
			strown<64> desc;
			uint16_t bk = BPAddr[b];
			desc.append("BREAK");
			desc.append(": $").append_num(bk, 4, 16);
			ImGui::Text(desc.c_str());
		}
	}

	for (int b = 0; b < numBP; ++b) {
		strown<64> desc;
		const ViceBP& bk = bp[b];
		switch (bk.type) {
			case VBP_Break: desc.append("BREAK"); break;
			case VBP_WatchStore: desc.append("STORE"); break;
			case VBP_WatchRead: desc.append("READ"); break;
		}
		if (bk.disabled) { desc.append(" (DIS)"); }
		desc.append(": $").append_num(bk.address, 4, 16);
		if (bk.address != bk.end) {
			desc.append("-$").append_num(bk.end, 4, 16);
		}
		if (bk.viceIndex >= 0) { desc.sprintf_append(" (%d)", bk.viceIndex); }
		ImGui::Text(desc.c_str());
	}

	ImGui::End();
}
