/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <dk_buttons_and_leds.h>

/* STEP 4 - Include the header file for the GNSS interface */


/* STEP 5 - Define the PVT data frame variable */


/* STEP 12.1 - Declare helper variables to find the TTFF */


static K_SEM_DEFINE(lte_connected, 0, 1);

LOG_MODULE_REGISTER(Lesson6_Exercise1, LOG_LEVEL_INF);

static int modem_configure(void)
{
	int err;

	LOG_INF("Initializing modem library");

	err = nrf_modem_lib_init();
	if (err) {
		LOG_ERR("Failed to initialize the modem library, error: %d", err);
		return err;
	}

	err = lte_lc_init();
	if (err) {
		LOG_ERR("Failed to initialize LTE Link Controller, error: %d", err);
		return err;
	}

	return 0;
}

/* STEP 6 - Define a function to log fix data in a readable format */


static void gnss_event_handler(int event)
{
	int err;

	switch (event) {
	/* STEP 7.1 - On a PVT event, confirm if PVT data is a valid fix */

	/* STEP 7.2 - Log when the GNSS sleeps and wakes up */
	default:
		break;
	}
}

int main(void)
{
	int err;

	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LEDs Library");
	}

	err = modem_configure();
	if (err) {
		LOG_ERR("Failed to configure the modem");
		return 0;
	}

	/* STEP 8 - Activate only the GNSS stack */


	/* STEP 9 - Register the GNSS event handler */


	/* STEP 10 - Set the GNSS fix interval and GNSS fix retry period */


	/* STEP 11 - Start the GNSS receiver*/


	/* STEP 12.2 - Log the current system uptime */


	return 0;
}
