/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
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
#include <modem/lte_lc.h>

#if NCS_VERSION_NUMBER < 0x20600
#include <zephyr/random/rand32.h>
#else 
#include <zephyr/random/random.h>
#endif


/* STEP 2.2 - Include the header file for the CoAP library */


/* STEP 4.1 - Define the macro for the message from the board */


/* STEP 4.2 - Define the macros for the CoAP version and message length */


/* STEP 5 - Declare the buffer coap_buf to receive the response. */


/* STEP 6.1 - Define the CoAP message token next_token */


static int sock;
static struct sockaddr_storage server;

K_SEM_DEFINE(lte_connected, 0, 1);

LOG_MODULE_REGISTER(Lesson5_Exercise1, LOG_LEVEL_INF);

/**@brief Resolves the configured hostname. */
static int server_resolve(void)
{
	int err;
	struct addrinfo *result;
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_DGRAM
	};
	char ipv4_addr[NET_IPV4_ADDR_LEN];

	err = getaddrinfo(CONFIG_COAP_SERVER_HOSTNAME, NULL, &hints, &result);
	if (err != 0) {
		LOG_ERR("ERROR: getaddrinfo failed %d\n", err);
		return -EIO;
	}

	if (result == NULL) {
		LOG_ERR("ERROR: Address not found\n");
		return -ENOENT;
	}

	/* IPv4 Address. */
	struct sockaddr_in *server4 = ((struct sockaddr_in *)&server);

	server4->sin_addr.s_addr =
		((struct sockaddr_in *)result->ai_addr)->sin_addr.s_addr;
	server4->sin_family = AF_INET;
	server4->sin_port = htons(CONFIG_COAP_SERVER_PORT);

	inet_ntop(AF_INET, &server4->sin_addr.s_addr, ipv4_addr,
		  sizeof(ipv4_addr));
	LOG_INF("IPv4 Address found %s\n", ipv4_addr);

	/* Free the address. */
	freeaddrinfo(result);

	return 0;
}

/**@brief Initialize the CoAP client */
static int client_init(void)
{
	int err;

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		LOG_ERR("Failed to create CoAP socket: %d.\n", errno);
		return -errno;
	}

	err = connect(sock, (struct sockaddr *)&server,
		      sizeof(struct sockaddr_in));
	if (err < 0) {
		LOG_ERR("Connect failed : %d\n", errno);
		return -errno;
	}

	LOG_INF("Successfully connected to server");

	/* STEP 6.2 - Generate a random token after the socket is connected */


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

	/* lte_lc_init deprecated in >= v2.6.0 */
	#if NCS_VERSION_NUMBER < 0x20600
	err = lte_lc_init();
	if (err) {
		LOG_ERR("Failed to initialize LTE link control library, error: %d", err);
		return err;
	}
	#endif
	
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

/**@biref Send CoAP GET request. */
static int client_get_send(void)
{
	/* STEP 7.1 - Create the CoAP message*/


	/* STEP 7.2 - Add an option specifying the resource path */


	/* STEP 7.3 - Send the configured CoAP packet */

	return 0;
}

/**@biref Send CoAP PUT request. */
static int client_put_send(void)
{
	int err;
	struct coap_packet request;

	next_token = sys_rand32_get();

	/* STEP 8.1 - Initialize the CoAP packet and append the resource path */


	/* STEP 8.2 - Append the content format as plain text */

	/* STEP 8.3 - Add the payload to the message */


	err = send(sock, request.data, request.offset, 0);
	if (err < 0) {
		LOG_ERR("Failed to send CoAP request, %d\n", errno);
		return -errno;
	}

	LOG_INF("CoAP PUT request sent: Token 0x%04x\n", next_token);

	return 0;
}

/**@brief Handles responses from the remote CoAP server. */
static int client_handle_response(uint8_t *buf, int received)
{
	struct coap_packet reply;
	uint8_t token[8];
	uint16_t token_len;
	const uint8_t *payload;
	uint16_t payload_len;
	uint8_t temp_buf[128];
	/* STEP 9.1 - Parse the received CoAP packet */


	/* STEP 9.2 - Confirm the token in the response matches the token sent */


	/* STEP 9.3 - Retrieve the payload and confirm it's nonzero */


	/* STEP 9.4 - Log the header code, token and payload of the response */

	return 0;
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	/* STEP 10 - Send a GET request or PUT request upon button triggers */

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

	if (client_init() != 0) {
		LOG_INF("Failed to initialize client");
		return 0;
	}

	while (1) {
		/* STEP 11 - Receive response from the CoAP server */


		/* STEP 12 - Parse the received CoAP packet */

	}

	(void)close(sock);

	return 0;
}
