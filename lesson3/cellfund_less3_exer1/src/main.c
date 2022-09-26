/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <modem/lte_lc.h>
#include <logging/log.h>
#include <dk_buttons_and_leds.h>

/* STEP 2 - Include the header file for the socket API */


/* STEP 3 - Define the hostname and port for the echo server */


#define MESSAGE_SIZE 256 
#define MESSAGE_TO_SEND "Hello from nRF9160 SiP"
#define SSTRLEN(s) (sizeof(s) - 1)

LOG_MODULE_REGISTER(Lesson3_Exercise1, LOG_LEVEL_INF);
K_SEM_DEFINE(lte_connected, 0, 1);

/* STEP 4 - Declare the structure for the socket and server address */

static uint8_t recv_buf[MESSAGE_SIZE];

/**@brief Resolves the configured hostname. */
static int server_resolve(void)
{
	/* STEP 6.1 - Call getaddrinfo() to get the IP address of the echo server */

	/* STEP 6.2 - Retrieve the relevant information from the result structure*/
	
	/* STEP 6.3 - Convert the address into a string and print it */
	
	/* STEP 6.4 - Free the memory allocated for result */

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

             LOG_INF("Connected to: %s network",
             evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ? "home" : "roaming");
			 k_sem_give(&lte_connected);
             break;
     default:
             break;
     }
}

static int udp_socket_connect(void)
{
	int err;
	/* STEP 7 - Create a UDP socket */

	/* STEP 8 - Connect the socket to the server */

	return 0;
}

static void button_handler(uint32_t button_states, uint32_t has_changed)
{
	switch (has_changed) {
	case DK_BTN1_MSK:
		/* STEP 9 - call send() when button 1 is pressed */

		}
		break;

	}
}

static void modem_configure(void)
{
	int err = lte_lc_init_and_connect_async(lte_handler);
	if (err) {
		LOG_INF("Modem could not be configured, error: %d", err);
		return;
	}
}

void main(void)
{
	int err;
	modem_configure();
	LOG_INF("Connecting to LTE network, this may take several minutes...");
	k_sem_take(&lte_connected, K_FOREVER);	
	err = dk_buttons_init(button_handler);
	if (err){
		LOG_ERR("Failed to initlize the Buttons Library");
	}
	err = dk_leds_init();
	if (err){
		LOG_ERR("Failed to initlize the LEDs Library");
	}
	dk_set_led_on(DK_LED2);	
	if (server_resolve() != 0) {
		LOG_INF("Failed to resolve server name");
		return;
	}
	
	if (udp_socket_connect() != 0) {
		LOG_INF("Failed to initialize client");
		return;
	}
	LOG_INF("Press Button 1 on your Development Kit or Thingy:91 to send your message ");
		while (1) {
			/* STEP 10 - Call recv() to listen to received messages */
			
		}
		(void)close(sock);
}