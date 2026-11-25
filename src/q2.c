/* Tic-Tac-Toe's platform layer: the Shanling Q2 under Q2 Pod, as a card game (as Challenger Deep's
 * and Wordle's).
 *
 * Q2 Pod (github.com/DiamondBond/q2-pod, docs/internals.md "Card games") forks this binary from
 * Library -> Games when /mnt/mmc/.tictactoe/q2app holds game=1. While it runs, demo stops painting
 * and the game owns the screen and input:
 *
 *   screen  /dev/fb0: the portrait panel, 320 x 375, 32 bpp, red at bit 16, two pages (320 x 750
 *           virtual). The UI's view is landscape, 375 x 320, the panel turned anticlockwise, so a
 *           view pixel (X, Y) is the panel's row X, column 319 - Y. Frames go to the hidden page,
 *           then FBIOPAN_DISPLAY, as q2video, demo and the other card games do.
 *   input   text datagrams on /tmp/q2-tictactoe.input.sock: "k d CODE" / "k u CODE", "t d|m|u X Y"
 *           (in the view's 375 x 320 px) and "w N" (the wheel's raw travel, 200 a turn, rising
 *           anticlockwise). Return held 700 ms is Q2 Pod's: it TERMs the game.
 *   sound   none: no audio=1, so the music plays on and Play/Pause and the side buttons stay the
 *           player's. */
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "tt.h"

#define VERSION "1.0"
#define APP_DIR "/mnt/mmc/.tictactoe"
#define INPUT_SOCK "/tmp/q2-tictactoe.input.sock"
#define LOG_FILE "tictactoe.log"
#define FB_W 320 /* the portrait panel */
#define FB_H 375

#define FBIOGET_VSCREENINFO 0x4600
#define FBIOGET_FSCREENINFO 0x4602
#define FBIOPAN_DISPLAY 0x4606

uint64_t now_us(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000u + (uint64_t)(t.tv_nsec / 1000);
}

void logf_(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fputc('\n', stdout);
}

static volatile sig_atomic_t quit_flag;
int quit_asked(void) { return quit_flag; }
static void on_term(int sig) {
    (void)sig;
    quit_flag = 1;
}

/* ---------------------------------------------------------------------------------------------
 * Screen */

static struct {
    int fd;
    unsigned var[40], fix[17]; /* fb_var_screeninfo, fb_fix_screeninfo */
    unsigned char *mem;
    unsigned line;     /* bytes a panel row */
    unsigned y0, back; /* demo's page when we started, the page we draw next */
    int open;
} fbd = { .fd = -1 };

static int fb_open(void) {
    fbd.fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
    if (fbd.fd < 0 || ioctl(fbd.fd, FBIOGET_VSCREENINFO, fbd.var) || ioctl(fbd.fd, FBIOGET_FSCREENINFO, fbd.fix))
        return 0;
    fbd.line = fbd.fix[11];
    /* the Q2's panel, or nothing: xres, yres, yres_virtual, bpp, red's offset */
    if (fbd.var[0] != FB_W || fbd.var[1] != FB_H || fbd.var[3] < 2 * FB_H || fbd.var[6] != 32 ||
        fbd.var[8] != 16 || fbd.line < FB_W * 4 || 2 * FB_H * fbd.line > fbd.fix[5])
        return 0;
    fbd.mem = mmap(0, fbd.fix[5], PROT_READ | PROT_WRITE, MAP_SHARED, fbd.fd, 0);
    if (fbd.mem == MAP_FAILED) return 0;
    fbd.y0 = fbd.var[5];
    fbd.back = fbd.y0 ? 0 : FB_H;
    fbd.open = 1;
    return 1;
}

/* view (X, Y) is panel row X, column 319 - Y: each panel row is one view column, written in order */
void present(const uint32_t *frame) {
    if (!fbd.open) return;
    unsigned char *page = fbd.mem + fbd.back * fbd.line;
    for (int x = 0; x < VIEW_W; ++x) {
        uint32_t *out = (uint32_t *)(page + (unsigned)x * fbd.line) + (FB_W - 1);
        const uint32_t *in = frame + x;
        for (int y = 0; y < VIEW_H; ++y, in += VIEW_W) out[-y] = *in | 0xff000000u;
    }
    fbd.var[5] = fbd.back;
    ioctl(fbd.fd, FBIOPAN_DISPLAY, fbd.var);
    fbd.back = fbd.back ? 0 : FB_H;
}

static void fb_close(void) {
    if (!fbd.open) return;
    if (fbd.var[5] != fbd.y0) fbd.var[5] = fbd.y0, ioctl(fbd.fd, FBIOPAN_DISPLAY, fbd.var);
    fbd.open = 0; /* demo repaints the whole screen once we have gone */
}

/* ---------------------------------------------------------------------------------------------
 * Input */

static int sock = -1;

int next_event(int *kind, int *a, int *b, int *c) {
    char m[64], ch = 0;
    ssize_t n;
    while (sock >= 0 && (n = recv(sock, m, sizeof m - 1, MSG_DONTWAIT)) > 0) {
        m[n] = 0;
        int x = 0, y = 0;
        if (sscanf(m, "k %c %d", &ch, &x) == 2 && (ch == 'd' || ch == 'u')) {
            *kind = 'k', *a = ch == 'd', *b = x, *c = 0;
            return 1;
        }
        if (sscanf(m, "t %c %d %d", &ch, &x, &y) == 3 && (ch == 'd' || ch == 'm' || ch == 'u')) {
            *kind = 't', *a = ch, *b = x, *c = y;
            return 1;
        }
        if (sscanf(m, "w %d", &x) == 1) {
            *kind = 'w', *a = x, *b = *c = 0;
            return 1;
        }
    }
    return 0;
}

static int input_open(void) {
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, INPUT_SOCK);
    sock = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    unlink(INPUT_SOCK);
    return sock >= 0 && !bind(sock, (const struct sockaddr *)&addr, sizeof addr);
}

/* ---------------------------------------------------------------------------------------------
 * Start and end */

static void cleanup(void) {
    fb_close();
    if (sock >= 0) close(sock), sock = -1, unlink(INPUT_SOCK);
}

int main(int argc, char **argv) {
    (void)argc;
    signal(SIGTERM, on_term);
    signal(SIGINT, on_term);
    signal(SIGHUP, on_term);
    signal(SIGPIPE, SIG_IGN);

    static char dir[256];
    snprintf(dir, sizeof dir, "%s", argv[0]);
    char *slash = strrchr(dir, '/');
    if (slash) *slash = 0;
    else snprintf(dir, sizeof dir, "%s", APP_DIR);
    chdir(dir);
    if (freopen(LOG_FILE, "w", stdout)) setvbuf(stdout, 0, _IONBF, 0);
    if (freopen(LOG_FILE, "a", stderr)) setvbuf(stderr, 0, _IONBF, 0);
    printf("tictactoe %s: started in %s\n", VERSION, dir);

    if (!fb_open()) {
        fprintf(stderr, "No Q2 framebuffer: fd %d, %u x %u, virtual %u, %u bpp, red at %u, line %u, "
                        "%u bytes (errno %d)\n", fbd.fd, fbd.var[0], fbd.var[1], fbd.var[3], fbd.var[6],
                fbd.var[8], fbd.fix[11], fbd.fix[5], errno);
        return 1;
    }
    if (!input_open()) fprintf(stderr, "No input socket at %s (errno %d)\n", INPUT_SOCK, errno);

    uint64_t t0 = now_us();
    int status = game_main();
    printf("Ended after %llu s\n", (unsigned long long)((now_us() - t0) / 1000000));
    cleanup();
    return status;
}
