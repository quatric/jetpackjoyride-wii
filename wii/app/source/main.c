/* Jetpack Joyride (Wii) - first playable slice: Barry, jetpack thrust, coin patterns.
 * Assets: sd:/apps/jetpackjoyride/{barry.spr,coin.spr,pattern.txt}  (see wii/README.md) */
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include "input_wii.h"
#include "wiiupdate.h"
#include "audio.h"
#include "vhead.h"

#define W 640
#define H 480
static const char *dir = "sd:/apps/jetpackjoyride/";   /* switched to fat: when libfat mounts the card there */

typedef struct { GXTexObj obj; void *data; int w, h; } Spr;

static void *fifo, *xfb[2]; static int fb; static GXRModeObj *rmode;

static int loadSpr(Spr *s, const char *name) {
    char p[128]; snprintf(p, sizeof p, "%s%s", dir, name);
    FILE *f = fopen(p, "rb"); if (!f) return 0;
    u8 hd[12]; if (fread(hd, 1, 12, f) != 12 || (memcmp(hd, "SPR0", 4) && memcmp(hd, "SPR1", 4))) { fclose(f); return 0; }
    int cmpr = hd[3] == '1';   /* SPR1 = GX_TF_CMPR (0.5 byte/px), SPR0 = RGBA8 */
    int pw = hd[4] << 8 | hd[5], ph = hd[6] << 8 | hd[7]; s->w = hd[8] << 8 | hd[9]; s->h = hd[10] << 8 | hd[11];
    size_t n = cmpr ? (size_t)pw * ph / 2 : (size_t)pw * ph * 4; s->data = memalign(32, n);
    if (!s->data || fread(s->data, 1, n, f) != n) { fclose(f); return 0; }
    fclose(f); DCFlushRange(s->data, n);
    GX_InitTexObj(&s->obj, s->data, pw, ph, cmpr ? GX_TF_CMPR : GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjFilterMode(&s->obj, GX_NEAR, GX_NEAR);
    return 1;
}

static int tick;   /* frame counter for the AUTOTEST build, which fakes input so Dolphin can run headless */
static u32 btnDown(void) {
#ifdef AUTOTEST
    if (tick == 60 || tick == 125 || tick == 135 || tick == 145 || tick == 155) return WPAD_BUTTON_DOWN; if (tick == 90 || tick == 120 || tick == 130 || tick == 140 || tick == 150 || tick == 160) return WPAD_BUTTON_A; if (tick == 170) return WPAD_BUTTON_B; if (tick == 180) return WPAD_BUTTON_UP;   /* shop: select, open, buy, close, back to START */
    return (tick == 420 || (tick > 420 && tick % 600 == 90)) ? WPAD_BUTTON_A : (tick % 600 == 300 || tick % 600 == 480) ? WPAD_BUTTON_PLUS : 0;   /* start / restart / pause / resume */
#else
    return INPUT_ButtonsDown();
#endif
}
static u32 btnHeld(void) {
#ifdef AUTOTEST
    return ((tick / 25) & 1) ? WPAD_BUTTON_A : 0;
#else
    return INPUT_ButtonsHeld();
#endif
}

static void fallbackSpr(Spr *s) {   /* magenta 4x4 so a missing asset is visible */
    u8 *d = memalign(32, 64); for (int i = 0; i < 32; i += 2) { d[i] = 255; d[i + 1] = 255; d[32 + i] = 0; d[33 + i] = 255; }
    DCFlushRange(d, 64); s->data = d; s->w = s->h = 4; GX_InitTexObj(&s->obj, d, 4, 4, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
}

static void quadRotUV(Spr *s, float cx, float cy, float w, float h, float deg, float u0, float v0, float u1, float v1) {   /* quad centred on (cx,cy), rotated */
    Mtx r, t, m; guMtxRotDeg(r, 'z', deg); guMtxTrans(t, cx, cy, 0); guMtxConcat(t, r, m); GX_LoadPosMtxImm(m, GX_PNMTX0);
    GX_LoadTexObj(&s->obj, GX_TEXMAP0);
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position2f32(-w / 2, -h / 2); GX_TexCoord2f32(u0, v0); GX_Position2f32(w / 2, -h / 2); GX_TexCoord2f32(u1, v0);
    GX_Position2f32(w / 2, h / 2); GX_TexCoord2f32(u1, v1); GX_Position2f32(-w / 2, h / 2); GX_TexCoord2f32(u0, v1);
    GX_End();
    guMtxIdentity(m); GX_LoadPosMtxImm(m, GX_PNMTX0);
}

static void quadRot(Spr *s, float cx, float cy, float w, float h, float deg) { quadRotUV(s, cx, cy, w, h, deg, 0, 0, 1, 1); }

static void quad(Spr *s, float x, float y, float w, float h, float u0, float v0, float u1, float v1) {
    GX_LoadTexObj(&s->obj, GX_TEXMAP0);
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position2f32(x, y);         GX_TexCoord2f32(u0, v0);
    GX_Position2f32(x + w, y);     GX_TexCoord2f32(u1, v0);
    GX_Position2f32(x + w, y + h); GX_TexCoord2f32(u1, v1);
    GX_Position2f32(x, y + h);     GX_TexCoord2f32(u0, v1);
    GX_End();
}

static void gfxInit(void) {
    VIDEO_Init(); rmode = VIDEO_GetPreferredMode(NULL);
    xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode)); xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    VIDEO_Configure(rmode); VIDEO_SetNextFramebuffer(xfb[0]); VIDEO_SetBlack(FALSE); VIDEO_Flush(); VIDEO_WaitVSync();
    fifo = memalign(32, 256 * 1024); memset(fifo, 0, 256 * 1024); GX_Init(fifo, 256 * 1024);
    GX_SetViewport(0, 0, rmode->fbWidth, rmode->efbHeight, 0, 1);
    GX_SetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
    f32 yscale = GX_GetYScaleFactor(rmode->efbHeight, rmode->xfbHeight);
    GX_SetDispCopyYScale(yscale); GX_SetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth, rmode->xfbHeight);
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter); GX_SetCopyClear((GXColor){58, 64, 78, 255}, GX_MAX_Z24);
    GX_SetCullMode(GX_CULL_NONE); GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    GX_ClearVtxDesc(); GX_SetVtxDesc(GX_VA_POS, GX_DIRECT); GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0); GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GX_SetNumChans(1); GX_SetNumTexGens(1); GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL); GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
    GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    Mtx44 p; guOrtho(p, 0, H, 0, W, 0, 1); GX_LoadProjectionMtx(p, GX_ORTHOGRAPHIC);
    Mtx m; guMtxIdentity(m); GX_LoadPosMtxImm(m, GX_PNMTX0);
}

#define MAXC 8192
static float cx[MAXC], cy[MAXC]; static u8 cgot[MAXC]; static int nc; static float patW;

static void loadPattern(void) {   /* '1' cells become coins; '=' starts the next chained pattern at a random height */
    char pp[128]; snprintf(pp, sizeof pp, "%spattern.txt", dir);
    FILE *f = fopen(pp, "r"); char ln[256]; int row = 0; float xoff = 0, yoff = 60; if (!f) return;
    while (fgets(ln, sizeof ln, f)) {
        if (ln[0] == '=') { xoff = patW + 350; yoff = 40 + rand() % 130; row = 0; continue; }
        if (ln[0] == '\n' || ln[0] == '\r') continue;
        for (int c = 0; ln[c] && ln[c] != '\n' && ln[c] != '\r'; c++) {
            if (ln[c] == '1' && nc < MAXC) { cx[nc] = xoff + c * 20.f; cy[nc] = yoff + row * 22.f; nc++; }
            if (xoff + c * 20.f + 20 > patW) patW = xoff + c * 20.f + 20;
        }
        row++;
    }
    fclose(f);
}

