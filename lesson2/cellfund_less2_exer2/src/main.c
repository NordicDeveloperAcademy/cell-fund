/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <dk_buttons_and_leds.h>
#include <logging/log.h>
/* STEP 4 - Include the header file of the LTE link controller library */

LOG_MODULE_REGISTER(Lesson2_Exercise1, LOG_LEVEL_INF);
#define REGISTERED_STATUS_LED          DK_LED2

/* STEP 5 - Define the semaphore lte_connected */

/* STEP 7 - Define the callback function lte_handler() */ 

/* STEP 6 - Define the function modem_configure() to initialize an LTE connection */

void main(void)
{
	int ret = dk_leds_init();
	if (ret){
		LOG_ERR("Failed to initlize the LEDs Library");
	}
	/* STEP 8 - Call modem_configure() to initiate the LTE connection */

	LOG_INF("Connecting to LTE network, this may take several minutes...");

	/* STEP 9 - Take the semaphore lte_connected */

	//We only reach here if we are registered to a network
	LOG_INF("Connected to LTE network");
	dk_set_led_on(REGISTERED_STATUS_LED);
}
