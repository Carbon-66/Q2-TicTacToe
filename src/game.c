/* Tic-Tac-Toe: the menu, the rounds, and how they look.
 *
 *   the wheel     moves the highlight (round the empty squares, or the menu)
 *   Centre        puts your mark in the highlighted square / picks
 *   touch         tap a square to play there; tap a menu line to pick it
 *   Return        back to the menu (in the menu, nothing: holding it is Q2 Pod's, and leaves)
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

/* colours */
#define BG 0x000000
#define GRID 0x3a4360
#define INK 0xe9edf5
#define DIM 0x8a93ab
#define X_COL 0xff6b81
#define O_COL 0x4fd6c8
#define CURSOR 0xffffff

/* the board: 3 x 3 squares of CS px, centred */
#define CS 78
#define BX ((VIEW_W - 3 * CS) / 2)
#define BY 43

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
    int menu_sel;
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
    if (x < BX || y < BY || x >= BX + 3 * CS || y >= BY + 3 * CS) return -1;
    return (y - BY) / CS * 3 + (x - BX) / CS;
}

#define ITEM_W 230
#define ITEM_H 40
#define ITEM_Y0 98
#define ITEM_GAP 50
static int item_at(int x, int y) {
    for (int i = 0; i < 3; ++i)
        if (x >= (VIEW_W - ITEM_W) / 2 - 6 && x < (VIEW_W + ITEM_W) / 2 + 6 && y >= ITEM_Y0 + i * ITEM_GAP - 4 &&
            y < ITEM_Y0 + i * ITEM_GAP + ITEM_H + 4)
            return i;
    return -1;
}

