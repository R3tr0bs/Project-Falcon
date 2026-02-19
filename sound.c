#include "sound.h"
#include "ports.h"
#include "utils.h"
#include <stddef.h>
#include "heap.h"

#define PCSPKR_PORT 0x61
#define PIT_CHANNEL2 0x42
#define PIT_COMMAND 0x43

static uint8_t current_volume = 128;
static uint32_t sound_enabled = 0;

void sound_init(void) {
    sound_enabled = 1;
    current_volume = 128;
    
    // Initialize PC speaker
    uint8_t tmp = inb(PCSPKR_PORT);
    if (tmp != (tmp | 3)) {
        outb(PCSPKR_PORT, tmp | 3);
    }
}

void sound_play_tone(uint32_t frequency, uint32_t duration_ms) {
    if (!sound_enabled) return;
    
    uint32_t divisor = 1193180 / frequency;
    
    // Send command to PIT
    outb(PIT_COMMAND, 0xB6);
    
    // Send frequency divisor
    outb(PIT_CHANNEL2, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL2, (uint8_t)((divisor >> 8) & 0xFF));
    
    // Enable speaker
    uint8_t tmp = inb(PCSPKR_PORT);
    outb(PCSPKR_PORT, tmp | 0x03);
    
    // Simple delay
    for (volatile uint32_t i = 0; i < duration_ms * 1000; i++);
    
    // Disable speaker
    tmp = inb(PCSPKR_PORT);
    outb(PCSPKR_PORT, tmp & 0xFC);
}

void sound_play_buffer(const void* buffer, uint32_t size, sound_format_t format) {
    if (!sound_enabled || !buffer || size == 0) return;
    
    // For PC speaker, we can only play simple tones
    // This is a simplified implementation that plays a tone based on the first sample
    
    if (format == SOUND_FORMAT_U8) {
        uint8_t* samples = (uint8_t*)buffer;
        uint8_t avg_sample = 0;
        
        // Calculate average of first few samples
        uint32_t samples_to_avg = (size > 16) ? 16 : size;
        for (uint32_t i = 0; i < samples_to_avg; i++) {
            avg_sample += samples[i];
        }
        avg_sample /= samples_to_avg;
        
        // Map sample to frequency (200-2000 Hz)
        uint32_t frequency = 200 + (avg_sample * 1800) / 255;
        
        // Play tone for a short duration
        sound_play_tone(frequency, 100);
    } else if (format == SOUND_FORMAT_S16) {
        int16_t* samples = (int16_t*)buffer;
        int16_t avg_sample = 0;
        
        uint32_t samples_to_avg = (size > 32) ? 16 : (size / 2);
        for (uint32_t i = 0; i < samples_to_avg; i++) {
            avg_sample += samples[i];
        }
        avg_sample /= samples_to_avg;
        
        // Convert to unsigned and map to frequency
        uint32_t frequency = 200 + ((avg_sample + 32768) * 1800) / 65535;
        
        sound_play_tone(frequency, 100);
    }
}

void sound_stop(void) {
    uint8_t tmp = inb(PCSPKR_PORT);
    outb(PCSPKR_PORT, tmp & 0xFC);
}

void sound_set_volume(uint8_t volume) {
    current_volume = volume;
}

uint8_t sound_get_volume(void) {
    return current_volume;
}

sound_stream_t* sound_create_stream(uint32_t sample_rate, uint32_t channels, sound_format_t format) {
    sound_stream_t* stream = (sound_stream_t*)kmalloc(sizeof(sound_stream_t));
    if (!stream) return NULL;
    
    stream->buffer = (uint8_t*)kmalloc(SOUND_BUFFER_SIZE);
    if (!stream->buffer) {
        kfree(stream);
        return NULL;
    }
    
    stream->size = SOUND_BUFFER_SIZE;
    stream->position = 0;
    stream->format = format;
    stream->sample_rate = sample_rate;
    stream->channels = channels;
    
    return stream;
}

void sound_destroy_stream(sound_stream_t* stream) {
    if (!stream) return;
    
    if (stream->buffer) {
        kfree(stream->buffer);
    }
    
    kfree(stream);
}

void sound_stream_write(sound_stream_t* stream, const void* data, uint32_t size) {
    if (!stream || !data || size == 0) return;
    
    uint32_t write_size = (size > stream->size - stream->position) ? (stream->size - stream->position) : size;
    
    const uint8_t* src = (const uint8_t*)data;
    uint8_t* dst = stream->buffer + stream->position;
    
    for (uint32_t i = 0; i < write_size; i++) {
        dst[i] = src[i];
    }
    
    stream->position += write_size;
    
    // If buffer is full, play it
    if (stream->position >= stream->size) {
        sound_play_buffer(stream->buffer, stream->size, stream->format);
        stream->position = 0;
    }
}