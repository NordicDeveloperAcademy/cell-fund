/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>

/* STEP 4 - Include the header file of the nRF Modem library and the LTE link controller library */

/* STEP 5 - Define the semaphore lte_connected */

LOG_MODULE_REGISTER(Lesson2_Exercise2, LOG_LEVEL_INF);

/* STEP 7 - Define the event handler for LTE link control */
static void lte_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
	/* STEP 7.1 - On changed registration status, print status */

	/* STEP 7.2 - On event RRC update, print RRC mode */

	default:
		break;
	}
}

/* STEP 6 - Define the function modem_configure() to initialize an LTE connection */

int main(void)
{
	int err;

	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LEDs Library");
	}

	/* STEP 8 - Call modem_configure() to initiate the LTE connection */



	/* STEP 9 - Take the semaphore lte_connected */

	LOG_INF("Connected to LTE network");

	/* STEP 10 - Turn on the LED status LED */

	return 0;
}
