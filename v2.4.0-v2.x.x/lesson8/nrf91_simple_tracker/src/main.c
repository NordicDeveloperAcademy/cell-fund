/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clausee
 * Th
 */

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/net/coap.h>
#include <zephyr/net/socket.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <zephyr/random/rand32.h>
#include <zephyr/net/tls_credentials.h>
#include <modem/modem_key_mgmt.h>
#include <dk_buttons_and_leds.h>
#include <nrf_modem_gnss.h>

#define SEC_TAG 12
#define APP_COAP_SEND_INTERVAL_MS 60000
#define APP_COAP_MAX_MSG_LEN 1280
#define APP_COAP_VERSION 1
static int sock;
static struct sockaddr_storage server;
static uint16_t next_token;
K_SEM_DEFINE(lte_connected, 0, 1);
K_SEM_DEFINE(gnss_fix_sem, 0, 1);
LOG_MODULE_REGISTER(Cellfund_Project, LOG_LEVEL_INF);
static uint8_t coap_buf[APP_COAP_MAX_MSG_LEN];
static uint8_t coap_sendbug[64];
static struct nrf_modem_gnss_pvt_data_frame current_pvt;
static struct nrf_modem_gnss_pvt_data_frame last_pvt;
static enum tracker_status {status_nolte = DK_LED1, status_searching = DK_LED2, status_fixed = DK_LED3} device_status;
static int resolve_address_lock = 0;
static void print_fix_data(struct nrf_modem_gnss_pvt_data_frame *pvt_data)
{
	printk("Latitude:       %.06f\n", pvt_data->latitude);
	printk("Longitude:      %.06f\n", pvt_data->longitude);
	printk("Altitude:       %.01f m\n", pvt_data->altitude);
	printk("Time (UTC):     %02u:%02u:%02u.%03u\n",
	       pvt_data->datetime.hour,
	       pvt_data->datetime.minute,
	       pvt_data->datetime.seconds,
	       pvt_data->datetime.ms);
}

static void gnss_event_handler(int event)
{
	int retval;
	switch (event) {
	case NRF_MODEM_GNSS_EVT_PVT:
		LOG_INF("Searching for GNSS Satellites....\n\r");
		device_status = status_searching;
		break;
	case NRF_MODEM_GNSS_EVT_FIX:
		LOG_INF("GNSS fix event\n\r");
		break;
	case NRF_MODEM_GNSS_EVT_PERIODIC_WAKEUP:
		LOG_INF("GNSS woke up in periodic mode\n\r");
		break;
	case NRF_MODEM_GNSS_EVT_BLOCKED:
		LOG_INF("GNSS is blocked by LTE event\n\r");
		break;
	case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_FIX:
		LOG_INF("GNSS enters sleep because fix was achieved in periodic mode\n\r");
		device_status = status_fixed;
		retval = nrf_modem_gnss_read(&last_pvt, sizeof(last_pvt), NRF_MODEM_GNSS_DATA_PVT);
		if (retval == 0){
			current_pvt = last_pvt;
			print_fix_data(&current_pvt);
			k_sem_give(&gnss_fix_sem);
		}
		break;
	case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_TIMEOUT:
		LOG_INF("GNSS enters sleep because fix retry timeout was reached\n\r");
		break;

	default:

		break;
	}
}

