/**
 * @file main.h
 * @author Bernd Giesecke (bernd@giesecke.tk)
 * @brief Includes, defines and globals
 * @version 0.1
 * @date 2023-04-25
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef _MAIN_H_
#define _MAIN_H_

#include <Arduino.h>
#include <WisBlock-API-V2.h>
#ifdef ESP32
#include <WiFi.h>
#endif
#include "RAK1906_env.h"
#include <ArduinoJson.h>

// Debug output set to 0 to disable app debug output
#ifndef MY_DEBUG
#define MY_DEBUG 1
#endif

#ifdef NRF52_SERIES
#if MY_DEBUG > 0
#define MYLOG(tag, ...)                     \
	do                                      \
	{                                       \
		if (tag)                            \
			PRINTF("[%s] ", tag);           \
		PRINTF(__VA_ARGS__);                \
		PRINTF("\n");                       \
		Serial.flush();                     \
		if (g_ble_uart_is_connected)        \
		{                                   \
			g_ble_uart.printf(__VA_ARGS__); \
			g_ble_uart.printf("\n");        \
		}                                   \
	} while (0)
#else
#define MYLOG(...)
#endif
#endif
#ifdef ESP32
#if MY_DEBUG > 0
#define MYLOG(tag, ...)                     \
	do                                      \
	{                                       \
		if (tag)                            \
			PRINTF("[%s] ", tag);           \
		PRINTF(__VA_ARGS__);                \
		PRINTF("\n");                       \
		Serial.flush();                     \
	} while (0)
#else
#define MYLOG(...)
#endif
#endif

/** Define the version of your SW */
#ifndef SW_VERSION_1
#define SW_VERSION_1 1 // major version increase on API change / not backwards compatible
#endif
#ifndef SW_VERSION_2
#define SW_VERSION_2 0 // minor version increase on API change / backward compatible
#endif
#ifndef SW_VERSION_3
#define SW_VERSION_3 0 // patch version increase on bugfix, no affect on API
#endif

/** Application function definitions */
void setup_app(void);
bool init_app(void);
void app_event_handler(void);
void ble_data_handler(void) __attribute__((weak));
void lora_data_handler(void);

// Wakeup flags
#define USE_CELLULAR   0b1000000000000000
#define N_USE_CELLULAR 0b0111111111111111
#define BLUES_ATTN     0b0100000000000000
#define N_BLUES_ATTN   0b1011111111111111
#define GNSS_FINISH    0b0010000000000000
#define N_GNSS_FINISH  0b1101111111111111

// Cayenne LPP Channel numbers per sensor value
#define LPP_CHANNEL_BATT 1		 // Base Board
#define LPP_CHANNEL_HUMID_2 6	 // RAK1906
#define LPP_CHANNEL_TEMP_2 7	 // RAK1906
#define LPP_CHANNEL_PRESS_2 8	 // RAK1906
#define LPP_CHANNEL_GAS_2 9		 // RAK1906
#define LPP_CHANNEL_GPS 10		 // RAK13102
#define LPP_CHANNEL_GPS_TOWER 11 // RAK13102

// Globals
extern WisCayenne g_solution_data;
#ifdef NRF52_SERIES
extern SoftwareTimer blink_green;
#endif
#ifdef ESP32
extern Ticker blink_green;
#endif

// Blues.io
struct s_blues_settings
{
	uint16_t valid_mark = 0xAA55;								 // Validity marker
	char product_uid[256] = "com.my-company.my-name:my-project"; // Blues Product UID
	bool conn_continous = false;								 // Use periodic connection
	uint8_t sim_usage = 0;										 // 0 int SIM, 1 ext SIM, 2 ext int SIM, 3 int ext SIM
	char ext_sim_apn[256] = "internet";							 // APN to be used with external SIM
	bool motion_trigger = true;									 // Send data on motion trigger
};

#include <blues-minimal-i2c.h>

bool init_blues(void);
// bool start_req(char *request);
// bool send_req(void);
void blues_hub_status(void);
bool blues_get_location(void);
bool blues_enable_attn(bool motion);
bool blues_disable_attn(void);
bool blues_send_payload(uint8_t *data, uint16_t data_len);
bool blues_switch_gnss_mode(bool continuous_on);
void blues_card_restore(void);
void blues_attn_cb(void);
uint8_t blues_attn_reason(void);
bool blues_hub_connected(void);
extern RAK_BLUES rak_blues;
extern s_blues_settings g_blues_settings;

// User AT commands
void init_user_at(void);
bool read_blues_settings(void);
void save_blues_settings(void);
#endif // _MAIN_H_