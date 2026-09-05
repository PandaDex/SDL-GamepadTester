#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Gamepad *gamepad = NULL;

static bool rumble_test_mode = false;
static bool rumble_combo_active = false;
static bool rumble_combo_fired = false;
static Uint64 rumble_combo_start = 0;

#define SCREEN_W 1600
#define SCREEN_H 900
#define RUMBLE_COMBO_HOLD_MS 700

static void DrawCircle(SDL_Renderer *renderer, float cx, float cy, float radius)
{
    const int segments = 40;
    SDL_FPoint points[41];
    int i;
    for (i = 0; i <= segments; i++)
    {
        const float angle = ((float)i / (float)segments) * 2.0f * SDL_PI_F;
        points[i].x = cx + radius * SDL_cosf(angle);
        points[i].y = cy + radius * SDL_sinf(angle);
    }
    SDL_RenderLines(renderer, points, segments + 1);
}

static void FillCircle(SDL_Renderer *renderer, float cx, float cy, float radius)
{
    float dy;
    for (dy = -radius; dy <= radius; dy += 1.0f)
    {
        const float dx = SDL_sqrtf(radius * radius - dy * dy);
        SDL_RenderLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void DrawCenteredText(SDL_Renderer *renderer, float cx, float cy, const char *text)
{
    const float x = cx - (SDL_strlen(text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) / 2.0f;
    const float y = cy - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE / 2.0f;
    SDL_RenderDebugText(renderer, x, y, text);
}

static void DrawRoundButton(SDL_Renderer *renderer, float cx, float cy, float radius, bool pressed, const char *label)
{
    if (pressed)
    {
        SDL_SetRenderDrawColor(renderer, 0x30, 0xE0, 0x30, 0xFF);
        FillCircle(renderer, cx, cy, radius);
    }
    SDL_SetRenderDrawColor(renderer, 0xC0, 0xC0, 0xC0, 0xFF);
    DrawCircle(renderer, cx, cy, radius);
    if (label)
    {
        if (pressed)
        {
            SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
        }
        else
        {
            SDL_SetRenderDrawColor(renderer, 0xC0, 0xC0, 0xC0, 0xFF);
        }
        DrawCenteredText(renderer, cx, cy, label);
    }
}

static void DrawSquareButton(SDL_Renderer *renderer, float cx, float cy, float half, bool pressed, const char *label)
{
    SDL_FRect rect;
    rect.x = cx - half;
    rect.y = cy - half;
    rect.w = half * 2.0f;
    rect.h = half * 2.0f;

    if (pressed)
    {
        SDL_SetRenderDrawColor(renderer, 0x30, 0xE0, 0x30, 0xFF);
        SDL_RenderFillRect(renderer, &rect);
    }
    SDL_SetRenderDrawColor(renderer, 0xC0, 0xC0, 0xC0, 0xFF);
    SDL_RenderRect(renderer, &rect);
    if (label)
    {
        if (pressed)
        {
            SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
        }
        else
        {
            SDL_SetRenderDrawColor(renderer, 0xC0, 0xC0, 0xC0, 0xFF);
        }
        DrawCenteredText(renderer, cx, cy, label);
    }
}

static void DrawStick(SDL_Renderer *renderer, float cx, float cy, float radius,
                      Sint16 axis_x, Sint16 axis_y, bool clicked, const char *label)
{
    const float nx = SDL_clamp(axis_x / 32767.0f, -1.0f, 1.0f);
    const float ny = SDL_clamp(axis_y / 32767.0f, -1.0f, 1.0f);
    const float dot_radius = radius * 0.22f;
    const float travel = radius - dot_radius;
    const float dot_x = cx + nx * travel;
    const float dot_y = cy + ny * travel;

    /* outer ring turns green while the stick is clicked (L3/R3) */
    if (clicked)
    {
        SDL_SetRenderDrawColor(renderer, 0x30, 0xE0, 0x30, 0xFF);
    }
    else
    {
        SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xFF);
    }
    DrawCircle(renderer, cx, cy, radius);

    /* crosshair */
    SDL_SetRenderDrawColor(renderer, 0x40, 0x40, 0x40, 0xFF);
    SDL_RenderLine(renderer, cx - radius, cy, cx + radius, cy);
    SDL_RenderLine(renderer, cx, cy - radius, cx, cy + radius);

    /* miiddle dot */
    SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x00, 0xFF);
    FillCircle(renderer, dot_x, dot_y, dot_radius);
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    DrawCircle(renderer, dot_x, dot_y, dot_radius);

    if (label)
    {
        SDL_SetRenderDrawColor(renderer, 0xC0, 0xC0, 0xC0, 0xFF);
        DrawCenteredText(renderer, cx, cy + radius + 16.0f, label);
    }
}

static void DrawTrigger(SDL_Renderer *renderer, float cx, float top, float w, float h, Sint16 value, const char *label)
{
    SDL_FRect outline, fill;
    const float frac = SDL_clamp(value / 32767.0f, 0.0f, 1.0f);

    outline.x = cx - w / 2.0f;
    outline.y = top;
    outline.w = w;
    outline.h = h;

    fill.x = outline.x;
    fill.w = outline.w;
    fill.h = h * frac;
    fill.y = top + (h - fill.h);

    SDL_SetRenderDrawColor(renderer, 0xFF, 0x60, 0x20, 0xFF);
    SDL_RenderFillRect(renderer, &fill);

    SDL_SetRenderDrawColor(renderer, 0xC0, 0xC0, 0xC0, 0xFF);
    SDL_RenderRect(renderer, &outline);

    if (label)
    {
        DrawCenteredText(renderer, cx, top + h + 16.0f, label);
    }
}

static void DrawRumbleBar(SDL_Renderer *renderer, float cx, float top, float w, float h, Uint16 value, const char *label)
{
    SDL_FRect outline, fill;
    const float frac = (float)value / 65535.0f;

    outline.x = cx - w / 2.0f;
    outline.y = top;
    outline.w = w;
    outline.h = h;

    fill.x = outline.x;
    fill.w = outline.w;
    fill.h = h * frac;
    fill.y = top + (h - fill.h);

    SDL_SetRenderDrawColor(renderer, 0xFF, 0xD0, 0x00, 0xFF);
    SDL_RenderFillRect(renderer, &fill);

    SDL_SetRenderDrawColor(renderer, 0xC0, 0xC0, 0xC0, 0xFF);
    SDL_RenderRect(renderer, &outline);

    if (label)
    {
        DrawCenteredText(renderer, cx, top + h + 16.0f, label);
    }
}

static void DrawSingleTouchpad(SDL_Renderer *renderer, SDL_Gamepad *gamepad, int touchpad_index,
                               float cx, float top, float w, float h, bool clicked, const char *label)
{
    SDL_FRect rect;
    const int num_fingers = SDL_GetNumGamepadTouchpadFingers(gamepad, touchpad_index);
    int f;

    rect.x = cx - w / 2.0f;
    rect.y = top;
    rect.w = w;
    rect.h = h;

    if (clicked)
    {
        SDL_SetRenderDrawColor(renderer, 0x30, 0xE0, 0x30, 0xFF);
    }
    else
    {
        SDL_SetRenderDrawColor(renderer, 0x60, 0x60, 0x60, 0xFF);
    }
    SDL_RenderRect(renderer, &rect);

    for (f = 0; f < num_fingers; f++)
    {
        bool down = false;
        float fx = 0.0f, fy = 0.0f, pressure = 0.0f;
        if (SDL_GetGamepadTouchpadFinger(gamepad, touchpad_index, f, &down, &fx, &fy, &pressure) && down)
        {
            /* finger coords are normalized 0..1 across the touchpad surface */
            const float px = rect.x + fx * rect.w;
            const float py = rect.y + fy * rect.h;
            /* let pressure influence the dot size a little, when the device reports it */
            const float dot_r = 6.0f + pressure * 6.0f;
            SDL_SetRenderDrawColor(renderer, 0xFF, 0xD0, 0x00, 0xFF);
            FillCircle(renderer, px, py, dot_r);
        }
    }

    SDL_SetRenderDrawColor(renderer, 0xC0, 0xC0, 0xC0, 0xFF);
    DrawCenteredText(renderer, cx, top + h + 16.0f, label);
}

static void DrawTouchpads(SDL_Renderer *renderer, SDL_Gamepad *gamepad, float cx, float top, float total_w, float h)
{
    const int num_touchpads = SDL_GetNumGamepadTouchpads(gamepad);
    const bool clicked = SDL_GamepadHasButton(gamepad, SDL_GAMEPAD_BUTTON_TOUCHPAD) &&
                         SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_TOUCHPAD);
    const float gap = 30.0f;
    float pad_w, start_x;
    int t;

    if (num_touchpads <= 0)
    {
        return;
    }

    pad_w = (total_w - gap * (num_touchpads - 1)) / (float)num_touchpads;
    start_x = cx - total_w / 2.0f + pad_w / 2.0f;

    for (t = 0; t < num_touchpads; t++)
    {
        const float pad_cx = start_x + t * (pad_w + gap);
        char label[16];
        if (num_touchpads == 1)
        {
            SDL_strlcpy(label, "Touchpad", sizeof(label));
        }
        else if (num_touchpads == 2)
        {
            SDL_strlcpy(label, t == 0 ? "L Pad" : "R Pad", sizeof(label));
        }
        else
        {
            SDL_snprintf(label, sizeof(label), "Pad %d", t + 1);
        }
        DrawSingleTouchpad(renderer, gamepad, t, pad_cx, top, pad_w, h, clicked, label);
    }
}

