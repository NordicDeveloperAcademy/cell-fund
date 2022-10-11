/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <logging/log.h>
#include <dk_buttons_and_leds.h>

/* STEP 4 - Include the header file of the LTE link controller library */

/* STEP 5 - Define the semaphore lte_connected */

LOG_MODULE_REGISTER(Lesson2_Exercise1, LOG_LEVEL_INF);

/* STEP 7 - Define the event handler for LTE link control */ 
static void lte_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
	/* STEP 7.1 - Define the callback function lte_handler()*/ 
	case LTE_LC_EVT_NW_REG_STATUS:
		if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) &&
			(evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING)) {
			break;
		}
		LOG_INF("Network registration status: %s",
				evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ?
				"Connected - home network" : "Connected - roaming");
		k_sem_give(&lte_connected);
		break;
	/* STEP 7.2 - On event RRC update, print RRC mode */
	case LTE_LC_EVT_RRC_UPDATE:
		LOG_INF("RRC mode: %s", evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ? 
				"Connected" : "Idle");
		break;			
	default:
		break;
	}
} 

/* STEP 6 - Define the function modem_configure() to initialize an LTE connection */

void main(void)
{
	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LEDs Library");
	}

	/* STEP 8 - Call modem_configure() to initiate the LTE connection */

	LOG_INF("Connecting to LTE network");

	/* STEP 9 - Take the semaphore lte_connected */

	LOG_INF("Connected to LTE network");

	/* STEP 10 - Turn on the LED status LED */
	dk_set_led_on(DK_LED2);
}
