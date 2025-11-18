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
#include <net/mqtt_helper.h>

LOG_MODULE_REGISTER(Lesson4_Exercise1, LOG_LEVEL_INF);

/* STEP 3 - Define the commands to control and monitor LEDs and buttons */
#define LED1_ON_CMD       "LED1ON"
#define LED1_OFF_CMD      "LED1OFF"
#define LED2_ON_CMD       "LED2ON"
#define LED2_OFF_CMD      "LED2OFF"
#define BUTTON_MSG        "Hi from nRF9151 SiP"
#define SUBSCRIBE_TOPIC_ID 1234

#define IMEI_LEN	15
#define CGSN_RESPONSE_LENGTH (IMEI_LEN + 6 + 1) /* Add 6 for \r\nOK\r\n and 1 for \0 */
#define CLIENT_ID_LEN sizeof("nrf-") + IMEI_LEN /* \0 included in sizeof() statement */

static K_SEM_DEFINE(lte_connected, 0, 1);

/* STEP 9.2 - Declare the variable to store the client ID */
static uint8_t client_id[CLIENT_ID_LEN];

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
		int len;
	int err;
	char imei_buf[CGSN_RESPONSE_LENGTH];

	if (!buffer || buffer_len == 0) {
		LOG_ERR("Invalid buffer parameters");
		return -EINVAL;
	}

	if (strlen(CONFIG_MQTT_SAMPLE_CLIENT_ID) > 0) {
		len = snprintk(buffer, buffer_len, "%s",
			 CONFIG_MQTT_SAMPLE_CLIENT_ID);
	if ((len < 0) || (len >= buffer_len)) {
		LOG_ERR("Failed to format client ID from config, error: %d", len);
		buffer[0] = '\0';
		return -EMSGSIZE;
	}
	LOG_DBG("client_id = %s", buffer);
	return 0;
	}

	err = nrf_modem_at_cmd(imei_buf, sizeof(imei_buf), "AT+CGSN");
	if (err) {
		LOG_ERR("Failed to obtain IMEI, error: %d", err);
		buffer[0] = '\0';
		return err;
	}

	/* Validate IMEI length before null termination */
	if (IMEI_LEN >= sizeof(imei_buf)) {
		LOG_ERR("IMEI_LEN exceeds buffer size");
		buffer[0] = '\0';
		return -EINVAL;
	}

	imei_buf[IMEI_LEN] = '\0';

	len = snprintk(buffer, buffer_len, "nrf-%.*s", IMEI_LEN, imei_buf);
	if ((len < 0) || (len >= buffer_len)) {
		LOG_ERR("Failed to format client ID from IMEI, error: %d", len);
		buffer[0] = '\0';
		return -EMSGSIZE;
	}

	LOG_DBG("client_id = %s", buffer);

	return 0;
}

/* STEP 4 - Define the function to subscribe to topics */
static void subscribe(void)
{
	int err;

	/* STEP 4.1 - Declare a variable of type mqtt_topic */
	struct mqtt_topic subscribe_topic = {
		.topic = {
			.utf8 = CONFIG_MQTT_SAMPLE_SUB_TOPIC, 
			.size = strlen(CONFIG_MQTT_SAMPLE_SUB_TOPIC)
		},
		.qos = MQTT_QOS_1_AT_LEAST_ONCE};

	/* STEP 4.2 - Define a subscription list */
	struct mqtt_subscription_list subscription_list = {
		.list = &subscribe_topic,
		.list_count = 1,
		.message_id = SUBSCRIBE_TOPIC_ID};

	/* STEP 4.3 - Subscribe to topics */
	LOG_INF("Subscribing to %s", CONFIG_MQTT_SAMPLE_SUB_TOPIC);
	err = mqtt_helper_subscribe(&subscription_list);
	if (err) {
		LOG_ERR("Failed to subscribe to topics, error: %d", err);
		return;
	}
}

/* STEP 5 - Define the function to publish data */
static int publish(uint8_t *data, size_t len)
{
	int err;
	/* STEP 5.1 - Declare and populate a variable of type mqtt_publish_param */	
	struct mqtt_publish_param mqtt_param;

	mqtt_param.message.payload.data = data;
	mqtt_param.message.payload.len = len;
	mqtt_param.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE;
	mqtt_param.message_id = mqtt_helper_msg_id_get(),
	mqtt_param.message.topic.topic.utf8 = CONFIG_MQTT_SAMPLE_PUB_TOPIC;
	mqtt_param.message.topic.topic.size = strlen(CONFIG_MQTT_SAMPLE_PUB_TOPIC);
	mqtt_param.dup_flag = 0;
	mqtt_param.retain_flag = 0;

	/* STEP 5.2 - Publish to MQTT broker */
	err = mqtt_helper_publish(&mqtt_param);
	if (err) {
		LOG_WRN("Failed to send payload, err: %d", err);
		return err;
	}

	LOG_INF("Published message: \"%.*s\" on topic: \"%.*s\"", mqtt_param.message.payload.len,
								  mqtt_param.message.payload.data,
								  mqtt_param.message.topic.topic.size,
								  mqtt_param.message.topic.topic.utf8);
	return 0;
}

