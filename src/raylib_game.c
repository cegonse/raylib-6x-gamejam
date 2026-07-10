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
#include <memory.h>

#define SUPPORT_LOG_INFO
#if defined(SUPPORT_LOG_INFO)
    #define LOG(...) printf(__VA_ARGS__)
#else
    #define LOG(...)
#endif

static const int screenWidth = 720;
static const int screenHeight = 720;
static Model hexagon_model;
static Model trinket_model;
static Model hexbot_model;
static ModelAnimation *hexbot_dance;
static ModelAnimation *hexbot_spin;
static ModelAnimation *hexbot_macarena;

#define HEX_STATUS_GOOD    0
#define HEX_STATUS_WARN    1
#define HEX_STATUS_BAD     2
#define HEX_STATUS_MERGED  3

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
static Vector3 end_camera_position = { 5.0f, 6.0f, 5.0f };

static Vector3 player_position = (Vector3){ 0.0f, 0.0f, 0.0f };
static float speed = 2.0f;
static Vector3 player_velocity = (Vector3){ 0.0f, 0.0f, 0.0f };
static Vector3 player_target = (Vector3){ 0.0f, 0.0f, 0.0f };
static int player_hexagon_x = NUM_HEXAGONS-1;
static int player_hexagon_y = NUM_HEXAGONS-1;
static float player_y_lerp_t = 1.0f;
static float player_xz_lerp_t = 0.0f;
static int trinket_hexagon_x = 0;
static int trinket_hexagon_y = 0;
static float camera_y_lerp_t = 0.0f;

#define ANIM_DANCE     0
#define ANIM_SPIN      1
#define ANIM_MACARENA  2
static int current_anim = ANIM_DANCE;
static int animation_frame = 0;

static int points = 0;
static float time = 1.0f;

#define GAME_STATE_INIT  0
#define GAME_STATE_PLAY  1
#define GAME_STATE_END   2
#define GAME_STATE_COUNT 3
static int game_state = GAME_STATE_INIT;

static float count_timer = 0.0f;
static int count_counter = 3;

static Color start_hover_color = GREEN;
static bool start_button_pressed = false;
static bool start_button_hover = false;

static RenderTexture2D trinket_ui_tex;
static RenderTexture2D hexbot_ui_tex;

static Color logo_c1 = ORANGE;
static Color logo_c2 = RED;
static Color logo_c3 = PINK;

static Rectangle play_button_rec;

static Vector2 not_found = { -1, -1 };

typedef struct {
    Vector2 position;
    Vector2 original_coords[16];
    Vector2 original_positions[16];
    int num_originals;
    float scale;
    float scale_lerp_t;
    Color color;
    float sync_offset;
    float angle;
} MergedHexagon;
#define MAX_MERGED_HEXAGONS   128
static MergedHexagon merged_hexagons[MAX_MERGED_HEXAGONS];
static int num_merged_hexagons;
static Color random_color_table[] = {
    RED,
    PINK,
    BROWN,
    BLUE,
    YELLOW,
    SKYBLUE,
    ORANGE,
    MAGENTA,
};

static int merge_text_position = 800;
static float merge_timer = 1.5f;

static Color lerpHue(Color c, float speed) {
    Vector3 c_hsv = ColorToHSV(c);
    c_hsv.x = c_hsv.x += 1.0f * speed;
    if (c_hsv.x >= 360.0f) c_hsv.x = 0.0f;
    return ColorFromHSV(c_hsv.x, c_hsv.y, c_hsv.z);
}

static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

static Vector3 vlerp(Vector3 a, Vector3 b, float t) {
    return (Vector3) {
        .x = lerp(a.x, b.x, t),
        .y = lerp(a.y, b.y, t),
        .z = lerp(a.z, b.z, t)
    };
}

static void finishGame()
{   
    animation_frame = 0;
    current_anim = ANIM_MACARENA;
    game_state = GAME_STATE_END;
}

