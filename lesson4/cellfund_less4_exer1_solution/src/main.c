/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <modem/lte_lc.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>
#include "mqtt_connection.h"

/* STEP 2.3 - Include the header file for the MQTT Library*/
#include <zephyr/net/mqtt.h>

#define REGISTERED_STATUS_LED          DK_LED2

static K_SEM_DEFINE(lte_connected, 0, 1);

LOG_MODULE_REGISTER(Lesson4_Exercise1, LOG_LEVEL_INF);

/* The mqtt client struct */
static struct mqtt_client client;
/* File descriptor */
static struct pollfd fds;

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

static void button_handler(uint32_t button_states, uint32_t has_changed)
{
	/* STEP 7.2 - When button 1 is pressed, call data_publish() to publish a message. */
	if (has_changed & button_states &
	    BIT(CONFIG_BUTTON_EVENT_BTN_NUM - 1)) {
		int ret;

		ret = data_publish(&client,
				   MQTT_QOS_1_AT_LEAST_ONCE,
				   CONFIG_BUTTON_EVENT_PUBLISH_MSG,
				   sizeof(CONFIG_BUTTON_EVENT_PUBLISH_MSG)-1);
		if (ret) {
			LOG_ERR("Publish failed: %d", ret);
		}
	}
}

static void modem_configure(void)
{
	/* Turn off LTE power saving features for a more responsive demo. Also,
	 * request power saving features before network registration. Some
	 * networks rejects timer updates after the device has registered to the
	 * LTE network.
	 */
	LOG_INF("Disabling PSM and eDRX");
	lte_lc_psm_req(false);
	lte_lc_edrx_req(false);

	int err = lte_lc_init_and_connect_async(lte_handler);
	if (err) {
		LOG_ERR("Modem could not be configured, error: %d", err);
		return;
	}
}

void main(void)
{
	int err;
	uint32_t connect_attempt = 0;

	err = dk_buttons_init(button_handler);
	if (err){
		LOG_ERR("Failed to initlize the Buttons Library");
	}
	err = dk_leds_init();
	if (err){
		LOG_ERR("Failed to initlize the LEDs Library");
	}

	modem_configure();

	LOG_INF("Connecting to LTE network, this may take several minutes...");
	k_sem_take(&lte_connected, K_FOREVER);

	//We only reach here if we are registered to a network
	LOG_INF("Connected to LTE network");
	dk_set_led_on(REGISTERED_STATUS_LED);

	err = client_init(&client);
	if (err != 0) {
		LOG_ERR("client_init: %d", err);
		return;
	}

do_connect:
	if (connect_attempt++ > 0) {
		LOG_INF("Reconnecting in %d seconds...",
			CONFIG_MQTT_RECONNECT_DELAY_S);
		k_sleep(K_SECONDS(CONFIG_MQTT_RECONNECT_DELAY_S));
	}
	err = mqtt_connect(&client);
	if (err != 0) {
		LOG_ERR("mqtt_connect %d", err);
		goto do_connect;
	}

	err = fds_init(&client,&fds);
	if (err != 0) {
		LOG_ERR("fds_init: %d", err);
		return;
	}

	while (1) {
		err = poll(&fds, 1, mqtt_keepalive_time_left(&client));
		if (err < 0) {
			LOG_ERR("poll: %d", errno);
			break;
		}

		err = mqtt_live(&client);
		if ((err != 0) && (err != -EAGAIN)) {
			LOG_ERR("ERROR: mqtt_live: %d", err);
			break;
		}

		if ((fds.revents & POLLIN) == POLLIN) {
			err = mqtt_input(&client);
			if (err != 0) {
				LOG_ERR("mqtt_input: %d", err);
				break;
			}
		}

		if ((fds.revents & POLLERR) == POLLERR) {
			LOG_ERR("POLLERR");
			break;
		}

		if ((fds.revents & POLLNVAL) == POLLNVAL) {
			LOG_ERR("POLLNVAL");
			break;
		}
	}

	LOG_INF("Disconnecting MQTT client...");

	err = mqtt_disconnect(&client);
	if (err) {
		LOG_ERR("Could not disconnect MQTT client: %d", err);
	}
	goto do_connect;
}
