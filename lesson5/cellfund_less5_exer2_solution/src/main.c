/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/net/coap.h>
#include <zephyr/net/socket.h>
#include <modem/lte_lc.h>
#include <zephyr/random/rand32.h>
#include <dk_buttons_and_leds.h>

#include <zephyr/net/tls_credentials.h>
#include <modem/modem_key_mgmt.h>
#define SEC_TAG 12
#define RX_CHECK_INTERVAL 6500
#define APP_COAP_MAX_MSG_LEN 1280
#define APP_COAP_VERSION 1
#define MESSAGE_TO_SEND "Hello from nRF9160 SiP"
static int sock;
static struct sockaddr_storage server;
static uint16_t next_token;
K_SEM_DEFINE(lte_connected, 0, 1);

static uint8_t coap_buf[APP_COAP_MAX_MSG_LEN];
LOG_MODULE_REGISTER(Lesson5_Exercise1, LOG_LEVEL_INF);
static struct k_work_delayable rx_work;

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
		printk("ERROR: getaddrinfo failed %d\n", err);
		return -EIO;
	}

	if (result == NULL) {
		printk("ERROR: Address not found\n");
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
	printk("IPv4 Address found %s\n", ipv4_addr);

	/* Free the address. */
	freeaddrinfo(result);

	return 0;
}

/**@brief Initialize the CoAP client */
static int client_init(void)
{
	int err;

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_DTLS_1_2);
	if (sock < 0) {
		printk("Failed to create CoAP socket: %d.\n", errno);
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
		printk("Failed to setup peer verification, errno %d\n", errno);
		return -errno;
	}

	err = setsockopt(sock, SOL_TLS, TLS_HOSTNAME, CONFIG_COAP_SERVER_HOSTNAME,
		 strlen(CONFIG_COAP_SERVER_HOSTNAME));
	if (err) {
		printk("Failed to setup TLS hostname (%s), errno %d\n",
			CONFIG_COAP_SERVER_HOSTNAME, errno);
		return -errno;
	}

	printk("Setting up TLS credentials, tag %d. Size: %d\n", 12, ARRAY_SIZE(sec_tag_list));
	err = setsockopt(sock, SOL_TLS, TLS_SEC_TAG_LIST, sec_tag_list,
			 sizeof(sec_tag_t) * ARRAY_SIZE(sec_tag_list));
	if (err) {
		printk("Failed to setup socket security tag, errno %d\n", errno);
		return -errno;
	}
	
	err = connect(sock, (struct sockaddr *)&server,
		      sizeof(struct sockaddr_in));
	if (err < 0) {
		printk("Connect failed : %d\n", errno);
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
	uint8_t temp_buf[128];

	err = coap_packet_parse(&reply, buf, received, NULL, 0);
	if (err < 0) {
		printk("Malformed response received: %d\n", err);
		return err;
	}

	payload = coap_packet_get_payload(&reply, &payload_len);
	token_len = coap_header_get_token(&reply, token);

	if ((token_len != sizeof(next_token)) ||
	    (memcmp(&next_token, token, sizeof(next_token)) != 0)) {
		printk("Invalid token received: 0x%02x%02x\n",
		       token[1], token[0]);
		return 0;
	}

	if (payload_len > 0) {
		snprintf(temp_buf, MIN(payload_len + 1, sizeof(temp_buf)), "%s", payload);
	} else {
		strcpy(temp_buf, "EMPTY");
	}

	printk("CoAP response: code: 0x%x, token 0x%02x%02x, payload: %s\n",
	       coap_header_get_code(&reply), token[1], token[0], temp_buf);

	return 0;
}

/**@biref Send CoAP GET request. */
static int client_get_send(void)
{
	int err;
	struct coap_packet request;

	next_token++;

	err = coap_packet_init(&request, coap_buf, sizeof(coap_buf),
			       APP_COAP_VERSION, COAP_TYPE_NON_CON,
			       sizeof(next_token), (uint8_t *)&next_token,
			       COAP_METHOD_GET, coap_next_id());
	if (err < 0) {
		printk("Failed to create CoAP request, %d\n", err);
		return err;
	}

	err = coap_packet_append_option(&request, COAP_OPTION_URI_PATH,
					(uint8_t *)CONFIG_COAP_RX_RESOURCE,
					strlen(CONFIG_COAP_RX_RESOURCE));
	if (err < 0) {
		printk("Failed to encode CoAP option, %d\n", err);
		return err;
	}

	err = send(sock, request.data, request.offset, 0);
	if (err < 0) {
		printk("Failed to send CoAP request, %d\n", errno);
		return -errno;
	}

	printk("CoAP request sent: token 0x%04x\n", next_token);

	return 0;
}