static void initMap()
{
    float distanceBetweenHexagons = 3.0f;
    float hexagonPositionOffset = (float)NUM_HEXAGONS * 0.5f * distanceBetweenHexagons;

    for (int i=0; i<NUM_HEXAGONS; i++) {
        for (int j=0; j<NUM_HEXAGONS; j++) {
            hexagon_positions[i][j].x = (float)i * distanceBetweenHexagons - hexagonPositionOffset;
            hexagon_positions[i][j].y = ((float)GetRandomValue(0, 20)) * 0.1f;
            hexagon_positions[i][j].z = (float)j * distanceBetweenHexagons - hexagonPositionOffset;
            hexagon_status[i][j] = HEX_STATUS_GOOD;
        }
    }
}

static void initGame()
{
    player_position = (Vector3){ 0.0f, 0.0f, 0.0f };
    speed = 2.0f;
    player_velocity = (Vector3){ 0.0f, 0.0f, 0.0f };
    player_target = (Vector3){ 0.0f, 0.0f, 0.0f };
    player_hexagon_x = NUM_HEXAGONS-1;
    player_hexagon_y = NUM_HEXAGONS-1;
    player_y_lerp_t = 1.0f;
    player_xz_lerp_t = 0.0f;
    trinket_hexagon_x = 0;
    trinket_hexagon_y = 0;
    camera_y_lerp_t = 0.0f;
    count_timer = 0.0f;
    count_counter = 3;
    points = 0;
    time = 100.0f;
    current_anim = ANIM_DANCE;
    animation_frame = 0;
    end_camera_position = (Vector3){ 4.0f, 4.0f, 4.0f };
    num_merged_hexagons = 0;
    memset(merged_hexagons, 0, sizeof(merged_hexagons));
    merge_timer = 1.5f;

    initMap();

    trinket_hexagon_x = GetRandomValue(3, player_hexagon_x - 1);
    trinket_hexagon_y = GetRandomValue(3, player_hexagon_y - 1);
    player_position = hexagon_positions[player_hexagon_x][player_hexagon_x];
    player_target = player_position;
    hexagon_status[player_hexagon_x][player_hexagon_x] = HEX_STATUS_WARN;
}

static bool detectTrapped() {
    int i_l_bound = max(0, player_hexagon_x-1);
    int j_l_bound = max(0, player_hexagon_y-1);
    int i_h_bound = min(NUM_HEXAGONS, i_l_bound+3);
    int j_h_bound = min(NUM_HEXAGONS, j_l_bound+3);

    for (int i=i_l_bound; i<i_h_bound; i++) {
        for (int j=j_l_bound; j<j_h_bound; j++) {
            if (hexagon_status[i][j] == HEX_STATUS_GOOD) return false;
        }
    }

    return true;
}

static Vector2 xyMean(Vector2 *v, int c)
{
    Vector2 ret = {0};

    for (int i=0; i<c; i++) {
        ret.x += v[i].x;
        ret.y += v[i].y;
    }

    ret.x /= (float)c;
    ret.y /= (float)c;

    return ret;
}