static void menu_pick(int i) {
    g.menu_sel = i;
    if (i == 0) start_game(VS_Q2);
    else if (i == 1) start_game(TWO_PLAYERS);
    else g.level = (g.level + 1) % LEVELS, save();
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
                if (g.screen == MENU) g.menu_sel = (g.menu_sel + 1) % 3;
                else human_move_step(1);
            }
            while (in.wheel_acc >= WHEEL_STEP) {
                in.wheel_acc -= WHEEL_STEP;
                if (g.screen == MENU) g.menu_sel = (g.menu_sel + 2) % 3;
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

static void draw_x(float cx, float cy, float half, float r, float t, uint32_t col, int alpha) {
    float t1 = ease(t * 2), t2 = ease(t * 2 - 1);
    if (t1 > 0) capsule(cx - half, cy - half, cx - half + 2 * half * t1, cy - half + 2 * half * t1, r, col, alpha);
    if (t2 > 0) capsule(cx + half, cy - half, cx + half - 2 * half * t2, cy - half + 2 * half * t2, r, col, alpha);
}

static void draw_o(float cx, float cy, float rad, float thick, float t, uint32_t col, int alpha) {
    arc(cx, cy, rad, thick, -1.5707963f, ease(t) * 6.2832f, col, alpha);
}

static void draw_mark(int who, float cx, float cy, float scale, float t, int alpha) {
    if (who == PX) draw_x(cx, cy, 19 * scale, 5.5f * scale, t, X_COL, alpha);
    else draw_o(cx, cy, 21 * scale, 10 * scale, t, O_COL, alpha);
}

static float cell_cx(int i) { return BX + (i % 3) * CS + CS / 2.0f; }
static float cell_cy(int i) { return BY + (i / 3) * CS + CS / 2.0f; }

static void draw_menu(void) {
    fill(BG);
    text_c(&font_big, "Tic-Tac-Toe", VIEW_W / 2, 72, INK, 256);
    float bob = (float)(int)((now / 1000) % 2400) / 2400.0f;
    bob = bob < 0.5f ? bob * 2 : 2 - bob * 2;
    draw_x(52, 58 - 3 * bob, 11, 4, 1, X_COL, 256);
    draw_o(VIEW_W - 52, 58 + 3 * bob - 3, 12, 6.5f, 1, O_COL, 256);

    char lvl[40];
    snprintf(lvl, sizeof lvl, "Q2 level:  %s", LEVEL_NAME[g.level]);
    const char *items[3] = { "Play vs Q2", "Play with a Friend", lvl };
    for (int i = 0; i < 3; ++i) {
        float x = (VIEW_W - ITEM_W) / 2.0f, y = ITEM_Y0 + i * ITEM_GAP;
        int sel = i == g.menu_sel;
        if (sel) rrect(x, y, ITEM_W, ITEM_H, 20, i == 2 ? 0x56607e : X_COL, 256);
        else rrect_line(x, y, ITEM_W, ITEM_H, 20, 2, GRID, 256);
        text_c(i == 2 ? &font_body : &font_bold, items[i], VIEW_W / 2, (int)y + 28, sel ? 0xffffff : INK, 256);
    }
    char rec[80];
    const int *r = g.record[g.level];
    snprintf(rec, sizeof rec, "vs Q2 on %s:  %d won  %d lost  %d drawn", LEVEL_NAME[g.level], r[0], r[1], r[2]);
    text_c(&font_small, rec, VIEW_W / 2, 274, DIM, 256);
}

static void draw_side(int who, int x, const char *name, int score, int active) {
    float cx = x + 35;
    if (active) rrect(x + 4, 92, 62, 140, 18, who == PX ? X_COL : O_COL, 40);
    draw_mark(who, cx, 124, 0.62f, 1, active ? 256 : 150);
    text_c(&font_small, name, (int)cx, 172, active ? INK : DIM, 256);
    char s[12];
    snprintf(s, sizeof s, "%d", score);
    text_c(&font_big, s, (int)cx, 216, active ? INK : DIM, 256);
}

static void draw_play(void) {
    fill(BG);
    int vs = g.mode == VS_Q2;
    int x_turn = !g.over && g.turn == PX, o_turn = !g.over && g.turn == PO;
    draw_side(PX, 0, vs ? "You" : "Player 1", g.score[0], x_turn || g.over == PX);
    draw_side(PO, VIEW_W - 70, vs ? "Q2" : "Player 2", g.score[1], o_turn || g.over == PO);

    /* the grid */
    for (int k = 1; k < 3; ++k) {
        capsule(BX + k * CS, BY + 6, BX + k * CS, BY + 3 * CS - 6, 2.5f, GRID, 256);
        capsule(BX + 6, BY + k * CS, BX + 3 * CS - 6, BY + k * CS, 2.5f, GRID, 256);
    }

    /* the highlight, with a faint mark of whoever plays */
    if (!g.over && !q2s_turn() && !g.board[g.cursor]) {
        float x = BX + (g.cursor % 3) * CS, y = BY + (g.cursor / 3) * CS;
        rrect(x + 7, y + 7, CS - 14, CS - 14, 14, CURSOR, 22);
        rrect_line(x + 7, y + 7, CS - 14, CS - 14, 14, 2, g.turn == PX ? X_COL : O_COL, 200);
        draw_mark(g.turn, cell_cx(g.cursor), cell_cy(g.cursor), 1, 1, 60);
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
        int alpha = g.over && g.over != 3 && !in_line ? 256 - (int)(150 * ease(since(g.over_at + 250000, 0.4f))) : 256;
        float scale = in_line ? 1 + 0.08f * pulse * ease(since(g.over_at + 500000, 0.3f)) : 1;
        draw_mark(g.board[i], cell_cx(i), cell_cy(i), scale, since(g.placed_at[i], 0.28f), alpha);
    }

    /* the line through three in a row */
    if (g.over && g.over != 3) {
        float t = ease(since(g.over_at + 250000, 0.35f));
        int a = g.win_line[0], z = g.win_line[2];
        float x0 = cell_cx(a), y0 = cell_cy(a), x1 = cell_cx(z), y1 = cell_cy(z);
        float ex = (x1 - x0) * 0.17f, ey = (y1 - y0) * 0.17f;
        x0 -= ex, y0 -= ey, x1 += ex, y1 += ey;
        if (t > 0) {
            float xe = x0 + (x1 - x0) * t, ye = y0 + (y1 - y0) * t;
            capsule(x0, y0, xe, ye, 7, 0x000000, 70);
            capsule(x0, y0, xe, ye, 4, 0xffffff, 240);
        }
    }

    /* what's happening, above the board, and below it */
    char top[48];
    uint32_t col = INK;
    const Font *f = &font_bold;
    if (g.over) {
        if (g.over == 3) snprintf(top, sizeof top, "It's a draw!");
        else if (vs) snprintf(top, sizeof top, g.over == PX ? "You win!" : "Q2 wins!");
        else snprintf(top, sizeof top, "Player %d wins!", g.over == PX ? 1 : 2);
        if (g.over != 3) col = g.over == PX ? X_COL : O_COL;
    } else if (q2s_turn()) {
        int dots = (int)(now / 300000 % 4);
        snprintf(top, sizeof top, "Q2 is thinking%.*s", dots, "...");
        col = DIM, f = &font_body;
        text(f, top, VIEW_W / 2 - text_w(f, "Q2 is thinking...") / 2, 33, col, 256);
        top[0] = 0;
    } else if (vs) snprintf(top, sizeof top, "Your turn"), f = &font_body;
    else snprintf(top, sizeof top, "Player %d's turn", g.turn == PX ? 1 : 2), col = g.turn == PX ? X_COL : O_COL, f = &font_body;
    if (top[0]) text_c(f, top, VIEW_W / 2, 33, col, 256);

    char bottom[64];
    if (g.over && now - g.over_at > 600000) {
        snprintf(bottom, sizeof bottom, "Press Centre or tap for another round");
        text_c(&font_small, bottom, VIEW_W / 2, 302, INK, 230);
    } else {
        if (vs) snprintf(bottom, sizeof bottom, "Draws %d    Q2 on %s", g.score[2], LEVEL_NAME[g.level]);
        else snprintf(bottom, sizeof bottom, "Draws %d", g.score[2]);
        text_c(&font_small, bottom, VIEW_W / 2, 302, DIM, 256);
    }
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
    while (!quit_asked()) {
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
