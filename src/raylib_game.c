/*******************************************************************************************
*
*   raylib gamejam template
*
*   Code licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2022-2026 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include "raymath.h"

#define min(a,b) a < b ? a : b
#define max(a,b) a > b ? a : b


#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>      // Emscripten library
#endif

#include <stdio.h>                          // Required for: printf()
#include <stdlib.h>                         // Required for: 
#include <string.h>                         // Required for:

#define SUPPORT_LOG_INFO
#if defined(SUPPORT_LOG_INFO)
    #define LOG(...) printf(__VA_ARGS__)
#else
    #define LOG(...)
#endif


float SmoothDamp(
  float current,
  float target,
  float *current_velocity,
  float smooth_time,
  float max_speed,
  float delta_time) {
 smooth_time = fmax(0.0001f, smooth_time);
 float num = 2.0f / smooth_time;
 float num2 = num * delta_time;
 float num3 = 1.0f / (1.0f + num2 + 0.48f * num2 * num2 + 0.235f * num2 * num2 * num2);
 float num4 = current - target;
 float num5 = target;
 float num6 = max_speed * smooth_time;
 num4 = Clamp(num4, -num6, num6);
 target = current - num4;
 float num7 = (*current_velocity + num * num4) * delta_time;
 *(current_velocity) = (*(current_velocity) - num * num7) * num3;
 float num8 = target + (num4 + num7) * num3;
 if ((num5 - current > 0.0f) == (num8 > num5))
 {
  num8 = num5;
  *(current_velocity) = (num8 - num5) / delta_time;
 }
 return num8;
}

Vector3 Vector3SmoothDamp(
  Vector3 current,
  Vector3 target,
  Vector3 *current_velocity,
  float smooth_time,
  float max_speed,
  float delta_time) {
  float *vx = &(current_velocity->x);
  float *vy = &(current_velocity->y);
  float *vz = &(current_velocity->z);

  float x = SmoothDamp(current.x, target.x, vx, smooth_time, max_speed, delta_time);
  float y = SmoothDamp(current.y, target.y, vy, smooth_time, max_speed, delta_time);
  float z = SmoothDamp(current.z, target.z, vz, smooth_time, max_speed, delta_time);

  current_velocity->x = *(vx);
  current_velocity->y = *(vy);
  current_velocity->z = *(vz);

  return (Vector3){ x, y, z };
}

static const int screenWidth = 720;
static const int screenHeight = 720;
static Model hexagon_model;
static Model trinket_model;

#define HEX_STATUS_GOOD 0
#define HEX_STATUS_WARN 1
#define HEX_STATUS_BAD  2

#define NUM_HEXAGONS 8
static Vector3 hexagon_positions[NUM_HEXAGONS][NUM_HEXAGONS];
static int hexagon_status[NUM_HEXAGONS][NUM_HEXAGONS];

static RenderTexture2D target = { 0 };
static Vector3 cameraTarget = { 5.0f, 6.0f, 5.0f };
static Camera3D camera = {
    .position = (Vector3){ 4.0f, 4.0f, 4.0f},
    .target = (Vector3){ 5.0f, 6.0f, 5.0f },
    .up = (Vector3){ 0.0f, 1.0f, 0.0f },
    .fovy = 60.0f,
    .projection = CAMERA_PERSPECTIVE
};

static Vector3 player_position = (Vector3){ 0.0f, 0.0f, 0.0f };
static float speed = 2.0f;
static Vector3 player_velocity = (Vector3){ 0.0f, 0.0f, 0.0f };
static Vector3 player_target = (Vector3){ 0.0f, 0.0f, 0.0f };
static int player_hexagon_x = NUM_HEXAGONS-1;
static int player_hexagon_y = NUM_HEXAGONS-1;
static float player_time = 1.0f;
static int trinket_hexagon_x = 0;
static int trinket_hexagon_y = 0;

static int points = 0;
static float time = 10.0f;

#define GAME_STATE_INIT 0
#define GAME_STATE_PLAY 1
#define GAME_STATE_END  2
static int game_state = GAME_STATE_INIT;

static void initMap()
{
    float distanceBetweenHexagons = 3.0f;
    float hexagonPositionOffset = (float)NUM_HEXAGONS * 0.5f * distanceBetweenHexagons;

    for (int i=0; i<NUM_HEXAGONS; i++) {
        for (int j=0; j<NUM_HEXAGONS; j++) {
            hexagon_positions[i][j].x = (float)i * distanceBetweenHexagons - hexagonPositionOffset;
            hexagon_positions[i][j].y = ((float)GetRandomValue(0, 20)) * 0.1f;
            hexagon_positions[i][j].z = (float)j * distanceBetweenHexagons - hexagonPositionOffset;
        }
    }
}

static void moveTrinketToNext() {
    int attempts = 0;
    bool found = false;

    while (attempts < 100) {
        int candidate_x = GetRandomValue(0, NUM_HEXAGONS-1);
        int candidate_y = GetRandomValue(0, NUM_HEXAGONS-1);

        if (hexagon_status[candidate_x][candidate_y] != HEX_STATUS_GOOD)
        {
            attempts++;
            continue;
        }

        trinket_hexagon_x = candidate_x;
        trinket_hexagon_y = candidate_y;
        found = true;
    }

    if (!found) {
        for (int i=0; i<NUM_HEXAGONS; i++) {
            for (int j=0; j<NUM_HEXAGONS; j++) { 
                if (hexagon_status[i][j] == HEX_STATUS_GOOD) {
                    trinket_hexagon_x = i;
                    trinket_hexagon_y = j;
                    break;
                }
            }
        }
    }
}

static void initFrame(void)
{
}

static void endFrame(void)
{
}

static void gameFrame(void)
{
    int hitHexagon_i = -1, hitHexagon_j = -1;
    if (IsMouseButtonPressed(0))
    {
        Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);

        int i_l_bound = max(0, player_hexagon_x-1);
        int j_l_bound = max(0, player_hexagon_y-1);
        int i_h_bound = min(NUM_HEXAGONS, i_l_bound+3);
        int j_h_bound = min(NUM_HEXAGONS, j_l_bound+3);
        for (int i=i_l_bound; i<i_h_bound; i++) {
            for (int j=j_l_bound; j<j_h_bound; j++) {
                Vector3 p = hexagon_positions[i][j];
                Matrix m = MatrixTranslate(p.x, p.y, p.z);
                RayCollision collision = GetRayCollisionMesh(ray, hexagon_model.meshes[0], m);

                if (collision.hit && hexagon_status[i][j] == HEX_STATUS_GOOD) {
                    hitHexagon_i = i;
                    hitHexagon_j = j;
                    printf("hit %d,%d\n",i,j);
                }
            }
        }
    }

    if (hitHexagon_i != -1 && hitHexagon_j != -1) {
        hexagon_status[player_hexagon_x][player_hexagon_y] = HEX_STATUS_BAD;
        hexagon_status[hitHexagon_i][hitHexagon_j] = HEX_STATUS_WARN;
        player_hexagon_x = hitHexagon_i;
        player_hexagon_y = hitHexagon_j;
        player_target = hexagon_positions[hitHexagon_i][hitHexagon_j];
        player_time = 0.0f;
    }

    player_position = Vector3SmoothDamp(
        player_position,
        player_target,
        &player_velocity,
        0.2f,
        100.0f,
        GetFrameTime()
    );
    player_position.y += 0.2f * player_time * (1.0f - player_time);

    if (player_time > 0.9f && player_hexagon_x == trinket_hexagon_x && player_hexagon_y == trinket_hexagon_y) {
        points++;
        time += 3.0f;
        moveTrinketToNext();
    }

    player_time = fmin(1.0f, player_time + GetFrameTime() * 2.0f);

    time -= GetFrameTime();
}

static void(*frame[])(void) = {
    [GAME_STATE_INIT] = initFrame,
    [GAME_STATE_PLAY] = gameFrame,
    [GAME_STATE_END] = endFrame
};

static void render(void)
{
    frame[game_state]();

    BeginTextureMode(target);
    ClearBackground(BLACK);

    camera.target = player_position;
    camera.position = camera.target;
    camera.position.x += cameraTarget.x;
    camera.position.y += cameraTarget.y;
    camera.position.z += cameraTarget.z;

    BeginMode3D(camera);

    for (int i=0; i<NUM_HEXAGONS; i++) {
        for (int j=0; j<NUM_HEXAGONS; j++) {
            Color hex_color = GREEN;
            if (hexagon_status[i][j] == HEX_STATUS_WARN) hex_color = ORANGE;
            if (hexagon_status[i][j] == HEX_STATUS_BAD) hex_color = RED;
            DrawModel(hexagon_model, hexagon_positions[i][j], 1.0f, hex_color);
        }
    }

    Vector3 trinket_position = hexagon_positions[trinket_hexagon_x][trinket_hexagon_y];
    trinket_position.y += 1.0f + sinf(GetTime() * 4.0f) * 0.3f;

    Matrix translation = MatrixTranslate(trinket_position.x, trinket_position.y, trinket_position.z);
    Matrix scale = MatrixScale(0.6f, 0.6f, 0.6f);
    Matrix x_rotation = MatrixRotateX(GetTime());
    Matrix y_rotation = MatrixRotateY(GetTime());
    trinket_model.transform = MatrixMultiply(scale, MatrixMultiply(x_rotation, MatrixMultiply(y_rotation, translation)));
    DrawModel(trinket_model, (Vector3){0,0,0}, 1.0f, WHITE);

    Vector3 start_position = player_position;
    start_position.y += 0.5f;
    Vector3 end_position = player_position;
    end_position.y += 2.0f;
    DrawCapsuleWires(start_position, end_position, 0.5f, 16, 3, RED);
    DrawGrid(20, 1.0f);
    EndMode3D();

    EndTextureMode();
    
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawTexturePro(
        target.texture,
        (Rectangle){ 0, 0, (float)target.texture.width, -(float)target.texture.height }, 
        (Rectangle){ 0, 0, (float)screenWidth, (float)screenWidth },
        (Vector2){ 0, 0 },
        0.0f,
        WHITE
    );

    char pts_tx[32];
    sprintf(pts_tx, "%d", points);
    DrawText(pts_tx, 10, 10, 90, ORANGE);
    DrawText(pts_tx, 12, 12, 90, RED);
    DrawText(pts_tx, 14, 14, 90, PINK);
    DrawText(pts_tx, 18, 18, 90, WHITE);

    char time_tx[32];
    sprintf(time_tx, "%.03f", time);
    int time_tx_sz = MeasureText("99.999", 90);
    int time_tx_x = screenWidth / 2 - time_tx_sz / 2;
    int time_tx_y = screenHeight - 120;
    DrawText(time_tx, time_tx_x, time_tx_y, 90, ORANGE);
    DrawText(time_tx, time_tx_x + 2, time_tx_y + 2, 90, RED);
    DrawText(time_tx, time_tx_x + 4, time_tx_y + 4, 90, PINK);
    DrawText(time_tx, time_tx_x + 8, time_tx_y + 8, 90, WHITE);

    EndDrawing();
}


int main(void)
{
#if !defined(_DEBUG)
    SetTraceLogLevel(LOG_NONE);         // Disable raylib trace log messages
#endif
    InitWindow(screenWidth, screenHeight, "raylib gamejam template");
    target = LoadRenderTexture(screenWidth / 3, screenHeight / 3);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    SetRandomSeed(GetFPS() * 10);

    hexagon_model = LoadModel("resources/hexagon.glb");
    trinket_model = LoadModel("resources/trinket.glb");

    initMap();

    trinket_hexagon_x = GetRandomValue(3, player_hexagon_x - 1);
    trinket_hexagon_y = GetRandomValue(3, player_hexagon_y - 1);
    player_position = hexagon_positions[player_hexagon_x][player_hexagon_x];
    player_target = player_position;
    hexagon_status[player_hexagon_x][player_hexagon_x] = HEX_STATUS_WARN;

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(render, 0, 1);
#else
    SetTargetFPS(60);     // Set our game frames-per-second

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button
    {
        render();
    }
#endif
    UnloadRenderTexture(target);
    CloseWindow();        // Close window and OpenGL context

    return 0;
}