static void mergeHexagons()
{
    for (int i=NUM_HEXAGONS-1; i>=1; i--) {
        for (int j=NUM_HEXAGONS-1; j>=1; j--) {
            int x, y;
            MergedHexagon merged = {0};

            if (hexagon_status[i][j] != HEX_STATUS_BAD) continue;

            x = i-1; y=j;
            if (hexagon_status[x][y] == HEX_STATUS_BAD) {
                merged.original_coords[merged.num_originals] = (Vector2){x,y};
                merged.original_positions[merged.num_originals] = (Vector2){hexagon_positions[x][y].x, hexagon_positions[x][y].z};
                merged.num_originals++;
            }

            x = i+1; y=j;
            if (hexagon_status[x][y] == HEX_STATUS_BAD) {
                merged.original_coords[merged.num_originals] = (Vector2){x,y};
                merged.original_positions[merged.num_originals] = (Vector2){hexagon_positions[x][y].x, hexagon_positions[x][y].z};
                merged.num_originals++;
            }

            x = i; y=j-1;
            if (hexagon_status[x][y] == HEX_STATUS_BAD) {
                merged.original_coords[merged.num_originals] = (Vector2){x,y};
                merged.original_positions[merged.num_originals] = (Vector2){hexagon_positions[x][y].x, hexagon_positions[x][y].z};
                merged.num_originals++;
            }

            x = i; y=j+1;
            if (hexagon_status[x][y] == HEX_STATUS_BAD) {
                merged.original_coords[merged.num_originals] = (Vector2){x,y};
                merged.original_positions[merged.num_originals] = (Vector2){hexagon_positions[x][y].x, hexagon_positions[x][y].z};
                merged.num_originals++;
            }

            x = i+1; y=j+1;
            if (hexagon_status[x][y] == HEX_STATUS_BAD) {
                merged.original_coords[merged.num_originals] = (Vector2){x,y};
                merged.original_positions[merged.num_originals] = (Vector2){hexagon_positions[x][y].x, hexagon_positions[x][y].z};
                merged.num_originals++;
            }

            x = i+1; y=j-1;
            if (hexagon_status[x][y] == HEX_STATUS_BAD) {
                merged.original_coords[merged.num_originals] = (Vector2){x,y};
                merged.original_positions[merged.num_originals] = (Vector2){hexagon_positions[x][y].x, hexagon_positions[x][y].z};
                merged.num_originals++;
            }

            x = i-1; y=j+1;
            if (hexagon_status[x][y] == HEX_STATUS_BAD) {
                merged.original_coords[merged.num_originals] = (Vector2){x,y};
                merged.original_positions[merged.num_originals] = (Vector2){hexagon_positions[x][y].x, hexagon_positions[x][y].z};
                merged.num_originals++;
            }

            x = i-1; y=j-1;
            if (hexagon_status[x][y] == HEX_STATUS_BAD) {
                merged.original_coords[merged.num_originals] = (Vector2){x,y};
                merged.original_positions[merged.num_originals] = (Vector2){hexagon_positions[x][y].x, hexagon_positions[x][y].z};
                merged.num_originals++;
            }

            merged.original_coords[merged.num_originals] = (Vector2){i,j};
            merged.original_positions[merged.num_originals] = (Vector2){hexagon_positions[i][j].x, hexagon_positions[i][j].z};
            merged.num_originals++;

            printf("num orig: %d\n", merged.num_originals);

            if (merged.num_originals < 4 || merged.num_originals % 2 != 0) continue;

            merged.position = xyMean(merged.original_positions, merged.num_originals);
            merged.color = random_color_table[GetRandomValue(0, 7)];
            merged.sync_offset = (float)GetRandomValue(0, 20);
            merged.angle = ((float)GetRandomValue(0, 359)) * DEG2RAD;
            
            merged_hexagons[num_merged_hexagons++] = merged;
            for (int aa=0; aa<merged.num_originals; aa++) printf("orig[%d]: %f,%f\n", aa, merged.original_positions[aa].x, merged.original_positions[aa].y);
            printf("merged pos: %f,%f\n", merged.position.x, merged.position.y);
            
            for (int i=0; i<merged.num_originals; i++) {
                hexagon_status[(int)merged.original_coords[i].x][(int)merged.original_coords[i].y] = HEX_STATUS_MERGED;
            }

            merge_timer = 0.0f;
            merge_text_position = 800;
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
                    found = true;
                    break;
                }
            }
        }
    }

    if (!found) {
        finishGame();
    }
}

static void initFrame(void)
{
    start_button_hover = CheckCollisionPointRec(GetMousePosition(), play_button_rec);

    if (start_button_hover && IsMouseButtonPressed(0))
    {
        game_state = GAME_STATE_COUNT;
    }
}

