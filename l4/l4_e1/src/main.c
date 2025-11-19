/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <stdio.h>
#include <ncs_version.h>
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>

#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include <modem/nrf_modem_lib.h>
#include <nrf_modem_at.h>
#include <modem/lte_lc.h>
/* STEP 2.3 - Include the header file for the MQTT helper library*/

LOG_MODULE_REGISTER(Lesson4_Exercise1, LOG_LEVEL_INF);

/* STEP 3 - Define the commands to control and monitor LEDs and buttons */


#define IMEI_LEN	15
#define CGSN_RESPONSE_LENGTH (IMEI_LEN + 6 + 1) /* Add 6 for \r\nOK\r\n and 1 for \0 */
#define CLIENT_ID_LEN sizeof("nrf-") + IMEI_LEN /* \0 included in sizeof() statement */

static K_SEM_DEFINE(lte_connected, 0, 1);

/* STEP 9.2 - Declare the variable to store the client ID */


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

static int modem_configure(void)
{
	int err;

	LOG_INF("Initializing modem library");
	err = nrf_modem_lib_init();
	if (err) {
		LOG_ERR("Failed to initialize the modem library, error: %d", err);
		return err;
	}
	
	LOG_INF("Connecting to LTE network");
	err = lte_lc_connect_async(lte_handler);
	if (err) {
		LOG_ERR("Error in lte_lc_connect_async, error: %d", err);
		return err;
	}

	k_sem_take(&lte_connected, K_FOREVER);
	LOG_INF("Connected to LTE network");
	dk_set_led_on(DK_LED2);

	return 0;
}

static int client_id_get(char * buffer, size_t buffer_len)
{
	/* STEP 9.1 Define the function to generate the client id */
}

/* STEP 4 - Define the function to subscribe to topics */
static void subscribe(void)
{
	int err;

	/* STEP 4.1 - Declare a variable of type mqtt_topic */


	/* STEP 4.2 - Define a subscription list */


	/* STEP 4.3 - Subscribe to topics */

}

/* STEP 5 - Define the function to publish data */
static int publish(uint8_t *data, size_t len)
{
	int err;
	/* STEP 5.1 - Declare and populate a variable of type mqtt_publish_param */	


	/* STEP 5.2 - Publish to MQTT broker */
	

	return 0;
}

/* STEP 6.1 - Define callback handler for CONNACK event */


/* STEP 6.2 - Define callback handler for SUBACK event */

/* STEP 6.3 - Define callback handler for PUBLISH event */


/* STEP 6.4 - Define callback handler for DISCONNECT event */


static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	switch (has_changed) {
	case DK_BTN1_MSK:
		/* STEP 7.2 - Publish message when button 1 is pressed */

		break;
	}
}

int main(void)
{
	int err;

	err = dk_leds_init();
	if (err) {
		LOG_ERR("Failed to initialize the LED library, error: %d", err);
		return 0;
	}

	err = modem_configure();
	if (err) {
		LOG_ERR("Failed to configure the modem, error: %d", err);
		return 0;
	}

	err = dk_buttons_init(button_handler);
	if (err) {
		LOG_ERR("Failed to initialize the buttons library, error: %d", err);
		return 0;
	}

		/* STEP 8 - Initialize the MQTT helper library */

	/* STEP 9.3 - Generate the client ID */

	/* STEP 10 - Establish a connection to the MQTT broker */

}
