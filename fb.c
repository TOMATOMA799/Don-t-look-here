// fb.c - HDMI framebuffer via the VideoCore mailbox property interface,
// plus a minimal built-in font so we can draw "HELLO FROM BARE METAL"
// without needing a font file on the SD/USB boot partition.
//
// See mailbox.c for the risk notes on the mailbox/framebuffer approach
// on Pi 5 specifically - this is the least-verified part of the project.

#include <stdint.h>
#include "mailbox.h"

void uart_puts(const char* s); // from uart.c, used to report fb status

#define FB_WIDTH   1024
#define FB_HEIGHT  768
#define FB_DEPTH   32   // bits per pixel (XRGB8888)

static volatile uint32_t mbox_buf[35] __attribute__((aligned(16)));

static uint32_t g_fb_addr = 0;
static uint32_t g_fb_pitch = 0;
static uint32_t g_fb_width = 0;
static uint32_t g_fb_height = 0;

// Returns 1 on success, 0 on failure (check via uart before trusting fb_*).
//
// Mailbox property tag word layout is: tag_id, value_buf_size_bytes,
// request_len_bytes, then value words (request writes in, response
// overwrites in place). Getting this wrong (e.g. an extra/missing word)
// desyncs every tag after it, so we track response offsets by name
// instead of hardcoding positions.
int fb_init(void) {
    int i = 0;
    mbox_buf[i++] = 0; // placeholder for total size, filled in below
    mbox_buf[i++] = 0; // request code

    mbox_buf[i++] = 0x00048003; // set physical (display) width/height
    mbox_buf[i++] = 8;          // value buffer size (bytes)
    mbox_buf[i++] = 8;          // request length (bytes)
    mbox_buf[i++] = FB_WIDTH;
    mbox_buf[i++] = FB_HEIGHT;

    mbox_buf[i++] = 0x00048004; // set virtual (buffer) width/height
    mbox_buf[i++] = 8;
    mbox_buf[i++] = 8;
    mbox_buf[i++] = FB_WIDTH;
    mbox_buf[i++] = FB_HEIGHT;

    mbox_buf[i++] = 0x00048005; // set depth
    mbox_buf[i++] = 4;
    mbox_buf[i++] = 4;
    mbox_buf[i++] = FB_DEPTH;

    mbox_buf[i++] = 0x00040001; // allocate framebuffer
    mbox_buf[i++] = 8;          // response needs 2 words (addr, size)
    mbox_buf[i++] = 4;          // request only sends 1 word (alignment)
    int addr_idx = i;
    mbox_buf[i++] = 4096;       // in: alignment -> out: base address
    mbox_buf[i++] = 0;          // out: size (unused here, but must exist)

    mbox_buf[i++] = 0x00040008; // get pitch
    mbox_buf[i++] = 4;
    mbox_buf[i++] = 0;          // no request payload
    int pitch_idx = i;
    mbox_buf[i++] = 0;          // out: pitch (bytes per row)

    mbox_buf[i++] = 0; // end tag
    mbox_buf[0] = i * 4;

    if (!mbox_call(mbox_buf)) {
        uart_puts("[fb] mailbox call failed\n");
        return 0;
    }

    g_fb_addr   = mbox_buf[addr_idx] & 0x3FFFFFFF; // mask off VC alias bits
    g_fb_pitch  = mbox_buf[pitch_idx];
    g_fb_width  = FB_WIDTH;
    g_fb_height = FB_HEIGHT;

    if (g_fb_addr == 0) {
        uart_puts("[fb] firmware returned null framebuffer address\n");
        return 0;
    }
    return 1;
}

static inline void fb_putpixel(uint32_t x, uint32_t y, uint32_t rgb) {
    if (x >= g_fb_width || y >= g_fb_height) return;
    volatile uint32_t* p =
        (volatile uint32_t*)(uintptr_t)(g_fb_addr + y * g_fb_pitch + x * 4);
    *p = rgb;
}

void fb_fill(uint32_t rgb) {
    for (uint32_t y = 0; y < g_fb_height; y++)
        for (uint32_t x = 0; x < g_fb_width; x++)
            fb_putpixel(x, y, rgb);
}

// --- Minimal 5x7 font, hand-authored (not copied from any table), covering
// only the letters needed for "HELLO FROM BARE METAL". Extend as needed. ---
typedef struct { char c; uint8_t rows[7]; } glyph_t;

static const glyph_t FONT[] = {
    {'H', {0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001}},
    {'E', {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111}},
    {'L', {0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111}},
    {'O', {0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110}},
    {'F', {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000}},
    {'R', {0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001}},
    {'M', {0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001}},
    {'B', {0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110}},
    {'A', {0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001}},
    {'T', {0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100}},
};
#define FONT_COUNT (sizeof(FONT) / sizeof(FONT[0]))
#define SCALE 6 // pixel scale factor per font cell

static const glyph_t* find_glyph(char c) {
    for (unsigned i = 0; i < FONT_COUNT; i++)
        if (FONT[i].c == c) return &FONT[i];
    return 0; // includes space -> blank cell
}

static void fb_putchar(uint32_t x, uint32_t y, char c, uint32_t rgb) {
    const glyph_t* g = find_glyph(c);
    if (!g) return; // space or unknown char: leave blank
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if (g->rows[row] & (1 << (4 - col))) {
                for (int sy = 0; sy < SCALE; sy++)
                    for (int sx = 0; sx < SCALE; sx++)
                        fb_putpixel(x + col * SCALE + sx,
                                    y + row * SCALE + sy, rgb);
            }
        }
    }
}

void fb_puts(uint32_t x, uint32_t y, const char* s, uint32_t rgb) {
    uint32_t cursor = x;
    while (*s) {
        fb_putchar(cursor, y, *s, rgb);
        cursor += 6 * SCALE; // 5 cols + 1 col spacing, scaled
        s++;
    }
}
