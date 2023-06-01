/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>

/* STEP 3 - Include the header file for the socket API */


/* STEP 4 - Define the hostname and port for the echo server */


#define MESSAGE_SIZE 256
#define MESSAGE_TO_SEND "Hello from nRF9160 SiP"
#define SSTRLEN(s) (sizeof(s) - 1)

/* STEP 5.1 - Declare the structure for the socket and server address */


/* STEP 5.2 - Declare the buffer for receiving from server */

K_SEM_DEFINE(lte_connected, 0, 1);

LOG_MODULE_REGISTER(Lesson3_Exercise1, LOG_LEVEL_INF);

static int server_resolve(void)
{
	/* STEP 6.1 - Call getaddrinfo() to get the IP address of the echo server */

	/* STEP 6.2 - Retrieve the relevant information from the result structure*/

	/* STEP 6.3 - Convert the address into a string and print it */

	/* STEP 6.4 - Free the memory allocated for result */

	return 0;
}

static int server_connect(void)
{
	int err;
	/* STEP 7 - Create a UDP socket */

	/* STEP 8 - Connect the socket to the server */

	LOG_INF("Successfully connected to server");

	return 0;
}

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

	err = lte_lc_init_and_connect_async(lte_handler);
	if (err) {
		LOG_INF("Modem could not be configured, error: %d", err);
		return err;
	}

	k_sem_take(&lte_connected, K_FOREVER);
	LOG_INF("Connected to LTE network");
	dk_set_led_on(DK_LED2);

	return 0;
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	switch (has_changed) {
	case DK_BTN1_MSK:
		/* STEP 9 - call send() when button 1 is pressed */

	break;
	}
}


int main(void)
{
	int err;
	int received;

	if (dk_leds_init() != 0) {
		LOG_ERR("Failed to initialize the LED library");
	}

	err = modem_configure();
	if (err) {
		LOG_ERR("Failed to configure the modem");
		return 0;
	}

	if (dk_buttons_init(button_handler) != 0) {
		LOG_ERR("Failed to initialize the buttons library");
	}

	if (server_resolve() != 0) {
		LOG_INF("Failed to resolve server name");
		return 0;
	}

	if (server_connect() != 0) {
		LOG_INF("Failed to initialize client");
		return 0;
	}

	LOG_INF("Press button 1 on your DK or Thingy:91 to send your message");

	while (1) {
		/* STEP 10 - Call recv() to listen to received messages */

	}

	(void)close(sock);

	return 0,
}
