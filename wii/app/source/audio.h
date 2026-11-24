#ifndef AUDIO_H
#define AUDIO_H
void audio_start(const char *oggPath);   /* loops until audio_stop() */
void audio_stop(void);
int sfx_load(const char *oggPath);          /* id, or -1 */
int sfx_play(int id, int loop);             /* voice, or -1 */
void sfx_stop(int voice);
int sfx_playing(int voice);
#endif
