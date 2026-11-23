
#include "render.h"
#include "tft.h"

#include <stdio.h>
#include <stdint.h>

#define SCREEN_W 240
#define SCREEN_H 320

// Playfield layout
#define FIELD_X       8
#define FIELD_TOP     52
#define FIELD_BOTTOM  275
#define HIT_LINE_Y    265
#define FIELD_W       224
#define LANE_COUNT    4
#define LANE_W        (FIELD_W / LANE_COUNT)

// Note appearance
#define NOTE_W        (LANE_W - 12)
#define TAP_HEIGHT    12
#define HOLD_MIN_H    8

// Colors
#define BG_COLOR       0x0841
#define PANEL_COLOR    0x1082
#define LANE_BG_1      0x10A3
#define LANE_BG_2      0x0862
#define DIVIDER_COLOR  0x3186

#define LANE_CYAN      0x07FF
#define LANE_PINK      0xF81F
#define LANE_YELLOW    0xFFE0
#define LANE_PURPLE    0xA01F

#define TEXT_COLOR     TFT_WHITE
#define HIT_COLOR      TFT_WHITE

#define MAX_RENDERED_NOTES MAX_NOTE_VIEWS

typedef struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint8_t lane;
} rendered_note_t;

static const uint16_t lane_colors[LANE_COUNT] = {
    LANE_CYAN,
    LANE_PINK,
    LANE_YELLOW,
    LANE_PURPLE
};

static rendered_note_t previous_notes[MAX_RENDERED_NOTES];
static int previous_count = 0;

// --------------------------------------------------
// Small 5x7 font
// Each byte represents one vertical column.
// --------------------------------------------------

static const uint8_t *get_glyph(char c)
{
    static const uint8_t blank[5] = {0, 0, 0, 0, 0};

    static const uint8_t digits[10][5] = {
        {0x3E,0x51,0x49,0x45,0x3E}, // 0
        {0x00,0x42,0x7F,0x40,0x00}, // 1
        {0x42,0x61,0x51,0x49,0x46}, // 2
        {0x21,0x41,0x45,0x4B,0x31}, // 3
        {0x18,0x14,0x12,0x7F,0x10}, // 4
        {0x27,0x45,0x45,0x45,0x39}, // 5
        {0x3C,0x4A,0x49,0x49,0x30}, // 6
        {0x01,0x71,0x09,0x05,0x03}, // 7
        {0x36,0x49,0x49,0x49,0x36}, // 8
        {0x06,0x49,0x49,0x29,0x1E}  // 9
    };

    static const uint8_t letters[26][5] = {
        {0x7E,0x11,0x11,0x11,0x7E}, // A
        {0x7F,0x49,0x49,0x49,0x36}, // B
        {0x3E,0x41,0x41,0x41,0x22}, // C
        {0x7F,0x41,0x41,0x22,0x1C}, // D
        {0x7F,0x49,0x49,0x49,0x41}, // E
        {0x7F,0x09,0x09,0x09,0x01}, // F
        {0x3E,0x41,0x49,0x49,0x7A}, // G
        {0x7F,0x08,0x08,0x08,0x7F}, // H
        {0x00,0x41,0x7F,0x41,0x00}, // I
        {0x20,0x40,0x41,0x3F,0x01}, // J
        {0x7F,0x08,0x14,0x22,0x41}, // K
        {0x7F,0x40,0x40,0x40,0x40}, // L
        {0x7F,0x02,0x0C,0x02,0x7F}, // M
        {0x7F,0x04,0x08,0x10,0x7F}, // N
        {0x3E,0x41,0x41,0x41,0x3E}, // O
        {0x7F,0x09,0x09,0x09,0x06}, // P
        {0x3E,0x41,0x51,0x21,0x5E}, // Q
        {0x7F,0x09,0x19,0x29,0x46}, // R
        {0x46,0x49,0x49,0x49,0x31}, // S
        {0x01,0x01,0x7F,0x01,0x01}, // T
        {0x3F,0x40,0x40,0x40,0x3F}, // U
        {0x1F,0x20,0x40,0x20,0x1F}, // V
        {0x3F,0x40,0x38,0x40,0x3F}, // W
        {0x63,0x14,0x08,0x14,0x63}, // X
        {0x07,0x08,0x70,0x08,0x07}, // Y
        {0x61,0x51,0x49,0x45,0x43}  // Z
    };

    if (c >= '0' && c <= '9')
        return digits[c - '0'];

    if (c >= 'A' && c <= 'Z')
        return letters[c - 'A'];

    return blank;
}

