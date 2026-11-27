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
#include <asndlib.h>
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
    if (tick == 60 || tick == 125 || tick == 135 || tick == 145 || tick == 155 || tick == 165 || tick == 175 || tick == 185 || tick == 195 || tick == 205 || tick == 215 || tick == 225 || tick == 235 || tick == 245 || tick == 255 || tick == 265 || tick == 275 || tick == 285) return WPAD_BUTTON_DOWN; if (tick == 90 || tick == 120 || tick == 130 || tick == 140 || tick == 150 || tick == 160 || tick == 170 || tick == 180 || tick == 190 || tick == 200 || tick == 210 || tick == 220 || tick == 230 || tick == 240 || tick == 250 || tick == 260 || tick == 270 || tick == 280 || tick == 290) return WPAD_BUTTON_A; if (tick == 300) return WPAD_BUTTON_B; if (tick == 310) return WPAD_BUTTON_UP;   /* shop: select, open, buy, close, back to START */
    return (tick == 420 || (tick > 420 && tick % 600 == 90)) ? WPAD_BUTTON_A : (tick % 600 == 300 || tick % 600 == 480) ? WPAD_BUTTON_PLUS : 0;   /* start / restart / pause / resume */
#else
    return INPUT_ButtonsDown();
#endif
}
static u32 btnHeld(void) {
#ifdef AUTOTEST
#if AUTOTEST == 2
    return (tick % 120) < 20 ? WPAD_BUTTON_A : 0;   /* short hops: mostly running on the floor */
#else
    return ((tick / 25) & 1) ? WPAD_BUTTON_A : 0;
#endif
#else
    return INPUT_ButtonsHeld();
#endif
}