#define MAXP 40
#define MAXY 16
typedef struct { int time, n, np; float y[MAXY]; int sd[8]; float w[8]; } Pat;
static Pat pats[MAXP]; static int npat;

static void loadLasers(void) {   /* lines: P time n y.. np (startDistance weight).. */
    char pp[128], ln[512]; snprintf(pp, sizeof pp, "%slasers.txt", dir);
    FILE *f = fopen(pp, "r"); if (!f) return;
    while (npat < MAXP && fgets(ln, sizeof ln, f)) {
        char *q = ln + 1; Pat *t = &pats[npat];
        t->time = strtol(q, &q, 10); t->n = strtol(q, &q, 10); if (t->n > MAXY) t->n = MAXY;
        for (int i = 0; i < t->n; i++) t->y[i] = strtod(q, &q);
        t->np = strtol(q, &q, 10); if (t->np > 8) t->np = 8;
        for (int i = 0; i < t->np; i++) { t->sd[i] = strtol(q, &q, 10); t->w[i] = strtod(q, &q); }
        npat++;
    }
    fclose(f);
}

static float patWeight(const Pat *t, int dist) { float w = 0; for (int i = 0; i < t->np; i++) if (t->sd[i] <= dist) w = t->w[i]; return w; }

static int spawnWait(int dist) {   /* LevelPacing.xml MIN/MAX_SPAWN_INTERVAL by distance (units taken as game px: x1.5 to our px, /4 px per frame); applied between laser patterns */
    static const short mn[16] = {280, 250, 220, 200, 200, 200, 200, 200, 190, 190, 180, 180, 170, 170, 160, 150}, mx[16] = {470, 420, 390, 350, 320, 280, 275, 270, 265, 260, 255, 250, 245, 240, 235, 230};
    int k = dist < 2500 ? 0 : (dist - 2500) / 500 + 1; if (k > 15) k = 15;
    return (int)((mn[k] + rand() % (mx[k] - mn[k] + 1)) * 1.5f / 4);
}
static int pickPattern(int dist) {   /* weighted by the game's own distance-based probabilities */
    float tot = 0; for (int i = 0; i < npat; i++) tot += patWeight(&pats[i], dist);
    if (tot <= 0) return -1;
    float r = tot * (rand() / (RAND_MAX + 1.f));
    for (int i = 0; i < npat; i++) { r -= patWeight(&pats[i], dist); if (r < 0) return i; }
    return -1;
}

static int mStart = 200; static float mProb = 15, mChase = 1.2f, mSpeed = 10;   /* defaults = MissileProbability.xml */
static void loadMissile(void) {
    char pp[128], ln[128]; snprintf(pp, sizeof pp, "%smissile.txt", dir);
    FILE *f = fopen(pp, "r"); if (!f) return;
    if (fgets(ln, sizeof ln, f)) sscanf(ln, "M %d %f %f %f", &mStart, &mProb, &mChase, &mSpeed);
    fclose(f);
}

typedef struct { short x, y, w, h, xo, yo, xa; } Glyph;
static Glyph glyph[128]; static Spr fontSpr; static int haveFont;

static void loadFont(void) {   /* BMFont text .fnt + font.spr (the game's vehicle_font) */
    if (!loadSpr(&fontSpr, "font.spr")) return;
    char pp[128], ln[256]; snprintf(pp, sizeof pp, "%sfont.fnt", dir);
    FILE *f = fopen(pp, "r"); if (!f) return;
    while (fgets(ln, sizeof ln, f)) {
        int id, x, y, w, h, xo, yo, xa;
        if (sscanf(ln, "char id=%d x=%d y=%d width=%d height=%d xoffset=%d yoffset=%d xadvance=%d", &id, &x, &y, &w, &h, &xo, &yo, &xa) == 8 && id >= 0 && id < 128)
            glyph[id] = (Glyph){x, y, w, h, xo, yo, xa};
    }
    fclose(f); haveFont = 1;
}

static void drawText(float x, float y, float sc, const char *t) {
    if (!haveFont) return;
    for (; *t; t++) {
        int c = *t; if (c >= 'a' && c <= 'z') c -= 32;
        Glyph *g = &glyph[c & 127];
        if (g->w) quad(&fontSpr, x + g->xo * sc, y + g->yo * sc, g->w * sc, g->h * sc, g->x / 256.f, g->y / 256.f, (g->x + g->w) / 256.f, (g->y + g->h) / 256.f);
        x += g->xa * sc;
    }
}

#define MAXL 4
#define MAXT 8
typedef struct { float speed, yoff, total; int n; Spr t[MAXT]; int w[MAXT], h[MAXT]; } Layer;
static Layer layer[MAXL]; static int nlayer;

/* foreground chunks (F lines): slices of the level's FG textures, strung together in a fixed pseudo-random order */
#define MAXF 16
static struct { Spr s; int w, h, prob; float top; } fgc[MAXF]; static int nfg; static unsigned char fgseq[128]; static float fgTotal;

static int nLevels = 1;

static void freeLevel(void) {
    for (int i = 0; i < nlayer; i++) for (int k = 0; k < layer[i].n; k++) { free(layer[i].t[k].data); layer[i].t[k].data = NULL; }
    nlayer = 0;
    for (int i = 0; i < nfg; i++) { free(fgc[i].s.data); fgc[i].s.data = NULL; }
    nfg = 0; fgTotal = 0;
}

static void loadLevel(int idx) {   /* levels/<idx>/level.txt from mklevel.py: S speed yoff n / T file w h; sets are back-to-front */
    freeLevel();
    char pp[128], ln[128]; snprintf(pp, sizeof pp, "%slevels/%d/level.txt", dir, idx);
    FILE *f = fopen(pp, "r"); if (!f) return;
    Layer *L = NULL; int want = 0;
    while (fgets(ln, sizeof ln, f)) {
        if (ln[0] == 'S' && nlayer < MAXL) { L = &layer[nlayer++]; memset(L, 0, sizeof *L); sscanf(ln, "S %f %f %d", &L->speed, &L->yoff, &want); }
        else if (ln[0] == 'F' && nfg < MAXF) {
            char fn[32], full[80]; int w, h, pr; float top;
            if (sscanf(ln, "F %31s %d %d %f %d", fn, &w, &h, &top, &pr) == 5 && (snprintf(full, sizeof full, "levels/%d/%s", idx, fn), loadSpr(&fgc[nfg].s, full))) { fgc[nfg].w = w; fgc[nfg].h = h; fgc[nfg].top = top; fgc[nfg].prob = pr; nfg++; }
        }
        else if (ln[0] == 'T' && L && L->n < MAXT) {
            char fn[32], full[80]; int w, h; if (sscanf(ln, "T %31s %d %d", fn, &w, &h) == 3 && (snprintf(full, sizeof full, "levels/%d/%s", idx, fn), loadSpr(&L->t[L->n], full))) { L->w[L->n] = w; L->h[L->n] = h; L->total += w * 1.5f; L->n++; }
        }
    }
    fclose(f);
    if (nfg) {
        int tot = 0; for (int i = 0; i < nfg; i++) tot += fgc[i].prob > 0 ? fgc[i].prob : 1;
        unsigned r = 12345 + idx;
        for (int k = 0; k < 128; k++) { r = r * 1103515245u + 12345u; int v = (r >> 8) % tot, i = 0; while (i < nfg - 1 && (v -= fgc[i].prob > 0 ? fgc[i].prob : 1) >= 0) i++; fgseq[k] = i; fgTotal += fgc[i].w * 1.5f; }
    }
}

