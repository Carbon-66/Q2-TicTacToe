/* Tic-Tac-Toe for the Shanling Q2 under Q2 Pod: shared declarations for every file in src/. */
#ifndef TT_H
#define TT_H
#include <stdint.h>

#define VIEW_W 375 /* the UI's landscape view: the portrait panel turned */
#define VIEW_H 320
#define CORNER 80 /* the glass's rounded corners, px */

/* Q2 Pod's AWTK key codes */
#define K_RETURN 170
#define K_PLAY 171
#define K_CENTRE 218
#define K_PREV 222
#define K_NEXT 223

/* ---------------------------------------------------------------------------------------------
 * q2.c: the platform */
uint64_t now_us(void);
void present(const uint32_t *frame); /* a VIEW_W x VIEW_H XRGB frame onto the panel */
int quit_asked(void);
int next_event(int *kind, int *a, int *b, int *c); /* 'k' (a 1 down/0 up, b code), 't' (a 'd'/'m'/'u', b x, c y), 'w' (a travel) */
void logf_(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* ---------------------------------------------------------------------------------------------
 * board.c: the rules and the Q2's player. Cells 0-8 in reading order: 0 empty, 1 X, 2 O. */
enum { EMPTY, PX, PO };
enum { EASY, NORMAL, HARD, LEVELS };
int board_winner(const uint8_t b[9], int line[3]); /* PX, PO, 3 for a draw, 0 still going; line: the three cells */
int ai_perfect_value(uint8_t b[9], int me, int turn); /* +1 me wins, 0 draw, -1 I lose, with perfect play */
int ai_move(const uint8_t b[9], int me, int level, unsigned (*rand)(void)); /* the cell the Q2 takes */

/* ---------------------------------------------------------------------------------------------
 * gfx.c: drawing into the frame */
typedef struct {
    short x, w, lsb, adv;
} Glyph;
typedef struct {
    int aw, h, ascent, cap; /* atlas width, line height, rows above the baseline, a capital's height */
    const uint8_t *px;
    Glyph g[95]; /* ' ' to '~' */
} Font;
extern const Font font_small, font_body, font_bold, font_big;

extern uint32_t *fb; /* VIEW_W x VIEW_H */
void fill(uint32_t c);
void rect(int x, int y, int w, int h, uint32_t c, int alpha);       /* alpha 0-256 */
void rrect(float x, float y, float w, float h, float r, uint32_t c, int alpha); /* rounded, smooth edges */
void rrect_line(float x, float y, float w, float h, float r, float t, uint32_t c, int alpha);
void capsule(float x0, float y0, float x1, float y1, float r, uint32_t c, int alpha); /* a thick line, round ends */
void arc(float cx, float cy, float r, float t, float a0, float sweep, uint32_t c, int alpha); /* ring from a0, radians, clockwise on screen */
void disc(float cx, float cy, float r, uint32_t c, int alpha);
int text_w(const Font *f, const char *s);
void text(const Font *f, const char *s, int x, int y, uint32_t c, int alpha); /* y: the baseline */
void text_c(const Font *f, const char *s, int cx, int y, uint32_t c, int alpha);
void darken(int alpha);

int game_main(void);
#endif