static int gnss_init_and_start(void)
{
#if defined(CONFIG_GNSS_HIGH_ACCURACY_TIMING_SOURCE)
	if (nrf_modem_gnss_timing_source_set(NRF_MODEM_GNSS_TIMING_SOURCE_TCXO)){
		LOG_ERR("Failed to set TCXO timing source");
		return -1;
	}
#endif
#if defined(CONFIG_GNSS_LOW_ACCURACY) || defined (CONFIG_BOARD_THINGY91_NRF9160_NS)
	uint8_t use_case;
	use_case = NRF_MODEM_GNSS_USE_CASE_MULTIPLE_HOT_START | NRF_MODEM_GNSS_USE_CASE_LOW_ACCURACY;
	if (nrf_modem_gnss_use_case_set(use_case) != 0) {
		LOG_ERR("Failed to set low accuracy use case");
		return -1;
	}
#endif
	/* Configure GNSS event handler . */
	if (nrf_modem_gnss_event_handler_set(gnss_event_handler) != 0) {
		LOG_ERR("Failed to set GNSS event handler");
		return -1;
	}

	if (nrf_modem_gnss_fix_interval_set(CONFIG_TRACKER_PERIODIC_INTERVAL) != 0) {
		LOG_ERR("Failed to set GNSS fix interval");
		return -1;
	}

	if (nrf_modem_gnss_fix_retry_set(CONFIG_TRACKER_PERIODIC_TIMEOUT) != 0) {
		LOG_ERR("Failed to set GNSS fix retry");
		return -1;
	}

	if (nrf_modem_gnss_start() != 0) {
		LOG_ERR("Failed to start GNSS");
		return -1;
	}
	if (nrf_modem_gnss_prio_mode_enable() != 0){
		LOG_ERR("Error setting GNSS priority mode");
		return -1;
	}
	return 0;
}

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
static int server_connect(void)
{
	int err;

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_DTLS_1_2);
	if (sock < 0) {
		LOG_ERR("Failed to create CoAP socket: %d.\n", errno);
		return -errno;
	}

	int verify;
	sec_tag_t sec_tag_list[] = { 12 };

	enum {
		NONE = 0,
		OPTIONAL = 1,
		REQUIRED = 2,
	};

	verify = REQUIRED;

	err = setsockopt(sock, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));
	if (err) {
		LOG_ERR("Failed to setup peer verification, errno %d\n", errno);
		return -errno;
	}

	err = setsockopt(sock, SOL_TLS, TLS_HOSTNAME, CONFIG_COAP_SERVER_HOSTNAME,
		 strlen(CONFIG_COAP_SERVER_HOSTNAME));
	if (err) {
		LOG_ERR("Failed to setup TLS hostname (%s), errno %d\n",
			CONFIG_COAP_SERVER_HOSTNAME, errno);
		return -errno;
	}

	err = setsockopt(sock, SOL_TLS, TLS_SEC_TAG_LIST, sec_tag_list,
			 sizeof(sec_tag_t) * ARRAY_SIZE(sec_tag_list));
	if (err) {
		LOG_ERR("Failed to setup socket security tag, errno %d\n", errno);
		return -errno;
	}

	err = connect(sock, (struct sockaddr *)&server,
		      sizeof(struct sockaddr_in));
	if (err < 0) {
		LOG_ERR("Connect failed : %d\n", errno);
		return -errno;
	}

	/* Randomize token. */
	next_token = sys_rand32_get();

	return 0;
}