static void DrawPaddleIfPresent(SDL_Renderer *renderer, SDL_Gamepad *gamepad, SDL_GamepadButton button,
                                float cx, float cy, float half, const char *label)
{
    if (SDL_GamepadHasButton(gamepad, button))
    {
        DrawSquareButton(renderer, cx, cy, half, SDL_GetGamepadButton(gamepad, button), label);
    }
}

static void UpdateRumbleTestCombo(SDL_Gamepad *gamepad)
{
    const bool back_held = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_BACK);
    const bool start_held = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START);
    const bool combo_held = back_held && start_held;

    if (!combo_held)
    {
        rumble_combo_active = false;
        return;
    }

    if (!rumble_combo_active)
    {
        /* combo just started being held - start the timer */
        rumble_combo_active = true;
        rumble_combo_fired = false;
        rumble_combo_start = SDL_GetTicks();
        return;
    }

    if (!rumble_combo_fired && (SDL_GetTicks() - rumble_combo_start) >= RUMBLE_COMBO_HOLD_MS)
    {
        rumble_combo_fired = true;
        rumble_test_mode = !rumble_test_mode;
        if (!rumble_test_mode)
        {
            /* leaving rumble test mode - make sure everything is off */
            SDL_RumbleGamepad(gamepad, 0, 0, 0);
            SDL_RumbleGamepadTriggers(gamepad, 0, 0, 0);
        }
    }
}