/* STEP 6.1 - Define callback handler for CONNACK event */
static void on_mqtt_connack(enum mqtt_conn_return_code return_code, bool session_present)
{
	ARG_UNUSED(session_present);

	if (return_code == MQTT_CONNECTION_ACCEPTED) {
		LOG_INF("Connected to MQTT broker");
		LOG_INF("Hostname: %s", CONFIG_MQTT_SAMPLE_BROKER_HOSTNAME);
		LOG_INF("Client ID: %s", (char *)client_id);
		LOG_INF("Port: %d", CONFIG_MQTT_HELPER_PORT);
		LOG_INF("TLS: %s", IS_ENABLED(CONFIG_MQTT_LIB_TLS) ? "Yes" : "No");
		subscribe();
	} else {
		LOG_WRN("Connection to broker not established, return_code: %d", return_code);
	}

}

/* STEP 6.2 - Define callback handler for SUBACK event */
static void on_mqtt_suback(uint16_t message_id, int result)
{	
	if (result != MQTT_SUBACK_FAILURE) {
		if (message_id == SUBSCRIBE_TOPIC_ID) {
			LOG_INF("Subscribed to %s with QoS %d", CONFIG_MQTT_SAMPLE_SUB_TOPIC, result);
			return;
		}
		LOG_WRN("Subscribed to unknown topic, id: %d with QoS %d", message_id, result);
		return;
	}
	LOG_ERR("Topic subscription failed, error: %d", result);
}

/* STEP 6.3 - Define callback handler for PUBLISH event */
static void on_mqtt_publish(struct mqtt_helper_buf topic, struct mqtt_helper_buf payload)
{
	int err; 
	LOG_INF("Received payload: %.*s on topic: %.*s", payload.size,
							 payload.ptr,
							 topic.size,
							 topic.ptr);
	
	if (strncmp(payload.ptr, LED1_ON_CMD,
			    sizeof(LED1_ON_CMD) - 1) == 0) {
				err = dk_set_led_on(DK_LED1);
				if (err) {
					LOG_ERR("Failed to set LED %d on, error: %d", DK_LED1, err);
					return;
				}
	} else if (strncmp(payload.ptr, LED1_OFF_CMD,
			   sizeof(LED1_OFF_CMD) - 1) == 0) {
				err = dk_set_led_off(DK_LED1);
				if (err) {
					LOG_ERR("Failed to set LED %d off, error: %d", DK_LED1, err);
					return;
				}
	} else if (strncmp(payload.ptr, LED2_ON_CMD,
			   sizeof(LED2_ON_CMD) - 1) == 0) {
				err = dk_set_led_on(DK_LED2);
				if (err) {
					LOG_ERR("Failed to set LED %d on, error: %d", DK_LED2, err);
					return;
				}
	} else if (strncmp(payload.ptr, LED2_OFF_CMD,
			   sizeof(LED2_OFF_CMD) - 1) == 0) {
				err = dk_set_led_off(DK_LED2);
				if (err) {
					LOG_ERR("Failed to set LED %d off, error: %d", DK_LED2, err);
					return;
				}
	}
}

/* STEP 6.4 - Define callback handler for DISCONNECT event */
static void on_mqtt_disconnect(int result)
{
	LOG_INF("MQTT client disconnected: %d", result);
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	switch (has_changed) {
	case DK_BTN1_MSK:
		/* STEP 7.2 - Publish message when button 1 is pressed */
		if (button_state & DK_BTN1_MSK){
			int err = publish(BUTTON_MSG, sizeof(BUTTON_MSG)-1);
			if (err) {
				LOG_INF("Failed to send message, %d", err);
				return;
			}
		}
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
	struct mqtt_helper_cfg config = {
		.cb = {
			.on_connack = on_mqtt_connack,
			.on_disconnect = on_mqtt_disconnect,
			.on_publish = on_mqtt_publish,
			.on_suback = on_mqtt_suback,
		},
	};

	err = mqtt_helper_init(&config);
	if (err) {
		LOG_ERR("Failed to initialize MQTT helper, error: %d", err);
		return 0;
	}

	/* STEP 9.3 - Generate the client ID */
	err = client_id_get(client_id, sizeof(client_id));
    if (err) {
        LOG_ERR("Failed to get client ID, error: %d", err);
        return 0;
    }

	/* STEP 10 - Establish a connection to the MQTT broker */
	struct mqtt_helper_conn_params conn_params = {
		.hostname.ptr = CONFIG_MQTT_SAMPLE_BROKER_HOSTNAME,
		.hostname.size = strlen(CONFIG_MQTT_SAMPLE_BROKER_HOSTNAME),
		.device_id.ptr = (char *)client_id,
		.device_id.size = strlen(client_id),
	};

	err = mqtt_helper_connect(&conn_params);
	if (err) {
		LOG_ERR("Failed to connect to MQTT, error code: %d", err);
		return 0;
	}
}