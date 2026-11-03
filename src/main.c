/* DINO RUN per Casio fx-9860GIII.
 * Add-in monocromatico leggero: salto con il tasto numerico 1.
 * Punteggio in tempo reale, record solo a fine partita.
 * Compilazione: fxsdk build-fx (fxSDK + gint).
 */
#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/clock.h>
#include <gint/gint.h>
#include <stdio.h>
#include <stdint.h>

#define GROUND 53
#define DINO_X 16
#define DINO_W 14
#define DINO_H 12
#define DINO_GROUND_Y (GROUND - DINO_H)
#define FILE_RECORD "DINO.SAV"

typedef struct {
    int y, speed_y;
    int obstacle_x, obstacle_height;
    unsigned int points;
    unsigned int frame;
    uint32_t random_state;
} game_t;

/* Executed in the original OS context: required for flash filesystem I/O. */
static int record_load_os(void)
{
    FILE *f = fopen(FILE_RECORD, "rb");
    uint32_t value = 0;
    if(!f) return 0;
    if(fread(&value, sizeof value, 1, f) != 1) value = 0;
    fclose(f);
    if(value > 999999U) value = 0;
    return (int)value;
}

static int record_save_os(uint32_t value)
{
    FILE *f = fopen(FILE_RECORD, "wb");
    if(!f) return 0;
    int ok = fwrite(&value, sizeof value, 1, f) == 1;
    fclose(f);
    return ok;
}

/* Deterministic small pseudo-random generator; no extra libraries needed. */
static unsigned int next_random(game_t *g)
{
    g->random_state = g->random_state * 1664525U + 1013904223U;
    return g->random_state >> 16;
}

static void draw_dino(int x, int y, int frame, int airborne)
{
    /* Body, tail, head, eye, front paw and animated legs. */
    drect(x + 2, y + 4, x + 9, y + 9, C_BLACK);
    drect(x + 8, y + 0, x + 13, y + 6, C_BLACK);
    drect(x + 0, y + 6, x + 3, y + 7, C_BLACK);
    drect(x + 8, y + 7, x + 10, y + 8, C_BLACK);
    drect(x + 11, y + 2, x + 11, y + 2, C_WHITE);
    if(airborne || ((frame / 4) % 2 == 0)) {
        drect(x + 4, y + 10, x + 5, y + 11, C_BLACK);
        drect(x + 8, y + 10, x + 9, y + 11, C_BLACK);
    }
    else {
        drect(x + 3, y + 10, x + 4, y + 11, C_BLACK);
        drect(x + 9, y + 10, x + 10, y + 11, C_BLACK);
    }
}

static void draw_cactus(int x, int h)
{
    int top = GROUND - h;
    drect(x + 3, top, x + 5, GROUND - 1, C_BLACK);
    drect(x + 1, top + 3, x + 2, top + 7, C_BLACK);
    drect(x + 6, top + 5, x + 7, top + 9, C_BLACK);
    drect(x + 1, top + 7, x + 4, top + 8, C_BLACK);
    drect(x + 5, top + 8, x + 7, top + 9, C_BLACK);
}

static void screen_start(void)
{
    dclear(C_WHITE);
    dtext(35, 3, C_BLACK, "DINO RUN");
    dline(0, GROUND, 127, GROUND, C_BLACK);
    draw_dino(19, GROUND - DINO_H, 0, 0);
    draw_cactus(103, 12);
    dtext(6, 19, C_BLACK, "PREMI 1 PER GIOCARE");
    dtext(31, 57, C_BLACK, "EXIT: ESCI");
    dupdate();
}

static void screen_end(unsigned int score, unsigned int record, int is_new)
{
    dclear(C_WHITE);
    dtext(26, 2, C_BLACK, "GAME OVER");
    dline(8, 13, 119, 13, C_BLACK);
    dprint(14, 18, C_BLACK, "PUNTI:  %u", score);
    dprint(14, 29, C_BLACK, "RECORD: %u", record);
    if(is_new) dtext(20, 40, C_BLACK, "NUOVO RECORD!");
    dtext(3, 55, C_BLACK, "1:RIPROVA EXIT:ESCI");
    dupdate();
}

/* A key press starts the game, EXiT leaves the add-in. */
static int wait_for_start(void)
{
    while(1) {
        int key = getkey().key;
        if(key == KEY_EXIT) return 0;
        if(key == KEY_1 || key == KEY_EXE) return 1;
    }
}

static void game_init(game_t *g)
{
    g->y = DINO_GROUND_Y;
    g->speed_y = 0;
    g->obstacle_x = 136;
    g->obstacle_height = 12;
    g->points = 0;
    g->frame = 0;
    g->random_state = 0x13579bdfU;
    clearevents();
}

/* Returns score; EXIT during play immediately ends the run. */
static unsigned int game_play(void)
{
    game_t g;
    game_init(&g);

    while(1) {
        /* Do not block animation while awaiting keys. */
        key_event_t ev;
        while((ev = pollevent()).type != KEYEV_NONE) {
            if(ev.type != KEYEV_DOWN) continue;
            if(ev.key == KEY_EXIT) return g.points;
            if(ev.key == KEY_1 && g.y == DINO_GROUND_Y)
                g.speed_y = -8;
        }

        /* Integer-only physics, limited to 30 frames per second. */
        if(g.speed_y != 0 || g.y != DINO_GROUND_Y) {
            g.y += g.speed_y;
            g.speed_y += 1;
            if(g.y >= DINO_GROUND_Y) {
                g.y = DINO_GROUND_Y;
                g.speed_y = 0;
            }
        }

        int speed = 3 + (int)(g.points / 170U);
        if(speed > 6) speed = 6;
        g.obstacle_x -= speed;
        if(g.obstacle_x < -10) {
            g.obstacle_x = 130 + (int)(next_random(&g) % 62U);
            g.obstacle_height = 10 + (int)(next_random(&g) % 5U);
        }

        /* AABB collision with some margin for fair jumps. */
        if(g.obstacle_x < DINO_X + DINO_W - 2 &&
           g.obstacle_x + 8 > DINO_X + 2 &&
           g.y + DINO_H - 1 > GROUND - g.obstacle_height + 2)
            break;

        g.frame++;
        if(g.frame % 3U == 0 && g.points < 999999U) g.points++;

        dclear(C_WHITE);
        dprint(2, 1, C_BLACK, "PUNTI: %u", g.points);
        dline(0, GROUND, 127, GROUND, C_BLACK);
        draw_dino(DINO_X, g.y, (int)g.frame, g.y != DINO_GROUND_Y);
        draw_cactus(g.obstacle_x, g.obstacle_height);
        dupdate();
        sleep_ms(33);
    }
    return g.points;
}

int main(void)
{
    uint32_t record = (uint32_t)(int)gint_world_switch(GINT_CALL(record_load_os));
    if(record > 999999U) record = 0;

    while(1) {
        screen_start();
        if(!wait_for_start()) break;

        unsigned int score = game_play();
        int is_new = score > record;
        if(is_new) {
            record = score;
            /* A single 4-byte file is written only when a new record occurs. */
            gint_world_switch(GINT_CALL(record_save_os, record));
        }
        screen_end(score, record, is_new);
        if(!wait_for_start()) break;
    }
    return 1;
}
