/**
 * @file blues-minimal-i2c.cpp
 * @author Bernd Giesecke (bernd@giesecke.tk)
 * @brief Minimized I2C communication with Blues Notecard
 * @version 0.1
 * @date 2023-10-18
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "blues-minimal-i2c.h"

/**
 * @brief Construct a new RAK_BLUES instance
 *
 */
RAK_BLUES::RAK_BLUES(byte addr)
{
	_deviceAddress = addr;
}

/**
 * @brief Restart I2C bus
 *
 */
void RAK_BLUES::I2C_RST(void)
{
#if WIRE_HAS_END
	Wire.end();
#endif
	Wire.begin();
}

/**
 * @brief Create a request structure to be sent to the NoteCard
 *
 * @param request_name name of request, e.g. card.wireless
 * @return true if request could be created
 * @return false if request could not be created
 */
bool RAK_BLUES::start_req(char *request)
{
	note_json.clear();
	note_json["req"] = request;
	// BLUES_LOG("BLUES","Added string %s", request);
	// serializeJson(note_json, Serial);
	// Serial.println("");
	return true;
}

/**
 * @brief Send a completed request to the NoteCard.
 *
 * @param response if not NULL, the response of the Notecard will copied into this buffer
 * @param resp_len max length of buffer for the Notecard response
 * @return true if request could be sent and the response does not have "err"
 * @return false if request could not be sent or the response did have "err"
 */
bool RAK_BLUES::send_req(char *response, uint16_t resp_len)
{

	// Send the request
#if BLUES_DEBUG > 0
	size_t jsonLen = serializeJson(note_json, in_out_buff, 4096);
	BLUES_LOG("BLUES", "Request: %s", in_out_buff);
#endif

	in_out_buff[jsonLen] = '\n';
	jsonLen += 1;

	// Transmit the request in chunks, but also in segments so as not to overwhelm the notecard's interrupt buffers
	uint8_t *chunk = (uint8_t *)in_out_buff;
	uint16_t sentInSegment = 0;
	while (jsonLen > 0)
	{
		// Constrain chunkLen to fit into 16 bits (blues_I2C_TX takes the buffer
		// size as a uint16_t).
		uint16_t chunkLen = (jsonLen > 0xFFFF) ? 0xFFFF : jsonLen;
		// Constrain chunkLen to be <= _I2CMax().
		chunkLen = (chunkLen > 32) ? 32 : chunkLen;

		delay(6);

		if (!blues_I2C_TX(_deviceAddress, chunk, chunkLen))
		{
			BLUES_LOG("BLUES", "blues_I2C_TX error");
			I2C_RST();
			return false;
		}
		chunk += chunkLen;
		jsonLen -= chunkLen;
		sentInSegment += chunkLen;
		if (sentInSegment > 250)
		{
			sentInSegment = 0;
			delay(250);
		}
		delay(20);
	}

	memset(in_out_buff, 0, JSON_BUFF_SIZE); // sizeof(blues_response));

	// Loop, building a reply buffer out of received chunks.  We'll build the reply in the same
	// buffer we used to transmit, and will grow it as necessary.
	bool receivedNewline = false;
	size_t jsonbufLen = 0;
	uint16_t chunkLen = 0;
	uint32_t startMs = millis();
	uint8_t *jsonbuf = (uint8_t *)in_out_buff;

	while (true)
	{
		// Read the chunk
		uint32_t available;
		delay(6);

		if (!blues_I2C_RX(_deviceAddress, &jsonbuf[jsonbufLen], chunkLen, &available))
		{
			BLUES_LOG("BLUES", "blues_I2C_RX error");
			return false;
		}

		// We've now received the chunk
		jsonbufLen += chunkLen;

		// If the last byte of the chunk is \n, chances are that we're done.  However, just so
		// that we pull everything pending from the module, we only exit when we've received
		// a newline AND there's nothing left available from the module.
		if (jsonbufLen > 0 && jsonbuf[jsonbufLen - 1] == '\n')
		{
			receivedNewline = true;
		}

		// Constrain chunkLen to fit into 16 bits (blues_I2C_RX takes the buffer
		// size as a uint16_t).
		chunkLen = (available > 0xFFFF) ? 0xFFFF : available;
		// Constrain chunkLen to be <= _I2CMax().
		chunkLen = (chunkLen > 32) ? 32 : chunkLen;

		// If there's something available on the notecard for us to receive, do it
		if (chunkLen > 0)
		{
			continue;
		}

		// If there's nothing available AND we've received a newline, we're done
		if (receivedNewline)
		{
			break;
		}

		// Check for buffer overflow
		if (jsonbufLen >= 4096)
		{
			BLUES_LOG("BLUES", "blues_I2C_RX buffer overflow");
			break;
		}

		if (chunkLen)
			// If we've timed out and nothing's available, exit
			if ((millis() - startMs) >= 30000)
			{
				BLUES_LOG("BLUES", "No Response");
				return false;
			}

		// Delay, simply waiting for the Note to process the request
		delay(50);
	}

	BLUES_LOG("BLUES", "Response: %s", in_out_buff);

	if (response != NULL)
	{
		snprintf(response, resp_len, "%s", (char *)in_out_buff);
	}

	// Make it JSON
	note_json.clear();
	deserializeJson(note_json, (char *)in_out_buff);

	return true;
}