/**@brief Handles responses from the remote CoAP server. */
static int client_handle_get_response(uint8_t *buf, int received)
{
	int err;
	struct coap_packet reply;
	const uint8_t *payload;
	uint16_t payload_len;
	uint8_t token[8];
	uint16_t token_len;
	static uint8_t temp_buf[128];

	err = coap_packet_parse(&reply, buf, received, NULL, 0);
	if (err < 0) {
		LOG_ERR("Malformed response received: %d\n", err);
		return err;
	}

	payload = coap_packet_get_payload(&reply, &payload_len);
	token_len = coap_header_get_token(&reply, token);

	if ((token_len != sizeof(next_token)) ||
	    (memcmp(&next_token, token, sizeof(next_token)) != 0)) {
		LOG_ERR("Invalid token received: 0x%02x%02x\n",
		       token[1], token[0]);
		return 0;
	}

	if (payload_len > 0) {
		snprintf(temp_buf, MIN(payload_len + 1, sizeof(temp_buf)), "%s", payload);
	} else {
		strcpy(temp_buf, "EMPTY");
	}

	LOG_INF("CoAP response: Code 0x%x, Token 0x%02x%02x, Payload: %s",
	       coap_header_get_code(&reply), token[1], token[0], temp_buf);
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

		LOG_INF("Network registration status: %s\n",
			evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ?
			"Connected - home network" : "Connected - roaming\n");
		k_sem_give(&lte_connected);
		break;
	case LTE_LC_EVT_PSM_UPDATE:
		LOG_INF("PSM parameter update: TAU: %d, Active time: %d\n",
			evt->psm_cfg.tau, evt->psm_cfg.active_time);
		break;
	case LTE_LC_EVT_EDRX_UPDATE: {
		char log_buf[60];
		ssize_t len;

		len = snprintf(log_buf, sizeof(log_buf),
			       "eDRX parameter update: eDRX: %f, PTW: %f\n",
			       evt->edrx_cfg.edrx, evt->edrx_cfg.ptw);
		if (len > 0) {
			LOG_INF("%s\n", log_buf);
		}
		break;
	}
	case LTE_LC_EVT_RRC_UPDATE:
		LOG_INF("RRC mode: %s\n",
			evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ?
			"Connected" : "Idle\n");
		break;
	case LTE_LC_EVT_CELL_UPDATE:
		LOG_INF("LTE cell changed: Cell ID: %d, Tracking area: %d\n",
		       evt->cell.id, evt->cell.tac);
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

	err = modem_key_mgmt_write(SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_IDENTITY, CONFIG_COAP_DEVICE_NAME, strlen(CONFIG_COAP_DEVICE_NAME));
	if (err) {
		LOG_ERR("Failed to write identity: %d\n", err);
		return err;
	}

	err = modem_key_mgmt_write(SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_PSK, CONFIG_COAP_SERVER_PSK, strlen(CONFIG_COAP_SERVER_PSK));
	if (err) {
		LOG_ERR("Failed to write identity: %d\n", err);
		return err;
	}


	err = lte_lc_init_and_connect_async(lte_handler);
	if (err) {
		LOG_ERR("Modem could not be configured, error: %d", err);
		return err;
	}

	/* Decativate LTE and enable GNSS. */
	err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_DEACTIVATE_LTE);
	if (err) {
		LOG_ERR("Failed to decativate LTE and enable GNSS functional mode");
		return err;
	}

	return 0;
}


static int client_post_send(void)
{
	int err,ret;
	struct coap_packet request;

	next_token++;

	err = coap_packet_init(&request, coap_buf, sizeof(coap_buf),
			       APP_COAP_VERSION, COAP_TYPE_CON,
			       sizeof(next_token), (uint8_t *)&next_token,
			       COAP_METHOD_POST, coap_next_id());
	if (err < 0) {
		LOG_ERR("Failed to create CoAP request, %d\n", err);
		return err;
	}

	err = coap_packet_append_option(&request, COAP_OPTION_URI_PATH,
					(uint8_t *)CONFIG_COAP_POST_RESOURCE,
					strlen(CONFIG_COAP_POST_RESOURCE));
	if (err < 0) {
		LOG_ERR("Failed to encode CoAP option, %d\n", err);
		return err;
	}

   err = coap_append_option_int(&request, COAP_OPTION_CONTENT_FORMAT,
                                COAP_CONTENT_FORMAT_TEXT_PLAIN);
   if (err < 0) {
      LOG_ERR("Failed to encode CoAP CONTENT_FORMAT option, %d", err);
      return err;
   }

   err = coap_packet_append_option(&request, COAP_OPTION_URI_QUERY,
                                   (uint8_t *)"keep",
                                   strlen("keep"));
   if (err < 0) {
      LOG_ERR("Failed to encode CoAP URI-QUERY option 'keep', %d", err);
      return err;
   }

	err = coap_packet_append_payload_marker(&request);
	if (err < 0) {
		LOG_ERR("Failed to append payload marker, %d\n", err);
		return err;
	}

   ret = snprintf (coap_sendbug, sizeof(coap_sendbug), "%.06f,%.06f\n%.01f m\n%04u-%02u-%02u %02u:%02u:%02u",
   current_pvt.latitude, current_pvt.longitude,current_pvt.accuracy,current_pvt.datetime.year,current_pvt.datetime.month,current_pvt.datetime.day,
   current_pvt.datetime.hour, current_pvt.datetime.minute, last_pvt.datetime.seconds);
	if (err < 0) {
		LOG_ERR("snprintf failed to format string, %d\n", err);
		return err;
	}
	err = coap_packet_append_payload(&request, (uint8_t *)coap_sendbug, ret);
	if (err < 0) {
		LOG_ERR("Failed to append payload, %d\n", err);
		return err;
	}

	err = send(sock, request.data, request.offset, 0);
	if (err < 0) {
		LOG_ERR("Failed to send CoAP request, %d\n", errno);
		return -errno;
	}

	LOG_INF("CoAP request sent: token 0x%04x\n", next_token);

	return 0;
}
static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	static bool toogle = 1;
	if (has_changed & DK_BTN1_MSK && button_state & DK_BTN1_MSK) {
		if (toogle ==1){
			dk_set_led_on(device_status);
		}else{
			dk_set_led_off(DK_LED1);
			dk_set_led_off(DK_LED2);
			dk_set_led_off(DK_LED3);
		}
		toogle = !toogle;
	}
}

