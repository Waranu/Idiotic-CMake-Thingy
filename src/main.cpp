/*******************************************************************************************
*
*   raylib-extras [ImGui] example - Simple Integration
*
*	This is a simple ImGui Integration
*	It is done using C++ but with C style code
*	It can be done in C as well if you use the C ImGui wrapper
*	https://github.com/cimgui/cimgui
*
*   Copyright (c) 2021 Jeffery Myers
*
********************************************************************************************/

#include "raylib.h"
#include "raymath.h"

#include "imgui.h"
#include "rlImGui.h"


// DPI scaling functions
float ScaleToDPIF(float value)
{
    return RL_GetWindowScaleDPI().x * value;
}

int ScaleToDPII(int value)
{
    return int(RL_GetWindowScaleDPI().x * value);
}

int main(int argc, char* argv[])
{
	// Initialization
	//--------------------------------------------------------------------------------------
	int screenWidth = 1280;
	int screenHeight = 800;

	RL_SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
	RL_InitWindow(screenWidth, screenHeight, "raylib-Extras [ImGui] example - simple ImGui Demo");
	RL_SetTargetFPS(144);
	rlImGuiSetup(true);

	RL_Texture image = RL_LoadTexture("resources/parrots.png");

	// Main game loop
	while (!RL_WindowShouldClose())    // Detect window close button or ESC key
	{
		RL_BeginDrawing();
		RL_ClearBackground(RL_DARKGRAY);

		// start ImGui Conent
		rlImGuiBegin();

		// show ImGui Content
		bool open = true;
		ImGui::ShowDemoWindow(&open);

		open = true;
		if (ImGui::Begin("Test Window", &open))
		{
			ImGui::TextUnformatted(ICON_FA_JEDI);

			rlImGuiImage(&image);
		}
		ImGui::End();

		// end ImGui Content
		rlImGuiEnd();

		RL_EndDrawing();
		//----------------------------------------------------------------------------------
	}

	// De-Initialization
	//--------------------------------------------------------------------------------------   
    rlImGuiShutdown();
	RL_UnloadTexture(image);
	RL_CloseWindow();        // Close window and OpenGL context
	//--------------------------------------------------------------------------------------

	return 0;
}