static void modem_configure(void)
{
	int err = lte_lc_init_and_connect_async(lte_handler);
	if (err) {
		LOG_INF("Modem could not be configured, error: %d", err);
		return;
	}
}


static int client_put_send(void)
{
	int err;
	struct coap_packet request;

	next_token++;

	err = coap_packet_init(&request, coap_buf, sizeof(coap_buf),
			       APP_COAP_VERSION, COAP_TYPE_NON_CON,
			       sizeof(next_token), (uint8_t *)&next_token,
			       COAP_METHOD_PUT, coap_next_id());
	if (err < 0) {
		printk("Failed to create CoAP request, %d\n", err);
		return err;
	}

	err = coap_packet_append_option(&request, COAP_OPTION_URI_PATH,
					(uint8_t *)CONFIG_COAP_TX_RESOURCE,
					strlen(CONFIG_COAP_TX_RESOURCE));
	if (err < 0) {
		printk("Failed to encode CoAP option, %d\n", err);
		return err;
	}

	const uint8_t text_plain = COAP_CONTENT_FORMAT_TEXT_PLAIN;
	err = coap_packet_append_option(&request, COAP_OPTION_CONTENT_FORMAT,
					&text_plain,
					sizeof(text_plain));
	if (err < 0) {
		printk("Failed to encode CoAP option, %d\n", err);
		return err;
	}

	err = coap_packet_append_payload_marker(&request);
	if (err < 0) {
		printk("Failed to append payload marker, %d\n", err);
		return err;
	}

	err = coap_packet_append_payload(&request, (uint8_t *)MESSAGE_TO_SEND, sizeof(MESSAGE_TO_SEND));
	if (err < 0) {
		printk("Failed to append payload, %d\n", err);
		return err;
	}

	err = send(sock, request.data, request.offset, 0);
	if (err < 0) {
		printk("Failed to send CoAP request, %d\n", errno);
		return -errno;
	}

	printk("CoAP request sent: token 0x%04x\n", next_token);

	return 0;
}

static void button_handler(uint32_t button_state, uint32_t has_changed) 
{
	#if defined (CONFIG_BOARD_NRF9160DK_NRF9160_NS) 
	if (has_changed & DK_BTN1_MSK && button_state & DK_BTN1_MSK) {
		client_get_send();
	} else if (has_changed & DK_BTN2_MSK && button_state & DK_BTN2_MSK) {
		client_put_send();
	}
	#elif defined (CONFIG_BOARD_THINGY91_NRF9160_NS) 
	static bool toogle = 1;
	if (has_changed & DK_BTN1_MSK && button_state & DK_BTN1_MSK) {
		if (toogle ==1){
			client_get_send();	
		}else{
			client_put_send();
		}
		toogle = !toogle;
	} 
	#endif
}


static void rx_work_fn(struct k_work *work){
	client_get_send();
}

void main(void)
{
	int err, received;
	err = modem_key_mgmt_write(SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_IDENTITY, CONFIG_COAP_DEVICE_NAME, strlen(CONFIG_COAP_DEVICE_NAME));
	if (err) {
		printk("Failed to write identity: %d\n", err);
		return;
	}

	err = modem_key_mgmt_write(SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_PSK, CONFIG_COAP_SERVER_PSK, strlen(CONFIG_COAP_SERVER_PSK));
	if (err) {
		printk("Failed to write identity: %d\n", err);
		return;
	}
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
		printk("Failed to resolve server name\n");
		return;
	}

	if (client_init() != 0) {
		printk("Failed to initialize CoAP client\n");
		return;
	}
	k_work_init_delayable(&rx_work,rx_work_fn);

	while (1) {
		k_work_reschedule(&rx_work,K_MSEC(RX_CHECK_INTERVAL));
		received = recv(sock, coap_buf, sizeof(coap_buf), 0);
		if (received < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				printk("socket EAGAIN\n");
				continue;
			} else {
				printk("Socket error %d, exit...\n", errno);
				break;
			}
		}

		if (received == 0) {
			printk("Empty datagram\n");
			continue;
		}

		err = client_handle_get_response(coap_buf, received);
		if (err < 0) {
			printk("Invalid response, exit...\n");
			break;
		}
		
	}

	(void)close(sock);
}