// Draw a character using horizontal pixel runs.
static void draw_char(int x, int y, char c,
                      uint16_t color, int scale)
{
    const uint8_t *glyph = get_glyph(c);

    for (int row = 0; row < 7; row++) {
        int col = 0;

        while (col < 5) {
            int bits[5];

            for (int i = 0; i < 5; i++)
                bits[i] = (glyph[i] >> row) & 1;

            if (!bits[col]) {
                col++;
                continue;
            }

            int start = col;

            while (col < 5 && bits[col])
                col++;

            tft_fill_rect(
                x + start * scale,
                y + row * scale,
                (col - start) * scale,
                scale,
                color
            );
        }
    }
}

static void draw_text(int x, int y, const char *text,
                      uint16_t color, int scale)
{
    while (*text) {
        draw_char(x, y, *text, color, scale);
        x += 6 * scale;
        text++;
    }
}

// --------------------------------------------------
// Static screen drawing
// --------------------------------------------------

static void draw_background(void)
{
    tft_fill_screen(BG_COLOR);

    // Top information panel
    tft_fill_rect(0, 0, SCREEN_W, 47, PANEL_COLOR);

    // Four lane backgrounds
    for (int lane = 0; lane < LANE_COUNT; lane++) {
        int x = FIELD_X + lane * LANE_W;

        uint16_t bg = (lane % 2 == 0)
                    ? LANE_BG_1 : LANE_BG_2;

        tft_fill_rect(
            x,
            FIELD_TOP,
            LANE_W,
            FIELD_BOTTOM - FIELD_TOP,
            bg
        );
    }

    // Lane dividers
    for (int i = 0; i <= LANE_COUNT; i++) {
        int x = FIELD_X + i * LANE_W;

        tft_fill_rect(
            x,
            FIELD_TOP,
            1,
            FIELD_BOTTOM - FIELD_TOP,
            DIVIDER_COLOR
        );
    }

    // Hit receptors
    for (int lane = 0; lane < LANE_COUNT; lane++) {
        int x = FIELD_X + lane * LANE_W + 6;

        tft_fill_rect(x, HIT_LINE_Y - 3, NOTE_W, 2,
                      lane_colors[lane]);

        tft_fill_rect(x, HIT_LINE_Y + 3, NOTE_W, 2,
                      lane_colors[lane]);
    }

    // Bottom judgement panel
    tft_fill_rect(0, 282, SCREEN_W, 38, PANEL_COLOR);
}

static uint16_t lane_background(uint8_t lane)
{
    return (lane % 2 == 0) ? LANE_BG_1 : LANE_BG_2;
}

// --------------------------------------------------
// Note positioning
// --------------------------------------------------

static int permille_to_y(int16_t value)
{
    if (value < 0)
        value = 0;

    int range = HIT_LINE_Y - FIELD_TOP;

    return FIELD_TOP + ((int)value * range) / 1000;
}

static rendered_note_t make_note_rect(const note_view_t *note)
{
    rendered_note_t r = {0};

    r.lane = note->lane;
    r.x = FIELD_X + note->lane * LANE_W + 6;
    r.w = NOTE_W;

    int head_y = permille_to_y(note->y_head);

    if (note->is_hold) {
        int tail_y = permille_to_y(note->y_tail);

        r.y = tail_y;
        r.h = head_y - tail_y;

        if (r.h < HOLD_MIN_H)
            r.h = HOLD_MIN_H;
    } else {
        // The head is the edge closest to the hit line.
        r.y = head_y - TAP_HEIGHT;
        r.h = TAP_HEIGHT;
    }

    // Clip to the playfield.
    if (r.y < FIELD_TOP) {
        r.h -= FIELD_TOP - r.y;
        r.y = FIELD_TOP;
    }

    if (r.y + r.h > FIELD_BOTTOM)
        r.h = FIELD_BOTTOM - r.y;

    return r;
}

