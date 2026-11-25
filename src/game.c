/* Tic-Tac-Toe: the menu, the rounds, and how they look.
 *
 *   the wheel     moves the highlight (round the empty squares, or the menu)
 *   Centre        puts your mark in the highlighted square / picks
 *   touch         tap a square to play there; tap a menu line to pick it
 *   Return        back to the menu (in the menu, nothing: holding it is Q2 Pod's, and leaves)
 *   Quit          the menu's last button, an X in an O: the game ends itself, and Q2 Pod repaints
 *                 (it eats the release of a Centre still down, so nothing under it is pressed)
 *
 * Two modes: against the Q2 (you are X, it is O, three levels) and two players on one Q2 (X and O
 * take turns). Who starts swaps every round. The record against the Q2 at each level, and the
 * level, are kept in tictactoe.sav. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tt.h"

#define SAVE_FILE "tictactoe.sav"
#define WHEEL_STEP 28 /* wheel travel a step: about seven a turn */

/* colours: the 50s diner look of the design mockup */
#define CREAM 0xf4ebcf
#define MUSTARD 0xf2bd2c
#define MUSTARD_DARK 0xe0a91c
#define RED 0xc8281e
#define RED_DARK 0x8e1a13
#define TEAL 0x1a8f80
#define TEAL_TEXT 0x1a6f64
#define TEAL_DARK 0x12584f
#define OCHRE 0xb48a17
#define BRICK 0xa2231a
#define UMBER 0x7a5a0a

/* the board: 3 x 3 squares in the 209 px square at BX, BY, inside the card baked in art.c */
#define BX 83
#define BY 56
#define BOARD 209
#define CS (BOARD / 3.0f)

static const char *LEVEL_NAME[LEVELS] = { "Easy", "Normal", "Hard" };

/* ---------------------------------------------------------------------------------------------
 * Randomness: xorshift */
static unsigned rs = 2463534242u;
static unsigned rnd(void) {
    rs ^= rs << 13, rs ^= rs >> 17, rs ^= rs << 5;
    return rs;
}

/* ---------------------------------------------------------------------------------------------
 * State */
enum { MENU, PLAY };
enum { VS_Q2, TWO_PLAYERS };

static struct {
    int screen, mode, level;
    int menu_sel, quit;
    /* the round */
    uint8_t board[9];
    uint64_t placed_at[9]; /* when each mark went down, for its stroke */
    int turn, starter, cursor, over, win_line[3];
    uint64_t over_at, think_until;
    int score[3]; /* this sitting: X wins, O wins, draws */
    int record[LEVELS][3]; /* against the Q2, kept: wins, losses, draws */
    int last_q2_move;
} g;

static uint64_t now; /* this frame's time, us */

/* ---------------------------------------------------------------------------------------------
 * The save file */
static void load(void) {
    FILE *f = fopen(SAVE_FILE, "r");
    char line[96];
    g.level = NORMAL;
    while (f && fgets(line, sizeof line, f)) {
        int a, b, c, d;
        if (sscanf(line, "level %d", &a) == 1 && a >= 0 && a < LEVELS) g.level = a;
        if (sscanf(line, "record %d %d %d %d", &a, &b, &c, &d) == 4 && a >= 0 && a < LEVELS)
            g.record[a][0] = b, g.record[a][1] = c, g.record[a][2] = d;
    }
    if (f) fclose(f);
}

static void save(void) {
    FILE *f = fopen(SAVE_FILE ".new", "w");
    if (!f) return;
    fprintf(f, "level %d\n", g.level);
    for (int i = 0; i < LEVELS; ++i) fprintf(f, "record %d %d %d %d\n", i, g.record[i][0], g.record[i][1], g.record[i][2]);
    fclose(f);
    rename(SAVE_FILE ".new", SAVE_FILE);
}

/* ---------------------------------------------------------------------------------------------
 * The round */
static int q2s_turn(void) { return g.mode == VS_Q2 && g.turn == PO && !g.over; }