/**
 * @brief Add C-String entry to JSON document
 *
 * @param type char * name
 * @param value char * value
 */
void RAK_BLUES::add_string_entry(char *type, char *value)
{
	note_json[type] = value;
}

/**
 * @brief Add boolean entry to JSON document
 *
 * @param type char * name
 * @param value bool value
 */
void RAK_BLUES::add_bool_entry(char *type, bool value)
{
	note_json[type] = value;
}

/**
 * @brief Add integer entry to JSON document
 *
 * @param type char * name
 * @param value integer value
 */
void RAK_BLUES::add_int32_entry(char *type, int32_t value)
{
	note_json[type] = value;
}

/**
 * @brief Add unsigned integer entry to JSON document
 *
 * @param type char * name
 * @param value unsigned integer value
 */
void RAK_BLUES::add_uint32_entry(char *type, uint32_t value)
{
	note_json[type] = value;
}

/**
 * @brief Add float entry to JSON document
 *
 * @param type char * name
 * @param value float value
 */
void RAK_BLUES::add_float_entry(char *type, float value)
{
	note_json[type] = value;
}

/**
 * @brief Add nested C-String entry to JSON document
 *
 * @param type char * name
 * @param nested char * nested name
 * @param value char * value
 */
void RAK_BLUES::add_nested_string_entry(char *type, char *nested, char *value)
{
	note_json[type][nested] = value;
}

/**
 * @brief Add nested integer entry to JSON document
 *
 * @param type char * name
 * @param nested char * nested name
 * @param value integer value
 */
void RAK_BLUES::add_nested_int32_entry(char *type, char *nested, int32_t value)
{
	note_json[type][nested] = value;
}

/**
 * @brief Add nested unsigned integer entry to JSON document
 *
 * @param type char * name
 * @param nested char * nested name
 * @param value unsigned integer value
 */
void RAK_BLUES::add_nested_uint32_entry(char *type, char *nested, uint32_t value)
{
	note_json[type][nested] = value;
}

/**
 * @brief Add nested bool entry to JSON document
 *
 * @param type char * name
 * @param nested char * nested name
 * @param value char * value
 */
void RAK_BLUES::add_nested_bool_entry(char *type, char *nested, bool value)
{
	note_json[type][nested] = value;
}

bool RAK_BLUES::has_entry(char *type)
{
	return note_json.containsKey(type);
}

bool RAK_BLUES::has_nested_entry(char *type, char *nested)
{
	return note_json[type].containsKey(nested);
}

bool RAK_BLUES::get_string_entry(char *type, char *value, uint16_t value_size)
{
	if (has_entry(type))
	{
		char *entry_str = (char *)note_json[type].as<const char *>();
		strncpy(value, entry_str, value_size);
		return true;
	}
	return false;
}

