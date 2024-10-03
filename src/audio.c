#include <stdint.h>
#include <stdio.h>

#include <zephyr/drivers/adc.h>

#include "audio.h"
#include "util.h"

#define ADC_NODE DT_ALIAS(adc0)
static const struct device* adc = DEVICE_DT_GET(ADC_NODE);
static const struct adc_channel_cfg channel_cfgs[] = {
    DT_FOREACH_CHILD_SEP(ADC_NODE, ADC_CHANNEL_CFG_DT, (,))
};

#define CHANNEL_COUNT ARRAY_SIZE(channel_cfgs)

uint16_t SAMPLE_BUFFER[SAMPLE_BUFFER_LEN];

const uint16_t* audio_record_audio(void)
{
    int err;

    return SAMPLE_BUFFER;

	// Set up ADC.
	const struct adc_sequence_options sequence_options = {
		.interval_us = 1000000 / SAMPLE_FREQ,
		.extra_samplings = SAMPLE_BUFFER_LEN - 1,
	};

	struct adc_sequence sampling_sequence = {
		.options = &sequence_options,
		.buffer = SAMPLE_BUFFER,
		.buffer_size = sizeof(SAMPLE_BUFFER),
		.resolution = 12,
	};

	while (!device_is_ready(adc))
	{
		k_msleep(1000);
	}

	sampling_sequence.channels = 1;
	err = adc_channel_setup(adc, &channel_cfgs[0]);
	if (err < 0)
	{
		printf("could not set up channel\n");
		failspin();
	}

    err = adc_read(adc, &sampling_sequence);
    if (err < 0)
    {
        printf("failed to sample audio");
        failspin();
    }

    return SAMPLE_BUFFER;
}

const uint16_t* audio_record_audio_raw(void)
{

    // Configure the ADC for 12-bit resolution
    NRF_SAADC->RESOLUTION = SAADC_RESOLUTION_VAL_12bit;

    // Configure channel 0 for single-ended input from AIN0
    NRF_SAADC->CH[0].PSELP = SAADC_CH_PSELP_PSELP_AnalogInput0;
    NRF_SAADC->CH[0].PSELN = SAADC_CH_PSELN_PSELN_NC;

    // Configure channel 0 with gain of 1/4, VDD/4 reference, and 10us acquisition time
    NRF_SAADC->CH[0].CONFIG = (SAADC_CH_CONFIG_RESP_Bypass << SAADC_CH_CONFIG_RESP_Pos) |
                              (SAADC_CH_CONFIG_RESN_Bypass << SAADC_CH_CONFIG_RESN_Pos) |
                              (SAADC_CH_CONFIG_GAIN_Gain1_4 << SAADC_CH_CONFIG_GAIN_Pos) |
                              (SAADC_CH_CONFIG_REFSEL_VDD1_4 << SAADC_CH_CONFIG_REFSEL_Pos) |
                              (SAADC_CH_CONFIG_TACQ_10us << SAADC_CH_CONFIG_TACQ_Pos) |
                              (SAADC_CH_CONFIG_MODE_SE << SAADC_CH_CONFIG_MODE_Pos);


    // Configure the sample rate
    // For 22050 Hz, CC = 16 MHz / 22050 Hz = 725.62
    // Round up to 726
    NRF_SAADC->SAMPLERATE = (726 << SAADC_SAMPLERATE_CC_Pos) |
                            (SAADC_SAMPLERATE_MODE_Timers << SAADC_SAMPLERATE_MODE_Pos);

    // Configure EasyDMA
    // NRF_SAADC->RESULT.PTR = (uint32_t) SAMPLE_BUFFER;
    uint32_t* const saadc_result_ptr = (uint32_t*) (0x40007000 + 0x62C);
    *saadc_result_ptr = (uint32_t) SAMPLE_BUFFER;
    // NRF_SAADC->RESULT.MAXCNT = SAMPLE_BUFFER_LEN;
    uint32_t* const saadc_result_maxcnt = (uint32_t*) (0x40007000 + 0x630);
    *saadc_result_maxcnt = SAMPLE_BUFFER_LEN;

    // uint32_t* const rtc_intenset = (uint32_t*) (0x40008000 + 0x304);
    // uint32_t* const rtc_intenclr = (uint32_t*) (0x40008000 + 0x308);
    // *rtc_intenclr = (1 << 19) | (1 << 18) | (1 << 17) | (1 << 16) | (1 << 1) | (1 << 0);

    // Enable the ADC
    NRF_SAADC->ENABLE = SAADC_ENABLE_ENABLE_Enabled;

    // Start the ADC
    NRF_SAADC->TASKS_START = 1;

    while(true);
    while ((NRF_SAADC->EVENTS_END & 1) == 0) {  }

    return SAMPLE_BUFFER;
}