static void cursor_to_empty(int dir) {
    for (int k = 0; k < 9; ++k) {
        if (!g.board[g.cursor]) return;
        g.cursor = (g.cursor + dir + 9) % 9;
    }
}

static void think(void) { g.think_until = now + 380000 + rnd() % 420000; }

static void new_round(void) {
    memset(g.board, 0, sizeof g.board);
    g.turn = g.starter;
    g.over = 0;
    g.cursor = 4;
    g.last_q2_move = -1;
    if (q2s_turn()) think();
}

static void start_game(int mode) {
    g.mode = mode;
    g.screen = PLAY;
    memset(g.score, 0, sizeof g.score);
    g.starter = PX;
    new_round();
    logf_("game: %s%s%s", mode == VS_Q2 ? "against the Q2, " : "two players", mode == VS_Q2 ? LEVEL_NAME[g.level] : "", "");
}

static void place(int cell) {
    if (cell < 0 || cell > 8 || g.board[cell] || g.over) return;
    g.board[cell] = (uint8_t)g.turn;
    g.placed_at[cell] = now;
    int w = board_winner(g.board, g.win_line);
    if (w) {
        g.over = w;
        g.over_at = now;
        g.score[w == 3 ? 2 : w - 1]++;
        if (g.mode == VS_Q2) {
            g.record[g.level][w == PX ? 0 : w == PO ? 1 : 2]++;
            save();
        }
        logf_("round over: %s", w == 3 ? "draw" : w == PX ? "X wins" : "O wins");
        return;
    }
    g.turn = 3 - g.turn;
    cursor_to_empty(1);
    if (q2s_turn()) think();
}

static void next_round(void) {
    g.starter = 3 - g.starter;
    new_round();
}

/* ---------------------------------------------------------------------------------------------
 * Input */
static struct {
    int wheel_acc;
    int return_down;
    int touch_cell, touch_item; /* where a touch went down */
} in = { .touch_cell = -1, .touch_item = -1 };

static int cell_at(int x, int y) {
    if (x < BX || y < BY || x >= BX + BOARD || y >= BY + BOARD) return -1;
    return (y - BY) * 3 / BOARD * 3 + (x - BX) * 3 / BOARD;
}

/* the menu's three buttons, as the mockup has them */
static const struct {
    short y, h;
} ITEM[3] = { { 118, 38 }, { 165, 36 }, { 210, 32 } };
#define ITEM_X 82
#define ITEM_W 211
#define MENU_ITEMS 4 /* the three buttons, then Quit */
#define QUIT_X (VIEW_W / 2.0f)
#define QUIT_Y 270.0f
#define QUIT_R 20.0f
static int item_at(int x, int y) {
    float dx = x - QUIT_X, dy = y - QUIT_Y;
    if (dx * dx + dy * dy < (QUIT_R + 8) * (QUIT_R + 8)) return 3;
    for (int i = 0; i < 3; ++i)
        if (x >= ITEM_X - 8 && x < ITEM_X + ITEM_W + 8 && y >= ITEM[i].y - 4 && y < ITEM[i].y + ITEM[i].h + 5) return i;
    return -1;
}

static void menu_pick(int i) {
    g.menu_sel = i;
    if (i == 0) start_game(VS_Q2);
    else if (i == 1) start_game(TWO_PLAYERS);
    else if (i == 2) g.level = (g.level + 1) % LEVELS, save();
    else g.quit = 1, logf_("quit from the menu");
}

static void human_move_step(int dir) {
    if (g.over || q2s_turn()) return;
    for (int k = 0; k < 9; ++k) {
        g.cursor = (g.cursor + dir + 9) % 9;
        if (!g.board[g.cursor]) break;
    }
}

static void on_centre(void) {
    if (g.screen == MENU) menu_pick(g.menu_sel);
    else if (g.over) {
        if (now - g.over_at > 350000) next_round();
    } else if (!q2s_turn()) place(g.cursor);
}