bool RAK_BLUES::get_bool_entry(char *type, bool &value)
{
	if (has_entry(type))
	{
		value = note_json[type].as<bool>();
		return true;
	}
	return false;
}

bool RAK_BLUES::get_int32_entry(char *type, int32_t &value)
{
	if (has_entry(type))
	{
		value = note_json[type].as<int32_t>();
		return true;
	}
	return false;
}

bool RAK_BLUES::get_uint32_entry(char *type, uint32_t &value)
{
	if (has_entry(type))
	{
		value = note_json[type].as<uint32_t>();
		return true;
	}
	return false;
}

bool RAK_BLUES::get_float_entry(char *type, float &value)
{
	if (has_entry(type))
	{
		value = note_json[type].as<float>();
		return true;
	}
	return false;
}

bool RAK_BLUES::get_nested_string_entry(char *type, char *nested, char *value, uint16_t value_size)
{
	if (note_json[type].containsKey(nested))
	{
		char *entry_str = (char *)note_json[type].as<const char *>();
		strncpy(value, entry_str, value_size);
		return true;
	}
	return false;
}

bool RAK_BLUES::get_nested_int32_entry(char *type, char *nested, int32_t &value)
{
	if (has_entry(type))
	{
		value = note_json[type].as<int32_t>();
		return true;
	}
	return false;
}

bool RAK_BLUES::get_nested_uint32_entry(char *type, char *nested, uint32_t &value)
{
	if (has_entry(type))
	{
		value = note_json[type].as<uint32_t>();
		return true;
	}
	return false;
}

bool RAK_BLUES::get_nested_bool_entry(char *type, char *nested, bool &value)
{
	if (has_entry(type))
	{
		value = note_json[type].as<bool>();
		return true;
	}
	return false;
}

/**
 * @brief Add nested float entry to JSON document
 *
 * @param type char * name
 * @param nested char * nested name
 * @param value float value
 */
void RAK_BLUES::add_nested_float_entry(char *type, char *nested, float value)
{
	note_json[type][nested] = value;
}

/**
 * @brief Encode a char buffer to Base64
 *
 * @param encoded (out) encoded string
 * @param string char buffer for encoding
 * @param len length of buffer
 * @return int length of encoded string
 */
int RAK_BLUES::myJB64Encode(char *encoded, const char *string, int len)
{
	int i;
	char *p;

	p = encoded;
	for (i = 0; i < len - 2; i += 3)
	{
		*p++ = basis_64[(string[i] >> 2) & 0x3F];
		*p++ = basis_64[((string[i] & 0x3) << 4) | ((int)(string[i + 1] & 0xF0) >> 4)];
		*p++ = basis_64[((string[i + 1] & 0xF) << 2) | ((int)(string[i + 2] & 0xC0) >> 6)];
		*p++ = basis_64[string[i + 2] & 0x3F];
	}
	if (i < len)
	{
		*p++ = basis_64[(string[i] >> 2) & 0x3F];
		if (i == (len - 1))
		{
			*p++ = basis_64[((string[i] & 0x3) << 4)];
			*p++ = '=';
		}
		else
		{
			*p++ = basis_64[((string[i] & 0x3) << 4) | ((int)(string[i + 1] & 0xF0) >> 4)];
			*p++ = basis_64[((string[i + 1] & 0xF) << 2)];
		}
		*p++ = '=';
	}

	*p++ = '\0';
	return p - encoded;
}

/**
 * @brief Transmit data to the NoteCard over I2C
 *
 * @param device_address_ NoteCard I2C address, default is 0x17
 * @param buffer_ Buffer with the data to send
 * @param size_ Size of buffer
 * @return bool true if no errors, otherwise false
 */
