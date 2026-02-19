#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

#define SOUND_SAMPLE_RATE 44100
#define SOUND_BUFFER_SIZE 4096
#define SOUND_CHANNELS 2

typedef enum {
    SOUND_FORMAT_U8,
    SOUND_FORMAT_S16,
    SOUND_FORMAT_S32
} sound_format_t;

typedef struct {
    uint8_t* buffer;
    uint32_t size;
    uint32_t position;
    sound_format_t format;
    uint32_t sample_rate;
    uint32_t channels;
} sound_stream_t;

void sound_init(void);
void sound_play_tone(uint32_t frequency, uint32_t duration_ms);
void sound_play_buffer(const void* buffer, uint32_t size, sound_format_t format);
void sound_stop(void);
void sound_set_volume(uint8_t volume);
uint8_t sound_get_volume(void);
sound_stream_t* sound_create_stream(uint32_t sample_rate, uint32_t channels, sound_format_t format);
void sound_destroy_stream(sound_stream_t* stream);
void sound_stream_write(sound_stream_t* stream, const void* data, uint32_t size);

#endif