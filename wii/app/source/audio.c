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
static s16 *buf[3];   /* one playing, one queued, one being decoded */
static lwp_t th;
static volatile int stop, gen;   /* gen changes when audio_switch picks another file */
static char path[160];
volatile int audioFed, audioRestarts;   /* debug counters for the test build */

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
            cur = (cur + 1) % 3; n = fill(&vf, buf[cur]); if (n <= 0) break;
            DCFlushRange(buf[cur], n);
            /* AddVoice says SND_INVALID (not BUSY) while the last SetVoice is still pending, so only an
               unused voice (an underrun) means start over; anything else is retried */
            while (!stop && g == gen) {
                if (ASND_AddVoice(0, buf[cur], n) == SND_OK) { audioFed++; break; }
                if (ASND_StatusVoice(0) == SND_UNUSED) { ASND_SetVoice(0, fmt, rate, 0, buf[cur], n, 200, 200, NULL); audioRestarts++; break; }
                usleep(2000);
            }
        }
        ASND_StopVoice(0);
        ov_clear(&vf);
    }
    return NULL;
}

void audio_start(const char *file) {
    snprintf(path, sizeof path, "%s", file);
    ASND_Init(); ASND_Pause(0);
    for (int i = 0; i < 3; i++) if (!(buf[i] = memalign(32, BYTES))) return;
    LWP_CreateThread(&th, run, NULL, NULL, 64 * 1024, 64);   /* Tremor wants a deep stack */
}

void audio_switch(const char *file) {   /* swap tracks; the thread notices at its next buffer */
    if (!strcmp(path, file)) return;
    snprintf(path, sizeof path, "%s", file); gen++;
}
void audio_stop(void) { stop = 1; }