static void countFrame(void)
{
    count_timer += GetFrameTime();
    if (count_timer >= 1.0f)
    {
        count_timer = 0.0f;
        count_counter--;
    }

    if (count_counter == -1)
    {
        game_state = GAME_STATE_PLAY;
    }
}

static void endFrame(void)
{
    Vector3 up = {0,1,0};
    end_camera_position = Vector3RotateByAxisAngle(end_camera_position, up, 0.01f);
    
    if (end_camera_position.y > player_position.y + 3.0f)
    {
        end_camera_position.y -= GetFrameTime() * 2.0f;
    }

    camera.position = Vector3Add(end_camera_position, player_position);

    start_button_hover = CheckCollisionPointRec(GetMousePosition(), play_button_rec);

    if (start_button_hover && IsMouseButtonPressed(0))
    {
        initGame();
        game_state = GAME_STATE_INIT;
    }
}

static void gameFrame(void)
{
    int hitHexagon_i = -1, hitHexagon_j = -1;
    if (IsMouseButtonPressed(0) && player_y_lerp_t >= 0.8f)
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
        player_y_lerp_t = 0.0f;
        player_xz_lerp_t = 0.0f;

        current_anim = ANIM_SPIN;
        animation_frame = 0;
    }

    player_position = vlerp(player_position, player_target, player_xz_lerp_t);
    player_xz_lerp_t = fmin(1.0f, player_xz_lerp_t + GetFrameTime() * 2.0f);
    player_position.y += player_y_lerp_t * (1.0f - player_y_lerp_t);

    if (player_y_lerp_t > 0.7f && current_anim == ANIM_SPIN) {
        current_anim = ANIM_DANCE;
        animation_frame = 0;

        mergeHexagons();

        if (detectTrapped()) {
            finishGame();
        }
    }

    if (player_y_lerp_t > 0.8f && player_hexagon_x == trinket_hexagon_x && player_hexagon_y == trinket_hexagon_y) {
        points++;
        time += 5.0f;
        
        current_anim = ANIM_DANCE;
        animation_frame = 0;
        
        moveTrinketToNext();
    }

    player_y_lerp_t = fmin(1.0f, player_y_lerp_t + GetFrameTime() * 3.0f);

    time -= GetFrameTime();
    if (time <= 0.0f) {
        finishGame();
    }

    if (merge_timer <= 1.5f) {
        merge_timer += GetFrameTime();
        merge_text_position -= 1500 * GetFrameTime();
    }
}

static void(*frame[])(void) = {
    [GAME_STATE_INIT] = initFrame,
    [GAME_STATE_PLAY] = gameFrame,
    [GAME_STATE_END] = endFrame,
    [GAME_STATE_COUNT] = countFrame
};