static void handle_input(void) {
    int kind, a, b, c;
    while (next_event(&kind, &a, &b, &c)) {
        if (kind == 'w') {
            in.wheel_acc += a;
            /* travel rises anticlockwise: clockwise is forward */
            while (in.wheel_acc <= -WHEEL_STEP) {
                in.wheel_acc += WHEEL_STEP;
                if (g.screen == MENU) g.menu_sel = (g.menu_sel + 1) % MENU_ITEMS;
                else human_move_step(1);
            }
            while (in.wheel_acc >= WHEEL_STEP) {
                in.wheel_acc -= WHEEL_STEP;
                if (g.screen == MENU) g.menu_sel = (g.menu_sel + MENU_ITEMS - 1) % MENU_ITEMS;
                else human_move_step(-1);
            }
        } else if (kind == 'k') {
            if (b == K_RETURN) { /* a held Return repeats its downs: act on the first */
                if (a && !in.return_down && g.screen == PLAY) g.screen = MENU;
                in.return_down = a;
            } else if (a && b == K_CENTRE) on_centre();
        } else if (kind == 't') {
            if (g.screen == MENU) {
                int i = item_at(b, c);
                if (a == 'd') in.touch_item = i;
                if (i >= 0) g.menu_sel = i;
                if (a == 'u') {
                    if (i >= 0 && i == in.touch_item) menu_pick(i);
                    in.touch_item = -1;
                }
            } else if (g.over) {
                if (a == 'u') on_centre();
            } else if (!q2s_turn()) {
                int cell = cell_at(b, c);
                if (a == 'd') in.touch_cell = cell;
                if (cell >= 0 && !g.board[cell]) g.cursor = cell;
                if (a == 'u') {
                    if (cell >= 0 && cell == in.touch_cell) place(cell);
                    in.touch_cell = -1;
                }
            }
        }
    }
}

/* ---------------------------------------------------------------------------------------------
 * Drawing */
static float ease(float t) { /* 0..1, out */
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return 1 - (1 - t) * (1 - t) * (1 - t);
}

/* seconds since t, over secs; 64-bit to float would need libgcc, so through 32 bits */
static float since(uint64_t t, float secs) {
    if (now < t) return 0;
    uint64_t d = now - t;
    return (float)(int)(d > 100000000u ? 100000000u : (unsigned)d) / (secs * 1e6f);
}

/* X: two brush strokes, the second after the first; O: a ring drawn round from the top */
static void draw_x(float cx, float cy, float scale, float t, int alpha) {
    float t1 = ease(t * 2), t2 = ease(t * 2 - 1), k = scale;
    if (t1 > 0) capsule(cx - 16 * k, cy - 15 * k, cx + (-16 + 32 * t1) * k, cy + (-15 + 31 * t1) * k, 4 * k, RED, alpha);
    if (t2 > 0) capsule(cx + 15 * k, cy - 16 * k, cx + (15 - 30 * t2) * k, cy + (-16 + 31 * t2) * k, 4 * k, RED, alpha);
}

static void draw_o(float cx, float cy, float scale, float t, int alpha) {
    arc(cx, cy, 17 * scale, 8 * scale, -1.5707963f, ease(t) * 6.2832f, TEAL, alpha);
}

static void draw_mark(int who, float cx, float cy, float scale, float t, int alpha) {
    if (who == PX) draw_x(cx, cy, scale, t, alpha);
    else draw_o(cx, cy, scale, t, alpha);
}

static float cell_cx(int i) { return BX + (i % 3) * CS + CS / 2; }
static float cell_cy(int i) { return BY + (i / 3) * CS + CS / 2; }

/* the atomic starburst: four strokes through a box `size` wide at x, y */
static void star(float x, float y, float size, float width, uint32_t c) {
    float k = size / 34, cx = x + size / 2, cy = y + size / 2, r = width * k / 2;
    capsule(cx, cy - 15 * k, cx, cy + 15 * k, r, c, 256);
    capsule(cx - 15 * k, cy, cx + 15 * k, cy, r, c, 256);
    capsule(cx - 9 * k, cy - 9 * k, cx + 9 * k, cy + 9 * k, r, c, 256);
    capsule(cx - 9 * k, cy + 9 * k, cx + 9 * k, cy - 9 * k, r, c, 256);
}

