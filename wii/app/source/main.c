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

#define W 640
#define H 480
static const char *dir = "sd:/apps/jetpackjoyride/";   /* switched to fat: when libfat mounts the card there */

typedef struct { GXTexObj obj; void *data; int w, h; } Spr;

static void *fifo, *xfb[2]; static int fb; static GXRModeObj *rmode;

static int loadSpr(Spr *s, const char *name) {
    char p[128]; snprintf(p, sizeof p, "%s%s", dir, name);
    FILE *f = fopen(p, "rb"); if (!f) return 0;
    u8 hd[12]; if (fread(hd, 1, 12, f) != 12 || memcmp(hd, "SPR0", 4)) { fclose(f); return 0; }
    int pw = hd[4] << 8 | hd[5], ph = hd[6] << 8 | hd[7]; s->w = hd[8] << 8 | hd[9]; s->h = hd[10] << 8 | hd[11];
    size_t n = (size_t)pw * ph * 4; s->data = memalign(32, n);
    if (!s->data || fread(s->data, 1, n, f) != n) { fclose(f); return 0; }
    fclose(f); DCFlushRange(s->data, n);
    GX_InitTexObj(&s->obj, s->data, pw, ph, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjFilterMode(&s->obj, GX_NEAR, GX_NEAR);
    return 1;
}

static void fallbackSpr(Spr *s) {   /* magenta 4x4 so a missing asset is visible */
    u8 *d = memalign(32, 64); for (int i = 0; i < 32; i += 2) { d[i] = 255; d[i + 1] = 255; d[32 + i] = 0; d[33 + i] = 255; }
    DCFlushRange(d, 64); s->data = d; s->w = s->h = 4; GX_InitTexObj(&s->obj, d, 4, 4, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
}

static void quadRot(Spr *s, float cx, float cy, float w, float h, float deg) {   /* quad centred on (cx,cy), rotated */
    Mtx r, t, m; guMtxRotDeg(r, 'z', deg); guMtxTrans(t, cx, cy, 0); guMtxConcat(t, r, m); GX_LoadPosMtxImm(m, GX_PNMTX0);
    GX_LoadTexObj(&s->obj, GX_TEXMAP0);
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position2f32(-w / 2, -h / 2); GX_TexCoord2f32(0, 0); GX_Position2f32(w / 2, -h / 2); GX_TexCoord2f32(1, 0);
    GX_Position2f32(w / 2, h / 2); GX_TexCoord2f32(1, 1); GX_Position2f32(-w / 2, h / 2); GX_TexCoord2f32(0, 1);
    GX_End();
    guMtxIdentity(m); GX_LoadPosMtxImm(m, GX_PNMTX0);
}

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
    GX_SetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter); GX_SetCopyClear((GXColor){40, 90, 140, 255}, GX_MAX_Z24);
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

#define MAXC 4096
static float cx[MAXC], cy[MAXC]; static u8 cgot[MAXC]; static int nc; static float patW;

static void loadPattern(void) {   /* '1' cells become coins; blank lines are skipped */
    char pp[128]; snprintf(pp, sizeof pp, "%spattern.txt", dir);
    FILE *f = fopen(pp, "r"); char ln[256]; int row = 0; if (!f) return;
    while (fgets(ln, sizeof ln, f)) {
        if (ln[0] == '\n' || ln[0] == '\r') continue;
        for (int c = 0; ln[c] && ln[c] != '\n' && ln[c] != '\r'; c++) {
            if (ln[c] == '1' && nc < MAXC) { cx[nc] = c * 20.f; cy[nc] = 120 + row * 22.f; nc++; }
            if (c * 20.f + 20 > patW) patW = c * 20.f + 20;
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

int main(int argc, char **argv) {
    gfxInit(); int fatOk = fatInitDefault();
    { FILE *t = fopen("sd:/apps/jetpackjoyride/pattern.txt", "r"); if (t) fclose(t); else dir = "fat:/apps/jetpackjoyride/"; } INPUT_Init(); INPUT_SetScreenSize(W, H);
    WIIUPDATE_SetAppPath(argc > 0 ? argv[0] : NULL); WIIUPDATE_Start("quatric", "jetpackjoyride_nx", "0.1.0");
    Spr barry = {0}, coin = {0};
    if (!loadSpr(&barry, "barry.spr")) fallbackSpr(&barry);
    if (!loadSpr(&coin, "coin.spr")) fallbackSpr(&coin);
    int haveB = 1, haveC = 1;
    loadPattern(); loadLasers();
    { char mp[160]; snprintf(mp, sizeof mp, "%smusic.ogg", dir); audio_start(mp); }
    Spr beam = {0}, emit = {0}; if (!loadSpr(&beam, "beam.spr")) fallbackSpr(&beam); if (!loadSpr(&emit, "emit.spr")) fallbackSpr(&emit);
    Spr missile = {0}, alert = {0}; if (!loadSpr(&missile, "missile.spr")) fallbackSpr(&missile); if (!loadSpr(&alert, "alert.spr")) fallbackSpr(&alert);
    loadMissile(); loadZappers(); loadFont();
    int sCoin, sHurt, sLaserW, sMissW, sMissL, sJet; { char pp[160]; const char *n[] = {"coin.ogg", "hurt.ogg", "laserw.ogg", "missw.ogg", "missl.ogg", "jet.ogg"}; int *o[] = {&sCoin, &sHurt, &sLaserW, &sMissW, &sMissL, &sJet};
        for (int i = 0; i < 6; i++) { snprintf(pp, sizeof pp, "%s%s", dir, n[i]); *o[i] = sfx_load(pp); } }
    int jetV = -1, prevCur = -1, prevMst = 0;
    Spr bolt = {0}; if (!loadSpr(&bolt, "bolt.spr")) fallbackSpr(&bolt);
    int zi = -1, ztick = 0, zwait = 150; float zx = 0, zy = 0, zlen = 0;
    int cur = -1, ptick = 0, wait = 90, dead = 0, mst = 0, mtick = 0, mwait = 120; float my = 0, mx = 0;   /* mst: 0 idle, 1 alert, 2 flying */
    if (!fatOk) GX_SetCopyClear((GXColor){160, 0, 0, 255}, GX_MAX_Z24);          /* red: no SD */
    else if (!nc) GX_SetCopyClear((GXColor){160, 120, 0, 255}, GX_MAX_Z24);    /* amber: assets missing */
    float y = 300, vy = 0, scroll = 0; int score = 0, frame = 0; (void)score;
    for (;;) {
        INPUT_Update();
        if (INPUT_ButtonsDown() & WPAD_BUTTON_HOME) break;
        vy += (INPUT_ButtonsHeld() & (WPAD_BUTTON_A | WPAD_BUTTON_B | WPAD_BUTTON_1 | WPAD_BUTTON_2) ? -0.9f : 0.6f);
        if (vy > 9) vy = 9;
        if (vy < -9) vy = -9;
        y += vy;
        if (y < 60) { y = 60; vy = 0; }
        if (y > H - 120) { y = H - 120; vy = 0; }   /* ceiling / floor */
        if (dead) { if (--dead == 0) { y = 300; vy = 0; scroll = 0; memset(cgot, 0, sizeof cgot); cur = -1; wait = 90; mst = 0; mwait = 120; zi = -1; zwait = 150; } }
        else scroll += 4;
        frame++;
        int dist = (int)(scroll / 2);   /* ~120 "distance" units per second */
        if (!dead) {
            if (cur < 0) { if (--wait <= 0) { cur = pickPattern(dist); ptick = 0; wait = 60; } }
            else if (++ptick > pats[cur].time + 60) cur = -1;
            if (cur >= 0 && ptick > 60) for (int i = 0; i < pats[cur].n; i++) {
                float ly = 240 - pats[cur].y[i] * 1.6f;
                if (fabsf(ly - (y + 45)) < 22) { dead = 60; sfx_play(sHurt, 0); break; }
            }
        }
        if (!dead && cur < 0) {   /* missiles share the screen with lasers only when no laser pattern is active */
            if (mst == 0 && dist >= mStart && --mwait <= 0) { mwait = 120; if (rand() % 100 < mProb) { mst = 1; mtick = 0; my = y + 45; } }
            else if (mst == 1) { float t = y + 45; my += fminf(fmaxf(t - my, -mChase * 2), mChase * 2); if (++mtick > 90) { mst = 2; mx = W; } }
        }
        if (mst == 2 && !dead) {
            mx -= mSpeed;
            if (mx < 150 && mx + 64 > 60 && fabsf(my - (y + 45)) < 28) { dead = 60; sfx_play(sHurt, 0); }
            if (mx < -64) { mst = 0; }
        }
        if (!dead) {
            if (zi < 0) { if (dist >= zGlobalSd && --zwait <= 0) { zwait = 150; if (rand() % 100 < zGlobalP) { zi = pickZapper(dist, cur >= 0); if (zi >= 0) { ztick = 0; zx = W + 100; zlen = zp[zi].minL + (zp[zi].maxL - zp[zi].minL) * (rand() / (RAND_MAX + 1.f)); zy = 90 + rand() % 220; } } } }
            else {
                ztick++; zx -= 4;
                ZPat *z = &zp[zi]; int period = z->on + z->off; int lethal = period <= 1 || (ztick % period) < z->on;
                float ang = z->startAngle + (z->rotT ? (z->cw ? 1 : -1) * 360.f * ztick / z->rotT : 0);
                float hx = cosf(ang * 0.0174533f) * zlen / 2, hy = sinf(ang * 0.0174533f) * zlen / 2;
                if (lethal && segDist(105, y + 45, zx - hx, zy - hy, zx + hx, zy + hy) < 20) { dead = 60; sfx_play(sHurt, 0); }
                if (zx < -200) zi = -1;
            }
        }
        if (cur >= 0 && prevCur < 0) sfx_play(sLaserW, 0);
        if (mst == 1 && prevMst == 0) sfx_play(sMissW, 0);
        if (mst == 2 && prevMst == 1) sfx_play(sMissL, 0);
        prevCur = cur; prevMst = mst;
        { int thrust = !dead && (INPUT_ButtonsHeld() & (WPAD_BUTTON_A | WPAD_BUTTON_B | WPAD_BUTTON_1 | WPAD_BUTTON_2));
          if (thrust && !sfx_playing(jetV)) jetV = sfx_play(sJet, 1); else if (!thrust && jetV > 0) { sfx_stop(jetV); jetV = -1; } }
        GX_InvVtxCache(); GX_InvalidateTexAll();
        if (haveC) for (int i = 0; i < nc; i++) {
            float px = cx[i] + 700 - fmodf(scroll, patW + 700 + 640); if (px < -20 || px > W || cgot[i]) { if (px < -20) cgot[i] = 0; continue; }
            if (px < 130 && px + 16 > 60 && cy[i] + 16 > y && cy[i] < y + 90) { cgot[i] = 1; score++; sfx_play(sCoin, 0); continue; }
            int fr = (frame / 4 + i) & 7; quad(&coin, px, cy[i], 16, 16, fr / 8.f, 0, (fr + 1) / 8.f, 1);
        }
        if (zi >= 0) {
            ZPat *z = &zp[zi]; int period = z->on + z->off; int lethal = period <= 1 || (ztick % period) < z->on;
            float ang = z->startAngle + (z->rotT ? (z->cw ? 1 : -1) * 360.f * ztick / z->rotT : 0);
            float hx = cosf(ang * 0.0174533f) * zlen / 2, hy = sinf(ang * 0.0174533f) * zlen / 2;
            if (lethal) quadRot(&bolt, zx, zy, zlen, 24, ang);
            quad(&emit, zx - hx - 16, zy - hy - 20, 32, 40, 0, 0, 64.f / 64, 85.f / 88);
            quad(&emit, zx + hx - 16, zy + hy - 20, 32, 40, 0, 0, 64.f / 64, 85.f / 88);
        }
        if (mst == 1 && (mtick / 8) % 2 == 0) { int fr = (frame / 8) & 3; quad(&alert, W - 70, my - 32, 64, 64, fr / 4.f, 0, (fr + 1) / 4.f, 1); }
        if (mst == 2) { int fr = (frame / 4) & 3; quad(&missile, mx, my - 16, 64, 32, fr / 4.f, 0, (fr + 1) / 4.f, 1); }
        if (cur >= 0) for (int i = 0; i < pats[cur].n; i++) {
            float ly = 240 - pats[cur].y[i] * 1.6f; int live = ptick > 60;
            if (!live && (ptick / 6) & 1) continue;                      /* flashing warning */
            quad(&beam, 64, ly - (live ? 12 : 4), W - 128, live ? 24 : 8, 0, 0, 1, 1);
            quad(&emit, 0, ly - 42, 64, 85, 0, 0, 64.f / 64, 85.f / 88);
            quad(&emit, W - 64, ly - 42, 64, 85, 64.f / 64, 0, 0, 85.f / 88);
        }
        if (haveB) quad(&barry, 60, y, 90, 90, 0, 0, (float)barry.w / ((barry.w + 3) / 4 * 4), (float)barry.h / ((barry.h + 3) / 4 * 4));
                { char hud[48]; snprintf(hud, sizeof hud, "%dM", dist); drawText(20, 14, 1.0f, hud); snprintf(hud, sizeof hud, "COINS %d", score); drawText(20, 50, 0.7f, hud); }
        GX_DrawDone(); GX_CopyDisp(xfb[fb ^= 1], GX_TRUE); VIDEO_SetNextFramebuffer(xfb[fb]); VIDEO_Flush(); VIDEO_WaitVSync();
    }
    audio_stop(); INPUT_Shutdown(); return 0;
}
