/* Music streaming: Tremor (libvorbisidec) decodes an .ogg from SD on its own thread into ASND voice 0. */
#include <asndlib.h>
#include <gccore.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <tremor/ivorbisfile.h>
#include <unistd.h>
#include "audio.h"

#define FRAMES 4096
#define BYTES (FRAMES * 4)
static s16 *buf[2];
static lwp_t th;
static volatile int stop, gen;   /* gen changes when audio_switch picks another file */
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
    while (!stop) {
        int g = gen; char p[160]; snprintf(p, sizeof p, "%s", path);
        FILE *f = fopen(p, "rb"); if (!f) { while (!stop && g == gen) usleep(20000); continue; }
        OggVorbis_File vf; if (ov_open(f, &vf, NULL, 0) < 0) { fclose(f); while (!stop && g == gen) usleep(20000); continue; }
        vorbis_info *vi = ov_info(&vf, -1); int rate = vi->rate; int fmt = vi->channels == 1 ? VOICE_MONO_16BIT : VOICE_STEREO_16BIT;
        int cur = 0; long n = fill(&vf, buf[0]); if (n <= 0) { ov_clear(&vf); while (!stop && g == gen) usleep(20000); continue; }
        DCFlushRange(buf[0], n); ASND_SetVoice(0, fmt, rate, 0, buf[0], n, 200, 200, NULL);
        while (!stop && g == gen) {
            cur ^= 1; n = fill(&vf, buf[cur]); if (n <= 0) break;
            DCFlushRange(buf[cur], n);
            while (!stop && g == gen && ASND_AddVoice(0, buf[cur], n) == SND_BUSY) usleep(2000);
        }
        ASND_StopVoice(0);
        ov_clear(&vf);
    }
    return NULL;
}

void audio_start(const char *file) {
    snprintf(path, sizeof path, "%s", file);
    ASND_Init(); ASND_Pause(0);
    buf[0] = memalign(32, BYTES); buf[1] = memalign(32, BYTES);
    if (buf[0] && buf[1]) LWP_CreateThread(&th, run, NULL, NULL, 16 * 1024, 64);
}

void audio_switch(const char *file) {   /* swap tracks; the thread notices at its next buffer */
    if (!strcmp(path, file)) return;
    snprintf(path, sizeof path, "%s", file); gen++;
}
void audio_stop(void) { stop = 1; }