static void drawLevel(float scroll) {   /* the game renders at 480x320; x1.5 fills 640x480 */
    for (int i = 0; i < nlayer; i++) {
        Layer *L = &layer[i]; if (!L->n || L->total <= 0) continue;
        float x = -fmodf(scroll * L->speed, L->total); int k = 0;
        for (int guard = 0; x < W && guard < 64; guard++, k = (k + 1) % L->n) {
            float w = L->w[k] * 1.5f, h = L->h[k] * 1.5f;
            quad(&L->t[k], x, 480 - h + 38 + L->yoff * 1.5f, w, h, 0, 0, 1, 1); x += w;
        }
    }
    if (nfg && fgTotal > 0) {   /* the foreground (floor, crates, ceiling) moves at the full scroll speed; the game's y-up frame is 512 tall */
        float x = -fmodf(scroll, fgTotal);
        for (int k = 0; x < W && k < 128; k++) {
            int i = fgseq[k]; float w = fgc[i].w * 1.5f, h = fgc[i].h * 1.5f;
            if (x + w > 0) quad(&fgc[i].s, x, fgc[i].top * 1.5f - 7, w, h, 0, 0, (float)fgc[i].w / ((fgc[i].w + 7) / 8 * 8), (float)fgc[i].h / ((fgc[i].h + 7) / 8 * 8));
            x += w;
        }
    }
}

typedef struct { float minL, maxL, startAngle; int scaleTime, cw, rotT, on, off, withLasers, np, sd[8]; float w[8]; } ZPat;
static ZPat zp[16]; static int nzp; static float zGlobalP = 80; static int zGlobalSd;

static void loadZappers(void) {   /* lines from mkzappers.py */
    char pp[128], ln[512]; snprintf(pp, sizeof pp, "%szappers.txt", dir);
    FILE *f = fopen(pp, "r"); if (!f) return;
    while (fgets(ln, sizeof ln, f)) {
        char *q = ln + 1;
        if (ln[0] == 'G') { zGlobalP = strtod(q, &q); zGlobalSd = strtol(q, &q, 10); continue; }
        if (ln[0] != 'Z' || nzp >= 16) continue;
        ZPat *z = &zp[nzp++];
        z->minL = strtod(q, &q); z->maxL = strtod(q, &q); z->scaleTime = strtol(q, &q, 10); z->startAngle = strtod(q, &q);
        z->cw = strtol(q, &q, 10); z->rotT = strtol(q, &q, 10); z->on = strtol(q, &q, 10); z->off = strtol(q, &q, 10);
        z->withLasers = strtol(q, &q, 10); z->np = strtol(q, &q, 10); if (z->np > 8) z->np = 8;
        for (int i = 0; i < z->np; i++) { z->sd[i] = strtol(q, &q, 10); z->w[i] = strtod(q, &q); }
    }
    fclose(f);
}

static int pickZapper(int dist, int lasers) {
    float tot = 0, wt[16];
    for (int i = 0; i < nzp; i++) { float w = 0; for (int k = 0; k < zp[i].np; k++) if (zp[i].sd[k] <= dist) w = zp[i].w[k]; if (lasers && !zp[i].withLasers) w = 0; wt[i] = w; tot += w; }
    if (tot <= 0) return -1;
    float r = tot * (rand() / (RAND_MAX + 1.f));
    for (int i = 0; i < nzp; i++) { r -= wt[i]; if (r < 0) return i; }
    return -1;
}

static float segDist(float px, float py, float ax, float ay, float bx, float by) {   /* point to segment */
    float dx = bx - ax, dy = by - ay, t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy + 1e-6f);
    t = t < 0 ? 0 : t > 1 ? 1 : t; float qx = ax + t * dx - px, qy = ay + t * dy - py; return sqrtf(qx * qx + qy * qy);
}

/* Mech (vehicles/Jetlane/Mech.xml): gravity .36, initial boost 6, boost .45 for 20 frames; absorbs one hit */
static const int mechDrive[14] = {6,6,1,1,2,2,2,3,3,4,4,5,5,5};
/* per vehicle: sheet columns, frame px, padded sheet px, gravity, initial boost, boost (Hog.xml / Mech.xml) */
static const struct { int cols, fw, fh, pw, ph; float g, ib, b; int bfc, jumps; const char *spr; } VEH[16] = {
    {6, 85, 85, 512, 256, 0.36f, 6.0f, 0.45f, 20, 1, "mech.spr"}, {4, 64, 64, 256, 448, 0.30f, 6.5f, 0.60f, 12, 1, "hog.spr"},
    {6, 39, 54, 236, 164, 0.20f, 5.0f, 0.00f, 1, 10, "bird.spr"},
    {6, 42, 42, 252, 168, 0.60f, 0.0f, 0.0f, 0, 0, "gsuit.spr"},
    {4, 64, 64, 256, 128, 0, 0, 0, 0, 0, "tele.spr"},
    {4, 34, 47, 136, 144, 0.30f, 6.5f, 0.60f, 12, 1, "hover.spr"},
    {4, 38, 50, 152, 200, 0.30f, 6.5f, 0.60f, 12, 1, "skate.spr"},
    {1, 256, 128, 256, 128, 0.30f, 6.5f, 0.60f, 14, 1, "ecto.spr"},
    {1, 157, 62, 160, 64, 0.30f, 6.5f, 0.60f, 14, 1, "dlr.spr"},
    {1, 232, 98, 232, 100, 0.30f, 6.5f, 0.60f, 14, 1, "ship.spr"},
    {4, 76, 64, 304, 128, 0.30f, 6.5f, 0.60f, 14, 1, "sled.spr"},
    {1, 64, 32, 64, 32, 0.30f, 6.5f, 0.60f, 14, 1, "jski.spr"},
    {1, 32, 32, 32, 32, 0, 0, 0, 0, 0, "drb.spr"},   /* 12: Dragon, drawn from the parts below (no pick table entry of its own) */
    {1, 64, 64, 64, 64, 0, 0, 0, 0, 0, "drh.spr"}, {1, 32, 32, 32, 32, 0, 0, 0, 0, 0, "drb.spr"}, {4, 32, 32, 128, 32, 0, 0, 0, 0, 0, "drf.spr"} };   /* 13 head, 14 body, 15 flap: real sprites, my own layout (offsets live in native code) */   /* Jetski: one static 64x32 frame, no rider or handle; Hog-like physics (guess) */   /* Sleigh: real 4x2 animation sheet, reindeer not drawn; Hog-like physics (guess) */   /* Starship: one static frame; Hog-like physics (guess) */   /* DeLorean: one static frame; Hog-like physics (guess) */   /* Ecto-1: one static frame, no rider; Hog-like physics (guess) */   /* Ice Skates: real 4x4 sheet, rider baked in; Hog-like physics (guess) */   /* Hoverboard: sheet is a real 4x3 grid with the rider baked in; the physics are the Hog's, a guess */   /* Teleporter: press = vanish and reappear at a random height; sheet is real, timings are mine */   /* Gravity Suit: sheet layout is real, physics are mine (no definition file in the APK) */   /* Mech/Hog/ProfitBird .xml: JUMP_COUNT = jumps before landing */
