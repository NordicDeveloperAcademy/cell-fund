/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <logging/log.h>
#include <modem/lte_lc.h>
#include <dk_buttons_and_leds.h>

/* STEP 4 - Include the header file for the GNSS interface */


static K_SEM_DEFINE(lte_connected, 0, 1);

/* STEP 5.2 - Define the PVT data frame variable */


/* STEP 12.1 - Declare helper variables to find the TTFF */


LOG_MODULE_REGISTER(Lesson6_Exercise1, LOG_LEVEL_INF);

static void lte_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
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
	case LTE_LC_EVT_RRC_UPDATE:
		LOG_INF("RRC mode: %s", evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ? 
				"Connected" : "Idle");
		break;	
	default:
		break;
	}
}

static void modem_configure(void)
{
	int err = lte_lc_init_and_connect_async(lte_handler);
	if (err) {
		LOG_ERR("Modem could not be configured, error: %d", err);
		return;
	}
	LOG_INF("Connecting to LTE network");
	k_sem_take(&lte_connected, K_FOREVER);
	LOG_INF("Connected to LTE network");
	dk_set_led_on(DK_LED2);
}

/* STEP 6 - Define a function to log fix data in a readable format */
static void print_fix_data(struct nrf_modem_gnss_pvt_data_frame *pvt_data)
{

}


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

void main(void)
{

	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LEDs Library");
	}

	modem_configure();
	
	/* STEP 8 - Deactivate LTE and activate GNSS */


	/* STEP 9 - Register the GNSS event handler */


	/* STEP 10 - Set the GNSS fix interval and GNSS fix retry period */


	/* STEP 11 - Start the GNSS receiver*/


	/* STEP 12.2 - Log the current system uptime */

}