/* a baseline that sets a font's capitals in the middle of y0..y0 + h */
static int mid(const Font *f, float y0, float h) { return (int)(y0 + h / 2 + f->cap / 2.0f + 0.5f); }

static void draw_menu(void) {
    backdrop(art_menu); /* the sign, the title and the stars */
    char lvl[40];
    snprintf(lvl, sizeof lvl, "Q2 LEVEL: %s", g.level == EASY ? "EASY" : g.level == NORMAL ? "NORMAL" : "HARD");
    const char *items[3] = { "Play vs Q2", "Play with a Friend", lvl };
    for (int i = 0; i < 3; ++i) {
        float y = ITEM[i].y, h = ITEM[i].h, r = h / 2;
        int sel = i == g.menu_sel, level = i == 2;
        uint32_t col = level ? TEAL : RED, shade = level ? TEAL_DARK : RED_DARK;
        if (sel) {
            rrect(ITEM_X, y + 3, ITEM_W, h, r, shade, 256);
            rrect(ITEM_X, y, ITEM_W, h, r, col, 256);
        } else rrect_line(ITEM_X, y, ITEM_W, h, r, 3, col, 256);
        uint32_t ink = sel ? CREAM : level ? TEAL_TEXT : RED;
        if (level) text_c(&font_caps, items[i], VIEW_W / 2, mid(&font_caps, y + 2, h), ink, 256, 1.5f);
        else text_c(&font_script, items[i], VIEW_W / 2, mid(&font_script, y, h), ink, 256, 0);
    }
    /* Quit: a red X in a teal O, like the board's marks; the board's highlight when selected */
    if (g.menu_sel == 3) {
        disc(QUIT_X, QUIT_Y, QUIT_R + 3, MUSTARD, 90);
        rrect_dash(QUIT_X - QUIT_R - 3, QUIT_Y - QUIT_R - 3, 2 * QUIT_R + 6, 2 * QUIT_R + 6, QUIT_R + 3, 3, MUSTARD_DARK, 256);
    }
    arc(QUIT_X, QUIT_Y, 13.5f, 3, 0, 6.2832f, TEAL, 256); /* 3 px, as the buttons' outlines */
    capsule(QUIT_X - 5.5f, QUIT_Y - 5.5f, QUIT_X + 5.5f, QUIT_Y + 5.5f, 1.5f, RED, 256);
    capsule(QUIT_X + 5.5f, QUIT_Y - 5.5f, QUIT_X - 5.5f, QUIT_Y + 5.5f, 1.5f, RED, 256);
}

/* a score tag beside the board: lit (red for X, teal for O) for whoever's turn it is, or who won */
#define TAG_W 66 /* wide enough for "Player 2" with room either side */
static void draw_tag(int x, int who, const char *name, int score, int lit, int small) {
    rrect(x, 99, TAG_W, 128, 16, 0x000000, 52);
    rrect(x, 96, TAG_W, 128, 16, MUSTARD, 256);
    rrect(x + 3, 99, TAG_W - 6, 122, 13, lit ? (who == PX ? RED : TEAL) : CREAM, 256);
    const Font *nf = small ? &font_name_sm : &font_name;
    text_c(nf, name, x + TAG_W / 2, mid(nf, 117, 26), lit ? CREAM : TEAL, 256, 0);
    char s[12];
    snprintf(s, sizeof s, "%d", score);
    text_c(&font_score, s, x + TAG_W / 2, mid(&font_score, 145, 44), lit ? CREAM : TEAL_TEXT, 256, 0);
    text_c(&font_caps_xs, who == PX ? "X" : "O", x + TAG_W / 2, mid(&font_caps_xs, 191, 12), lit ? MUSTARD : OCHRE, 256, 1.5f);
}