int main(void)
{
	int err;
	int received;
	LOG_INF("The nRF91 Simple Tracker Version %d.%d.%d started\n",CONFIG_TRACKER_VERSION_MAJOR,CONFIG_TRACKER_VERSION_MINOR,CONFIG_TRACKER_VERSION_PATCH);

	err = dk_leds_init();
	if (err){
		LOG_ERR("Failed to initlize the LEDs Library");
	}

	device_status = status_nolte;

	err = modem_configure();
	if (err) {
		LOG_ERR("Failed to configure the modem");
		return 0;
	}

	err = dk_buttons_init(button_handler);
	if (err) {
		LOG_ERR("Failed to initlize button handler: %d\n", err);
		return 0;
	}

	LOG_INF("Starting GNSS....");
	gnss_init_and_start();

	while (1) {
		k_sem_take(&gnss_fix_sem, K_FOREVER);
		err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_NORMAL);
		if (err != 0){
			LOG_ERR("Failed to activate LTE");
			break;
		}
		k_sem_take(&lte_connected, K_FOREVER);
		if (resolve_address_lock == 0){
			LOG_INF("Resolving the server address\n\r");
			if (server_resolve() != 0) {
				LOG_ERR("Failed to resolve server name\n");
					return 0;
			}
			resolve_address_lock = 1;
		}

		LOG_INF("Sending Data over LTE\r\n");
		if (server_connect() != 0) {
			LOG_ERR("Failed to initialize CoAP client\n");
			return 0;
		}

		if (client_post_send() != 0) {
			LOG_ERR("Failed to send GET request, exit...\n");
			break;
		}

		received = recv(sock, coap_buf, sizeof(coap_buf), 0);
		if (received < 0) {
			LOG_ERR("Error reading response\n");
			break;
		} else if (received == 0) {
			LOG_ERR("Disconnected\n");
			break;
		}

		err = client_handle_get_response(coap_buf, received);
		if (err < 0) {
			LOG_ERR("Invalid response, exit...\n");
			break;
		}

		(void)close(sock);

		err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_DEACTIVATE_LTE);
		if (err != 0){
			LOG_ERR("Failed to decativate LTE and enable GNSS functional mode");
			break;
		}
	}

	device_status = status_nolte;
	LOG_ERR("Error occoured. Shutting down modem");
	(void)lte_lc_power_off();
	(void)nrf_modem_lib_shutdown();

	return 0;
}
