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

#include <nrf_modem_gnss.h>

#define SLEEP_TIME_MS   60*1000

static K_SEM_DEFINE(lte_connected, 0, 1);
K_SEM_DEFINE(pvt_data_sem, 0, 1);
K_SEM_DEFINE(active_time, 0, 1);

LOG_MODULE_REGISTER(Lesson6_Exercise1, LOG_LEVEL_INF);

static struct nrf_modem_gnss_pvt_data_frame current_pvt;
static struct nrf_modem_gnss_pvt_data_frame last_pvt;

static struct nrf_modem_gnss_nmea_data_frame gnss_nmea_data;

static bool nrf_modem_gnss_fix;

enum gnss_status device_status; 
enum gnss_status{
	status_searching = DK_LED2, 
	status_connected = DK_LED1,
	status_fixed = DK_LED3};

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

static void print_fix_data(struct nrf_modem_gnss_pvt_data_frame *pvt_data)
{
	LOG_INF("Latitude:       %.06f\n", pvt_data->latitude);
	LOG_INF("Longitude:      %.06f\n", pvt_data->longitude);
	LOG_INF("Altitude:       %.01f m\n", pvt_data->altitude);
	LOG_INF("Time (UTC):     %02u:%02u:%02u.%03u\n",
	       pvt_data->datetime.hour,
	       pvt_data->datetime.minute,
	       pvt_data->datetime.seconds,
	       pvt_data->datetime.ms);
}

static void gnss_event_handler(int event)
{
	int err;

	switch (event) {
	case NRF_MODEM_GNSS_EVT_PVT:
		LOG_INF("Searching...");
		err = nrf_modem_gnss_read(&last_pvt, sizeof(last_pvt), NRF_MODEM_GNSS_DATA_PVT);
		if (err) {
			LOG_ERR("nrf_modem_gnss_read failed, err %d", err);
			return;
		}
		if (last_pvt.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID) {
			nrf_modem_gnss_fix = true;
			current_pvt = last_pvt;
			print_fix_data(&current_pvt);
			k_sem_give(&pvt_data_sem);
		} else {
			nrf_modem_gnss_fix = false;
		}
		break;
	case NRF_MODEM_GNSS_EVT_BLOCKED:
		LOG_INF("NRF_MODEM_GNSS_EVT_BLOCKED");
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

	if (nrf_modem_gnss_event_handler_set(gnss_event_handler) != 0) {
		LOG_ERR("Failed to set GNSS event handler");
		return;
	}

	if (nrf_modem_gnss_fix_interval_set(CONFIG_GNSS_PERIODIC_INTERVAL) != 0) {
		LOG_ERR("Failed to set GNSS fix interval");
		return;
	}

	if (nrf_modem_gnss_fix_retry_set(CONFIG_GNSS_PERIODIC_TIMEOUT) != 0) {
		LOG_ERR("Failed to set GNSS fix retry");
		return;
	}

	LOG_INF("Starting GNSS");
	if (nrf_modem_gnss_start() != 0) {
		LOG_ERR("Failed to start GNSS");
		return;
	}

	while (1) {
		k_sem_take(&pvt_data_sem, K_FOREVER);

		k_msleep(SLEEP_TIME_MS);

		}
}