static void draw_play(void) {
    backdrop(art_play); /* the pattern, the board's card and grid, the bottom strip */
    int vs = g.mode == VS_Q2;
    int x_lit = g.over ? g.over == PX : g.turn == PX, o_lit = g.over ? g.over == PO : g.turn == PO;
    draw_tag(4, PX, vs ? "You" : "Player 1", g.score[0], x_lit, !vs);
    draw_tag(VIEW_W - 4 - TAG_W, PO, vs ? "Q2" : "Player 2", g.score[1], o_lit, !vs);

    /* the highlight: mustard, a dashed edge, and a faint X (or O) */
    if (!g.over && !q2s_turn() && !g.board[g.cursor]) {
        float x = BX + (g.cursor % 3) * CS, y = BY + (g.cursor / 3) * CS;
        rrect(x + 8, y + 8, CS - 16, CS - 16, 12, MUSTARD, 90);
        rrect_dash(x + 8, y + 8, CS - 16, CS - 16, 12, 3, MUSTARD_DARK, 256);
        draw_mark(g.turn, cell_cx(g.cursor), cell_cy(g.cursor), 1, 1, 77);
    }

    /* the marks; at the end the three in a row pulse and the rest fade */
    float pulse = 0;
    if (g.over && g.over != 3) {
        float t = since(g.over_at, 0.9f);
        t -= (float)(int)t;
        pulse = t < 0.5f ? t * 2 : 2 - t * 2;
    }
    for (int i = 0; i < 9; ++i) {
        if (!g.board[i]) continue;
        int in_line = g.over && g.over != 3 && (i == g.win_line[0] || i == g.win_line[1] || i == g.win_line[2]);
        int alpha = g.over && g.over != 3 && !in_line ? 256 - (int)(130 * ease(since(g.over_at + 250000, 0.4f))) : 256;
        float scale = in_line ? 1 + 0.08f * pulse * ease(since(g.over_at + 500000, 0.3f)) : 1;
        draw_mark(g.board[i], cell_cx(i), cell_cy(i), scale, since(g.placed_at[i], 0.28f), alpha);
    }

    /* the mustard line through three in a row */
    if (g.over && g.over != 3) {
        float t = ease(since(g.over_at + 250000, 0.35f));
        int a = g.win_line[0], z = g.win_line[2];
        float x0 = cell_cx(a), y0 = cell_cy(a), x1 = cell_cx(z), y1 = cell_cy(z);
        float ex = (x1 - x0) * 0.1f, ey = (y1 - y0) * 0.1f;
        x0 -= ex, y0 -= ey, x1 += ex, y1 += ey;
        if (t > 0) {
            float xe = x0 + (x1 - x0) * t, ye = y0 + (y1 - y0) * t;
            capsule(x0, y0, xe, ye, 7, UMBER, 90);
            capsule(x0, y0, xe, ye, 4.5f, MUSTARD, 256);
        }
    }

    /* the banner at the top */
    char top[48];
    if (g.over) {
        uint32_t fill = g.over == PX ? RED : g.over == PO ? TEAL : MUSTARD;
        uint32_t shade = g.over == PX ? RED_DARK : g.over == PO ? TEAL_DARK : MUSTARD_DARK;
        if (g.over == 3) snprintf(top, sizeof top, "It's a Draw!");
        else if (vs) snprintf(top, sizeof top, g.over == PX ? "You Win!" : "Q2 Wins!");
        else snprintf(top, sizeof top, "Player %d Wins!", g.over == PX ? 1 : 2);
        float w = text_w(&font_script_big, top, 0) + 40, angle = -0.0524f; /* tilted back 3 degrees */
        if (w < 151) w = 151;
        float cx = VIEW_W / 2.0f, cy = 24;
        rrect_rot(cx, cy + 3, w, 40, 20, angle, shade, 256);
        rrect_rot(cx, cy, w, 40, 20, angle, MUSTARD, 256);
        rrect_rot(cx, cy, w - 6, 34, 17, angle, fill, 256);
        text_rot(&font_script_big, top, cx, cy + 1, angle, g.over == 3 ? RED : CREAM);
        star(cx - w / 2 - 28, 10, 22, 3.5f, MUSTARD);
        star(cx + w / 2 + 9, 22, 16, 3.5f, CREAM);
    } else {
        uint32_t ink = RED;
        if (q2s_turn()) {
            int dots = (int)(now / 300000 % 4);
            snprintf(top, sizeof top, "Q2's Thinking%.*s", dots, "...");
            ink = TEAL;
        } else if (vs) snprintf(top, sizeof top, "Your Turn!");
        else {
            snprintf(top, sizeof top, "Player %d's Turn!", g.turn == PX ? 1 : 2);
            ink = g.turn == PX ? RED : TEAL;
        }
        float w = (float)text_w(&font_script, q2s_turn() ? "Q2's Thinking..." : top, 0) + 36;
        if (w < 161) w = 161;
        float x = (VIEW_W - w) / 2;
        rrect(x, 9, w, 36, 18, 0x000000, 46);
        rrect(x, 6, w, 36, 18, MUSTARD, 256);
        rrect(x + 3, 9, w - 6, 30, 15, CREAM, 256);
        if (q2s_turn()) text(&font_script, top, VIEW_W / 2.0f - text_w(&font_script, "Q2's Thinking...", 0) / 2.0f,
                             mid(&font_script, 6, 36), ink, 256, 0);
        else text_c(&font_script, top, VIEW_W / 2, mid(&font_script, 6, 36), ink, 256, 0);
    }

    /* the strip along the bottom */
    char bottom[64];
    if (g.over && now - g.over_at > 600000) snprintf(bottom, sizeof bottom, "PRESS CENTRE FOR ANOTHER ROUND");
    else if (vs)
        snprintf(bottom, sizeof bottom, "DRAWS %d " DOT " Q2 ON %s", g.score[2],
                 g.level == EASY ? "EASY" : g.level == NORMAL ? "NORMAL" : "HARD");
    else snprintf(bottom, sizeof bottom, "DRAWS %d", g.score[2]);
    text_c(&font_caps_sm, bottom, VIEW_W / 2, mid(&font_caps_sm, 284, 30), BRICK, 256, 1.5f);
}

