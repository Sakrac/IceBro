#include "imgui/imgui.h"
#include "machine.h"
#include "TimeView.h"
#include "Config.h"

TimeView::TimeView() : open( false ), cursor( 0 )
{
}

void TimeView::WriteConfig(UserData & config)
{
	config.AddValue(strref("open"), config.OnOff(open));
}

void TimeView::ReadConfig(strref config)
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

void TimeView::Draw()
{
	if (!open) { return; }
	ImGui::SetNextWindowSize(ImVec2(480, 40), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Timeline", &open)) {
		ImGui::End();
		return;
	}

	uint32_t history_max;
	uint32_t history_count = GetHistoryCount(history_max);
	uint32_t history_slide = history_count;
	if (history_max < 256) { history_max = 64; }

	ImGui::SliderInt("History", (int*)&history_slide, 0, (int)history_max);

	if (history_slide < history_count) {
		CPUReverse(history_count - history_slide);
	} else if (history_slide > history_count) {
		CPUGo(history_slide - history_count);
	}

	ImGui::End();
}
