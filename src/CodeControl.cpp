#include "GLFW/include/GLFW/glfw3.h"
#include "imgui/imgui.h"
#include "ViceConnect.h"
#include "machine.h"

void StepInstructionVice()
{
	CPUStep();
	if( ViceConnected() )
	{
		ViceSend( "z\12", 2 );
	}
}

void StepOverVice()
{
	CPUStepOver();
	if( ViceConnected() )
	{
		ViceSend( "n\12", 2 );
	}
}

void StepInstruction()
{
	CPUStep();
}

void StepOver()
{
	CPUStepOver();
}

void StepInstructionBack()
{
	CPUStepBack();
}

void StepOverBack()
{
	CPUStepOverBack();
}

void UpdateCodeControl()
{
	CheckRegChange();

	bool shift = ImGui::IsKeyDown( GLFW_KEY_LEFT_SHIFT ) || ImGui::IsKeyDown( GLFW_KEY_RIGHT_SHIFT );
	bool ctrl = ImGui::IsKeyDown( GLFW_KEY_LEFT_CONTROL ) || ImGui::IsKeyDown( GLFW_KEY_RIGHT_CONTROL );

	if( ImGui::IsKeyPressed( GLFW_KEY_F5, false ) ) {
		if( ctrl ) { }
		else if( shift ) { CPUReverse(); }
		else { CPUGo(); }
	}
	if( ImGui::IsKeyPressed( GLFW_KEY_F10, false ) ) {
		if( ctrl ) { StepOverVice(); }
		else if( shift ) { StepOverBack(); } else { StepOver(); }
	}
	if( ImGui::IsKeyPressed( GLFW_KEY_F11, false ) ) {
		if( ctrl ) { StepInstructionVice(); }
		else if( shift ) { StepInstructionBack(); } else { StepInstruction(); }
	}
}