static void render(void)
{
    frame[game_state]();

    logo_c1 = lerpHue(logo_c1, 1.0f);
    logo_c2 = lerpHue(logo_c2, 1.0f);
    logo_c3 = lerpHue(logo_c3, 1.0f);

    BeginTextureMode(target);
    ClearBackground(BLACK);

    if (game_state != GAME_STATE_END)
    {
        camera.target = player_position;
        camera.position = camera.target;
        camera.position.x += cameraTarget.x;
        camera.position.y += cameraTarget.y;
        camera.position.z += cameraTarget.z;
    }

    BeginMode3D(camera);

    for (int i=0; i<NUM_HEXAGONS; i++) {
        for (int j=0; j<NUM_HEXAGONS; j++) {
            Color hex_color = GREEN;
            if (hexagon_status[i][j] == HEX_STATUS_WARN) hex_color = ORANGE;
            if (hexagon_status[i][j] == HEX_STATUS_BAD) hex_color = RED;
            if (hexagon_status[i][j] == HEX_STATUS_MERGED) continue;

            DrawModel(hexagon_model, hexagon_positions[i][j], 1.0f, hex_color);
        }
    }

    for (int i=0; i<num_merged_hexagons; i++) {
        Vector3 pos;

        for (int j=0; j<merged_hexagons[i].num_originals; j++) {
            Vector2 pp = merged_hexagons[i].original_positions[j];
            pos = (Vector3){pp.x, 1.0f + sinf(GetTime() + merged_hexagons[i].sync_offset), pp.y};
            merged_hexagons[i].color = lerpHue(merged_hexagons[i].color, 0.3f);
    
            DrawModelEx(hexagon_model, pos, (Vector3){0}, 0.0f, (Vector3){1.5f,1,1.5f}, merged_hexagons[i].color);
        }

        pos = (Vector3){merged_hexagons[i].position.x, 1.0f + sinf(GetTime() + merged_hexagons[i].sync_offset + 15), merged_hexagons[i].position.y};
        DrawModelEx(hexagon_model, pos, (Vector3){0,1,0}, merged_hexagons[i].angle, (Vector3){1.5f,0.6,1.5f}, merged_hexagons[i].color);
        merged_hexagons[i].angle += GetFrameTime() * 50.0f;
    }

    Vector3 trinket_position = hexagon_positions[trinket_hexagon_x][trinket_hexagon_y];
    trinket_position.y += 1.0f + sinf(GetTime() * 4.0f) * 0.3f;

    Matrix translation = MatrixTranslate(trinket_position.x, trinket_position.y, trinket_position.z);
    Matrix scale = MatrixScale(0.6f, 0.6f, 0.6f);
    Matrix x_rotation = MatrixRotateX(GetTime());
    Matrix y_rotation = MatrixRotateY(GetTime());
    trinket_model.transform = MatrixMultiply(scale, MatrixMultiply(x_rotation, MatrixMultiply(y_rotation, translation)));

    if (game_state == GAME_STATE_PLAY || (game_state == GAME_STATE_END && trinket_hexagon_x != player_hexagon_x && trinket_hexagon_y != player_hexagon_y)) {
        DrawModel(trinket_model, (Vector3){0,0,0}, 1.0f, WHITE);
    }

    Vector3 start_position = player_position;
    start_position.y += 0.5f;
    Vector3 end_position = player_position;
    end_position.y += 2.0f;
    
    translation = MatrixTranslate(player_position.x, player_position.y, player_position.z);
    x_rotation = MatrixRotateX(PI/2);
    scale = MatrixScale(0.01f, 0.01f, 0.01f);
    hexbot_model.transform = MatrixMultiply(scale, MatrixMultiply(x_rotation, translation));

    ModelAnimation *animation;
    int keyframe_count = 0;
    switch (current_anim) {
        case ANIM_DANCE:
            keyframe_count = hexbot_dance[0].keyframeCount;
            animation = &hexbot_dance[0];
            break;
        case ANIM_SPIN:
            keyframe_count = hexbot_spin[0].keyframeCount;
            animation = &hexbot_spin[0];
            break;
        case ANIM_MACARENA:
            keyframe_count = hexbot_macarena[0].keyframeCount;
            animation = &hexbot_macarena[0];
            break;
    }

    animation_frame = (animation_frame + 2) % keyframe_count;
    UpdateModelAnimation(hexbot_model, *animation, animation_frame);
    DrawModel(hexbot_model, (Vector3){0,0,0}, 1.0f, WHITE);

    DrawGrid(20, 1.0f);
    EndMode3D();
    EndTextureMode();

    Camera trinket_cam = {
        .position = (Vector3){2,2,2},
        .target = (Vector3){0,0,0},
        .up = (Vector3){1,0,0},
        .fovy = 45.0f,
        .projection = CAMERA_PERSPECTIVE
    };
    BeginTextureMode(trinket_ui_tex);
    Color color_transp = WHITE;
    color_transp.a = 0;
    ClearBackground(color_transp);
    BeginMode3D(trinket_cam);
    translation = MatrixTranslate(0,0,0);
    scale = MatrixIdentity();
    x_rotation = MatrixRotateX(GetTime());
    y_rotation = MatrixRotateY(GetTime());
    trinket_model.transform = MatrixMultiply(scale, MatrixMultiply(x_rotation, MatrixMultiply(y_rotation, translation)));
    DrawModel(trinket_model, (Vector3){0,0,0}, 1.0f, WHITE);
    EndMode3D();
    EndTextureMode();

    BeginTextureMode(hexbot_ui_tex);
    ClearBackground(color_transp);
    trinket_cam.position.x = 0.0f;
    trinket_cam.position.y = 0.0f;
    trinket_cam.position.z = 5.0f;
    trinket_cam.target.y = 2.0f;
    BeginMode3D(trinket_cam);
    translation = MatrixTranslate(0,0,0);
    scale = MatrixIdentity();
    x_rotation = MatrixRotateX(PI/2);
    // y_rotation = MatrixRotateZ(GetTime());
    scale = MatrixScale(0.02f, 0.02f, 0.02f);
    hexbot_model.transform = MatrixMultiply(scale, MatrixMultiply(x_rotation, translation));
    DrawModel(hexbot_model, (Vector3){0,0,0}, 1.0f, WHITE);
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

    if (game_state == GAME_STATE_PLAY)
    {
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

        Color trinket_ui_pts_color = WHITE;
        trinket_ui_pts_color.a = 160;
        DrawTexture(trinket_ui_tex.texture, 20 + MeasureText(pts_tx, 90), 10, trinket_ui_pts_color);

        if (merge_text_position < 800) {
            DrawText("MEEEERGE!!!!!", merge_text_position,     sinf(GetTime() * 5)*50 + 150, 200,     logo_c1);
            DrawText("MEEEERGE!!!!!", merge_text_position + 2, sinf(GetTime() * 5)*50 + 150 + 2, 200, logo_c2);
            DrawText("MEEEERGE!!!!!", merge_text_position + 4, sinf(GetTime() * 5)*50 + 150 + 4, 200, logo_c3);
            DrawText("MEEEERGE!!!!!", merge_text_position + 8, sinf(GetTime() * 5)*50 + 150 + 8, 200, WHITE);
        }
    }

    if (game_state == GAME_STATE_INIT)
    {
        int logo_x = screenWidth - MeasureText("HEXBOT", 144) - 60;
        int logo_y = 60;

        DrawText("HEXBOT", logo_x, logo_y, 144, logo_c1);
        DrawText("HEXBOT", logo_x + 2, logo_y + 2, 144, logo_c2);
        DrawText("HEXBOT", logo_x + 4, logo_y + 4, 144, logo_c3);
        DrawText("HEXBOT", logo_x + 8, logo_y + 8, 144, WHITE);

        DrawText("© cesargonzalez.dev 2026 - raylib 6.x gamejam", 10, screenHeight - 24, 18, WHITE);
    }
    
    if (game_state == GAME_STATE_INIT || game_state == GAME_STATE_END)
    {
        Rectangle rec = play_button_rec;

        if (start_button_hover)
        {
            start_hover_color = lerpHue(start_hover_color, 1.0f);
            start_hover_color.a = 120;
            DrawRectangleRounded(rec, 0.5f, 16, start_hover_color);
        }

        DrawRectangleRoundedLinesEx(rec, 0.5f, 16, 3.0f, ORANGE);
        rec.x += 3; rec.y += 3;
        DrawRectangleRoundedLinesEx(rec, 0.5f, 16, 3.0f, RED);
        rec.x += 3; rec.y += 3;
        DrawRectangleRoundedLinesEx(rec, 0.5f, 16, 3.0f, PINK);
        rec.x += 3; rec.y += 3;
        DrawRectangleRoundedLinesEx(rec, 0.5f, 16, 3.0f, WHITE);

        int play_tx_x = rec.x + 60;
        int play_tx_y = rec.y;
        DrawText("PLAY", play_tx_x, play_tx_y, 100, ORANGE);
        DrawText("PLAY", play_tx_x + 2, play_tx_y + 2, 100, RED);
        DrawText("PLAY", play_tx_x + 4, play_tx_y + 4, 100, PINK);
        DrawText("PLAY", play_tx_x + 8, play_tx_y + 8, 100, WHITE);
    }

    if (game_state == GAME_STATE_END)
    {
        char score_tx[128];
        sprintf(score_tx, "GOT %d", points);
        int score_tx_sz = MeasureText(score_tx, 100);
        int score_tx_x = screenWidth / 2 - score_tx_sz / 2 - 50;
        int score_tx_y = screenHeight / 2 + 20;
        DrawText(score_tx, score_tx_x, score_tx_y, 100, ORANGE);
        DrawText(score_tx, score_tx_x + 2, score_tx_y + 2, 100, RED);
        DrawText(score_tx, score_tx_x + 4, score_tx_y + 4, 100, PINK);
        DrawText(score_tx, score_tx_x + 8, score_tx_y + 8, 100, WHITE);
        DrawTexture(trinket_ui_tex.texture, score_tx_x + score_tx_sz + 20, score_tx_y, WHITE);

        char merged_tx[128];
        sprintf(merged_tx, "MERGED %d", num_merged_hexagons);
        int merged_tx_sz = MeasureText(merged_tx, 100);
        int merged_tx_x = screenWidth / 2 - merged_tx_sz / 2;
        int merged_tx_y = 40;
        DrawText(merged_tx, merged_tx_x, merged_tx_y, 100, ORANGE);
        DrawText(merged_tx, merged_tx_x + 2, merged_tx_y + 2, 100, RED);
        DrawText(merged_tx, merged_tx_x + 4, merged_tx_y + 4, 100, PINK);
        DrawText(merged_tx, merged_tx_x + 8, merged_tx_y + 8, 100, WHITE);
    }

    if (game_state == GAME_STATE_COUNT)
    {
        char counter_tx[32];
        
        count_counter > 0 ?
            sprintf(counter_tx, "%d", count_counter) :
            sprintf(counter_tx, "GO!");

        int counter_tx_sz = MeasureText(counter_tx, 160);
        int counter_tx_x = screenWidth / 2 - counter_tx_sz / 2;
        int counter_tx_y = screenHeight / 2 + 40;
        DrawText(counter_tx, counter_tx_x, counter_tx_y, 160, ORANGE);
        DrawText(counter_tx, counter_tx_x + 2, counter_tx_y + 2, 160, RED);
        DrawText(counter_tx, counter_tx_x + 4, counter_tx_y + 4, 160, PINK);
        DrawText(counter_tx, counter_tx_x + 8, counter_tx_y + 8, 160, WHITE);
    }

    EndDrawing();
}


