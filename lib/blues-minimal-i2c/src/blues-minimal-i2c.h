/**
 * @file blues-minimal-i2c.h
 * @author Bernd Giesecke (bernd@giesecke.tk)
 * @brief Minimized I2C communication with Blues Notecard
 * @version 0.1
 * @date 2023-10-18
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef _BLUES_MINIMAL_I2C_H_
#define _BLUES_MINIMAL_I2C_H_

#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>

#define BLUES_I2C_ADDRESS 0x17

#ifndef JSON_BUFF_SIZE
#define JSON_BUFF_SIZE 4096
#endif

// Debug output set to 0 to disable app debug output
#ifndef BLUES_DEBUG
#define BLUES_DEBUG 1
#endif

#if BLUES_DEBUG > 0
#define BLUES_LOG(tag, ...)                     \
	do                                      \
	{                                       \
		if (tag)                            \
			PRINTF("[%s] ", tag);           \
		PRINTF(__VA_ARGS__);                \
		PRINTF("\n");                       \
		Serial.flush();                     \
	} while (0)
#else
#define BLUES_LOG(...)
#endif

class RAK_BLUES
{
public:
	RAK_BLUES(byte addr = BLUES_I2C_ADDRESS);
	// RAK_BLUES(void);

	/** JSON document for sending and response */
	StaticJsonDocument<JSON_BUFF_SIZE> note_json;

	/** Buffer for serialized JSON response */
	uint8_t in_out_buff[JSON_BUFF_SIZE];

	/** NoteCard default I2C address */
	uint8_t note_i2c_addr = BLUES_I2C_ADDRESS;

	/** Base64 helper */
	const char basis_64[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	bool start_req(char *request);
	bool send_req(char *response = NULL, uint16_t resp_len = 0);

	void add_string_entry(char *type, char *value);
	void add_bool_entry(char *type, bool value);
	void add_int32_entry(char *type, int32_t value);
	void add_uint32_entry(char *type, uint32_t value);
	void add_float_entry(char *type, float value);
	void add_nested_string_entry(char *type, char *nested, char *value);
	void add_nested_int32_entry(char *type, char *nested, int32_t value);
	void add_nested_uint32_entry(char *type, char *nested, uint32_t value);
	void add_nested_bool_entry(char *type, char *nested, bool value);
	void add_nested_float_entry(char *type, char *nested, float value);

	bool has_entry(char *type);
	bool has_nested_entry(char *type, char *nested);

	bool get_string_entry(char *type, char *value, uint16_t value_size);
	bool get_bool_entry(char *type, bool &value);
	bool get_int32_entry(char *type, int32_t &value);
	bool get_uint32_entry(char *type, uint32_t &value);
	bool get_float_entry(char *type, float &value);
	bool get_nested_string_entry(char *type, char *nested, char *value, uint16_t value_size);
	bool get_nested_int32_entry(char *type, char *nested, int32_t &value);
	bool get_nested_uint32_entry(char *type, char *nested, uint32_t &value);
	bool get_nested_bool_entry(char *type, char *nested, bool &value);

	int myJB64Encode(char *encoded, const char *string, int len);

private:
	void I2C_RST(void);
	bool blues_I2C_TX(uint16_t device_address_, uint8_t *buffer_, uint16_t size_);
	bool blues_I2C_RX(uint16_t device_address_, uint8_t *buffer_, uint16_t requested_byte_count_, uint32_t *available_);

	uint8_t _deviceAddress;
};
#endif // _BLUES_MINIMAL_I2C_H_