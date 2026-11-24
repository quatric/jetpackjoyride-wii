/* Music streaming: Tremor (libvorbisidec) decodes an .ogg from SD on its own thread into ASND voice 0. */
#include <asndlib.h>
#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <tremor/ivorbisfile.h>
#include <unistd.h>
#include "audio.h"

#define FRAMES 4096
#define BYTES (FRAMES * 4)
static s16 *buf[2];
static lwp_t th;
static volatile int stop;
static char path[160];

static long fill(OggVorbis_File *vf, s16 *b) {   /* returns bytes decoded, looping at EOF */
    long got = 0; int bs;
    while (got < BYTES) {
        long r = ov_read(vf, (char *)b + got, BYTES - got, &bs);
        if (r > 0) got += r;
        else if (r == 0) ov_pcm_seek(vf, 0);
        else return got ? got : -1;
    }
    return got;
}

static void *run(void *arg) {
    (void)arg;
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    OggVorbis_File vf; if (ov_open(f, &vf, NULL, 0) < 0) { fclose(f); return NULL; }
    vorbis_info *vi = ov_info(&vf, -1); int rate = vi->rate; int fmt = vi->channels == 1 ? VOICE_MONO_16BIT : VOICE_STEREO_16BIT;
    int cur = 0; long n = fill(&vf, buf[0]); if (n <= 0) goto done;
    DCFlushRange(buf[0], n); ASND_SetVoice(0, fmt, rate, 0, buf[0], n, 200, 200, NULL);
    while (!stop) {
        cur ^= 1; n = fill(&vf, buf[cur]); if (n <= 0) break;
        DCFlushRange(buf[cur], n);
        while (!stop && ASND_AddVoice(0, buf[cur], n) == SND_BUSY) usleep(2000);
    }
    ASND_StopVoice(0);
done:
    ov_clear(&vf); return NULL;
}

void audio_start(const char *file) {
    snprintf(path, sizeof path, "%s", file);
    ASND_Init(); ASND_Pause(0);
    buf[0] = memalign(32, BYTES); buf[1] = memalign(32, BYTES);
    if (buf[0] && buf[1]) LWP_CreateThread(&th, run, NULL, NULL, 16 * 1024, 64);
}

void audio_stop(void) { stop = 1; }
