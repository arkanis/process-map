#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "rlgl.h"
#include "raylib.h"
#include "glad.h"


float randInRange(float min, float max) {
	return min + rand() / (float)RAND_MAX * (max - min);
}

int main() {
	const int screenWidth = 800;
	const int screenHeight = 600;
	
	InitWindow(screenWidth, screenHeight, "particles");
	
	Shader shader = LoadShader("particle.vs", "particle.fs");
	int colorLoc = GetShaderLocation(shader, "color");
	
	size_t particleCount = 10000;
	struct { float x, y, t; } particles[particleCount];
	for (size_t i = 0; i < particleCount; i++) {
		particles[i].x = randInRange(20, screenWidth - 20);
		particles[i].y = randInRange(50, screenHeight - 20);
		particles[i].t = randInRange(0, 100);
		//printf("particle[%zu]: %f %f %f\n", i, particles[i].x, particles[i].y, particles[i].t);
	}
	
	GLuint vao = 0, vbo = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(particles), particles, GL_STATIC_DRAW);
		// Note: Attribute Index 0 is bound to "vertexPosition" by LoadShader()
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glEnable(GL_PROGRAM_POINT_SIZE);  // Needed to set the point size in the vertex shader
	//printf("vao %d vbo %d\n", vao, vbo);
	
	
	Camera2D camera = (Camera2D){
		.target = (Vector2){ screenWidth/2, screenHeight/2 },
		.offset = (Vector2){ screenWidth/2, screenHeight/2 },
		.zoom = 1.0
	};
	
	bool isPanning = false;
	Vector2 prevMousePos = { 0 };
	while ( !WindowShouldClose() ) {
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
				DrawRectangle(10, 10, 210, 30, MAROON);
				DrawText("Many particles in one vertex buffer", 20, 20, 10, RAYWHITE);
				
				FlushRenderer();
				glUseProgram(shader.id);
					Vector4 color = ColorNormalize((Color){ 255, 0, 0, 128 });
					SetShaderValue(shader, colorLoc, &color, UNIFORM_VEC4);
					// Have to set the model view projection matrix manually because raylib has not matrix uniform type
					float16 mvp = GetMatrixModelviewProjection();
					glUniformMatrix4fv(shader.locs[LOC_MATRIX_MVP], 1, false, mvp.v);
					
				    glBindVertexArray(vao);
				    	glDrawArrays(GL_POINTS, 0, particleCount);
				    glBindVertexArray(0);
			    glUseProgram(0);
		    EndMode2D();
		EndDrawing();
	}
	
	glDeleteBuffers(1, (const GLuint[]){ vbo });
	glDeleteVertexArrays(1, (const GLuint[]){ vao });
	
	UnloadShader(shader);
	CloseWindow();
	
	return 0;
}