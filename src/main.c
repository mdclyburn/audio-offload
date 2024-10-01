/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>

#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/openthread.h>
#include <zephyr/net/socket.h>
#include <zephyr/usb/usb_device.h>

#include <openthread/ip6.h>
#include <openthread/thread.h>

LOG_MODULE_REGISTER(cli_sample, CONFIG_OT_COMMAND_LINE_INTERFACE_LOG_LEVEL);

#define WELLCOME_TEXT \
	"\n\r"\
	"\n\r"\
	"OpenThread Command Line Interface is now running.\n\r"

#define DEST_IP "fde3:8f5e:f206:2::c0a8:023d"
#define DEST_PORT 25560
#define PAYLOAD_LEN_MAX 768

#define SAMPLE_TIME_SECS 1
#define SAMPLE_FREQ 11025
#define SAMPLE_BUFFER_LEN (SAMPLE_FREQ * SAMPLE_TIME_SECS)

uint16_t SAMPLE_BUFFER[SAMPLE_BUFFER_LEN];

void failspin(void);

int main(void)
{
	LOG_INF(WELLCOME_TEXT);

	struct openthread_context* const ot_context = openthread_get_default_context();
	otInstance* const ot_instance = ot_context->instance;

	printf("Sampler running.\n");

	bool ready = false;
	while (!ready)
	{
		openthread_api_mutex_lock(ot_context);
		const otNetifAddress* const ip6_addrs = otIp6GetUnicastAddresses(ot_instance);
		openthread_api_mutex_unlock(ot_context);

		uint8_t addr_count = 0;
		for (const otNetifAddress* if_addr = ip6_addrs; if_addr; if_addr = if_addr->mNext)
		{
			addr_count++;
		}

		// An uncertain check to see if Thread is ready.
		// printf("IP6 addr count: %d\n", addr_count);
		ready = addr_count == 4;

		k_msleep(1000);
	}

	printf("Thread appears ready.\n");

	while (true)
	{
		// Get audio.

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
		const uint32_t buffer_byte_len = ((uint32_t) (SAMPLE_BUFFER + SAMPLE_BUFFER_LEN))
			- ((uint32_t) SAMPLE_BUFFER);
		uint32_t bytes_sent = 0;
		while (bytes_sent < buffer_byte_len)
		{
			const uint32_t bytes_remaining = buffer_byte_len - bytes_sent;
			uint32_t payload_len = bytes_remaining < PAYLOAD_LEN_MAX ? bytes_remaining : PAYLOAD_LEN_MAX;
			// printf("remaining: %d, payload len: %d\n", bytes_remaining, payload_len);
			err = sendto(
				sock, 
				(const void*) (((uint8_t* const) SAMPLE_BUFFER) + bytes_sent),
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

		k_msleep(5000);
	}

	return 0;
}

void failspin(void)
{
	while (true) {  }
}