int main(void)
{
#if !defined(_DEBUG)
    SetTraceLogLevel(LOG_NONE);         // Disable raylib trace log messages
#endif
    InitWindow(screenWidth, screenHeight, "raylib gamejam template");
    SetTargetFPS(60);
    target = LoadRenderTexture(screenWidth / 3, screenHeight / 3);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    trinket_ui_tex = LoadRenderTexture(100, 100);
    hexbot_ui_tex = LoadRenderTexture(128, 128);
    SetTextureFilter(trinket_ui_tex.texture, TEXTURE_FILTER_POINT);

    SetRandomSeed(GetFPS() * 10);

    int num_animations = 0;
    hexagon_model = LoadModel("resources/hexagon.glb");
    trinket_model = LoadModel("resources/trinket.glb");
    hexbot_model = LoadModel("resources/dance.glb");
    hexbot_dance = LoadModelAnimations("resources/dance.glb", &num_animations);
    hexbot_spin = LoadModelAnimations("resources/spinning.glb", &num_animations);
    hexbot_macarena = LoadModelAnimations("resources/macarena.glb", &num_animations);
    play_button_rec = (Rectangle){
        .x = screenWidth / 2 - 200 - 12,
        .y = screenHeight - 200,
        .width = 400,
        .height = 100
    };

    initGame();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(render, 60, 1);
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
