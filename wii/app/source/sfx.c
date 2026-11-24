/* Sound effects: short .ogg files decoded fully into RAM by Tremor, played on ASND voices 1..15. */
#include <asndlib.h>
#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <tremor/ivorbisfile.h>
#include "audio.h"

#define MAXSFX 16
#define MAXBYTES (512 * 1024)
static struct { s16 *d; u32 len; int rate, fmt; } fx[MAXSFX];
static int nfx;

int sfx_load(const char *path) {   /* returns id or -1 */
    if (nfx >= MAXSFX) return -1;
    FILE *f = fopen(path, "rb"); if (!f) return -1;
    OggVorbis_File vf; if (ov_open(f, &vf, NULL, 0) < 0) { fclose(f); return -1; }
    vorbis_info *vi = ov_info(&vf, -1);
    char *buf = memalign(32, MAXBYTES); u32 got = 0; int bs; long r;
    while (buf && got < MAXBYTES && (r = ov_read(&vf, buf + got, MAXBYTES - got, &bs)) > 0) got += r;
    fx[nfx].rate = vi->rate; fx[nfx].fmt = vi->channels == 1 ? VOICE_MONO_16BIT : VOICE_STEREO_16BIT;
    ov_clear(&vf);
    if (!buf || got < 32) { free(buf); return -1; }
    got &= ~31u; DCFlushRange(buf, got); fx[nfx].d = (s16 *)buf; fx[nfx].len = got;
    return nfx++;
}

int sfx_play(int id, int loop) {   /* returns voice or -1; loop voices are re-armed by sfx_keep() */
    if (id < 0) return -1;
    for (int v = 1; v < 16; v++) if (ASND_StatusVoice(v) == SND_UNUSED) {
        ASND_SetVoice(v, fx[id].fmt, fx[id].rate, 0, fx[id].d, fx[id].len, 255, 255, NULL);
        (void)loop; return v;
    }
    return -1;
}

void sfx_stop(int voice) { if (voice > 0) ASND_StopVoice(voice); }
int sfx_playing(int voice) { return voice > 0 && ASND_StatusVoice(voice) != SND_UNUSED; }