static const int vpick[16] = {6, 1, 13, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
static int gMagnet, gSlow, gDud, gVMag, gRub;   /* shop: Coin Magnet owned (third number in save.txt) */
static void loadSave(int *best, int *total) {   /* save.txt: best distance, lifetime coins */
    char pp[128]; snprintf(pp, sizeof pp, "%ssave.txt", dir); FILE *f = fopen(pp, "r"); if (!f) return;
    int b = 0, t = 0, m = 0; if (fscanf(f, "%d %d %d", &b, &t, &m) >= 2 && b >= 0 && t >= 0) { *best = b; *total = t; gMagnet = m & 1; gSlow = m >> 1 & 1; gDud = m >> 2 & 1; gVMag = m >> 3 & 1; gRub = m >> 4 & 1; } fclose(f);
}
static void writeSave(int best, int total) {
    char pp[128]; snprintf(pp, sizeof pp, "%ssave.txt", dir); FILE *f = fopen(pp, "w"); if (!f) return; fprintf(f, "%d %d %d\n", best, total, gMagnet | gSlow << 1 | gDud << 2 | gVMag << 3 | gRub << 4); fclose(f);
}
static int mflip;   /* draw the next vehicle quad upside down (Gravity Suit on the ceiling) */
static void mechQuad(Spr *s, int vt, float x, float yb, float h, int f) {   /* h = drawn height; width keeps the frame's aspect */
    int c = (f - 1) % VEH[vt].cols, r = (f - 1) / VEH[vt].cols, fw = VEH[vt].fw, fh = VEH[vt].fh;
    float va = r * (float)fh / VEH[vt].ph, vb = (r + 1) * (float)fh / VEH[vt].ph;
    quad(s, x, yb - h, h * fw / fh, h, c * (float)fw / VEH[vt].pw, mflip ? vb : va, (c + 1) * (float)fw / VEH[vt].pw, mflip ? va : vb);
}
#define HIT() do { if (inv) {} else if (mech == 2) { mech = 0; inv = 90; sfx_play(sVDie, 0); } else if (!dead && state == 1) { dead = 60; sfx_play(sHurt, 0); } } while (0)

int main(int argc, char **argv) {
    gfxInit(); int fatOk = fatInitDefault();
    { FILE *t = fopen("sd:/apps/jetpackjoyride/pattern.txt", "r"); if (t) fclose(t); else dir = "fat:/apps/jetpackjoyride/"; } INPUT_Init(); INPUT_SetScreenSize(W, H);
    WIIUPDATE_SetAppPath(argc > 0 ? argv[0] : NULL); WIIUPDATE_Start("quatric", "jetpackjoyride-wii", "0.1.0");
    Spr barry = {0}, coin = {0};
    if (!loadSpr(&barry, "barry.spr")) fallbackSpr(&barry);
    if (!loadSpr(&coin, "coin.spr")) fallbackSpr(&coin);
    int haveB = 1, haveC = 1;
    loadPattern(); loadLasers();
    { char mp[160]; snprintf(mp, sizeof mp, "%smusic.ogg", dir); audio_start(mp); }
    char musicPath[4][160]; { const char *mn[4] = {"music.ogg", "mshop.ogg", "mhog.ogg", "mcolo.ogg"}; for (int i = 0; i < 4; i++) snprintf(musicPath[i], 160, "%s%s", dir, mn[i]); }   /* Music_Level, Music_Shop, Music_Hog, Music_Colossatron from SoundFiles/musicDesc; which one plays when is my mapping */
    Spr beam = {0}, emit = {0}; if (!loadSpr(&beam, "beam.spr")) fallbackSpr(&beam); if (!loadSpr(&emit, "emit.spr")) fallbackSpr(&emit);
    Spr missile = {0}, alert = {0}; if (!loadSpr(&missile, "missile.spr")) fallbackSpr(&missile); if (!loadSpr(&alert, "alert.spr")) fallbackSpr(&alert);
    loadMissile(); loadZappers(); loadFont();
    { char cp[128]; snprintf(cp, sizeof cp, "%slevels/count.txt", dir); FILE *cf = fopen(cp, "r"); if (cf) { if (fscanf(cf, "%d", &nLevels) != 1 || nLevels < 1) nLevels = 1; fclose(cf); } }
    int lvl = 0; loadLevel(lvl);
    Spr flame = {0}; int haveFlame = loadSpr(&flame, "flame.spr");   /* 4x2 grid of 16x32 frames, top row = burning */
    Spr bBody = {0}, bHead = {0}; int haveRig = loadSpr(&bBody, "body.spr") && loadSpr(&bHead, "head.spr");   /* 4x8 grids of 32x32 frames */
    int sCoin, sHurt, sLaserW, sMissW, sMissL, sJet, sVm[13], sVj[13], sGC, sTP, sSt1, sSt2, sLand, sZap, sVDie, sJS, sJE, sC2, sC3, sMS, sLS, sME; { char pp[160]; const char *n[] = {"coin.ogg", "hurt.ogg", "laserw.ogg", "missw.ogg", "missl.ogg", "jet.ogg", "mmount.ogg", "hmount.ogg", "mjump.ogg", "hjump.ogg", "zap.ogg", "vdie.ogg", "bmount.ogg", "bflap.ogg", "jstart.ogg", "jstop.ogg", "coin2.ogg", "coin3.ogg", "mstep.ogg", "lstart.ogg", "mexp.ogg", "gchg.ogg", "cft.ogg", "step1.ogg", "step2.ogg", "land.ogg"}; int *o[] = {&sCoin, &sHurt, &sLaserW, &sMissW, &sMissL, &sJet, &sVm[0], &sVm[1], &sVj[0], &sVj[1], &sZap, &sVDie, &sVm[2], &sVj[2], &sJS, &sJE, &sC2, &sC3, &sMS, &sLS, &sME, &sGC, &sTP, &sSt1, &sSt2, &sLand};
        for (int i = 0; i < 26; i++) { snprintf(pp, sizeof pp, "%s%s", dir, n[i]); *o[i] = sfx_load(pp); } sVm[3] = sVm[0]; sVj[3] = sGC; sVm[4] = sVm[0]; sVj[4] = sTP; sVm[5] = sVm[1]; sVj[5] = sVj[1]; sVm[6] = sVm[1]; sVj[6] = sVj[1]; sVm[7] = sVm[1]; sVj[7] = sVj[1]; sVm[8] = sVm[1]; sVj[8] = sVj[1]; sVm[9] = sVm[1]; sVj[9] = sVj[1]; sVm[10] = sVm[1]; sVj[10] = sVj[1]; sVm[11] = sVm[1]; sVj[11] = sVj[1]; sVm[12] = sVm[1]; sVj[12] = sVj[1]; }
    int jetV = -1, prevCur = -1, prevMst = 0;
    Spr bolt = {0}; if (!loadSpr(&bolt, "bolt.spr")) fallbackSpr(&bolt);
    int zi = -1, ztick = 0, zwait = 150; float zx = 0, zy = 0, zlen = 0;
    int cur = -1, ptick = 0, wait = 90, dead = 0, mst = 0, mtick = 0, mwait = 120; float my = 0, mx = 0;   /* mst: 0 idle, 1 alert, 2 flying */
    if (!fatOk) GX_SetCopyClear((GXColor){160, 0, 0, 255}, GX_MAX_Z24);          /* red: no SD */
    else if (!nc) GX_SetCopyClear((GXColor){160, 120, 0, 255}, GX_MAX_Z24);    /* amber: assets missing */
    Spr logoT = {0}, logoB = {0}; int haveLogo = loadSpr(&logoT, "logot.spr") && loadSpr(&logoB, "logob.spr");   /* title art: textures/logo/default (downscaled to fit) */
    Spr mechS[16] = {{0}}; int haveV[16], nV = 0, vt = 0, jl = 0, gd = 1, tt = 0; for (int i = 0; i < 16; i++) haveV[i] = loadSpr(&mechS[i], VEH[i].spr);
    int paused = 0, mech = 0, inv = 0, bf = 0, wasHeld = 0, mkwait = 600; float mpick = -999;   /* mech: 0 none, 2 riding */
    Spr pbk = {0}, pic[3] = {{0}, {0}, {0}}; int havePause = loadSpr(&pbk, "pbk.spr") && loadSpr(&pic[0], "pq.spr") && loadSpr(&pic[1], "pr.spr") && loadSpr(&pic[2], "pg.spr"), psel = 2, doRestart = 0, tsel = 0, inShop = 0, shopI = 0; Spr colH = {0}, colB = {0}, boomS = {0}; int haveCol = loadSpr(&colH, "colh.spr") && loadSpr(&colB, "colb.spr") && loadSpr(&boomS, "boom.spr"), boomOn = 0, boomDone = 0, boomHit = 0, newBest = 0;
    float boomX = 0, boomY = 0;   /* Colossatron sonic boom (real sonicBoom.png), timing mine */
    int cst = 0, bossDone = 0, bossHit = 0, nextBoss = 1500; float cbx = 0;   /* "boss": Colossatron art, behaviour is mine */
    Spr shopS[5] = {{0}, {0}, {0}, {0}, {0}}; int haveMag = loadSpr(&shopS[0], "magnet.spr") && loadSpr(&shopS[1], "smiss.spr") && loadSpr(&shopS[2], "dmiss.spr") && loadSpr(&shopS[3], "magnet.spr") && loadSpr(&shopS[4], "rub.spr"), dudLeft = 0;   /* shop items: Coin Magnet 500, Slow Missiles 400, Dud Missiles 450 (prices mine) */ Spr startG = {0}; int haveStart = loadSpr(&startG, "sg.spr");   /* Pause.uiscreen: three 216px buttons (Quit, Retry, Resume) on buttonBacking, icon at -20, label at +68; drawn at 0.6 */
    float y = 300, vy = 0, scroll = 0; int score = 0, frame = 0, state = 0, best = 0, lastDist = 0, total = 0;   /* state: 0 title, 1 playing, 2 game over */
    loadSave(&best, &total);
#ifdef AUTOTEST
    total = 2500; gMagnet = 0; gSlow = 0; gDud = 0; gVMag = 0; gRub = 0;
#endif
    for (;;) {
        INPUT_Update(); tick++;
        int dist = 0;
        { int mi = inShop && state == 0 ? 1 : cst ? 3 : (mech == 2 && vt == 1) ? 2 : 0; static int lastMi = 0; if (mi != lastMi) { lastMi = mi; audio_switch(musicPath[mi]);
#ifdef AUTOTEST
 { char pp[128]; snprintf(pp, sizeof pp, "%sdbg.txt", dir); FILE *df = fopen(pp, "a"); if (df) { fprintf(df, "music %d tick %d\n", mi, tick); fclose(df); } }
#endif
 } }
        if (btnDown() & WPAD_BUTTON_HOME) break;
        if (state == 1 && (btnDown() & WPAD_BUTTON_PLUS)) { paused ^= 1; psel = 2; }
        if (paused && state == 1 && havePause) {
            u32 bd = btnDown(); if ((bd & WPAD_BUTTON_LEFT) && psel > 0) psel--; if ((bd & WPAD_BUTTON_RIGHT) && psel < 2) psel++;
            if (bd & (WPAD_BUTTON_A | WPAD_BUTTON_2)) { if (psel == 2) paused = 0; else if (psel == 1) { paused = 0; state = 2; doRestart = 1; } else { paused = 0; state = 0; } }
        }
        if (paused && state == 1) { dist = (int)(scroll / 2); if (jetV > 0) { sfx_stop(jetV); jetV = -1; } goto render; }
        if (state != 1) { paused = 0;
            u32 td = btnDown();
            if (state == 0 && haveMag) {   /* title: START / SHOP; the shop sells the Coin Magnet (price is mine: the APK has none) */
                if (inShop) { if (td & WPAD_BUTTON_DOWN) shopI = (shopI + 1) % 5; if (td & WPAD_BUTTON_UP) shopI = (shopI + 4) % 5; if (td & WPAD_BUTTON_A) { int *own = shopI == 4 ? &gRub : shopI == 3 ? &gVMag : shopI == 2 ? &gDud : shopI ? &gSlow : &gMagnet, pr = shopI == 4 ? 350 : shopI == 3 ? 600 : shopI == 2 ? 450 : shopI ? 400 : 500; if (!*own && total >= pr) { total -= pr; *own = 1; writeSave(best, total); sfx_play(sC3, 0); } } if (td & (WPAD_BUTTON_B | WPAD_BUTTON_1)) inShop = 0; }
                else { if (td & (WPAD_BUTTON_UP | WPAD_BUTTON_DOWN)) tsel ^= 1; if (tsel == 1 && (td & (WPAD_BUTTON_A | WPAD_BUTTON_2))) inShop = 1; }
            }
            if (doRestart || ((btnDown() & (WPAD_BUTTON_A | WPAD_BUTTON_B | WPAD_BUTTON_1 | WPAD_BUTTON_2)) && !(state == 0 && haveMag && (inShop || tsel == 1)))) { doRestart = 0; y = 300; vy = 0; scroll = 0; score = 0; memset(cgot, 0, sizeof cgot); dudLeft = gDud ? 3 : 0; cst = 0;
#ifdef AUTOTEST
            nextBoss = 300;
#else
            nextBoss = 1500;
#endif
            cur = -1; wait = 90; mst = 0; mwait = 120; zi = -1; zwait = 150; dead = 0; mech = 0; inv = 0; mpick = -999; mkwait = 600; state = 1; if (nLevels > 1) { lvl = (lvl + 1) % nLevels; loadLevel(lvl); } }
            frame++; dist = lastDist; if (jetV > 0) { sfx_stop(jetV); jetV = -1; }
            goto render;
        }
        { int held = (btnHeld() & (WPAD_BUTTON_A | WPAD_BUTTON_B | WPAD_BUTTON_1 | WPAD_BUTTON_2)) != 0;
          if (mech == 2 && vt == 4) {   /* Teleporter: hovers; a press vanishes (invulnerable) and reappears at a random height */
              vy = 0;
              if (tt > 0) { inv = 2; if (--tt == 12) y = 60 + rand() % (H - 180); }
              else if (held && !wasHeld) { tt = 24; sfx_play(sTP, 0); }
          } else if (mech == 2 && vt == 3) {   /* Gravity Suit: a press flips gravity between floor and ceiling */
              if (held && !wasHeld) { gd = -gd; sfx_play(sGC, 0); }
              vy += gd * 0.9f; if (vy > 12) vy = 12; if (vy < -12) vy = -12;
          } else if (mech == 2) {
              if (y >= H - 121) jl = VEH[vt].jumps;
              if (held && !wasHeld && jl > 0) { jl--; vy = -1.5f * VEH[vt].ib; bf = VEH[vt].bfc; sfx_play(sVj[vt], 0); }
              vy += 1.5f * VEH[vt].g; if (held && bf > 0) { vy -= 1.5f * VEH[vt].b; bf--; } if (!held) bf = 0;
              if (vy > 12) vy = 12;
          } else {
              vy += (held ? -0.9f : 0.6f);
              if (vy > 9) vy = 9;
              if (vy < -9) vy = -9;
          }
          wasHeld = held; }
        y += vy; if (inv) inv--;
        if (!mech && mpick < -900 && !dead && (int)(scroll / 2) >= 150 && --mkwait <= 0) { mpick = W; nV = 0; static const int vw[13] = {20, 20, 50, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20};   /* Vehicles/*.xml <probability>: Mech 20, Hog 20, ProfitBird 50; Gravity Suit and Teleporter have no file in the APK, so 20 */
            int tw = 0; for (int i = 0; i < 13; i++) if (haveV[i] && haveV[13] && haveV[14] && haveV[15]) tw += vw[i];
            if (tw) { int r = rand() % tw; for (int i = 0; i < 13; i++) if (haveV[i] && haveV[13] && haveV[14] && haveV[15] && (r -= vw[i]) < 0) { vt = i; break; } nV = 1; } else mpick = -999; }
#ifdef AUTOTEST
        if (!mech && (int)(scroll / 2) == 100) { mech = 2; vt = haveV[lvl % 13] ? lvl % 13 : 0; gd = 1; tt = 0; }   /* test build: start on the Mech */
#endif
        if (mpick > -900) { mpick -= 4; if (mpick < -100) { mpick = -999; mkwait = 900; }
            else if (!mech && mpick < 140 && mpick + 80 > 60 && y > H - 220) { mech = 2; mpick = -999; gd = 1; tt = 0; sfx_play(sVm[vt], 0); } }
        if (y < 60) { y = 60; vy = 0; }
        if (y > H - 120) { y = H - 120; vy = 0; }   /* ceiling / floor */
        if (dead && mech != 2) {   /* death tumble: settings.xml DEATH_GRAVITY 0.24 (x1.5 screen scale), MIN_BOUNCE_VELOCITY 4, DEATH_BOUNCE_DAMPENING 0.99 (damped further here: the unit of that one is a guess) */
            vy += 0.24f * 1.5f; y += vy; if (y >= H - 121) { y = H - 121; vy = fabsf(vy) > 4 * 1.5f ? -vy * (gRub ? 0.8f : 0.5f) : 0;   /* Rubber Bones: DEATH_BOUNCE_DAMPENING_RUBBER 1.6 vs 0.99, scaled onto my 0.5 */ }
        }
        if (dead) { if (--dead == 0) { state = 2; lastDist = (int)(scroll / 2); newBest = lastDist > best; if (newBest) best = lastDist; total += score; writeSave(best, total);
#ifdef AUTOTEST
            { char pp[128]; snprintf(pp, sizeof pp, "%sdbg.txt", dir); FILE *df = fopen(pp, "a"); if (df) { fprintf(df, "run lvl %d vt %d dist %d tick %d mag %d slow %d dud %d vmag %d rub %d boss %d bhit %d boom %d/%d coins %d left %d\n", lvl, vt, lastDist, tick, gMagnet, gSlow, gDud, gVMag, gRub, bossDone, bossHit, boomDone, boomHit, score, total); fclose(df); } }   /* headless check without screenshots */
#endif
        } }
        else scroll += 4;
        frame++;
        dist = (int)(scroll / 2);   /* ~120 "distance" units per second */
        if (!dead) {
            if (cur < 0) { if (--wait <= 0) { cur = pickPattern(dist); ptick = 0; wait = spawnWait(dist); } }
            else if (++ptick > pats[cur].time + 60) cur = -1;
            if (cur >= 0 && ptick > 60) for (int i = 0; i < pats[cur].n; i++) {
                float ly = 240 - pats[cur].y[i] * 1.6f;
                if (fabsf(ly - (y + 45)) < 22) { HIT(); break; }
            }
        }
        if (!dead && cur < 0) {   /* missiles share the screen with lasers only when no laser pattern is active */
            if (mst == 0 && dist >= mStart && --mwait <= 0) { mwait = 120; if (rand() % 100 < mProb) { mst = 1; mtick = 0; my = y + 45; } }
            else if (mst == 1) { float t = y + 45; my += fminf(fmaxf(t - my, -mChase * 2), mChase * 2); if (++mtick > 90) { mst = 2; mx = W; } }
        }
        if (haveCol && !dead && state == 1) {   /* Colossatron: a head and 6 body segments sweep in from the right in a sine wave; touching one is a hit; 25 coins for outlasting it */
            if (!cst && dist >= nextBoss && cur < 0 && mst == 0) { cst = 1; cbx = W + 40; nextBoss = dist + 2500; }
            if (cst) {
                cbx -= 4.5f;
                for (int i = 0; i < 7; i++) { float sx = cbx + i * 44, sy = 170 + 90 * sinf(frame * 0.05f + i * 0.5f); if (sx < 150 && sx + 56 > 60 && fabsf(sy + 28 - (y + 45)) < 42) { int d0 = dead, m0 = mech; HIT(); if (dead != d0 || mech != m0) bossHit++; } }
                if (!boomOn && (frame % 100) == 50 && cbx < W - 120) { boomOn = 1; boomX = cbx; boomY = 170 + 90 * sinf(frame * 0.05f) + 28; }
                if (boomOn) {
                    boomX -= 7.f; boomDone += boomX < -130;
                    if (boomX < 150 && boomX + 120 > 60 && fabsf(boomY - (y + 45)) < 30) { int d0 = dead, m0 = mech; HIT(); if (dead != d0 || mech != m0) boomHit++; boomOn = 0; }
                    if (boomX < -130) boomOn = 0;
                }
                if (cbx + 7 * 44 < -64) { cst = 0; boomOn = 0; score += 25; bossDone++; }
            }
        }
        if (mst == 2 && !dead) {
            mx -= mSpeed * (gSlow ? 0.72f : 1.f);   /* Slow Missiles gadget: perks.xml value1 0.72 */
            if (mx < 150 && mx + 64 > 60 && fabsf(my - (y + 45)) < 28) { if (dudLeft > 0) dudLeft--; else HIT(); mst = 0; sfx_play(sME, 0); }   /* Dud Missiles: perks.xml value1 3 */
            if (mx < -64) { mst = 0; }
        }
        if (!dead) {
            if (zi < 0) { if (dist >= zGlobalSd && --zwait <= 0) { zwait = 150; if (rand() % 100 < zGlobalP) { zi = pickZapper(dist, cur >= 0); if (zi >= 0) { ztick = 0; sfx_play(sZap, 0); zx = W + 100; zlen = zp[zi].minL + (zp[zi].maxL - zp[zi].minL) * (rand() / (RAND_MAX + 1.f)); zy = 90 + rand() % 220; } } } }
            else {
                ztick++; zx -= 4;
                ZPat *z = &zp[zi]; int period = z->on + z->off; int lethal = period <= 1 || (ztick % period) < z->on;
                float ang = z->startAngle + (z->rotT ? (z->cw ? 1 : -1) * 360.f * ztick / z->rotT : 0);
                float hx = cosf(ang * 0.0174533f) * zlen / 2, hy = sinf(ang * 0.0174533f) * zlen / 2;
                if (lethal && segDist(105, y + 45, zx - hx, zy - hy, zx + hx, zy + hy) < 20) { HIT(); }
                if (zx < -200) zi = -1;
            }
        }
        if (cur >= 0 && ptick == 61) sfx_play(sLS, 0);
        if (cur >= 0 && prevCur < 0) sfx_play(sLaserW, 0);
        if (mst == 1 && prevMst == 0) sfx_play(sMissW, 0);
        if (mst == 2 && prevMst == 1) sfx_play(sMissL, 0);
        prevCur = cur; prevMst = mst;
        { int thrust = !dead && mech != 2 && (btnHeld() & (WPAD_BUTTON_A | WPAD_BUTTON_B | WPAD_BUTTON_1 | WPAD_BUTTON_2));
          if (thrust && !sfx_playing(jetV)) { sfx_play(sJS, 0); jetV = sfx_play(sJet, 1); } else if (!thrust && jetV > 0) { sfx_stop(jetV); jetV = -1; sfx_play(sJE, 0); }
          if (mech == 2 && vt == 0 && y >= H - 121 && frame % 14 == 0) sfx_play(sMS, 0);
          { static int wasAir; int air = y < H - 121;
            if (!dead && mech != 2 && !air && frame % 9 == 0) sfx_play((frame / 9) & 1 ? sSt1 : sSt2, 0);
            if (!dead && mech != 2 && wasAir && !air) sfx_play(sLand, 0);
            wasAir = air; } }
        render:
        GX_InvVtxCache(); GX_InvalidateTexAll();
        drawLevel(scroll);
        if (haveC) for (int i = 0; i < nc; i++) {
            float px = cx[i] + 700 - fmodf(scroll, patW + 700 + 640); if (px < -20 || px > W || cgot[i]) { if (px < -20) cgot[i] = 0; continue; }
            if ((gMagnet && hypotf(px + 8 - 95, cy[i] + 8 - (y + 45)) < 130) || (gVMag && mech == 2 && hypotf(px + 8 - 95, cy[i] + 8 - (y + 50)) < 190) || (px < 130 && px + 16 > 60 && cy[i] + 16 > y && cy[i] < y + 90)) { cgot[i] = 1; score++; sfx_play(rand() % 3 == 0 ? sC2 : rand() & 1 ? sC3 : sCoin, 0); continue; }
            int fr = (frame / 4 + i) & 7; quad(&coin, px, cy[i], 16, 16, fr / 8.f, 0, (fr + 1) / 8.f, 1);
        }
        if (zi >= 0) {
            ZPat *z = &zp[zi]; int period = z->on + z->off; int lethal = period <= 1 || (ztick % period) < z->on;
            float ang = z->startAngle + (z->rotT ? (z->cw ? 1 : -1) * 360.f * ztick / z->rotT : 0);
            float hx = cosf(ang * 0.0174533f) * zlen / 2, hy = sinf(ang * 0.0174533f) * zlen / 2;
            if (lethal) {   /* zapEffects.tex: a glow band and six arc bands (y ranges in the 512x236 sheet); the arcs flicker over the glow */
                static const short zb[7][2] = {{4, 55}, {64, 87}, {90, 113}, {124, 143}, {153, 171}, {183, 200}, {213, 230}};
                quadRotUV(&bolt, zx, zy, zlen, 30, ang, 0, zb[0][0] / 236.f, 1, zb[0][1] / 236.f);
                for (int q = 0; q < 2; q++) { int b = 1 + ((frame / 3) + q * 3) % 6; quadRotUV(&bolt, zx, zy, zlen, (zb[b][1] - zb[b][0]) * 1.3f, ang, 0, zb[b][0] / 236.f, 1, zb[b][1] / 236.f); }
            }
            quad(&emit, zx - hx - 16, zy - hy - 20, 32, 40, 0, 0, 64.f / 64, 85.f / 88);
            quad(&emit, zx + hx - 16, zy + hy - 20, 32, 40, 0, 0, 64.f / 64, 85.f / 88);
        }
        if (cst && boomOn) quad(&boomS, boomX, boomY - 36, 128, 72, 1, 0, 0, 1);
        if (cst) for (int i = 6; i >= 0; i--) { float sx = cbx + i * 44, sy = 170 + 90 * sinf(frame * 0.05f + i * 0.5f); if (i) quad(&colB, sx, sy, 56, 56, 0, 0, 1, 1); else quad(&colH, sx - 8, sy - 4, 64, 64, 1, 0, 0, 1); }
        if (mst == 1 && (mtick / 8) % 2 == 0) { int fr = (frame / 8) & 3; quad(&alert, W - 70, my - 32, 64, 64, fr / 4.f, 0, (fr + 1) / 4.f, 1); }
        if (mst == 2) { int fr = (frame / 4) & 3; quad(&missile, mx, my - 16, 64, 32, fr / 4.f, 0, (fr + 1) / 4.f, 1); }
        if (cur >= 0) for (int i = 0; i < pats[cur].n; i++) {
            float ly = 240 - pats[cur].y[i] * 1.6f; int live = ptick > 60;
            if (!live && (ptick / 6) & 1) continue;                      /* flashing warning */
            quad(&beam, 64, ly - (live ? 12 : 4), W - 128, live ? 24 : 8, 0, 0, 1, 1);
            quad(&emit, 0, ly - 42, 64, 85, 0, 0, 64.f / 64, 85.f / 88);
            quad(&emit, W - 64, ly - 42, 64, 85, 64.f / 64, 0, 0, 85.f / 88);
        }
        if (mpick > -900 && haveV[vt]) mechQuad(&mechS[vt], vt, mpick, H - 30, 80, vpick[vt]);
        if (mech == 2 && haveV[vt]) {
            int g = y >= H - 121, held = (btnHeld() & (WPAD_BUTTON_A | WPAD_BUTTON_B | WPAD_BUTTON_1 | WPAD_BUTTON_2)) != 0, f;
            static const int birdFlap[8] = {15, 17, 1, 3, 5, 7, 9, 11};
            int gs = vt == 3, ceil = gs && y <= 61; mflip = gs && gd < 0;
            if (vt == 4) f = tt > 12 ? 5 + (24 - tt) / 3 : tt > 0 ? 8 - (12 - tt) / 3 : 1 + (frame / 6) % 4;
            else if (gs) f = (g || ceil) ? 1 + (frame / 4) % 6 : gd > 0 ? 7 + (frame / 5) % 2 : 13 + (frame / 5) % 2;
            else if (vt == 2) f = g ? 13 : birdFlap[(frame / 2) % 8];
            else if (vt == 10) f = 1 + (frame / 6) % 8;
            else if (vt >= 7) f = 1;
            else if (vt == 6) f = g ? 1 + (frame / 4) % 4 : 5 + (frame / 6) % 2;
            else if (vt == 5) f = g ? 1 + (frame / 4) % 4 : 5 + (frame / 5) % 4;
            else if (vt) f = g ? 1 + (frame / 4) % 4 : 5 + (frame / 4) % 4;
            else f = g ? mechDrive[(frame / 2) % 14] : held && bf > 0 ? 13 + ((frame / 4) & 1) : vy < 0 ? 12 : 16;
            float sz = vt == 1 ? 130 : vt == 5 ? 110 : vt == 6 ? 120 : vt == 7 ? 100 : vt == 8 ? 80 : vt == 9 ? 80 : vt == 10 ? 90 : vt == 11 ? 70 : vt == 2 ? 110 : vt == 3 ? 100 : 150, k = sz / VEH[vt].fh, w = sz * VEH[vt].fw / VEH[vt].fh; if (vt == 12) {   /* head leads, three body segments trail in a wave, flaps on the middle one */
                float yb = y + 100; for (int i = 2; i >= 0; i--) mechQuad(&mechS[14], 14, 24 + i * 30, yb + 6 * sinf((frame + i * 12) * 0.15f), 40, 1);
                mechQuad(&mechS[15], 15, 50, yb - 22 + 4 * sinf((frame + 12) * 0.15f), 40, 1 + (frame / 4) % 4); mechQuad(&mechS[13], 13, 100, yb - 2, 56, 1);
            } else mechQuad(&mechS[vt], vt, 20, y + 100, sz, f); mflip = 0;
            if (haveRig && vt < 3) {   /* head placement from Vehicle*Data.xml (offset from frame centre; rotation ignored) */
                const short *h = vt == 2 ? VHEAD2[f] : vt ? VHEAD1[f] : VHEAD0[f]; int c = (h[2] - 1) % 4, r = (h[2] - 1) / 4; float hs = 32 * k;
                quadRotUV(&bHead, 20 + w / 2 + h[0] * k, y + 100 - sz / 2 + h[1] * k, hs, hs, (float)h[3], c / 4.f, r / 8.f, (c + 1) / 4.f, (r + 1) / 8.f);
            }
        } else if (inv && (frame / 3) & 1) { } else
        if (haveRig) {   /* the game's own default costume: row 0 run, 1 thrust, 2 fall */
            int onGround = y >= H - 121, thrust = btnHeld() & (WPAD_BUTTON_A | WPAD_BUTTON_B | WPAD_BUTTON_1 | WPAD_BUTTON_2);
            int row = onGround ? 0 : thrust ? 1 : 2, col = (frame / 6) & 3;
            float u0 = col / 4.f, u1 = (col + 1) / 4.f, v0 = row / 8.f, v1 = (row + 1) / 8.f;
            if (haveFlame && thrust && !dead) { int fc = (frame / 3) & 3; quad(&flame, 76, y + 56, 28, 56, fc / 4.f, 0, (fc + 1) / 4.f, 0.5f); }
            quad(&bBody, 60, y + 14, 96, 96, u0, v0, u1, v1);
            quad(&bHead, 66, y - 8, 80, 80, u0, v0, u1, v1);   /* head offset is a guess: the real anchors live in native code */
        } else
        if (haveB) quad(&barry, 60, y, 90, 90, 0, 0, (float)barry.w / ((barry.w + 3) / 4 * 4), (float)barry.h / ((barry.h + 3) / 4 * 4));
                if (state != 1) {
            char t[48];
            if (state == 0) { snprintf(t, sizeof t, "BEST %dM   COINS %d", best, total); drawText(W / 2 - 170, 240, 0.7f, t); }
            if (state == 0 && haveLogo) {
                quad(&logoT, 24, 40, 360, 218, 0, 0, 360.f / 360, 218.f / 220);
                quad(&logoB, 400, 80, 192, 270, 0, 0, 192.f / 192, 270.f / 272);
            } else drawText(W / 2 - 230, 120, 1.4f, state == 0 ? "JETPACK JOYRIDE" : "GAME OVER");
            if (state == 2) { snprintf(t, sizeof t, "DISTANCE %dM   BEST %dM", lastDist, best); drawText(W / 2 - 230, 190, 0.8f, t); snprintf(t, sizeof t, "COINS %d   TOTAL %d", score, total); drawText(W / 2 - 230, 225, 0.7f, t); if (newBest && ((frame / 20) & 1)) drawText(W / 2 - 110, 265, 0.9f, "NEW BEST"); }
            if (state == 0 && havePause && haveStart) {   /* TitleScreen buttons.startPressed: 332x96 buttonBacking, glow bar at -144, "START" at +38; drawn at 0.75 */
                float cx = W / 2, cy = 320, bw = 332 * 0.75f, bh = 96 * 0.75f; quad(&pbk, cx - bw / 2, cy - bh / 2, bw, bh, 0, 0, 1, 1); quad(&startG, cx - 144 * 0.75f - 24, cy - 48, 48, 96, 0, 0, 1, 1);
                drawText(cx + 38 * 0.75f - 55, cy - 16, 0.9f, "START");
                if (haveMag) {
                    float sy = 400, sw = 260 * 0.6f, sh = 80 * 0.6f; if (tsel == 1) { sw += 8; sh += 6; } quad(&pbk, cx - sw / 2, sy - sh / 2, sw, sh, 0, 0, 1, 1); drawText(cx - 28, sy - 12, 0.7f, "SHOP");
                    if (inShop) {
                        quad(&pbk, 120, 150, 400, 250, 0, 0, 1, 1); quad(&shopS[shopI], 140, 200, 80, 80, 0, 0, 1, 1);
                        drawText(240, 180, 0.8f, shopI == 4 ? "RUBBER BONES" : shopI == 3 ? "VEHICLE MAGNET" : shopI == 2 ? "DUD MISSILES" : shopI ? "SLOW MISSILES" : "COIN MAGNET"); drawText(240, 220, 0.6f, shopI == 4 ? "BOUNCE FURTHER WHEN HIT" : shopI == 3 ? "VEHICLES ATTRACT COINS" : shopI == 2 ? "FIRST 3 MISSILES ARE DUDS" : shopI ? "MISSILES FLY SLOWER" : "PULLS IN NEARBY COINS");
                        snprintf(t, sizeof t, (shopI == 4 ? gRub : shopI == 3 ? gVMag : shopI == 2 ? gDud : shopI ? gSlow : gMagnet) ? "OWNED" : shopI == 4 ? "350 COINS  A: BUY" : shopI == 3 ? "600 COINS  A: BUY" : shopI == 2 ? "450 COINS  A: BUY" : shopI ? "400 COINS  A: BUY" : "500 COINS  A: BUY"); drawText(240, 260, 0.7f, t);
                        snprintf(t, sizeof t, "YOU HAVE %d   B: BACK", total); drawText(150, 350, 0.6f, t);
                    }
                }
            } else if ((frame / 30) & 1) drawText(W / 2 - 170, 300, 0.9f, "PRESS A TO START");
        }
        if (paused && havePause) {
            static const char *const lab[3] = {"QUIT", "RETRY", "RESUME"};
            for (int i = 0; i < 3; i++) {
                float cx = W / 2 + (i - 1) * 150, cy = 240, bs = i == psel ? 140 : 130;
                quad(&pbk, cx - bs / 2, cy - bs / 2, bs, bs, 0, 0, 1, 1); quad(&pic[i], cx - 38, cy - 38 - 12, 77, 77, 0, 0, 1, 1);
                drawText(cx - strlen(lab[i]) * 10.5f, cy + 24, 0.6f, lab[i]);
            }
            drawText(W / 2 - 110, 130, 1.2f, "PAUSED");
        } else if (paused) drawText(W / 2 - 110, 200, 1.2f, "PAUSED");
        { char hud[48]; snprintf(hud, sizeof hud, "%dM", dist); drawText(20, 14, 1.0f, hud); snprintf(hud, sizeof hud, "COINS %d", score); drawText(20, 50, 0.7f, hud); snprintf(hud, sizeof hud, "LEVEL %d/%d", lvl + 1, nLevels); drawText(20, 78, 0.5f, hud); }
        GX_DrawDone(); GX_CopyDisp(xfb[fb ^= 1], GX_TRUE); VIDEO_SetNextFramebuffer(xfb[fb]); VIDEO_Flush(); VIDEO_WaitVSync();
    }
    audio_stop(); INPUT_Shutdown(); return 0;
}