/* ---------------------------------------------------------------------------------------------
 * The loop */
static void write_state(void) {
    static int on = -1;
    if (on < 0) on = getenv("TT_STATE") != 0;
    if (!on) return;
    FILE *f = fopen("state.txt.new", "w");
    if (!f) return;
    fprintf(f, "screen %d\nmode %d\nlevel %d\nmenu %d\nturn %d\ncursor %d\nover %d\nx_wins %d\no_wins %d\ndraws %d\nthinking %d\n",
            g.screen, g.mode, g.level, g.menu_sel, g.turn, g.cursor, g.over, g.score[0], g.score[1], g.score[2], q2s_turn());
    for (int i = 0; i < 9; ++i) fprintf(f, "b%d %d\n", i, g.board[i]);
    fclose(f);
    rename("state.txt.new", "state.txt");
}

int game_main(void) {
    const char *seed = getenv("TT_SEED");
    rs = seed ? (unsigned)strtol(seed, 0, 10) : (unsigned)time(0) ^ (unsigned)now_us();
    if (!rs) rs = 1;
    load();
    logf_("level %s", LEVEL_NAME[g.level]);
    g.screen = MENU;
    uint64_t next = now_us();
    while (!quit_asked() && !g.quit) {
        now = now_us();
        handle_input();
        if (g.screen == PLAY && q2s_turn() && now >= g.think_until) {
            int m = ai_move(g.board, PO, g.level, rnd);
            g.last_q2_move = m;
            place(m);
        }
        if (g.screen == MENU) draw_menu();
        else draw_play();
        present(fb);
        write_state();
        next += 33333; /* 30 frames a second */
        uint64_t t = now_us();
        if (next > t) usleep((unsigned)(next - t));
        else next = t;
    }
    return 0;
}
