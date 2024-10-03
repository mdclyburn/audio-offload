#include <stdint.h>

#define SAMPLE_TIME_SECS 1
#define SAMPLE_FREQ 22050
#define SAMPLE_BUFFER_LEN (SAMPLE_FREQ * SAMPLE_TIME_SECS)

const uint16_t* audio_record_audio(void);
const uint16_t* audio_record_audio_raw(void);