static void draw_note(const note_view_t *note,
                      const rendered_note_t *r)
{
    if (r->h <= 0)
        return;

    uint16_t color = lane_colors[note->lane];

    tft_fill_rect(r->x, r->y, r->w, r->h, color);

    // White outline
    tft_fill_rect(r->x, r->y, r->w, 1, TFT_WHITE);
    tft_fill_rect(r->x, r->y + r->h - 1, r->w, 1, TFT_WHITE);
    tft_fill_rect(r->x, r->y, 1, r->h, TFT_WHITE);
    tft_fill_rect(r->x + r->w - 1, r->y, 1, r->h, TFT_WHITE);

    // Indicate a currently-held note with a white center stripe.
    if (note->is_hold && note->holding && r->h > 5) {
        tft_fill_rect(
            r->x + 3,
            r->y + r->h / 2,
            r->w - 6,
            2,
            TFT_WHITE
        );
    }
}

// --------------------------------------------------
// Score, combo, and judgement
// --------------------------------------------------

static void draw_status(const game_state_t *st)
{
    char number[16];

    // Clear the changing status areas.
    tft_fill_rect(5, 4, 105, 39, PANEL_COLOR);
    tft_fill_rect(135, 4, 100, 39, PANEL_COLOR);

    draw_text(10, 6, "SCORE", TEXT_COLOR, 1);
    snprintf(number, sizeof(number), "%lu",
             (unsigned long)st->score);
    draw_text(10, 20, number, TEXT_COLOR, 1);

    draw_text(145, 6, "COMBO", TEXT_COLOR, 1);
    snprintf(number, sizeof(number), "%u",
             (unsigned int)st->combo);
    draw_text(145, 20, number, TEXT_COLOR, 1);

    // Clear and redraw judgement feedback.
    tft_fill_rect(0, 282, SCREEN_W, 38, PANEL_COLOR);

    const char *judgement = "";

    switch (st->last_judgement) {
        case JUDGE_PERFECT:
            judgement = "PERFECT";
            break;

        case JUDGE_GOOD:
            judgement = "GOOD";
            break;

        case JUDGE_MISS:
            judgement = "MISS";
            break;

        default:
            break;
    }

    int text_width = 0;
    for (const char *p = judgement; *p; p++)
        text_width += 6;

    int x = (SCREEN_W - text_width) / 2;

    draw_text(x, 296, judgement, TEXT_COLOR, 1);
}

// --------------------------------------------------
// Public renderer interface
// --------------------------------------------------

void render_init(void)
{
    tft_init();
    tft_set_rotation(0);

    previous_count = 0;

    draw_background();
}

void render_frame(const game_state_t *st,
                  const note_view_t *notes,
                  int count)
{
    if (st == NULL)
        return;

    if (count < 0)
        count = 0;

    if (count > MAX_RENDERED_NOTES)
        count = MAX_RENDERED_NOTES;

    // Erase the notes from the previous frame.
    for (int i = 0; i < previous_count; i++) {
        rendered_note_t *r = &previous_notes[i];

        if (r->lane >= LANE_COUNT || r->h <= 0)
            continue;

        tft_fill_rect(
            r->x,
            r->y,
            r->w,
            r->h,
            lane_background(r->lane)
        );
    }

    // Draw the current notes and remember their positions.
    previous_count = 0;

    if (notes != NULL) {
        for (int i = 0; i < count; i++) {
            if (notes[i].lane >= LANE_COUNT)
                continue;

            rendered_note_t r = make_note_rect(&notes[i]);

            if (r.h <= 0)
                continue;

            draw_note(&notes[i], &r);

            previous_notes[previous_count++] = r;
        }
    }

    // Restore the hit line in case a note passed over it.
    tft_fill_rect(
        FIELD_X,
        HIT_LINE_Y,
        FIELD_W,
        2,
        HIT_COLOR
    );

    draw_status(st);
}