static void dimSpr(Spr *s) {   /* translucent black 4x4 for the pause backing (Pause.uiscreen BlackBacking) */
    u8 *d = memalign(32, 64); memset(d, 0, 64); for (int i = 0; i < 32; i += 2) d[i] = 150;
    DCFlushRange(d, 64); s->data = d; s->w = s->h = 4; GX_InitTexObj(&s->obj, d, 4, 4, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
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

static int nlg, lgSd[8]; static float lgV[8];   /* LazerPatterns.xml root: chance of a laser event by distance */
static float laserProb(int dist) { float v = 0; for (int i = 0; i < nlg; i++) if (lgSd[i] <= dist) v = lgV[i]; return v; }
static void loadLasers(void) {   /* lines: P time n y.. np (startDistance weight).. */
    char pp[128], ln[512]; snprintf(pp, sizeof pp, "%slasers.txt", dir);
    FILE *f = fopen(pp, "r"); if (!f) return;
    while (npat < MAXP && fgets(ln, sizeof ln, f)) {
        char *q = ln + 1; Pat *t = &pats[npat];
        if (ln[0] == 'G') { nlg = 0; while (nlg < 8 && *q && *q != '\n' && *q != '\r') { char *e; int sd = strtol(q, &e, 10); if (e == q) break; q = e; lgSd[nlg] = sd; lgV[nlg++] = strtod(q, &q); } continue; }
        t->time = strtol(q, &q, 10); t->n = strtol(q, &q, 10); if (t->n > MAXY) t->n = MAXY;
        for (int i = 0; i < t->n; i++) t->y[i] = strtod(q, &q);
        t->np = strtol(q, &q, 10); if (t->np > 8) t->np = 8;
        for (int i = 0; i < t->np; i++) { t->sd[i] = strtol(q, &q, 10); t->w[i] = strtod(q, &q); }
        npat++;
    }
    fclose(f);
}

static float patWeight(const Pat *t, int dist) { float w = 0; for (int i = 0; i < t->np; i++) if (t->sd[i] <= dist) w = t->w[i]; return w; }

static float spawnGap(int dist) {   /* LevelPacing.xml MIN/MAX_SPAWN_INTERVAL by distance: game px of travel between spawn events (x1.5 to our px) */
    static const short mn[16] = {280, 250, 220, 200, 200, 200, 200, 200, 190, 190, 180, 180, 170, 170, 160, 150}, mx[16] = {470, 420, 390, 350, 320, 280, 275, 270, 265, 260, 255, 250, 245, 240, 235, 230};
    int k = dist < 2500 ? 0 : (dist - 2500) / 500 + 1; if (k > 15) k = 15;
    return (mn[k] + (mx[k] - mn[k]) * (rand() / (RAND_MAX + 1.f))) * 1.5f;
}
static int pickPattern(int dist) {   /* weighted by the game's own distance-based probabilities */
    float tot = 0; for (int i = 0; i < npat; i++) tot += patWeight(&pats[i], dist);
    if (tot <= 0) return -1;
    float r = tot * (rand() / (RAND_MAX + 1.f));
    for (int i = 0; i < npat; i++) { r -= patWeight(&pats[i], dist); if (r < 0) return i; }
    return -1;
}

static int zset;   /* obstacle/missile set in use: 0 on foot, 1+vt while riding vehicle vt */
static int mStartS[13]; static float mProbS[13], mChaseS[13], mSpeedS[13];   /* defaults = MissileProbability.xml; sets 1+vt from Streaks/<vehicle>Missles.xml */
#define mStart (mStartS[zset])
#define mProb (mProbS[zset])
#define mChase (mChaseS[zset])
#define mSpeed (mSpeedS[zset])
static void loadMissile(void) {
    char pp[128], ln[128]; mStartS[0] = 200; mProbS[0] = 15; mChaseS[0] = 1.2f; mSpeedS[0] = 10;
    snprintf(pp, sizeof pp, "%smissile.txt", dir);
    FILE *f = fopen(pp, "r");
    if (f) { if (fgets(ln, sizeof ln, f)) sscanf(ln, "M %d %f %f %f", &mStartS[0], &mProbS[0], &mChaseS[0], &mSpeedS[0]); fclose(f); }
    for (int i = 1; i < 13; i++) {   /* a vehicle without a missile file of its own keeps the on-foot numbers */
        mStartS[i] = mStartS[0]; mProbS[i] = mProbS[0]; mChaseS[i] = mChaseS[0]; mSpeedS[i] = mSpeedS[0];
        snprintf(pp, sizeof pp, "%smv%d.txt", dir, i - 1); f = fopen(pp, "r");
        if (f) { if (fgets(ln, sizeof ln, f)) sscanf(ln, "M %d %f %f %f", &mStartS[i], &mProbS[i], &mChaseS[i], &mSpeedS[i]); fclose(f); }
    }
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
        if (!g->xa) g = &glyph[' '];   /* no glyph (punctuation): leave a space */
        if (g->w) quad(&fontSpr, x + g->xo * sc, y + g->yo * sc, g->w * sc, g->h * sc, g->x / 256.f, g->y / 256.f, (g->x + g->w) / 256.f, (g->y + g->h) / 256.f);
        x += g->xa * sc;
    }
}

#define NSHOP_MAX 24
static char gadTxt[NSHOP_MAX][200];   /* real gadget descriptions from gadgets.txt; empty = use shopDesc */
static void loadGadgets(const char *dir) {
    char pp[128]; snprintf(pp, sizeof pp, "%sgadgets.txt", dir); FILE *f = fopen(pp, "r"); if (!f) return;
    for (int i = 0; i < NSHOP_MAX && fgets(gadTxt[i], sizeof gadTxt[i], f); i++) gadTxt[i][strcspn(gadTxt[i], "\r\n")] = 0;
    fclose(f);
}
/* word-wrap t into lines of at most n characters */
static void drawWrapped(float x, float y, float sc, float lh, int n, const char *t) {
    char ln[64]; int len = 0;
    while (*t) {
        int w = 0; while (t[w] && t[w] != ' ') w++;
        if (len && len + 1 + w > n) { ln[len] = 0; drawText(x, y, sc, ln); y += lh; len = 0; }
        if (w >= (int)sizeof ln - len - 2) w = sizeof ln - len - 2;
        if (len) ln[len++] = ' ';
        memcpy(ln + len, t, w); len += w; t += w; while (*t == ' ') t++;
    }
    if (len) { ln[len] = 0; drawText(x, y, sc, ln); }
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

/* Red alarm lights: the level files put LIGHT_HALLWAY_ALARM on wall pieces (25% of them, offset 48 along the piece).
   This port does not carry per-piece objects, so every foreground piece gets the same 25% roll (a fixed hash, so a light stays put),
   at one wall height of my choosing.
   The game's alarm art is not among the extracted textures; the light drawn here is a plain generated red glow. */
static Spr alarmS; static int haveAlarm;
static void makeAlarm(void) {   /* 32x32 RGBA8 red disc with a soft edge */
    u8 *d = memalign(32, 32 * 32 * 4); if (!d) return;
    for (int yy = 0; yy < 32; yy++) for (int xx = 0; xx < 32; xx++) {
        float r = hypotf(xx - 15.5f, yy - 15.5f) / 16; int a = r >= 1 ? 0 : (int)(255 * (1 - r * r)), o = ((yy / 4) * 8 + xx / 4) * 64 + ((yy % 4) * 4 + xx % 4) * 2;
        d[o] = a; d[o + 1] = 255; d[o + 32] = r < 0.35f ? 200 : 20; d[o + 33] = r < 0.35f ? 200 : 20;   /* RGBA8 tiles: AR plane then GB plane */
    }
    DCFlushRange(d, 32 * 32 * 4); alarmS.data = d; alarmS.w = alarmS.h = 32; GX_InitTexObj(&alarmS.obj, d, 32, 32, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE); haveAlarm = 1;
}
/* calls f(key, x, y) for each alarm light on screen; key identifies the light across frames */
static void eachAlarm(float scroll, void (*f)(int, float, float, void *), void *u) {
    if (!nfg || fgTotal <= 0) return;
    int lap = (int)(scroll / fgTotal); float x = -fmodf(scroll, fgTotal);
    for (int k = 0; x < W && k < 128; k++) {
        int i = fgseq[k]; float w = fgc[i].w * 1.5f, h = fgc[i].h * 1.5f; unsigned hsh = (unsigned)(k * 2654435761u) ^ (unsigned)(lap * 40503u);
        if ((hsh >> 7) % 100 < 25 && x + w > -40) f(lap * 128 + k, x + w / 2 + 48 * 1.5f, 330, u);   /* one wall height for all (mine): the piece-relative offset lands on the floor for floor pieces */
        x += w;
    }
}
static void drawAlarmCb(int key, float ax, float ay, void *u) { int fr = *(int *)u; float k = (fr / 8 + key) & 1 ? 26 : 34; quad(&alarmS, ax - k / 2, ay - k / 2, k, k, 0, 0, 1, 1); if ((fr / 8 + key) & 1) {} else quad(&alarmS, ax - 30, ay - 30, 60, 60, 0, 0, 1, 1); }
typedef struct { float by; int hit; } AlarmQ;
static void touchAlarmCb(int key, float ax, float ay, void *u) { AlarmQ *q = u; if (fabsf(ax - 105) < 40 && fabsf(ay - q->by) < 50) q->hit = key; }

typedef struct { float minL, maxL, startAngle; int scaleTime, cw, rotT, on, off, withLasers, np, sd[8]; float w[8]; } ZPat;
/* set 0 = ObstaclePatterns.xml (on foot); set 1+vt = Streaks/<vehicle>Obstacles.xml, used while riding vehicle vt (LevelPacing.xml streaks) */
static ZPat zpS[13][16]; static int nzpS[13]; static float zGlobalPS[13]; static int zGlobalSdS[13];
#define zp (zpS[zset])
#define nzp (nzpS[zset])
#define zGlobalP (zGlobalPS[zset])
#define zGlobalSd (zGlobalSdS[zset])

static void loadZapperSet(int set) {   /* lines from mkzappers.py */
    char pp[128], ln[512]; if (set) snprintf(pp, sizeof pp, "%szv%d.txt", dir, set - 1); else snprintf(pp, sizeof pp, "%szappers.txt", dir);
    FILE *f = fopen(pp, "r"); if (!f) return; zGlobalPS[set] = 80;
    while (fgets(ln, sizeof ln, f)) {
        char *q = ln + 1;
        if (ln[0] == 'G') { zGlobalPS[set] = strtod(q, &q); zGlobalSdS[set] = strtol(q, &q, 10); continue; }
        if (ln[0] != 'Z' || nzpS[set] >= 16) continue;
        ZPat *z = &zpS[set][nzpS[set]++];
        z->minL = strtod(q, &q); z->maxL = strtod(q, &q); z->scaleTime = strtol(q, &q, 10); z->startAngle = strtod(q, &q);
        z->cw = strtol(q, &q, 10); z->rotT = strtol(q, &q, 10); z->on = strtol(q, &q, 10); z->off = strtol(q, &q, 10);
        z->withLasers = strtol(q, &q, 10); z->np = strtol(q, &q, 10); if (z->np > 8) z->np = 8;
        for (int i = 0; i < z->np; i++) { z->sd[i] = strtol(q, &q, 10); z->w[i] = strtod(q, &q); }
    }
    fclose(f);
}

static void loadZappers(void) { for (int i = 0; i < 13; i++) loadZapperSet(i); }

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
static int gMagnet, gSlow, gDud, gVMag, gRub, gMul, gVS, gGB, gDZ, gXR, gBR, gFM, gIcy, gDog, gTM, gTG, gBM, gLT, gFP, gAB, gNR, gHS, gFB, gHSnone;   /* gFB: Final Blasts in stock */   /* gHS: Head Starts in stock (consumable); gHSnone stays 0 so the shop never shows it as owned */
#define FB_SLOT 22
#define HS_SLOT 21   /* shop: Coin Magnet owned (third number in save.txt) */
static void loadSave(int *best, int *total) {   /* save.txt: best distance, lifetime coins */
    char pp[128]; snprintf(pp, sizeof pp, "%ssave.txt", dir); FILE *f = fopen(pp, "r"); if (!f) return;
    int b = 0, t = 0, m = 0, hs = 0, fb = 0; if (fscanf(f, "%d %d %d %d %d", &b, &t, &m, &hs, &fb) >= 2 && b >= 0 && t >= 0) { *best = b; *total = t; gHS = hs > 0 ? hs : 0; gFB = fb > 0 ? fb : 0; gMagnet = m & 1; gSlow = m >> 1 & 1; gDud = m >> 2 & 1; gVMag = m >> 3 & 1; gRub = m >> 4 & 1; gMul = m >> 5 & 1; gVS = m >> 6 & 1; gGB = m >> 7 & 1; gDZ = m >> 8 & 1; gXR = m >> 9 & 1; gBR = m >> 10 & 1; gFM = m >> 11 & 1; gIcy = m >> 12 & 1; gDog = m >> 13 & 1; gTM = m >> 14 & 1; gTG = m >> 15 & 1; gBM = m >> 16 & 1; gLT = m >> 17 & 1; gFP = m >> 18 & 1; gAB = m >> 19 & 1; gNR = m >> 20 & 1; } fclose(f);
}
#define NSHOP 23
static const char *const shopName[NSHOP] = {"COIN MAGNET", "EZY-DODGE MISSILES", "MISSILE JAMMER", "VEHICLE MAGNET", "INSTA-BALL", "GEMOLOGY", "FREE RIDE", "GRAVITY BELT", "DEZAPINATOR", "X-RAY SPECS", "HALF A BRICK", "FRUIT FLINGER", "FREEZE-O-MATIC", "FLASH", "MAGNETIC TOKENS", "TOKEN GIFT", "TURBO BOOST", "LUCKY LAST", "FLYING PIG", "AIR BARRYS", "NERD REPELLENT", "HEAD START", "FINAL BLAST"};
static const char *const shopDesc[NSHOP] = {"PULLS IN NEARBY COINS", "MISSILES FLY SLOWER", "FIRST 3 MISSILES ARE DUDS", "VEHICLES ATTRACT COINS", "BOUNCE FURTHER WHEN HIT", "EVERY COIN COUNTS DOUBLE", "BEGIN EACH RUN IN A VEHICLE", "LESS VERTICAL SPEED", "FIRST ZAPPER IS A DUD", "VEHICLE CRATES COME SOONER", "HEAVY THUD WHEN HIT", "DODGED MISSILES PAY 3 COINS", "SLIDE FURTHER WHEN HIT", "A DOG GRABS GROUND COINS", "TOKENS ARE EASIER TO GRAB", "A FREE FINAL SPIN EVERY GAME", "TOKENS GIVE A SPEED BOOST", "THE LAST SPIN ALWAYS WINS", "A PIG FULL OF COINS FLIES BY", "JUMP HIGHER OFF THE FLOOR", "NO SCIENTISTS IN THE LAB", "SKIP THE FIRST 750M", "AFTER A CRASH PRESS A TO BLAST FURTHER"};
static const int shopPrice[NSHOP] = {500, 400, 450, 600, 350, 700, 800, 450, 500, 500, 300, 400, 350, 600, 500, 500, 600, 450, 650, 400, 300, 250, 300};   /* Head Start: one use per purchase */   /* prices are mine; the APK has none */
static int *const shopOwn[NSHOP] = {&gMagnet, &gSlow, &gDud, &gVMag, &gRub, &gMul, &gVS, &gGB, &gDZ, &gXR, &gBR, &gFM, &gIcy, &gDog, &gTM, &gTG, &gBM, &gLT, &gFP, &gAB, &gNR, &gHSnone, &gHSnone};
static void writeSave(int best, int total) {
    char pp[128]; snprintf(pp, sizeof pp, "%ssave.txt", dir); FILE *f = fopen(pp, "w"); if (!f) return; fprintf(f, "%d %d %d %d %d\n", best, total, gMagnet | gSlow << 1 | gDud << 2 | gVMag << 3 | gRub << 4 | gMul << 5 | gVS << 6 | gGB << 7 | gDZ << 8 | gXR << 9 | gBR << 10 | gFM << 11 | gIcy << 12 | gDog << 13 | gTM << 14 | gTG << 15 | gBM << 16 | gLT << 17 | gFP << 18 | gAB << 19 | gNR << 20, gHS, gFB); fclose(f);
}
static int mflip;   /* draw the next vehicle quad upside down (Gravity Suit on the ceiling) */
static void mechQuad(Spr *s, int vt, float x, float yb, float h, int f) {   /* h = drawn height; width keeps the frame's aspect */
    int c = (f - 1) % VEH[vt].cols, r = (f - 1) / VEH[vt].cols, fw = VEH[vt].fw, fh = VEH[vt].fh;
    float va = r * (float)fh / VEH[vt].ph, vb = (r + 1) * (float)fh / VEH[vt].ph;
    quad(s, x, yb - h, h * fw / fh, h, c * (float)fw / VEH[vt].pw, mflip ? vb : va, (c + 1) * (float)fw / VEH[vt].pw, mflip ? va : vb);
}
#ifdef AUTOTEST
#define PKLOG(...) do { char pp_[128]; snprintf(pp_, sizeof pp_, "%sdbg.txt", dir); FILE *df_ = fopen(pp_, "a"); if (df_) { fprintf(df_, __VA_ARGS__); fputc('\n', df_); fclose(df_); } } while (0)
#else
#define PKLOG(...) do {} while (0)
#endif
#ifdef AUTOTEST
#define PKFIRST 40   /* test builds: first pickup early so Dolphin runs see it */
#else
#define PKFIRST 300
#endif
#define DIST(sc) ((int)((sc) / 27.f))
/* Configs/game_mode_rules/classic_1.json: start_velocity 175, end_velocity 480 (units/s of the 480x320 game frame), velocity_stop_distance 2250 m.
   x1.5 to our screen, /60 per frame. The file gives no curve, so the ramp is linear (my reading). */
/* Configs/vehicles/*_1.json START_VELOCITY / MAX_VELOCITY / VELOCITY_INCREASE per VEH index (mech, hog, bird, gsuit, tele, hover, skate, ecto, delorean, starship, sled, jetski).
   Vehicle::UpdateLevelSpeed: while riding, speed += INCREASE * sin(ease) every frame (ease 0..90 deg in 0.5 s), capped at MAX; after it speed returns by 0.975 per 1/60 s.
   Where the ride's speed starts (START, as read here) and what it returns to (the level ramp) are my reading of a cut-off decompile. */
static const float vStart[12] = {200, 400, 250, 250, 250, 300, 300, 400, 400, 400, 200, 250}, vMax[12] = {1000, 6000, 1000, 1000, 1000, 5600, 5600, 6000, 6000, 6000, 800, 1000}, vInc[12] = {0.22f, 0.40f, 0.09f, 0.25f, 0.25f, 0.2f, 0.15f, 0.40f, 0.40f, 0.40f, 0.15f, 0.09f};
static float runSpeed(int d) { float t = d >= 2250 ? 1.f : d / 2250.f; return (175.f + 305.f * t) * 1.5f / 60.f; }

/* Missions: missions.txt (tools/mkmissions.py: the game's Missions.xml types this port can track, with their real text);
   three active at a time like the game, progress in missions.sav. Which mission comes next is simply file order. */
enum { M_PLAY, M_NOCOIN, M_VEHANY, M_TOKG, M_TOKT, M_COINT, M_COING, M_SPEND, M_FLYD, M_NOVEH, M_DISTT, M_GRAV, M_RUN, M_RUNT, M_RUB, M_RUBT, M_VEHDT, M_NEAR, M_NEART, M_HI5, M_HI5T, M_SLOTC, M_SLOTW, M_ALARM, M_ALARMT, M_VCOL, M_VDIST, M_VDT, M_KOV, M_SLIDET, M_START, M_USEP, M_USEHS, M_USEFB, M_N };
static const char *const mName[M_N] = {"PLAY_GAME", "COINS_NOT_COLLECT", "VEHICLE_COLLECT_ANY", "TOKENS_COLLECT_GAME", "TOKENS_COLLECT_TOTAL", "COINS_COLLECT_TOTAL", "COINS_COLLECT_GAME", "SPEND_COINS",
    "FLY_DIST", "FLY_NO_VEHICLE", "FLY_DIST_TOTAL", "CHANGE_GRAVITY", "RUN_DISTANCE", "RUN_DISTANCE_TOTAL", "RUB_HEAD_DISTANCE", "RUB_HEAD_DISTANCE_TOTAL", "VEHICLE_FLY_TOTAL_ANY", "CLOSE_CALL", "CLOSE_CALL_TOTAL", "SCIENTISTS_HIGH_FIVE", "SCIENTISTS_HIGH_FIVE_TOTAL", "SLOTS_CASH_IN", "SLOTS_WIN_TOTAL", "TOUCH_ALARM", "TOUCH_ALARM_TOTAL", "VEHICLE_COLLECT", "VEHICLE_DISTANCE", "VEHICLE_FLY_TOTAL", "SCIENTISTS_KNOCK_OVER", "SLIDE_DIST_TOTAL", "START_FROM_MENU", "USE_ITEM_PERK", "USE_ITEM_HEADSTART", "USE_ITEM_BOMB"};
#define MAXM 64
static int nmis, mType[MAXM], mP1[MAXM], mP2[MAXM], mDiff[MAXM], mStars, mRank, nRanks; static char rankName[16][24];
/* Configs/mission_system/mission_level_system_1.json: 15 levels, each with the stars needed to advance and the coins paid for it */
static const int rankStars[15] = {3, 3, 3, 5, 5, 7, 7, 7, 9, 9, 11, 11, 11, 13, 13};
static const int rankCoins[15] = {400, 800, 1200, 1600, 2000, 2400, 2800, 3200, 3600, 4000, 4400, 4800, 5200, 5600, 6000};
#define STARS_PER_RANK (mRank < 15 ? rankStars[mRank] : 99)
static int mAct[3] = {0, 1, 2}, mProg[3], mNext = 3, mDone, mBanner; static char mText[MAXM][64], mBannerText[64];
static int vColT[5], vKoT[5]; static float vPxT[5];   /* this run, per named vehicle (0 Mech, 1 Hog, 2 Bird, 3 Gravity Suit, 4 Teleporter): pickups, staff knocked down, px travelled */
static int mTotal(int t) { return t == M_PLAY || t == M_VEHANY || t == M_TOKT || t == M_COINT || t == M_SPEND || t == M_DISTT || t == M_RUNT || t == M_RUBT || t == M_VEHDT || t == M_NEART || t == M_HI5T || t == M_SLOTC || t == M_SLOTW || t == M_ALARMT || t == M_VCOL || t == M_VDT || t == M_KOV || t == M_SLIDET || t == M_START || t == M_USEP || t == M_USEHS || t == M_USEFB; }
static void saveMissions(void) {
    char pp[128]; snprintf(pp, sizeof pp, "%smissions.sav", dir); FILE *f = fopen(pp, "w"); if (!f) return;
    fprintf(f, "%d %d %d %d %d %d %d %d %d %d\n", mAct[0], mAct[1], mAct[2], mProg[0], mProg[1], mProg[2], mNext, mDone, mStars, mRank); fclose(f);
}
static void loadMissions(int useSave) {
    char pp[128], ln[160]; snprintf(pp, sizeof pp, "%smissions.txt", dir); FILE *f = fopen(pp, "r"); if (!f) return;
    while (nmis < MAXM && fgets(ln, sizeof ln, f)) {
        char *h = strtok(ln, "\t"), *a = strtok(NULL, "\t"), *b = strtok(NULL, "\t"), *t = strtok(NULL, "\t\r\n"), *df = strtok(NULL, "\t\r\n"); if (!h || !a || !b || !t) continue;
        int k = 0; while (k < M_N && strcmp(h, mName[k])) k++; if (k == M_N) continue;
        mType[nmis] = k; mDiff[nmis] = df ? atoi(df) : 1; if (mDiff[nmis] < 1) mDiff[nmis] = 1; mP1[nmis] = atoi(a); mP2[nmis] = atoi(b); snprintf(mText[nmis], sizeof mText[nmis], "%s", t); nmis++;
    }
    fclose(f);
    if (nmis < 3) { nmis = 0; return; }
    snprintf(pp, sizeof pp, "%sranks.txt", dir); f = fopen(pp, "r");
    if (f) { while (nRanks < 16 && fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; if (ln[0]) snprintf(rankName[nRanks++], sizeof rankName[0], "%s", ln); } fclose(f); }
    snprintf(pp, sizeof pp, "%smissions.sav", dir); f = useSave ? fopen(pp, "r") : NULL;
    if (f) { int v[10] = {0}; if (fscanf(f, "%d %d %d %d %d %d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8], &v[9]) >= 8 && v[0] >= 0 && v[0] < nmis && v[1] >= 0 && v[1] < nmis && v[2] >= 0 && v[2] < nmis) {
        for (int i = 0; i < 3; i++) { mAct[i] = v[i]; mProg[i] = v[3 + i]; } mNext = v[6] % nmis; mDone = v[7]; mStars = v[8] > 0 ? v[8] : 0; mRank = v[9] > 0 ? v[9] : 0; } fclose(f); }
}
static int mRankCoins, mRankUp;   /* coins owed for a rank-up (main adds them to the total), frames left on the rank-up banner */
static void missionDone(int s) {
    snprintf(mBannerText, sizeof mBannerText, "%s", mText[mAct[s]]); mBanner = 180; mDone++;
    mStars += mDiff[mAct[s]];   /* stars = the mission's DIFFICULTY (mine) */
    while (mRank < 15 && mStars >= STARS_PER_RANK) { mStars -= STARS_PER_RANK; mRankCoins += rankCoins[mRank]; PKLOG("rank up from %d paid %d stars left %d", mRank + 1, rankCoins[mRank], mStars); mRank++; mRankUp = 180; }   /* the stars and coins of the level just finished, from mission_level_system_1.json */
    for (int k = 0; k < nmis; k++) { int c = (mNext + k) % nmis; if (c != mAct[0] && c != mAct[1] && c != mAct[2]) { mAct[s] = c; mNext = (c + 1) % nmis; break; } }
    mProg[s] = 0; saveMissions();
}
static void missionEvent(int type, int n) {   /* a counted event outside the per-frame checks (slide distance, game starts, gadget use) */
    if (n <= 0) return;
    for (int s = 0; s < 3 && nmis; s++) { int m = mAct[s]; if (mType[m] == type && (mProg[s] += n) >= mP1[m]) missionDone(s); }
    saveMissions();
}
static void missionSlot(int win) {   /* one Final Spin: a token cashed in, maybe a prize */
    for (int s = 0; s < 3 && nmis; s++) { int m = mAct[s]; if (mType[m] == M_SLOTC || (mType[m] == M_SLOTW && win)) { if (++mProg[s] >= mP1[m]) missionDone(s); } }
    saveMissions();
}
/* ending: the run is over (totals are added then); the rest are this run's numbers, distances in metres */
static void missionCheck(int ending, int dist, int coins, int toks, int veh, int flips, int foot, int roof, int vehM, int spent, int nearM, int nearZ, int hi5, int alarms) {
    for (int s = 0; s < 3 && nmis; s++) {
        int m = mAct[s], t = mType[m], p1 = mP1[m], ok = 0;
        switch (t) {
        case M_NOCOIN: ok = !coins && dist >= p1; break;
        case M_NOVEH: ok = !vehM && dist >= p1; break;
        case M_COING: ok = coins >= p1; break;
        case M_TOKG: ok = toks >= p1; break;
        case M_GRAV: ok = flips >= p1; break;
        case M_RUN: ok = foot >= p1; break;
        case M_RUB: ok = roof >= p1; break;
        case M_NEAR: ok = (mP2[m] ? nearZ : nearM) >= p1; break;
        case M_HI5: ok = hi5 >= p1; break;
        case M_ALARM: ok = alarms >= p1; break;
        case M_VDIST: ok = DIST(vPxT[mP2[m] % 5]) >= p1; break;
        case M_FLYD: ok = ending && dist >= p1 && dist <= mP2[m]; break;
        case M_SPEND: ok = (mProg[s] += spent) >= p1; break;
        default: if (ending && mTotal(t) && t != M_SLOTC && t != M_SLOTW && t != M_SLIDET && t != M_START && t != M_USEP && t != M_USEHS && t != M_USEFB) { mProg[s] += t == M_PLAY ? 1 : t == M_VEHANY ? veh : t == M_TOKT ? toks : t == M_COINT ? coins : t == M_DISTT ? dist : t == M_RUNT ? foot : t == M_RUBT ? roof : t == M_NEART ? (mP2[m] ? nearZ : nearM) : t == M_HI5T ? hi5 : t == M_ALARMT ? alarms : t == M_VCOL ? vColT[mP2[m] % 5] : t == M_VDT ? DIST(vPxT[mP2[m] % 5]) : t == M_KOV ? vKoT[mP2[m] % 5] : vehM; ok = mProg[s] >= p1; }
        }
        if (ok) missionDone(s);
    }
    if (ending || spent) saveMissions();
}
/* inv = 42: settings.xml IMMUNITY_TIME 700, read as milliseconds (42 frames at 60 fps; was 90) */
#define HIT() do { if (inv) {} else if (mech == 2) { mech = 0; inv = 42; sfx_play(sVDie, 0); } else if (shield > 0) { shield--; inv = 42; sfx_play(sVDie, 0); } else if (!dead && state == 1) { dead = 60; dvx = 4; slideT = 0; blastUsed = 0; blastWait = 0; sfx_play(sHurt, 0); } } while (0)

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
    loadMissile(); loadZappers(); loadFont(); makeAlarm();
#if AUTOTEST == 2
    mProb = 100;   /* on-foot test: a missile at every chance, so close passes happen */
#endif
    { char cp[128]; snprintf(cp, sizeof cp, "%slevels/count.txt", dir); FILE *cf = fopen(cp, "r"); if (cf) { if (fscanf(cf, "%d", &nLevels) != 1 || nLevels < 1) nLevels = 1; fclose(cf); } }
    int lvl = 0; loadLevel(lvl);
    Spr flame = {0}; int haveFlame = loadSpr(&flame, "flame.spr");   /* 4x2 grid of 16x32 frames, top row = burning */
    Spr bBody = {0}, bHead = {0}; int haveRig = loadSpr(&bBody, "body.spr") && loadSpr(&bHead, "head.spr");   /* 4x8 grids of 32x32 frames */
    int sCoin, sHurt, sLaserW, sMissW, sMissL, sJet, sVm[13], sVj[13], sGC, sTP, sSt1, sSt2, sLand, sZap, sVDie, sJS, sJE, sC2, sC3, sMS, sLS, sME; { char pp[160]; const char *n[] = {"coin.ogg", "hurt.ogg", "laserw.ogg", "missw.ogg", "missl.ogg", "jet.ogg", "mmount.ogg", "hmount.ogg", "mjump.ogg", "hjump.ogg", "zap.ogg", "vdie.ogg", "bmount.ogg", "bflap.ogg", "jstart.ogg", "jstop.ogg", "coin2.ogg", "coin3.ogg", "mstep.ogg", "lstart.ogg", "mexp.ogg", "gchg.ogg", "cft.ogg", "step1.ogg", "step2.ogg", "land.ogg"}; int *o[] = {&sCoin, &sHurt, &sLaserW, &sMissW, &sMissL, &sJet, &sVm[0], &sVm[1], &sVj[0], &sVj[1], &sZap, &sVDie, &sVm[2], &sVj[2], &sJS, &sJE, &sC2, &sC3, &sMS, &sLS, &sME, &sGC, &sTP, &sSt1, &sSt2, &sLand};
        for (int i = 0; i < 26; i++) { snprintf(pp, sizeof pp, "%s%s", dir, n[i]); *o[i] = sfx_load(pp); } sVm[3] = sVm[0]; sVj[3] = sGC; sVm[4] = sVm[0]; sVj[4] = sTP; sVm[5] = sVm[1]; sVj[5] = sVj[1]; sVm[6] = sVm[1]; sVj[6] = sVj[1]; sVm[7] = sVm[1]; sVj[7] = sVj[1]; sVm[8] = sVm[1]; sVj[8] = sVj[1]; sVm[9] = sVm[1]; sVj[9] = sVj[1]; sVm[10] = sVm[1]; sVj[10] = sVj[1]; sVm[11] = sVm[1]; sVj[11] = sVj[1]; sVm[12] = sVm[1]; sVj[12] = sVj[1]; }
    int jetV = -1, prevCur = -1, prevMst = 0;
    Spr bolt = {0}; if (!loadSpr(&bolt, "bolt.spr")) fallbackSpr(&bolt);
    int zi = -1, ztick = 0, zwait = 150; float zx = 0, zy = 0, zlen = 0;
    float evAt = -1; int evL = 0, evM = 0, evZ = 0;   /* spawn events (Level::SetupNextSpawner): one roll picks a coin pattern, laser, missile or gate */
    int cur = -1, ptick = 0, wait = 90, dead = 0, mst = 0, mtick = 0, mwait = 120; float my = 0, mx = 0;   /* mst: 0 idle, 1 alert, 2 flying */
    if (!fatOk) GX_SetCopyClear((GXColor){160, 0, 0, 255}, GX_MAX_Z24);          /* red: no SD */
    else if (!nc) GX_SetCopyClear((GXColor){160, 120, 0, 255}, GX_MAX_Z24);    /* amber: assets missing */
    Spr dimS = {0}; dimSpr(&dimS);
    Spr tbS = {0}; int haveTb = loadSpr(&tbS, "tb.spr");   /* Pause.uiscreen PauseHeaderBacking: textures/menus/paused/titleBar, 128 high across the top */
    Spr hazS = {0}; int haveHaz = loadSpr(&hazS, "hz.spr");   /* TitleScreen mainMenu.hazzardStrip: textures/menus/backings/hazzardTile */
    Spr logoT = {0}, logoB = {0}; int haveLogo = loadSpr(&logoT, "logot.spr") && loadSpr(&logoB, "logob.spr");
    Spr titleS = {0}; int haveTitle = loadSpr(&titleS, "title.spr");   /* the lab room behind the title (Room0/bgTitle) */   /* title art: textures/logo/default (downscaled to fit) */
    Spr mechS[16] = {{0}}; int haveV[16], nV = 0, vt = 0, jl = 0, gd = 1, tt = 0; for (int i = 0; i < 16; i++) haveV[i] = loadSpr(&mechS[i], VEH[i].spr);
    int paused = 0, mech = 0, inv = 0, bf = 0, wasHeld = 0, mkwait = 600; float mpick = -999;   /* mech: 0 none, 2 riding */
    Spr pbk = {0}, pic[3] = {{0}, {0}, {0}}; int havePause = loadSpr(&pbk, "pbk.spr") && loadSpr(&pic[0], "pq.spr") && loadSpr(&pic[1], "pr.spr") && loadSpr(&pic[2], "pg.spr"), psel = 2, doRestart = 0, tsel = 0, inShop = 0, shopI = 0; Spr popS = {0}; int havePop = loadSpr(&popS, "popup.spr");   /* menus/popupBacking behind the game-over text */
    /* in-run pickups, Configs/pickups: manager_1 (first at 300 m, then every 500 m + 0-500 random, equal weights 25), level 1 of each:
       Shield numHits 1; Boost 100 m at speed 2000 (slowing over the last 30 m); Coin Special 300 coins over 60 m. Slow Motion (mission level 5+): 7 s at time multiplier 0.5, applied here to the run speed only. */
    Spr pkS[4] = {{0}}; int havePk = loadSpr(&pkS[0], "pkcoin.spr") && loadSpr(&pkS[1], "pkboost.spr") && loadSpr(&pkS[2], "pkshield.spr") && loadSpr(&pkS[3], "pkslow.spr"), pkOn = 0, pkKind = 0, pkNext = PKFIRST, shield = 0, slowT = 0; float pkX = 0, pkY = 0, boostPx = 0, cspPx = 0, cspAcc = 0;
    Spr tokS = {0}; int haveTok = loadSpr(&tokS, "tok.spr"), tokOn = 0, tokWait = 400, tokGot = 0, tokFirst = 300, tokLast = 0, tokKm = 0, tokInKm = 0, tokStep = 0, spinsLeft = 0, spinAnim = 0, spinShow = 0, spinPrize = 0, spinWins = 0, next500 = 0; const char *spinLbl = ""; float tokX = 0, tokY = 0, tokTY = 0;   /* spin token: RewardSystem/Tokens/pickup.png; how often and what it pays is mine */
    Spr dogS = {0}; int haveDog = loadSpr(&dogS, "rdog.spr");   /* Robot Dog art: robodog-alt, row 2 is the 4-frame trot */
    Spr colH = {0}, colB = {0}, boomS = {0}; int haveCol = loadSpr(&colH, "colh.spr") && loadSpr(&colB, "colb.spr") && loadSpr(&boomS, "boom.spr"), boomOn = 0, zDud = 0, zapDudLeft = 0, bossHp = 3, bossCd = 0, bossKills = 0, boomDone = 0, boomHit = 0, newBest = 0;
    Spr pigS = {0}; int havePig = loadSpr(&pigS, "pig.spr"), pigOn = 0, pigWait = 1200, pigGot = 0, pigFirst = 300, pigLast = 0, pigKm = 0, pigInKm = 0, pigStep = 0; float pigX = 0, pigY = 0, pigTY = 0;   /* Flying Pig: Entities/distanceMarkerPig (rows 3-4 = flying frames); when and what it pays is mine */
    Spr sciS[4] = {{0}}; int haveSci = loadSpr(&sciS[0], "scib.spr") && loadSpr(&sciS[1], "scih.spr") && loadSpr(&sciS[2], "wrkb.spr") && loadSpr(&sciS[3], "wrkh.spr"), sciWait = 120, sciHit = 0;   /* lab staff: Characters/Barry/{scientist,worker}{Body,Head}, the same 4x8 rig as Barry (row 0 run, row 2 fall) */
#define MAXS 6
    struct { float x, sp, h, vx, vy, ang, spin; int on, dir, kind, down, alerted, alertD, hi5; } sci[MAXS] = {{0}};   /* sp: own speed (screen px/frame), dir +1 right / -1 left; down: knocked over (h = height above the floor) */
    float boomX = 0, boomY = 0;   /* Colossatron sonic boom (real sonicBoom.png), timing mine */
    int cst = 0, bossDone = 0, bossHit = 0, nextBoss = 1500; float cbx = 0;   /* "boss": Colossatron art, behaviour is mine */
    Spr shopS[NSHOP] = {{0}}; int haveMag = loadSpr(&shopS[0], "magnet.spr") && loadSpr(&shopS[1], "smiss.spr") && loadSpr(&shopS[2], "dmiss.spr") && loadSpr(&shopS[3], "magnet.spr") && loadSpr(&shopS[4], "rub.spr") && loadSpr(&shopS[5], "gem.spr") && loadSpr(&shopS[6], "vstart.spr") && loadSpr(&shopS[7], "gbelt.spr") && loadSpr(&shopS[8], "dzap.spr") && loadSpr(&shopS[9], "xray.spr") && loadSpr(&shopS[10], "brick.spr") && loadSpr(&shopS[11], "fruit.spr") && loadSpr(&shopS[12], "icy.spr") && loadSpr(&shopS[13], "rdogi.spr") && loadSpr(&shopS[14], "tmag.spr") && loadSpr(&shopS[15], "tgift.spr") && loadSpr(&shopS[16], "boost.spr") && loadSpr(&shopS[17], "lucky.spr") && loadSpr(&shopS[18], "flypig.spr") && loadSpr(&shopS[19], "airb.spr") && loadSpr(&shopS[20], "nerd.spr") && loadSpr(&shopS[21], "hs.spr") && loadSpr(&shopS[22], "fb.spr"), dudLeft = 0;   /* shop items: Coin Magnet 500, Slow Missiles 400, Dud Missiles 450 (prices mine) */ Spr startG = {0}; int haveStart = loadSpr(&startG, "sg.spr");   /* Pause.uiscreen: three 216px buttons (Quit, Retry, Resume) on buttonBacking, icon at -20, label at +68; drawn at 0.6 */
    float curU = 175, vPhase = 0; int wasMech = 0, spdDecay = 0;
    float spd = 4.375f, dvx = 0, slidePx = 0; int slideT = 0, blastWait = 0, blastUsed = 0, blastFx = 0; float y = 300, vy = 0, scroll = 0, footPx = 0, roofPx = 0, vehPx = 0; int abKicks = 0, runVeh = 0, runFlip = 0, runTok = 0, runNearM = 0, runNearZ = 0, runHi5 = 0, runAlarm = 0, lastAlarm = -1, zHitF = 0; float mNearMin = 999, zNearMin = 999; int goFrame = 0, score = 0, frame = 0, state = 0, best = 0, lastDist = 0, total = 0;   /* state: 0 title, 1 playing, 2 game over */
    loadSave(&best, &total); loadGadgets(dir);
#ifdef AUTOTEST
    loadMissions(0);   /* the test build starts on missions one run can finish, so completion runs */
    mRank = 4;   /* mission level 5, which is what unlocks the Slow Motion pickup */
    mStars = STARS_PER_RANK - 1;   /* and one star short of a rank, so the rank-up runs */
    for (int i = 0, k = 0; i < nmis && k < 2; i++) if (mType[i] == (k ? M_TOKG : M_COING)) { mAct[k++] = i; i = -1; }
#else
    loadMissions(1);
#endif
#ifdef AUTOTEST
    total = 17000; gMagnet = 0; gSlow = 0; gDud = 0; gVMag = 0; gRub = 0; gMul = 0; gVS = 0; gGB = 0; gDZ = 0; gXR = 0; gBR = 0; gFM = 0; gIcy = 0; gDog = 0; gTM = 0; gTG = 0; gBM = 0; gLT = 0; gFP = 1; gAB = 1; gNR = 0; gHS = 2; gFB = 3;   /* the two newest gadgets start owned so their paths run */
#endif
    for (;;) {
        INPUT_Update(); tick++;
        int dist = 0;
        { int mi = inShop && state == 0 ? 1 : state != 1 ? 0 : cst ? 3 : (mech == 2 && vt == 1) ? 2 : 0; static int lastMi = 0; if (mi != lastMi) { lastMi = mi; audio_switch(musicPath[mi]);
#ifdef AUTOTEST
 { char pp[128]; snprintf(pp, sizeof pp, "%sdbg.txt", dir); FILE *df = fopen(pp, "a"); if (df) { { extern volatile int audioFed, audioRestarts; fprintf(df, "music %d tick %d fed %d restarts %d voice0 %d\n", mi, tick, audioFed, audioRestarts, ASND_StatusVoice(0)); } fclose(df); } }
#endif
 } }
        if (btnDown() & WPAD_BUTTON_HOME) break;
        if (state == 1 && (btnDown() & WPAD_BUTTON_PLUS)) { paused ^= 1; psel = 2; }
        if (paused && state == 1 && havePause) {
            u32 bd = btnDown(); if ((bd & WPAD_BUTTON_LEFT) && psel > 0) psel--; if ((bd & WPAD_BUTTON_RIGHT) && psel < 2) psel++;
            if (bd & (WPAD_BUTTON_A | WPAD_BUTTON_2)) { if (psel == 2) paused = 0; else if (psel == 1) { paused = 0; state = 2; doRestart = 1; } else { paused = 0; state = 0; cst = 0; boomOn = 0; mech = 0; mst = 0; cur = -1; zi = -1; tokOn = 0; pigOn = 0; memset(sci, 0, sizeof sci); mpick = -999; dead = 0; y = 300; vy = 0; } }
        }
        if (paused && state == 1) { dist = DIST(scroll); if (jetV > 0) { sfx_stop(jetV); jetV = -1; } goto render; }
        if (state != 1) { paused = 0;
            u32 td = btnDown();
            if (state == 0 && haveMag) {   /* title: START / SHOP; the shop sells the Coin Magnet (price is mine: the APK has none) */
                if (inShop) { if (td & WPAD_BUTTON_DOWN) shopI = (shopI + 1) % NSHOP; if (td & WPAD_BUTTON_UP) shopI = (shopI + NSHOP - 1) % NSHOP; if (td & WPAD_BUTTON_A) { if (!*shopOwn[shopI] && total >= shopPrice[shopI]) { total -= shopPrice[shopI]; if (shopI == HS_SLOT) gHS++; else if (shopI == FB_SLOT) gFB++; else *shopOwn[shopI] = 1; writeSave(best, total); missionCheck(0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0); sfx_play(sC3, 0); } } if (td & (WPAD_BUTTON_B | WPAD_BUTTON_1)) inShop = 0; }
                else { if (td & (WPAD_BUTTON_UP | WPAD_BUTTON_DOWN)) tsel ^= 1; if (tsel == 1 && (td & (WPAD_BUTTON_A | WPAD_BUTTON_2))) inShop = 1; }
            }
            if (state == 2 && (spinsLeft || spinAnim || spinShow)) {   /* Final Spin: one spin per token; the prize table is mine (the game's is in native code) */
                if (spinAnim) { if (--spinAnim == 0) {
                    /* Configs/slots/default_1.json: weighted outcomes; coin prizes pay `rewards` 200 / 1000 / 2000; three explosions are the near misses */
                    static const int sw[11] = {120, 90, 50, 120, 75, 40, 100, 70, 120, 100, 100};   /* COINS_100/500/1000, EXPLOSION1-3, REVIVE, TOKENS, DOUBLE_COINS, START_500M x2 */
                    int r = rand() % 985, k = 0; while (r >= sw[k]) r -= sw[k++];
                    if (gLT && !spinsLeft && k >= 3 && k <= 6) k = 0;   /* Lucky Last: the last spin always wins (at least 200 here) */
                    spinPrize = k == 0 ? 200 : k == 1 ? 1000 : k == 2 ? 2000 : k == 8 ? score : 0;
                    spinLbl = k == 0 ? "200 COINS" : k == 1 ? "1000 COINS" : k == 2 ? "2000 COINS" : k <= 5 ? "BOOM" : k == 6 ? "REVIVE" : k == 7 ? "EXTRA TOKEN" : k == 8 ? "DOUBLE COINS" : "START 500M";
                    if (k == 7) spinsLeft++; if (k >= 9) next500 = 1;   /* an extra spin; the next run starts at 500 m */
                    total += spinPrize; spinWins += k < 3 || k >= 7; PKLOG("spin kind %d prize %d next500 %d", k, spinPrize, next500); writeSave(best, total); missionSlot(k < 3 || k >= 7); spinShow = 75; sfx_play(k < 3 || k >= 7 ? sC3 : sJE, 0); } }
                else if (spinShow) spinShow--;
                if (!spinAnim && spinsLeft && frame - goFrame >= 45 && (!spinShow || spinShow < 45) && (td & (WPAD_BUTTON_A | WPAD_BUTTON_2)
#ifdef AUTOTEST
                    || 1
#endif
                    )) { spinsLeft--; spinAnim = 60; spinShow = 0; sfx_play(sTP, 0); }
            }
            else if (doRestart || (!(state == 2 && frame - goFrame < 45) && (btnDown() & (WPAD_BUTTON_A | WPAD_BUTTON_B | WPAD_BUTTON_1 | WPAD_BUTTON_2)) && !(state == 0 && haveMag && (inShop || tsel == 1)))) { doRestart = 0; y = 300; vy = 0; scroll = 0; score = 0; memset(cgot, 0, sizeof cgot); runVeh = runFlip = runTok = runNearM = runNearZ = runHi5 = runAlarm = 0; memset(vColT, 0, sizeof vColT); memset(vKoT, 0, sizeof vKoT); memset(vPxT, 0, sizeof vPxT); lastAlarm = -1; footPx = roofPx = vehPx = 0; tokOn = 0; tokFirst = 200 + rand() % 251; tokLast = 0; tokKm = 0; tokInKm = 0; tokStep = 0; pigOn = 0; pigWait = 1200; pigFirst = 200 + rand() % 201; pigLast = 0; pigKm = 0; pigInKm = 0; pigStep = 0; memset(sci, 0, sizeof sci); sciWait = 120; boomOn = 0; bossCd = 0; dudLeft = gDud ? 3 : 0; zapDudLeft = gDZ ? 1 : 0; mkwait = gXR ? 300 : 600; cst = 0;
#ifdef AUTOTEST
            pigFirst = 40; tokFirst = 30; gBR = 0; gIcy = 0; gRub = 0;   /* plain 0.94 slide in the test runs */
            nextBoss = 15;
#else
            nextBoss = 1500;
#endif
            cur = -1; wait = 90; mst = 0; mwait = 120; zi = -1; zwait = 150; evAt = -1; evL = evM = evZ = 0; dead = 0; mech = 0; inv = 0; slowT = 0; pkOn = 0; pkNext = PKFIRST; shield = 0; boostPx = 0; cspPx = 0; mpick = -999; mkwait = gXR ? 300 : 600; { int fromMenu = state == 0; state = 1; dvx = 0; slidePx = 0; blastUsed = 0; blastWait = 0; if (fromMenu) missionEvent(M_START, 1);
              if (next500) { next500 = 0; scroll = 500 * 27.f; inv = 120; }   /* a Final Spin win: Start 500 m, for the next run */
              if (fromMenu && gHS > 0) { gHS--; scroll = 750 * 27.f; inv = 120; writeSave(best, total); missionEvent(M_USEHS, 1); }   /* Head Start: skip the first 750 m (the item text); 2 s of cover is mine */
              int anyG = 0; for (int g = 0; g < NSHOP; g++) anyG |= *shopOwn[g]; if (anyG) missionEvent(M_USEP, 1); }
#if AUTOTEST == 2
            gVS = 0; mkwait = 1 << 30;
#endif
            if (gVS) { mech = 2; do vt = rand() % 13; while (!haveV[vt] || (vt == 12 && !(haveV[13] && haveV[14] && haveV[15]))); jl = 0; gd = 1; tt = 0; mpick = -999; }   /* Vehicle Start gadget */
                if (nLevels > 1) { lvl = (lvl + 1) % nLevels; loadLevel(lvl); } }
            frame++; dist = lastDist; if (jetV > 0) { sfx_stop(jetV); jetV = -1; }
            goto render;
        }
        { float base = (175.f + 305.f * (dist >= 2250 ? 1.f : dist / 2250.f));
          if (mech == 2 && vt >= 0 && vt < 12) {
              if (!wasMech) { curU = vStart[vt]; vPhase = 0; PKLOG("vehicle mount vt %d speed %d dist %d", vt, (int)curU, dist); }
              vPhase = fminf(vPhase + 3.f, 90.f); curU = fminf(vMax[vt], curU + vInc[vt] * sinf(vPhase * 0.017453292f)); spdDecay = 1;
          } else if (spdDecay) { curU = curU * 0.975f + base * 0.025f; if (fabsf(curU - base) < 1.f) spdDecay = 0; }
          else curU = base;
          wasMech = mech == 2; spd = curU * 1.5f / 60.f; }
        if (boostPx > 0) { float full = 2000.f * 1.5f / 60.f; spd = boostPx > 810 ? full : spd + (full - spd) * boostPx / 810.f; boostPx -= spd; inv = inv > 2 ? inv : 2; }
        if (slowT > 0) { slowT--; spd *= 0.5f; }
        { int held = (btnHeld() & (WPAD_BUTTON_A | WPAD_BUTTON_B | WPAD_BUTTON_1 | WPAD_BUTTON_2)) != 0;
          if (mech == 2 && vt == 4) {   /* Teleporter: hovers; a press vanishes (invulnerable) and reappears at a random height */
              vy = 0;
              if (tt > 0) { inv = 2; if (--tt == 12) y = 60 + rand() % (H - 180); }
              else if (held && !wasHeld) { tt = 24; sfx_play(sTP, 0); }
          } else if (mech == 2 && vt == 3) {   /* Gravity Suit: a press flips gravity between floor and ceiling */
              if (held && !wasHeld) { gd = -gd; runFlip++; sfx_play(sGC, 0); }
              vy += gd * 0.9f; if (vy > 12) vy = 12; if (vy < -12) vy = -12;
          } else if (mech == 2) {
              if (y >= H - 121) jl = VEH[vt].jumps;
              if (held && !wasHeld && jl > 0) { jl--; vy = -1.5f * VEH[vt].ib; bf = VEH[vt].bfc; sfx_play(sVj[vt], 0); }
              vy += 1.5f * VEH[vt].g; if (held && bf > 0) { vy -= 1.5f * VEH[vt].b; bf--; } if (!held) bf = 0;
              if (vy > 12) vy = 12;
          } else {
              if (gAB && held && !wasHeld && y >= H - 121) { vy = -9; abKicks++; }   /* Air Barrys: a kick off the floor (perks.xml gives no number; 9 is mine) */
              vy += (held ? -0.9f : 0.6f);
              { float cap = gGB ? 9 * 0.78f : 9; if (vy > cap) vy = cap; if (vy < -cap) vy = -cap; }   /* Gravity Belt: perks.xml value 0.22 as a 22% lower vertical speed cap (my reading) */
          }
          wasHeld = held; }
        y += vy; if (inv) inv--;
#if AUTOTEST == 2
        inv = 2; if (DIST(scroll) > 400) if (!dead) { dead = 60; dvx = 4; slideT = 0; }   /* on-foot test: cannot be hit (fake input cannot dodge), ends itself at 400 m */
#endif
        if (!mech && mpick < -900 && !dead && DIST(scroll) >= 150 && --mkwait <= 0) { mpick = W; nV = 0; static const int vw[13] = {20, 20, 50, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20};   /* Vehicles/*.xml <probability>: Mech 20, Hog 20, ProfitBird 50; Gravity Suit and Teleporter have no file in the APK, so 20 */
            int tw = 0; for (int i = 0; i < 13; i++) if (haveV[i] && haveV[13] && haveV[14] && haveV[15]) tw += vw[i];
            if (tw) { int r = rand() % tw; for (int i = 0; i < 13; i++) if (haveV[i] && haveV[13] && haveV[14] && haveV[15] && (r -= vw[i]) < 0) { vt = i; break; } nV = 1; } else mpick = -999; }
#ifdef AUTOTEST
        if (AUTOTEST != 2 && !mech && DIST(scroll) == 100) { mech = 2; vt = haveV[lvl % 13] ? lvl % 13 : 0; gd = 1; tt = 0; }   /* test build: start on the Mech */
#endif
        if (mpick > -900) { mpick -= spd; if (mpick < -100) { mpick = -999; mkwait = gXR ? 450 : 900; }
            else if (!mech && mpick < 140 && mpick + 80 > 60 && y > H - 220) { mech = 2; mpick = -999; gd = 1; tt = 0; runVeh++; if (vt < 5) vColT[vt]++; sfx_play(sVm[vt], 0); } }
        if (y < 60) { y = 60; vy = 0; }
        if (y > H - 120) { y = H - 120; vy = 0; }   /* ceiling / floor */
        if (dead && mech != 2) {   /* death tumble: settings.xml DEATH_GRAVITY 0.24 (x1.5 screen scale), MIN_BOUNCE_VELOCITY 4, DEATH_BOUNCE_DAMPENING 0.99 (damped further here: the unit of that one is a guess) */
            /* the slide: settings.xml DEATH_X_GROUND_FRICTION 0.94 (Insta-ball 1.0, Half a Brick 0.1, Freeze-o-matic 1.0), DEATH_X_AIR_FRICTION 1.0;
               starting speed = the run speed. The slide is capped at 4 s (mine), which is what ends a frictionless one. */
            if (y >= H - 121) { dvx *= gBR ? 0.1f : (gRub || gIcy) ? 1.0f : 0.94f; slidePx += dvx; }
            scroll += dvx;
            vy += 0.24f * 1.5f; y += vy; if (y >= H - 121) { y = H - 121; vy = fabsf(vy) > 4 * 1.5f ? -vy * (gRub ? 0.8f : gBR ? 0.25f : 0.5f) : 0;   /* Rubber Bones: DEATH_BOUNCE_DAMPENING_RUBBER 1.6 vs 0.99, scaled onto my 0.5 */ }
        }
        if (dead == 1 && dvx > 0.3f && ++slideT < 240) dead = 2;   /* still sliding: the run ends when Barry stops */
        if (dead == 1 && gFB > 0 && !blastUsed && state == 1) {   /* Final Blast: a 2 s window to blow Barry on further (launch numbers mine) */
            if (!blastWait) blastWait = 120;
            if (btnDown() & (WPAD_BUTTON_A | WPAD_BUTTON_2)
#ifdef AUTOTEST
                || blastWait == 60
#endif
                ) { gFB--; blastUsed = 1; blastWait = 0; blastFx = 40; dvx = 16; vy = -14; slideT = 0; dead = 60; writeSave(best, total); missionEvent(M_USEFB, 1); sfx_play(sME, 0); }
            else if (--blastWait > 0) dead = 2;
        }
        if (dead) { if (--dead == 0) { state = 2; goFrame = frame; lastDist = DIST(scroll); newBest = lastDist > best; if (newBest) best = lastDist; total += score; writeSave(best, total); spinsLeft = runTok + (gTG ? 1 : 0); spinAnim = spinShow = 0; missionEvent(M_SLIDET, DIST(slidePx)); missionCheck(1, lastDist, score, runTok, runVeh, runFlip, DIST(footPx), DIST(roofPx), DIST(vehPx), 0, runNearM, runNearZ, runHi5, runAlarm);
#ifdef AUTOTEST
            { char pp[128]; snprintf(pp, sizeof pp, "%sdbg.txt", dir); FILE *df = fopen(pp, "a"); if (df) { fprintf(df, "run lvl %d vt %d dist %d tick %d mag %d slow %d dud %d vmag %d rub %d mul %d vs %d gb %d dz %d xr %d br %d boss %d bhit %d boom %d/%d kills %d tok %d pig %d sci %d mis %d rank %d stars %d near %d/%d hi5 %d ab %d spin %d/%d alarm %d slide %d fb %d vm %d/%d/%d/%d/%d vc %d ko %d coins %d left %d\n", lvl, vt, lastDist, tick, gMagnet, gSlow, gDud, gVMag, gRub, gMul, gVS, gGB, gDZ, gXR, gBR, bossDone, bossHit, boomDone, boomHit, bossKills, tokGot, pigGot, sciHit, mDone, mRank, mStars, runNearM, runNearZ, runHi5, abKicks, tokGot, spinWins, runAlarm, DIST(slidePx), gFB, DIST(vPxT[0]), DIST(vPxT[1]), DIST(vPxT[2]), DIST(vPxT[3]), DIST(vPxT[4]), vColT[0] + vColT[1] + vColT[2] + vColT[3] + vColT[4], vKoT[0] + vKoT[1] + vKoT[2] + vKoT[3] + vKoT[4], score, total); fclose(df); } }   /* headless check without screenshots */
#endif
        } }
        else scroll += spd;
        frame++;
        dist = DIST(scroll);
        if (haveAlarm && !dead && state == 1) { AlarmQ q = {y + 45, -1}; eachAlarm(scroll, touchAlarmCb, &q); if (q.hit >= 0 && q.hit != lastAlarm) { lastAlarm = q.hit; runAlarm++; } }   /* brushed past (within 40/50 px: mine) */
        if (!dead && state == 1) { if (mech == 2 && vt < 5) vPxT[vt] += spd; if (mech == 2) vehPx += spd; else if (y >= H - 121) footPx += spd; else if (y <= 61) roofPx += spd;
            missionCheck(0, dist, score, runTok, runVeh, runFlip, DIST(footPx), DIST(roofPx), DIST(vehPx), 0, runNearM, runNearZ, runHi5, runAlarm); }
        /* settings.xml DISTANCE_DIVISOR 18, scroll in game px (ours / 1.5): about 9 m per second at 4 px per frame */
        if (!dead) {
            if (evAt < 0) evAt = scroll + 400;
            if (scroll >= evAt) {   /* Level::SetupNextSpawner: one weighted roll; the next event comes MIN..MAX_SPAWN_INTERVAL later plus the extent of what was spawned */
                static const float coinV[12] = {4, 4, 4, 4, 4, 9, 9, 4, 4, 4, 4, 30};   /* Configs/coins_patterns/Streaks_*Coins: global chance while riding */
                int riding = mech == 2 && vt >= 0 && vt < 12;
                float wc = riding ? coinV[vt] : (dist >= 50 ? 14.f : 0.f), wm = dist >= mStart ? mProb : 0, wg = dist >= zGlobalSd ? zGlobalP : 0;
                float wl = (!riding && boostPx <= 0 && zi < 0 && mst == 0 && cur < 0) ? laserProb(dist) : 0, tot = wc + wl + wm + wg, ext = 0;
                evL = evM = evZ = 0;
                if (tot > 0) { float r = tot * (rand() / (RAND_MAX + 1.f));
                    if (r < wc) PKLOG("event coin dist %d", dist);
                    else if (r < wc + wl) { evL = 1; ext = 240; PKLOG("event laser dist %d", dist); }
                    else if (r < wc + wl + wm) { evM = 1; ext = 150; PKLOG("event missile dist %d", dist); }
                    else { evZ = 1; ext = 150; PKLOG("event gate dist %d", dist); } }
                evAt = scroll + spawnGap(dist) + ext * spd;
            }
            if (cur < 0) { if (evL) { evL = 0; cur = pickPattern(dist); ptick = 0; } }
            else if (++ptick > pats[cur].time + 60) cur = -1;
            if (cur >= 0 && ptick > 60) for (int i = 0; i < pats[cur].n; i++) {
                float ly = 240 - pats[cur].y[i] * 1.6f;
                if (fabsf(ly - (y + 45)) < 22) { HIT(); break; }
            }
        }
        if (!dead && cur < 0) {   /* missiles share the screen with lasers only when no laser pattern is active */
            if (mst == 0 && evM) { evM = 0; mst = 1; mtick = 0; my = y + 45; }
            else if (mst == 1) { float t = y + 45; my += fminf(fmaxf(t - my, -mChase * 2), mChase * 2); if (++mtick > 90) { mst = 2; mx = W; mNearMin = 999; } }
        }
        if (haveTok && !dead && state == 1) {
            /* slotTokens.xml: first token 200-450 m, at most 3 per 1000 m, never more than 1050 m without one, probability 2.8 (read as a 2.8% roll
               every 10 m), MAX_Y_MOVE_SPEED 1.8 */
            if (dist / 1000 != tokKm) { tokKm = dist / 1000; tokInKm = 0; }
            if (!tokOn && dist >= tokFirst && dist / 10 != tokStep && tokInKm < 3) { tokStep = dist / 10;
                if (rand() % 1000 < 28 || dist - tokLast >= 1050 || !tokLast) { tokOn = 1; tokLast = dist; tokInKm++; tokX = W + 40; tokY = tokTY = 120 + rand() % (H - 300); } }
            if (tokOn && !gTM) { if (fabsf(tokTY - tokY) < 1) tokTY = 120 + rand() % (H - 300); tokY += fmaxf(-2.7f, fminf(2.7f, tokTY - tokY)); }
            if (tokOn) {
                tokX -= spd;
                if (gTM && hypotf(tokX - 95, tokY - (y + 45)) < 200) { tokX += (95 - tokX) * 0.12f; tokY += (y + 45 - tokY) * 0.12f; }   /* Token Magnet pulls it in */
                if (hypotf(tokX + 20 - 95, tokY + 20 - (y + 45)) < 44) { tokOn = 0; tokGot++; runTok++; if (gBM) scroll += 270; sfx_play(sC3, 0); }   /* Boost Machine: a token also carries you 10 m ahead (my reading) */
                else if (tokX < -60) tokOn = 0;
            }
            if (havePk) {
                if (!pkOn && dist >= pkNext) { pkOn = 1; pkKind = rand() % (mRank + 1 >= 5 ? 4 : 3); pkX = W + 40; pkY = 120 + rand() % (H - 300); pkNext = dist + 500 + rand() % 500; PKLOG("pickup spawn kind %d dist %d next %d", pkKind, dist, pkNext); }
                if (pkOn) {
                    pkX -= spd;
                    if (hypotf(pkX + 32 - 95, pkY + 16 - (y + 45)) < 50) {
                        pkOn = 0; sfx_play(sC3, 0);
                        PKLOG("pickup got kind %d dist %d score %d", pkKind, dist, score); if (pkKind == 0) { cspPx = 60 * 27; cspAcc = 0; } else if (pkKind == 1) boostPx = 100 * 27; else if (pkKind == 2) shield = 1; else slowT = 7 * 60;
                    } else if (pkX < -80) pkOn = 0;
                }
                if (cspPx > 0) { cspAcc += 300.f * spd / (60 * 27); cspPx -= spd; score += (int)cspAcc; cspAcc -= (int)cspAcc; }
            }
        }
        if (haveSci && state == 1) {
            /* WorkerInfo.xml: a spawn roll (40%) about once a second; walk 0.5-2 or run 2-6 (20%); alert when Barry is 200-500 away, then
               run off (60%, 75% past 1000 m). Knocked over: 4-8 at -45..45 degrees, gravity 0.5, spin 8-45, Y friction 0.98, ground friction 0.94.
               Game units x1.5 onto the screen; the check interval and the knock-over rule are mine. */
            if (!gNR && !dead && --sciWait <= 0) { sciWait = 60; if (rand() % 100 < 40) for (int i = 0; i < MAXS; i++) if (!sci[i].on) {
                memset(&sci[i], 0, sizeof sci[i]); sci[i].on = 1; sci[i].dir = rand() & 1 ? 1 : -1; sci[i].x = W + 20;   /* the floor outruns them, so both directions enter on the right */
                int run = rand() % 100 < 20; float lo = run ? 2 : 0.5f, hi = run ? 6 : 2; sci[i].sp = 1.5f * (lo + (hi - lo) * (rand() / (RAND_MAX + 1.f)));
                sci[i].alertD = (int)(1.5f * (200 + rand() % 301)); sci[i].kind = rand() % 3 == 0; break; } }
            int fire = !dead && mech != 2 && y < H - 121 && (btnHeld() & (WPAD_BUTTON_A | WPAD_BUTTON_B | WPAD_BUTTON_1 | WPAD_BUTTON_2));
            for (int i = 0; i < MAXS; i++) if (sci[i].on) {
                sci[i].x -= dead ? 0 : 4;                                   /* the floor scrolls under them */
                if (!sci[i].down) {
                    if (!sci[i].alerted && sci[i].x - 95 < sci[i].alertD) { sci[i].alerted = 1; if (rand() % 100 < (dist >= 1000 ? 75 : 60)) { sci[i].dir = 1; sci[i].sp = 1.5f * (2 + 4 * (rand() / (RAND_MAX + 1.f))); } }
                    sci[i].x += sci[i].dir * sci[i].sp;
                    if (!sci[i].hi5 && !dead && mech != 2 && y >= H - 121 && sci[i].x + 48 > 60 && sci[i].x + 48 < 130) { sci[i].hi5 = 1; runHi5++; }   /* high five: passing them on foot (my rule) */
                    if (sci[i].x + 48 > 70 && sci[i].x + 48 < 120 && (fire || (mech == 2 && y >= H - 200))) {
                        float v = 1.5f * (4 + 4 * (rand() / (RAND_MAX + 1.f))), a = (-45 + rand() % 91) * 0.0174533f;
                        sci[i].down = 1; sci[i].vx = v * cosf(a); sci[i].vy = -fabsf(v * sinf(a)) - 2; sci[i].spin = (8 + rand() % 38) * (rand() & 1 ? 1 : -1); sciHit++; if (mech == 2 && vt < 5) vKoT[vt]++;
                    }
                } else {
                    sci[i].down++; sci[i].x += sci[i].vx; sci[i].h -= sci[i].vy; sci[i].vy += 0.5f * 1.5f; sci[i].vy *= 0.98f; sci[i].ang += sci[i].spin;
                    if (sci[i].h <= 0) { sci[i].h = 0; sci[i].vx *= 0.94f; sci[i].vy = fabsf(sci[i].vy) > 2 * 1.5f ? -fabsf(sci[i].vy) * 0.7f : 0; sci[i].spin *= 0.7f; if (!sci[i].vy) { sci[i].spin = 0; sci[i].ang = sci[i].dir > 0 ? 90 : -90; } }
                }
                if (sci[i].x < -100 || sci[i].x > W + 120) sci[i].on = 0;
            }
        }
        if (gFP && havePig && !dead && state == 1) {
            /* flyingPigs.xml: first pig 200-400 m, at most 2 per "kilometre" of 950 m, never more than 770 m without one, probability 4
               (read here as a 4% roll every 10 m), MAX_Y_MOVE_SPEED 0.5 */
            if (dist / 950 != pigKm) { pigKm = dist / 950; pigInKm = 0; }
            if (!pigOn && dist >= pigFirst && dist / 10 != pigStep && pigInKm < 2) { pigStep = dist / 10;
                if (rand() % 100 < 4 || dist - pigLast >= 770 || !pigLast) { pigOn = 1; pigLast = dist; pigInKm++; pigX = W + 40; pigY = pigTY = 90 + rand() % 160;
#ifdef AUTOTEST
                    { char pp[128]; snprintf(pp, sizeof pp, "%sdbg.txt", dir); FILE *df = fopen(pp, "a"); if (df) { fprintf(df, "pig spawn dist %d y %d\n", dist, (int)pigY); fclose(df); } }
#endif
                } }
            if (pigOn) {
                pigX -= 3; if (fabsf(pigTY - pigY) < 1) pigTY = 90 + rand() % 160; pigY += fmaxf(-0.75f, fminf(0.75f, pigTY - pigY)); float py = pigY;
                if (hypotf(pigX + 32 - 95, py + 32 - (y + 45)) < 52) { pigOn = 0; pigGot++; score += gMul ? 60 : 30; sfx_play(sC2, 0); sfx_play(sC3, 0); }
                else if (pigX < -80) pigOn = 0;
            }
        }
#ifdef AUTOTEST
        if (AUTOTEST != 2 && cst) { if (mech != 2) { mech = 2; vt = 0; } y = 170 + 90 * sinf(frame * 0.05f) + 28 - 45; }   /* test build only: give a vehicle and steer it into the boss head so the ram path runs */
#endif
        if (haveCol && !dead && state == 1) {   /* Colossatron: a head and 6 body segments sweep in from the right in a sine wave; touching one is a hit; 25 coins for outlasting it */
            if (!cst && dist >= nextBoss && cur < 0 && mst == 0) { cst = 1; cbx = W + 40; bossHp = 3; bossCd = 0; nextBoss = dist + 2500; }
            if (cst) {
                cbx -= 4.5f;
                if (bossCd) bossCd--;
                for (int i = 0; i < 7; i++) {
                    float sx = cbx + i * 44, sy = 170 + 90 * sinf(frame * 0.05f + i * 0.5f);
                    if (sx < 150 && sx + 56 > 60 && fabsf(sy + 28 - (y + 45)) < 42) {
                        if (i == 0 && mech == 2) { if (!bossCd) { bossCd = 40; cbx += 130; if (--bossHp <= 0) { cst = 0; boomOn = 0; score += 100; bossKills++; sfx_play(sME, 0); break; } sfx_play(sME, 0); } }   /* ramming the head from a vehicle costs it health and knocks it back so it can be rammed again (my rule) */
                        else { int d0 = dead, m0 = mech; HIT(); if (dead != d0 || mech != m0) bossHit++; }
                    }
                }
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
            if (mst == 2 && mx < 150 && mx + 64 > 60) mNearMin = fminf(mNearMin, fabsf(my - (y + 45)));
            if (mx < -64) { mst = 0; if (gFM) score += 3; if (mNearMin < 70) runNearM++; }   /* near miss: passed within 70 px (my threshold) */   /* Fruit Missiles (perks.xml value 3): a missile you dodge pays 3 coins (my reading) */
        }
        if (!dead) {
            { int ns = (mech == 2 && vt >= 0 && vt < 12 && nzpS[1 + vt]) ? 1 + vt : 0; if (ns != zset) { zset = ns; zi = -1; PKLOG("zapper set %d patterns %d missile %d%% from %d", zset, nzp, (int)mProb, mStart); } }   /* riding: that vehicle's own obstacle file */
            if (zi < 0) { if (evZ) { evZ = 0; { zi = pickZapper(dist, cur >= 0); if (zi >= 0) { zDud = zapDudLeft > 0 ? (zapDudLeft--, 1) : 0; ztick = 0; zNearMin = 999; zHitF = 0; sfx_play(sZap, 0); zx = W + 100; zlen = zp[zi].minL + (zp[zi].maxL - zp[zi].minL) * (rand() / (RAND_MAX + 1.f)); zy = 90 + rand() % 220; } } } }
            else {
                ztick++; zx -= spd;
                ZPat *z = &zp[zi]; int period = z->on + z->off; int lethal = period <= 1 || (ztick % period) < z->on;
                float ang = z->startAngle + (z->rotT ? (z->cw ? 1 : -1) * 360.f * ztick / z->rotT : 0);
                float hx = cosf(ang * 0.0174533f) * zlen / 2, hy = sinf(ang * 0.0174533f) * zlen / 2;
                { float zd = segDist(105, y + 45, zx - hx, zy - hy, zx + hx, zy + hy); if (lethal && !zDud && zd < 20) { HIT(); zHitF = 1; } if (!zDud) zNearMin = fminf(zNearMin, zd); }
                if (zx < -200) { zi = -1; if (!zHitF && zNearMin < 45) runNearZ++; }   /* near miss: within 45 px of the beam (my threshold) */
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
        if (state == 0 && haveTitle) quad(&titleS, 0, 0, W, H, 0, 0, 1, 1); else drawLevel(scroll);
        if (haveAlarm && !(state == 0 && haveTitle)) eachAlarm(scroll, drawAlarmCb, &frame);
        if (haveC) for (int i = 0; i < nc; i++) {
            float px = cx[i] + 700 - fmodf(scroll, patW + 700 + 640); if (px < -20 || px > W || cgot[i]) { if (px < -20) cgot[i] = 0; continue; }
            if ((gDog && cy[i] > H - 170 && px < 140 && px + 16 > 20) || (gMagnet && hypotf(px + 8 - 95, cy[i] + 8 - (y + 45)) < 130) || (gVMag && mech == 2 && hypotf(px + 8 - 95, cy[i] + 8 - (y + 50)) < 190) || (px < 130 && px + 16 > 60 && cy[i] + 16 > y && cy[i] < y + 90)) { cgot[i] = 1; score += gMul ? 2 : 1; sfx_play(rand() % 3 == 0 ? sC2 : rand() & 1 ? sC3 : sCoin, 0); continue; }
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
        if (pkOn && havePk && state == 1) quad(&pkS[pkKind], pkX, pkY, 64, 32, 0, 0, 1, 1);
        if (shield > 0 && havePk && state == 1) quad(&pkS[2], 70, y - 20, 48, 24, 0, 0, 1, 1);
        if (tokOn && haveTok && state == 1) { float k = 36 + 4 * sinf(frame * 0.2f); quad(&tokS, tokX + 20 - k / 2, tokY + 20 - k / 2, k, k, 0, 0, 1, 1); }
        if (haveSci && state == 1) for (int i = 0; i < MAXS; i++) if (sci[i].on) {
            Spr *b = &sciS[sci[i].kind * 2], *hd = &sciS[sci[i].kind * 2 + 1]; float gy = H - 121 + 14, x = sci[i].x;
            if (!sci[i].down) {
                int c = (frame / 6 + i) & 3; float u0 = c / 4.f, u1 = (c + 1) / 4.f; if (sci[i].dir < 0) { float t2 = u0; u0 = u1; u1 = t2; }
                quad(b, x, gy, 96, 96, u0, 0, u1, 1 / 8.f); quad(hd, x + (sci[i].dir > 0 ? 6 : 10), gy - 22, 80, 80, u0, 0, u1, 1 / 8.f);
            } else {
                float ang = sci[i].ang, cy2 = gy + 62 - sci[i].h;
                quadRotUV(b, x + 48, cy2, 96, 96, ang, 0, 2 / 8.f, 1 / 4.f, 3 / 8.f); quadRotUV(hd, x + 48 + 30 * sinf(ang * 0.0174533f), cy2 - 30 * cosf(ang * 0.0174533f), 80, 80, ang, 0, 2 / 8.f, 1 / 4.f, 3 / 8.f);
            }
        }
        if (pigOn && havePig && state == 1) { int i = (frame / 5) % 8; quad(&pigS, pigX, pigY, 64, 64, (i % 4) / 4.f, (2 + i / 4) / 4.f, (i % 4 + 1) / 4.f, (3 + i / 4) / 4.f); }
        if (gDog && haveDog && state == 1) { int c = (frame / 6) & 3; quad(&dogS, 26, H - 92, 64, 64, c / 4.f, 0.5f, (c + 1) / 4.f, 0.75f); }
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
            if (state == 0) { snprintf(t, sizeof t, "BEST %dM   COINS %d", best, total); drawText(270, 424, 0.6f, t);
                if (nmis && !inShop) { snprintf(t, sizeof t, "RANK %d %s   %d OF %d STARS", mRank + 1, nRanks ? rankName[mRank % nRanks] : "", mStars, STARS_PER_RANK); drawText(270, 448, 0.45f, t); } }
            int spinning = state == 2 && (spinsLeft || spinAnim || spinShow);
            if (state == 2 && havePop) { quad(&popS, 6, 14, 308, 452, 0, 0, 1, 1); quad(&popS, 326, 14, 308, 78, 0, 0, 1, 1); }   /* ResultsScreen_New: LeftSide (overview panel) and rightSide (profile/coin balance), each 480 of 960 wide */
            if (state == 0 && haveHaz) for (float ty = 0; ty < H; ty += 96) for (float tx = 22; tx < 254; tx += 86) { float fw = 254 - tx < 86 ? 254 - tx : 86, fh = H - ty < 96 ? H - ty : 96; quad(&hazS, tx, ty, fw, fh, 0, 0, fw / 86, fh / 96); }   /* tiled; the tile is 128 px */   /* mainMenu is 310 wide at x -450 from the centre of the 960x640 screen: the left panel */
            if (state == 0 && haveLogo) {
                quad(&logoT, 270, 20, 360 * 0.95f, 218 * 0.95f, 0, 0, 360.f / 360, 218.f / 220);
                quad(&logoB, 400, 230, 192 * 0.8f, 270 * 0.8f, 0, 0, 192.f / 192, 270.f / 272);
            } else if (state == 0) drawText(W / 2 - 230, 120, 1.4f, "JETPACK JOYRIDE"); else drawText(340, 140, 1.0f, "GAME OVER");
            if (spinning) {
                static const int pz[5] = {0, 100, 250, 500, 1000};
                drawText(30, 90, 0.6f, "FINAL SPIN");
                for (int k = 0; k < 3; k++) { snprintf(t, sizeof t, "%d", spinAnim ? pz[(frame / 3 + k * 2) % 5] : spinPrize); drawText(40 + k * 90, 125, 1.1f, t); }
                if (!spinAnim && spinShow) { drawText(30, 200, 0.7f, spinLbl[0] ? spinLbl : "NO PRIZE"); }
                snprintf(t, sizeof t, "TOKENS LEFT %d", spinsLeft); drawText(30, 250, 0.6f, t);
            } else
            if (state == 2) {   /* layout follows ResultsScreen_New: a distance box, then a collected row */
                drawText(30, 90, 0.6f, "DISTANCE"); snprintf(t, sizeof t, "%dM", lastDist); drawText(30, 112, 1.3f, t);
                snprintf(t, sizeof t, "BEST %dM", best); drawText(30, 165, 0.7f, t);
                drawText(30, 215, 0.7f, "COLLECTED"); snprintf(t, sizeof t, "%d COINS  %d TOKENS", score, runTok); drawText(30, 243, 0.6f, t);
                { int ng = 0; for (int g = 0; g < NSHOP; g++) ng += *shopOwn[g]; snprintf(t, sizeof t, "TOTAL %d   GADGETS %d", total, ng); drawText(30, 290, 0.6f, t); snprintf(t, sizeof t, "COINS %d", total); drawText(344, 44, 0.8f, t); }   /* the real overview also lists collected tokens and the gadgets used ("assists") */
                if (newBest && ((frame / 20) & 1)) drawText(30, 340, 0.9f, "NEW BEST!");
            }
            if (state == 0 && havePause && haveStart) {   /* TitleScreen buttons.startPressed: 332x96 buttonBacking, glow bar at -144, "START" at +38; drawn at 0.75 */
                float cx = 138, cy = 300, bw = 332 * 0.65f, bh = 96 * 0.65f; quad(&pbk, cx - bw / 2, cy - bh / 2, bw, bh, 0, 0, 1, 1); quad(&startG, cx - 144 * 0.65f - 20, cy - 40, 40, 80, 0, 0, 1, 1);
                drawText(cx + 38 * 0.65f - 45, cy - 14, 0.8f, "START");
                if (haveMag) {
                    float sy = 370, sw = 260 * 0.6f, sh = 80 * 0.6f; if (tsel == 1) { sw += 8; sh += 6; } quad(&pbk, cx - sw / 2, sy - sh / 2, sw, sh, 0, 0, 1, 1); drawText(cx - 28, sy - 12, 0.7f, "SHOP");
                    if (inShop) {
                        quad(&pbk, 120, 150, 400, 250, 0, 0, 1, 1); quad(&shopS[shopI], 140, 200, 80, 80, 0, 0, 1, 1);
                        drawText(240, 180, 0.8f, shopName[shopI]); if (gadTxt[shopI][0]) drawWrapped(240, 212, 0.45f, 18, 31, gadTxt[shopI]); else drawText(240, 220, 0.6f, shopDesc[shopI]);
                        if (shopI == HS_SLOT || shopI == FB_SLOT) snprintf(t, sizeof t, "%d COINS   YOU HAVE %d", shopPrice[shopI], shopI == HS_SLOT ? gHS : gFB); else snprintf(t, sizeof t, *shopOwn[shopI] ? "OWNED" : "%d COINS   A TO BUY", shopPrice[shopI]); drawText(240, 318, 0.7f, t);
                        snprintf(t, sizeof t, "YOU HAVE %d   B TO GO BACK", total); drawText(150, 362, 0.6f, t);
                    }
                }
            } else if ((frame / 30) & 1) { if (state == 2 && !spinning) { quad(&pbk, 340, 380, 280, 80, 0, 0, 1, 1); } } drawText(state == 2 ? 372 : W / 2 - 170, state == 2 ? 410 : 300, state == 2 ? 0.7f : 0.9f, spinning ? (spinsLeft && !spinAnim ? "PRESS A TO SPIN" : "") : "A  PLAY AGAIN");
        }
        if (paused && havePause) {
            quad(&dimS, 0, 0, W, H, 0, 0, 1, 1);
            if (haveTb) for (int k = 0; k < 3; k++) quad(&tbS, k * W / 3.f, 0, W / 3.f + 1, 96, 0, 0, 1, 1);
            static const char *const lab[3] = {"QUIT", "RETRY", "RESUME"};
            for (int i = 0; i < 3; i++) {
                float cx = W / 2 + (i - 1) * 150, cy = 240, bs = i == psel ? 140 : 130;
                quad(&pbk, cx - bs / 2, cy - bs / 2, bs, bs, 0, 0, 1, 1); quad(&pic[i], cx - 38, cy - 38 - 12, 77, 77, 0, 0, 1, 1);
                drawText(cx - strlen(lab[i]) * 10.5f, cy + 24, 0.6f, lab[i]);
            }
            drawText(W / 2 - 110, haveTb ? 34 : 130, 1.2f, "PAUSED");
        } else if (paused) drawText(W / 2 - 110, 200, 1.2f, "PAUSED");
        if (blastWait > 0 && state == 1) { char bl[48]; snprintf(bl, sizeof bl, "PRESS A TO BLAST   %d LEFT", gFB); if ((frame / 10) & 1) drawText(W / 2 - 190, 200, 0.8f, bl); }
        if (blastFx > 0 && haveCol) { blastFx--; quad(&boomS, 40, y - 10, 160, 110, 1, 0, 0, 1); }
        if (mRankCoins) { total += mRankCoins; mRankCoins = 0; writeSave(best, total); }
        if (mRankUp > 0 && state == 1 && !paused && mBanner <= 0) { char rb[64]; mRankUp--; snprintf(rb, sizeof rb, "RANK UP  %s", nRanks ? rankName[mRank % nRanks] : ""); drawText(W / 2 - strlen(rb) * 8.f, 120, 0.8f, rb); }
        if (mBanner > 0 && state == 1 && !paused) { mBanner--; drawText(W / 2 - 150, 120, 0.8f, "MISSION COMPLETE"); drawText(W / 2 - strlen(mBannerText) * 4.2f, 152, 0.45f, mBannerText); }
        if (paused && nmis) { char rl[64]; snprintf(rl, sizeof rl, "RANK %d %s   %d OF %d STARS", mRank + 1, nRanks ? rankName[mRank % nRanks] : "", mStars, STARS_PER_RANK); drawText(40, 306, 0.45f, rl); }
        if (paused && nmis) for (int s = 0; s < 3; s++) {   /* the three active missions, as the game's pause screen lists them */
            char ml[96]; int m = mAct[s];
            if (mTotal(mType[m]) && mType[m] != M_SPEND) snprintf(ml, sizeof ml, "%s  %d OF %d", mText[m], mProg[s], mP1[m]); else snprintf(ml, sizeof ml, "%s", mText[m]);
            drawText(40, 330 + s * 26, 0.45f, ml);
        }
        if (state == 1) { char hud[48]; snprintf(hud, sizeof hud, "%dM", dist); drawText(20, 14, 1.0f, hud); snprintf(hud, sizeof hud, "COINS %d", score); drawText(20, 50, 0.7f, hud); snprintf(hud, sizeof hud, "LEVEL %d", lvl + 1); drawText(20, 78, 0.5f, hud); }
        GX_DrawDone(); GX_CopyDisp(xfb[fb ^= 1], GX_TRUE); VIDEO_SetNextFramebuffer(xfb[fb]); VIDEO_Flush(); VIDEO_WaitVSync();
    }
    audio_stop(); INPUT_Shutdown(); return 0;
}
