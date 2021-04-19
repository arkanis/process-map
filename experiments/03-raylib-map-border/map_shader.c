#include <stdio.h>
#include <math.h>
#include "raylib.h"


int main() {
	const int screenWidth = 800;
	const int screenHeight = 600;
	
	InitWindow(screenWidth, screenHeight, "map shaders");
	
	Texture2D map = LoadTexture("map.png");
	// NOTE: Defining 0 (NULL) for vertex shader forces usage of internal default vertex shader
	Shader shader = LoadShader(0, "map.fs");
	int selectedColorLoc = GetShaderLocation(shader, "selectedColor");
	
	Camera2D camera = (Camera2D){
		.target = (Vector2){ map.width/2, map.height/2 },
		.offset = (Vector2){ screenWidth/2, screenHeight/2 },
		.zoom = 1.0
	};
	
	bool wasWindowFocused = true, isPanning = false;
	Vector2 prevMousePos = { 0 };
	while ( !WindowShouldClose() ) {
		// Reduce FPS when window isn't focused
		if ( IsWindowFocused() && !wasWindowFocused ) {
			wasWindowFocused = true;
			SetTargetFPS(0);
		} else if ( !IsWindowFocused() && wasWindowFocused ) {
			wasWindowFocused = false;
			SetTargetFPS(30);
		}
		
		// Mouse panning
		if ( IsMouseButtonPressed(MOUSE_LEFT_BUTTON) ) {
			isPanning = true;
			prevMousePos = GetMousePosition();
		} else if ( IsMouseButtonReleased(MOUSE_LEFT_BUTTON) ) {
			isPanning = false;
		} else if ( isPanning ) {
			float dx = prevMousePos.x - GetMouseX(), dy = prevMousePos.y - GetMouseY();
			camera.target.x += dx / camera.zoom;
			camera.target.y += dy / camera.zoom;
			prevMousePos = GetMousePosition();
		}
		
		// Mousewheel zooming (world space position remains the same directly at the mouse pointer)
		const float factor = powf(0.9, -GetMouseWheelMove());  // 0.9^1 = 0.9, 0.9^-1 = 1/0.9
		Vector2 mousePosWs = GetScreenToWorld2D(GetMousePosition(), camera);
		float oldScale = camera.zoom, newScale = camera.zoom * factor;
		camera.target.x = mousePosWs.x + (camera.target.x - mousePosWs.x) * (oldScale / newScale);
		camera.target.y = mousePosWs.y + (camera.target.y - mousePosWs.y) * (oldScale / newScale);
		camera.zoom = newScale;
		
		
		BeginDrawing();
			ClearBackground(WHITE);
			
			BeginMode2D(camera);
				BeginShaderMode(shader);
					Vector4 selectedColor = ColorNormalize((Color){ 222, 210, 167, 255 });
					SetShaderValue(shader, selectedColorLoc, &selectedColor, UNIFORM_VEC4);
					
					DrawText("USING CUSTOM SHADER", 190, 40, 10, RED);
					DrawTexture(map, 0, 0, BLANK);
				EndShaderMode();
			EndMode2D();
		EndDrawing();
	}
	
	UnloadShader(shader);
	UnloadTexture(map);
	CloseWindow();
	
	return 0;
}