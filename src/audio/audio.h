#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>
#include <stdint.h>

void audio_init(void);
bool audio_load(const char *path);
void audio_start(void);
void audio_refill(void);
uint32_t audio_time_ms(void);

#endif // AUDIO_H



