/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <modem/lte_lc.h>
#include <logging/log.h>
#include <dk_buttons_and_leds.h>


#define MESSAGE_SIZE 256 
#define SERVER_HOSTNAME "nordicecho.westeurope.cloudapp.azure.com"
#define SERVER_PORT "2555"
#define MESSAGE_TO_SEND "Hello from nRF9160 SiP"
#define SSTRLEN(s) (sizeof(s) - 1)

LOG_MODULE_REGISTER(Lesson3_Exercise1, LOG_LEVEL_INF);
K_SEM_DEFINE(lte_connected, 0, 1);

static int sock;
static struct sockaddr_storage server;
static uint8_t recv_buf[MESSAGE_SIZE];

/**@brief Resolves the configured hostname. */
static int server_resolve(void)
{
	int err;
	struct sockaddr_in *server4 = ((struct sockaddr_in *)&server);
	struct addrinfo *result;
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM
	};
	

	err = getaddrinfo(SERVER_HOSTNAME, SERVER_PORT, &hints, &result);
	if (err != 0) {
		LOG_INF("ERROR: getaddrinfo failed %d", err);
		return -EIO;
	}

	if (result == NULL) {
		LOG_INF("ERROR: Address not found");
		return -ENOENT;
	} 	
	server4->sin_addr.s_addr =
		((struct sockaddr_in *)result->ai_addr)->sin_addr.s_addr;
	server4->sin_family = AF_INET;
	server4->sin_port = ((struct sockaddr_in *)result->ai_addr)->sin_port;
	
	/* Convert the network address structure  in server4->sin_addr.s_addr into 
	a character string in ipv4_addr to print on the console */
	char ipv4_addr[NET_IPV4_ADDR_LEN];
	inet_ntop(AF_INET, &server4->sin_addr.s_addr, ipv4_addr,
		  sizeof(ipv4_addr));
	LOG_INF("IPv4 Address found %s", ipv4_addr);
	
	/* Free the address. */
	freeaddrinfo(result);

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

static int tcp_socket_conent(void)
{
	int err;
	//Create a stream socket in the IPv4 family that uses TCP. 
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		LOG_ERR("Failed to create socket: %d.", errno);
		return -errno;
	}

	err = connect(sock, (struct sockaddr *)&server,
		      sizeof(struct sockaddr_in));
	if (err < 0) {
		LOG_ERR("Connect failed : %d", errno);
		return -errno;
	}

	return 0;
}

static void button_handler(uint32_t button_states, uint32_t has_changed)
{
	switch (has_changed) {
	case DK_BTN1_MSK:
		if (button_states & DK_BTN1_MSK){	
			int err = send(sock, MESSAGE_TO_SEND, SSTRLEN(MESSAGE_TO_SEND), 0);
			if (err < 0) {
				LOG_INF("Failed to send message, %d", errno);
				return;	
			}
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
	for (;;){
		if (tcp_socket_conent() != 0) {
			LOG_INF("Failed to initialize client");
			return;
		}
		while (1) {
			// This is a blocking function 
			int len = recv(sock, recv_buf, sizeof(recv_buf) - 1, 0);

			if (len < 0) {
				LOG_ERR("Error reading response\n");
				return;
			}

			if (len == 0) {
				//TCP Connection closed by the server
				break;
			}

			recv_buf[len] = 0;
			LOG_INF("Data received from the server:\n%s", recv_buf);
			
		}
		(void)close(sock);
	}
}