/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>

#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/openthread.h>
#include <zephyr/net/socket.h>
#include <zephyr/usb/usb_device.h>

#include <openthread/ip6.h>
#include <openthread/thread.h>

#include "audio.h"
#include "util.h"

LOG_MODULE_REGISTER(cli_sample, CONFIG_OT_COMMAND_LINE_INTERFACE_LOG_LEVEL);

#define WELLCOME_TEXT \
	"\n\r"\
	"\n\r"\
	"OpenThread Command Line Interface is now running.\n\r"

#define DEST_IP "fde3:8f5e:f206:2::c0a8:023d"
#define DEST_PORT 25560
#define PAYLOAD_LEN_MAX 768

int main(void)
{
	LOG_INF(WELLCOME_TEXT);

	struct openthread_context* const ot_context = openthread_get_default_context();
	otInstance* const ot_instance = ot_context->instance;

	printf("Started.\n");

	bool thread_ready = false;
	printk("Awaiting Thread readiness.");
	while (!thread_ready)
	{
		openthread_api_mutex_lock(ot_context);
		const otNetifAddress* const ip6_addrs = otIp6GetUnicastAddresses(ot_instance);
		openthread_api_mutex_unlock(ot_context);

		uint8_t addr_count = 0;
		for (const otNetifAddress* if_addr = ip6_addrs;
				if_addr;
				if_addr = if_addr->mNext)
		{
			addr_count++;
		}

		// An uncertain check to see if Thread is ready.
		// printf("IP6 addr count: %d\n", addr_count);
		thread_ready = addr_count >= 4;

		k_msleep(1000);
	}

	printk(" Thread appears ready.\n");

	while (true)
	{
		k_msleep(3000);

		// Get audio.
		const uint16_t* const audio_samples = audio_record_audio();

		// Offload to controller for processing.
		int sock, err;
		struct sockaddr_in6 dest_addr;

		sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
		if (sock < 0)
		{
				printf("Failed to create socket: %d\n", errno);
				failspin();
		}

		dest_addr.sin6_family = AF_INET6;
		dest_addr.sin6_port = htons(DEST_PORT);
		err = inet_pton(AF_INET6, DEST_IP, &dest_addr.sin6_addr);
		if (err <= 0)
		{
			printf("Invalid address: %d\n", err);
			failspin();
		}

		// printf("Sending audio data.\n");
		const uint32_t buffer_byte_len = ((uint32_t) (audio_samples + SAMPLE_BUFFER_LEN))
			- ((uint32_t) audio_samples);
		uint32_t bytes_sent = 0;
		while (bytes_sent < buffer_byte_len)
		{
			const uint32_t bytes_remaining = buffer_byte_len - bytes_sent;
			uint32_t payload_len = bytes_remaining < PAYLOAD_LEN_MAX ? bytes_remaining : PAYLOAD_LEN_MAX;
			// printf("remaining: %d, payload len: %d\n", bytes_remaining, payload_len);
			err = sendto(
				sock,
				(const void*) (((uint8_t* const) audio_samples) + bytes_sent),
				payload_len,
				0,
				(struct sockaddr*) &dest_addr,
				sizeof(dest_addr));
			if (err < 0)
			{
				printf("sendto failed: %d\n", errno);
				break;
			}

			bytes_sent += payload_len;
			// printf("Sent %d B of %d B of audio data.\n", bytes_sent, buffer_byte_len);
		}
		close(sock);
		// printf("Sent %d bytes of audio data.\n", bytes_sent);
	}

	return 0;
}
