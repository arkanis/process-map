#include <stdlib.h>
#include <math.h>
#include "raylib.h"

int main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [text] example - SDF fonts");

    // NOTE: Textures/Fonts MUST be loaded after Window initialization (OpenGL context is required)

    const char* msg = "Almost before we knew it, we {had left} the ground. äöüß?";
    
    // Loading file to memory
    unsigned int fileSize = 0;
    unsigned char *fileData = LoadFileData("NotoSans-Regular.ttf", &fileSize);

    // Default font generation from TTF font
    Font fontDefault = { .baseSize = 16, .charsCount = 95 };
    
    // Loading font data from memory data
    // Parameters > font size: 16, no chars array provided (0), chars count: 95 (autogenerate chars array)
    fontDefault.chars = LoadFontData(fileData, fileSize, 16, 0, 95, FONT_DEFAULT);
    // Parameters > chars count: 95, font size: 16, chars padding in image: 4 px, pack method: 0 (default)
    Image atlas = GenImageFontAtlas(fontDefault.chars, &fontDefault.recs, 95, 16, 4, 0);
    fontDefault.texture = LoadTextureFromImage(atlas);
    UnloadImage(atlas);

    // SDF font generation from TTF font
    Font fontSDF = { .baseSize = 32, .charsCount = 95 };
    // Parameters > font size: 16, no chars array provided (0), chars count: 0 (defaults to 95)
    fontSDF.chars = LoadFontData(fileData, fileSize, fontSDF.baseSize, 0, 0, FONT_SDF);
    // Parameters > chars count: 95, font size: 16, chars padding in image: 0 px, pack method: 1 (Skyline algorythm)
    atlas = GenImageFontAtlas(fontSDF.chars, &fontSDF.recs, 95, fontSDF.baseSize, 0, 0);
    fontSDF.texture = LoadTextureFromImage(atlas);
    UnloadImage(atlas);
    
    UnloadFileData(fileData);      // Free memory from loaded file

    // Load SDF required shader (we use default vertex shader)
    Shader shader = LoadShader(NULL, "sdf-text.fs");
    SetTextureFilter(fontSDF.texture, FILTER_BILINEAR);    // Required for SDF font
    
    float fontSize = 16.0f;
    Vector2 textSize = MeasureTextEx(fontSDF, msg, fontSize, 0);
    
    Camera2D camera = (Camera2D){
        .target = (Vector2){ screenWidth / 2, screenHeight / 2 },
        .offset = (Vector2){ screenWidth / 2, screenHeight / 2 },
        .zoom = 1.0
    };

    // Main game loop
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
        
        if (IsKeyPressed(KEY_R))
            shader = LoadShader(NULL, "sdf-text.fs");
        if (IsKeyPressed(KEY_Z))
            camera.zoom = 1.0;
        
        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(RAYWHITE);
            
            BeginMode2D(camera);
                DrawTexture(fontSDF.texture, 0, 0, BLACK);
                DrawRectangleV((Vector2){ 0, 300 }, textSize, (Color){ 0, 0, 255, 64 });
                BeginShaderMode(shader);    // Activate SDF font shader
                    DrawTextEx(fontSDF, msg, (Vector2){ 0, 300 }, fontSize, 0, BLACK);
                    DrawTextEx(fontSDF, msg, (Vector2){ 0, 320 }, fontSize / 2, 0, BLACK);
                    DrawTextEx(fontSDF, msg, (Vector2){ 0, 330 }, fontSize / 4, 0, BLACK);
                EndShaderMode();            // Activate our default shader for next drawings
                
                DrawTexture(fontDefault.texture, 300, 0, BLACK);
                DrawTextEx(fontDefault, msg, (Vector2){ 300, 300 }, fontSize, 0, BLACK);
            EndMode2D();
        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadFont(fontDefault);    // Default font unloading
    UnloadFont(fontSDF);        // SDF font unloading

    UnloadShader(shader);       // Unload SDF shader

    CloseWindow();              // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}