bool RAK_BLUES::blues_I2C_TX(uint16_t device_address_, uint8_t *buffer_, uint16_t size_)
{
	bool result = true;
	uint8_t transmission_error = 0;

	Wire.beginTransmission(static_cast<uint8_t>(device_address_));
	Wire.write(static_cast<uint8_t>(size_));
	Wire.write(buffer_, size_);
	transmission_error = Wire.endTransmission();

	if (transmission_error)
	{
		switch (transmission_error)
		{
		case 1:
			BLUES_LOG("BLUES_I2C", "TX data too long to fit in transmit buffer");
			break;
		case 2:
			BLUES_LOG("BLUES_I2C", "TX received NACK on transmit of address");
			break;
		case 3:
			BLUES_LOG("BLUES_I2C", "TX received NACK on transmit of data");
			break;
		case 4:
			BLUES_LOG("BLUES_I2C", "TX unknown error on TwoWire::endTransmission()");
			break;
		case 5:
			BLUES_LOG("BLUES_I2C", "TX timeout");
			break;
		default:
			BLUES_LOG("BLUES_I2C", "TX unknown error encounter during I2C transmission");
		}
		result = false;
		I2C_RST();
	}

	return result;
}

/**
 * @brief Receive data from the NoteCard over I2C
 *
 * @param device_address_ NoteCard I2C address, default is 0x17
 * @param buffer_ Buffer for the received data
 * @param requested_byte_count_ Number of bytes requested
 * @param available_ Number of bytes available
 * @return bool true if no errors, otherwise false
 */
bool RAK_BLUES::blues_I2C_RX(uint16_t device_address_, uint8_t *buffer_, uint16_t requested_byte_count_, uint32_t *available_)
{
	bool result = true;
	uint8_t transmission_error = 0;

	// Request response data from Notecard
	for (size_t i = 0; i < 3; ++i)
	{
		Wire.beginTransmission(static_cast<uint8_t>(device_address_));
		Wire.write(static_cast<uint8_t>(0));
		Wire.write(static_cast<uint8_t>(requested_byte_count_));
		transmission_error = Wire.endTransmission();

		// Break out of loop on success
		if (!transmission_error)
		{
			break;
		}

		switch (transmission_error)
		{
		case 1:
			BLUES_LOG("BLUES_I2C", "RX data too long to fit in transmit buffer");
			break;
		case 2:
			BLUES_LOG("BLUES_I2C", "RX received NACK on transmit of address");
			break;
		case 3:
			BLUES_LOG("BLUES_I2C", "RX received NACK on transmit of data");
			break;
		case 4:
			BLUES_LOG("BLUES_I2C", "RX unknown error on TwoWire::endTransmission()");
			break;
		case 5:
			BLUES_LOG("BLUES_I2C", "RX timeout");
			break;
		default:
			BLUES_LOG("BLUES_I2C", "RX unknown error encounter during I2C transmission");
		}
		result = false;
		I2C_RST();
	}

	// Delay briefly ensuring that the Notecard can
	// deliver the data in real-time to the I2C ISR
	::delay(2);

	// Read and cache response from Notecard
	if (!transmission_error)
	{
		const int request_length = requested_byte_count_ + 2;
		const int response_length = Wire.requestFrom((int)device_address_, request_length);
		if (!response_length)
		{
			result = false;
			BLUES_LOG("BLUES_I2C", "RX no response to read request");
		}
		else if (response_length != request_length)
		{
			result = false;
			BLUES_LOG("BLUES_I2C", "RX unexpected raw byte count");
		}
		else
		{
			// Ensure available byte count is within expected range
			static const size_t AVAILBLE_MAX = (255 - 2);
			uint32_t available = Wire.read();
			if (available > AVAILBLE_MAX)
			{
				result = false;
				BLUES_LOG("BLUES_I2C", "RX available byte count greater than max allowed");
			}
			// Ensure protocol response length matches size request
			else if (requested_byte_count_ != static_cast<uint8_t>(Wire.read()))
			{
				result = false;
				BLUES_LOG("BLUES_I2C", "RX unexpected protocol byte count");
			}
			// Update available with remaining bytes
			else
			{
				*available_ = available;

				for (size_t i = 0; i < requested_byte_count_; ++i)
				{
					// TODO: Perf test against indexed buffer writes
					// *buffer_++ = Wire.read();
					buffer_[i] = Wire.read();
					delay(6);
				}
			}
		}
	}
	return result;
}
