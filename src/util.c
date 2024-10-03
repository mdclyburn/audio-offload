#include <zephyr/kernel.h>

void failspin(void)
{
	while (true) { k_msleep(1000); }
}