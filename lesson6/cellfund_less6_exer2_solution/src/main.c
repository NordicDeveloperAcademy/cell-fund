/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <string.h>

#include <logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>

#include <modem/lte_lc.h>
#include <dk_buttons_and_leds.h>
#include <nrf_modem_gnss.h>

#define SERVER_HOSTNAME "nordicecho.westeurope.cloudapp.azure.com"
#define SERVER_PORT "2444"

#define MESSAGE_SIZE 256 
#define MESSAGE_TO_SEND "Hello"

static struct nrf_modem_gnss_pvt_data_frame pvt_data;
static int sock;
static struct sockaddr_storage server;
static uint8_t recv_buf[MESSAGE_SIZE];
static uint8_t send_buf[MESSAGE_SIZE];

static int64_t gnss_start_time;
static bool first_fix = false;

LOG_MODULE_REGISTER(Lesson6_Exercise2, LOG_LEVEL_INF);

static K_SEM_DEFINE(lte_connected, 0, 1);
static K_SEM_DEFINE(pvt_data_sem, 0, 1);
static K_SEM_DEFINE(active_time, 0, 1);

static int server_resolve(void)
{
	int err;
	struct addrinfo *result;
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_DGRAM
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

	struct sockaddr_in *server4 = ((struct sockaddr_in *)&server);
	server4->sin_addr.s_addr =
		((struct sockaddr_in *)result->ai_addr)->sin_addr.s_addr;
	server4->sin_family = AF_INET;
	server4->sin_port = ((struct sockaddr_in *)result->ai_addr)->sin_port;
	
	char ipv4_addr[NET_IPV4_ADDR_LEN];
	inet_ntop(AF_INET, &server4->sin_addr.s_addr, ipv4_addr,
		  sizeof(ipv4_addr));
	LOG_INF("IPv4 Address found %s", ipv4_addr);
	
	freeaddrinfo(result);

	return 0;
}

static int client_init(void)
{
	int err;
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
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
	case LTE_LC_EVT_PSM_UPDATE:
		LOG_INF("PSM parameter update: TAU: %d, Active time: %d",
			evt->psm_cfg.tau, evt->psm_cfg.active_time);
		if (evt->psm_cfg.active_time == -1){
			LOG_ERR("Network rejected PSM parameters. Failed to setup network");
		} else {
			k_sem_give(&active_time);
		}
		break;
	case LTE_LC_EVT_EDRX_UPDATE:
		LOG_INF("eDRX parameter update: eDRX: %f, PTW: %f",
			evt->edrx_cfg.edrx, evt->edrx_cfg.ptw);
		break;	
	case LTE_LC_EVT_RRC_UPDATE:
		LOG_INF("RRC mode: %s", 
				evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ? 
				"Connected" : "Idle");
		break;		
	default:
		break;
	}
}

static void modem_configure(void)
{
	int err;
	
	err = lte_lc_psm_req(true);
	if (err) {
		LOG_ERR("lte_lc_psm_req, error: %d", err);
	} 

	LOG_INF("Connecting to LTE network");

	err = lte_lc_init_and_connect_async(lte_handler);
	if (err) {
		LOG_ERR("Modem could not be configured, error: %d", err);
		return;
	}
	k_sem_take(&lte_connected, K_FOREVER);
	LOG_INF("Connected to LTE network");
	dk_set_led_on(DK_LED2);
}

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
	
	int err = snprintf(send_buf, MESSAGE_SIZE, "Latitude: %.06f, Longitude: %.06f", pvt_data->latitude, pvt_data->longitude);		
	if (err < 0) {
		LOG_ERR("Failed to print to buffer: %d", err);
	}   
}

static void gnss_event_handler(int event)
{
	int err;

	switch (event) {
	case NRF_MODEM_GNSS_EVT_PVT:
		dk_set_led_off(DK_LED1);
		LOG_INF("Searching...");
		err = nrf_modem_gnss_read(&pvt_data, sizeof(pvt_data), NRF_MODEM_GNSS_DATA_PVT);
		if (err) {
			LOG_ERR("nrf_modem_gnss_read failed, err %d", err);
			return;
		}
		if (pvt_data.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID) {
			print_fix_data(&pvt_data);
			if (!first_fix) {
				first_fix = true;
				dk_set_led_on(DK_LED1);
				LOG_INF("Time to first fix: %2.1lld s", (k_uptime_get() - gnss_start_time)/1000);
			}
		}
		break;
	default:
		break;
	}
}

static int gnss_init_and_start(void)
{
	
	if (lte_lc_func_mode_set(LTE_LC_FUNC_MODE_NORMAL) != 0) {
		LOG_ERR("Failed to activate GNSS functional mode");
		return -1;
	}

	if (nrf_modem_gnss_event_handler_set(gnss_event_handler) != 0) {
		LOG_ERR("Failed to set GNSS event handler");
		return -1;
	}

	if (nrf_modem_gnss_fix_interval_set(CONFIG_GNSS_PERIODIC_INTERVAL) != 0) {
		LOG_ERR("Failed to set GNSS fix interval");
		return -1;
	}

	if (nrf_modem_gnss_fix_retry_set(CONFIG_GNSS_PERIODIC_TIMEOUT) != 0) {
		LOG_ERR("Failed to set GNSS fix retry");
		return -1;
	}

	if (nrf_modem_gnss_start() != 0) {
		LOG_ERR("Failed to start GNSS");
		return -1;
	}
	LOG_INF("GNSS succesfully started");
	gnss_start_time = k_uptime_get();
	return 0;
}

static void button_handler(uint32_t button_state, uint32_t has_changed) 
{
	switch (has_changed) {
	case DK_BTN1_MSK:
		if (button_state & DK_BTN1_MSK){	
			int err = send(sock, &send_buf, sizeof(send_buf), 0);
			LOG_INF("Message sent to server: %d", send_buf);
			if (err < 0) {
				LOG_INF("Failed to send message, %d", errno);
				return;	
			}
		}
		break;

	}
}

void main(void)
{

	int err, received;
	
	err = dk_leds_init();
	if (err){
		LOG_ERR("Failed to initialize the LEDs Library");
	}
	modem_configure();

	err = dk_buttons_init(button_handler);

	if (server_resolve() != 0) {
		LOG_ERR("Failed to resolve server name");
		return;
	}

	if (client_init() != 0) {
		LOG_ERR("Failed to initialize client");
		return;
	}
	if (gnss_init_and_start() != 0) {
		LOG_ERR("Failed to initialize and start GNSS");
		return;
	}

	while (1) {
			received = recv(sock, recv_buf, sizeof(recv_buf) - 1, 0);

			if (received < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					LOG_ERR("Socket EAGAIN");
					continue;				
				} else {
					LOG_ERR("Socket error: %d, exit", errno);
				}
				break;
			}

			if (received == 0) {
				break;
			}

			recv_buf[received] = 0;
			LOG_INF("Data received from the server: (%s)", recv_buf);

		}
}