static void UpdateAndDrawRumbleTest(SDL_Renderer *renderer, SDL_Gamepad *gamepad)
{
    const bool btn_a = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
    const bool btn_b = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_EAST);
    const bool btn_x = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_WEST);
    const bool btn_y = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_NORTH);
    const Sint16 lt = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    const Sint16 rt = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    const Uint16 lt_rumble = (Uint16)(SDL_clamp(lt, 0, 32767) * 2);
    const Uint16 rt_rumble = (Uint16)(SDL_clamp(rt, 0, 32767) * 2);
    Uint16 low_freq = 0;
    Uint16 high_freq = 0;

    if (btn_y)
    {
        /* explicit stop, takes priority over the other face buttons */
    }
    else if (btn_x)
    {
        low_freq = 0xFFFF;
        high_freq = 0xFFFF;
    }
    else if (btn_a)
    {
        low_freq = 0xFFFF;
    }
    else if (btn_b)
    {
        high_freq = 0xFFFF;
    }

    SDL_RumbleGamepad(gamepad, low_freq, high_freq, 100);
    SDL_RumbleGamepadTriggers(gamepad, lt_rumble, rt_rumble, 100);

    SDL_SetRenderDrawColor(renderer, 0xFF, 0xD0, 0x00, 0xFF);
    DrawCenteredText(renderer, SCREEN_W / 2.0f, 60.0f, "RUMBLE TEST MODE");

    SDL_SetRenderDrawColor(renderer, 0xC0, 0xC0, 0xC0, 0xFF);
    DrawCenteredText(renderer, SCREEN_W / 2.0f, 100.0f, "A/Cross: Low Motor   B/Circle: High Motor   X/Square: Both Motors   Y/Triangle: Stop");
    DrawCenteredText(renderer, SCREEN_W / 2.0f, 124.0f, "LT / RT: Adaptive Trigger Rumble (Xbox Only)");

    {
        const float bar_w = 70.0f, bar_h = 300.0f, top = 260.0f;
        DrawRumbleBar(renderer, 420.0f, top, bar_w, bar_h, low_freq, "Low Freq");
        DrawRumbleBar(renderer, 540.0f, top, bar_w, bar_h, high_freq, "High Freq");
        DrawRumbleBar(renderer, 1060.0f, top, bar_w, bar_h, lt_rumble, "L Trigger");
        DrawRumbleBar(renderer, 1180.0f, top, bar_w, bar_h, rt_rumble, "R Trigger");
    }

    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xFF);
    DrawCenteredText(renderer, SCREEN_W / 2.0f, SCREEN_H - 60.0f, "Hold BACK + START to exit Rumble Test");
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_SetAppMetadata("Gamepad Input Tester", "1.0", "com.pandadex.gamepad-input");

    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK, "1");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("Gamepad Input Tester", SCREEN_W, SCREEN_H, SDL_WINDOW_RESIZABLE, &window, &renderer))
    {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(renderer, SCREEN_W, SCREEN_H, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT)
    {
        return SDL_APP_SUCCESS;
    }
    else if (event->type == SDL_EVENT_GAMEPAD_ADDED)
    {
        if (gamepad == NULL)
        {
            gamepad = SDL_OpenGamepad(event->gdevice.which);
            if (!gamepad)
            {
                SDL_Log("Failed to open gamepad ID %u: %s", (unsigned int)event->gdevice.which, SDL_GetError());
                return SDL_APP_CONTINUE;
            }
            if (SDL_GetNumGamepadTouchpads(gamepad) > 0)
            {
                SDL_Log("Gamepad '%s' has %d touchpad(s)", SDL_GetGamepadName(gamepad), SDL_GetNumGamepadTouchpads(gamepad));
            }
        }
    }
    else if (event->type == SDL_EVENT_GAMEPAD_REMOVED)
    {
        if (gamepad && (SDL_GetGamepadID(gamepad) == event->gdevice.which))
        {
            SDL_CloseGamepad(gamepad);
            gamepad = NULL;
            rumble_test_mode = false;
            rumble_combo_active = false;
            rumble_combo_fired = false;
        }
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    const char *text = "Plug in a gamepad, please.";
    float x, y;

    if (gamepad)
    {
        text = SDL_GetGamepadName(gamepad);
    }

    SDL_SetRenderDrawColor(renderer, 0x18, 0x18, 0x18, 0xFF);
    SDL_RenderClear(renderer);

    if (gamepad)
    {
        UpdateRumbleTestCombo(gamepad);
    }

    if (gamepad && rumble_test_mode)
    {
        UpdateAndDrawRumbleTest(renderer, gamepad);
    }
    else if (gamepad)
    {
        const Sint16 lx = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX);
        const Sint16 ly = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY);
        const Sint16 rx = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX);
        const Sint16 ry = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY);
        const Sint16 lt = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
        const Sint16 rt = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

        /* analog sticks */
        DrawStick(renderer, 300.0f, 340.0f, 100.0f, lx, ly,
                  SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK), "Left Stick (L3)");
        DrawStick(renderer, 1300.0f, 340.0f, 100.0f, rx, ry,
                  SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_RIGHT_STICK), "Right Stick (R3)");

        /* triggers */
        DrawTrigger(renderer, 500.0f, 180.0f, 60.0f, 140.0f, lt, "LT");
        DrawTrigger(renderer, 1100.0f, 180.0f, 60.0f, 140.0f, rt, "RT");

        /* shoulder buttons */
        DrawSquareButton(renderer, 500.0f, 420.0f, 34.0f,
                         SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER), "LB");
        DrawSquareButton(renderer, 1100.0f, 420.0f, 34.0f,
                         SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER), "RB");

        /* face buttons */
        {
            const float fx = 1150.0f, fy = 600.0f, spacing = 55.0f, r = 26.0f;
            DrawRoundButton(renderer, fx, fy + spacing, r, SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH), "A");
            DrawRoundButton(renderer, fx + spacing, fy, r, SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_EAST), "B");
            DrawRoundButton(renderer, fx - spacing, fy, r, SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_WEST), "X");
            DrawRoundButton(renderer, fx, fy - spacing, r, SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_NORTH), "Y");
        }

        /* d-pad */
        {
            const float dx = 450.0f, dy = 600.0f, spacing = 46.0f, half = 20.0f;
            DrawSquareButton(renderer, dx, dy - spacing, half, SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP), "^");
            DrawSquareButton(renderer, dx, dy + spacing, half, SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN), "v");
            DrawSquareButton(renderer, dx - spacing, dy, half, SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT), "<");
            DrawSquareButton(renderer, dx + spacing, dy, half, SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT), ">");
        }

        /* start / back / logo  */
        DrawRoundButton(renderer, 750.0f, 120.0f, 22.0f, SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_BACK), "Back");
        DrawRoundButton(renderer, 800.0f, 120.0f, 26.0f, SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_GUIDE), "Logo");
        DrawRoundButton(renderer, 850.0f, 120.0f, 22.0f, SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START), "Start");

        /* -- touchpad (DualShock, DualSense) / touchpads (Steam Deck, Steam Controller) */
        DrawTouchpads(renderer, gamepad, 800.0f, 680.0f, 460.0f, 150.0f);

        /* -- rear paddles / back grip buttons (Xbox Elite, DualSense Edge, Steam Deck) */
        DrawPaddleIfPresent(renderer, gamepad, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1, 110.0f, 260.0f, 26.0f, "L4");
        DrawPaddleIfPresent(renderer, gamepad, SDL_GAMEPAD_BUTTON_LEFT_PADDLE2, 110.0f, 320.0f, 26.0f, "L5");
        DrawPaddleIfPresent(renderer, gamepad, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, 1490.0f, 260.0f, 26.0f, "R4");
        DrawPaddleIfPresent(renderer, gamepad, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, 1490.0f, 320.0f, 26.0f, "R5");

        /* hint for entering rumble test mode */
        SDL_SetRenderDrawColor(renderer, 0x60, 0x60, 0x60, 0xFF);
        DrawCenteredText(renderer, SCREEN_W / 2.0f, 40.0f, "Hold BACK + START for Rumble Test");
    }

    if (!rumble_test_mode)
    {
        x = (((float)SCREEN_W) - (SDL_strlen(text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE)) / 2.0f;
        if (gamepad)
        {
            y = (float)(SCREEN_H - (SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 2));
        }
        else
        {
            y = (((float)SCREEN_H / 2) - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) / 2.0f;
        }
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
        SDL_RenderDebugText(renderer, x, y, text);
    }

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
}