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
#include <nrf_modem_gnss.h>

static K_SEM_DEFINE(lte_connected, 0, 1);

/* STEP 5.1 - Define the semaphore pvt_data_sem */
static K_SEM_DEFINE(pvt_data_sem, 0, 1);

/* STEP 5.2 - Define the PVT data frame variable */
static struct nrf_modem_gnss_pvt_data_frame pvt_data;

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
}

/* STEP 6 - Define a function to log fix data in a readable format */
static void print_fix_data(struct nrf_modem_gnss_pvt_data_frame *pvt_data)
{
	LOG_INF("Latitude:       %.06f", pvt_data->latitude);
	LOG_INF("Longitude:      %.06f", pvt_data->longitude);
	LOG_INF("Altitude:       %.01f m", pvt_data->altitude);
	LOG_INF("Time (UTC):     %02u:%02u:%02u.%03u",
	       pvt_data->datetime.hour,
	       pvt_data->datetime.minute,
	       pvt_data->datetime.seconds,
	       pvt_data->datetime.ms);
}


static void gnss_event_handler(int event)
{
	int err;

	switch (event) {
	/* STEP 7 - On a PVT event, confirm of PVT data is a valid fix */
	case NRF_MODEM_GNSS_EVT_PVT:
		LOG_INF("Searching...");
		err = nrf_modem_gnss_read(&pvt_data, sizeof(pvt_data), NRF_MODEM_GNSS_DATA_PVT);
		if (err) {
			LOG_ERR("nrf_modem_gnss_read failed, err %d", err);
			return;
		}
		if (pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID) {
			print_fix_data(&pvt_data);
			k_sem_give(&pvt_data_sem);
		}
		break;
	default:
		break;
	}
}	

void main(void)
{

	int err;
	
	err = dk_leds_init();
	if (err){
		LOG_ERR("Failed to initialize the LEDs Library");
	}

	modem_configure();
	LOG_INF("Connecting to LTE network, this may take several minutes...");
	k_sem_take(&lte_connected, K_FOREVER);
	LOG_INF("Connected to LTE network");
	dk_set_led_on(DK_LED2);	
	
	/* STEP 8 - Deactivate LTE and activate GNSS */
	LOG_INF("Deactivating LTE");
	if (lte_lc_func_mode_set(LTE_LC_FUNC_MODE_DEACTIVATE_LTE) != 0) {
		LOG_ERR("Failed to activate GNSS functional mode");
		return;
	}

	LOG_INF("Activating GNSS");
	if (lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_GNSS) != 0) {
		LOG_ERR("Failed to activate GNSS functional mode");
		return;
	}

	/* STEP 9 - Register the GNSS event handler */
	if (nrf_modem_gnss_event_handler_set(gnss_event_handler) != 0) {
		LOG_ERR("Failed to set GNSS event handler");
		return;
	}

	/* STEP 10 - Set the GNSS fix interval and GNSS fix retry period */
	if (nrf_modem_gnss_fix_interval_set(CONFIG_GNSS_PERIODIC_INTERVAL) != 0) {
		LOG_ERR("Failed to set GNSS fix interval");
		return;
	}

	if (nrf_modem_gnss_fix_retry_set(CONFIG_GNSS_PERIODIC_TIMEOUT) != 0) {
		LOG_ERR("Failed to set GNSS fix retry");
		return;
	}

	/* STEP 11 - Start the GNSS receiver*/
	LOG_INF("Starting GNSS");
	if (nrf_modem_gnss_start() != 0) {
		LOG_ERR("Failed to start GNSS");
		return;
	}

	while (1) {
		/* STEP 12.1 - Take the pvt_data_sem semaphore */
		k_sem_take(&pvt_data_sem, K_FOREVER);

		/* STEP 12.2 - Sleep for 1 minute */
		k_sleep(K_SECONDS(60));
		}
}

