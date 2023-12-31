/**
 * @file blues.cpp
 * @author Bernd Giesecke (bernd@giesecke.tk)
 * @brief Blues.IO NoteCard handler
 * @version 0.1
 * @date 2023-04-27
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "main.h"
#include <blues-minimal-i2c.h>

// I2C functions for Blues NoteCard
RAK_BLUES rak_blues;

#ifndef PRODUCT_UID
#define PRODUCT_UID "com.my-company.my-name:my-project"
#endif
#define myProductID PRODUCT_UID

char card_response[1024];

/**
 * @brief Initialize Blues NoteCard
 *
 * @return true if NoteCard was found and setup was successful
 * @return false if NoteCard was not found or the setup failed
 */
bool init_blues(void)
{
	Wire.begin();
	Wire.setClock(100000);

	pinMode(WB_IO5, INPUT);

	bool request_success = false;

	//  Check if Notecard is plugged in
	for (int try_send = 0; try_send < 5; try_send++)
	{
		if (rak_blues.start_req((char *)"card.version"))
		{
			if (rak_blues.send_req())
			{
				if (rak_blues.has_entry((char *)"device"))
				{
					rak_blues.get_string_entry((char *)"device", card_response, 1024);
					AT_PRINTF("+EVT:IMSI-%s", &card_response[4]);
				}
				else
				{
					MYLOG("BLUES", "Did not find Device");
				}
				request_success = true;
				break;
			}
		}
		else
		{
			MYLOG("BLUES", "start_req failed");
		}
	}

	if (!request_success)
	{
		MYLOG("BLUES", "card.version request failed");
		return false;
	}
	request_success = false;

	/*******************************************************************************/
	/** Reset all location and motion modes to non-active, just in case            */
	/*******************************************************************************/
	// Disable location (just in case)
	for (int try_send = 0; try_send < 5; try_send++)
	{
		if (rak_blues.start_req((char *)"card.location.mode"))
		{
			rak_blues.add_string_entry((char *)"mode", (char *)"off");
			if (rak_blues.send_req())
			{
				request_success = true;
				break;
			}
		}
	}
	if (!request_success)
	{
		MYLOG("BLUES", "card.location.mode request failed");
	}
	request_success = false;

	// Disable location tracking (just in case)
	for (int try_send = 0; try_send < 5; try_send++)
	{
		if (rak_blues.start_req((char *)"card.location.track"))
		{
			rak_blues.add_bool_entry((char *)"stop", true);
			if (rak_blues.send_req())
			{
				request_success = true;
				break;
			}
		}
	}
	if (!request_success)
	{
		MYLOG("BLUES", "card.location.track request failed");
	}
	request_success = false;

	// Disable motion mode (just in case)
	for (int try_send = 0; try_send < 5; try_send++)
	{
		if (rak_blues.start_req((char *)"card.motion.mode"))
		{
			rak_blues.add_bool_entry((char *)"stop", true);
			if (rak_blues.send_req())
			{
				request_success = true;
				break;
			}
		}
	}
	if (!request_success)
	{
		MYLOG("BLUES", "card.motion.mode request failed");
	}
	request_success = false;

	// Disable motion sync (just in case)
	for (int try_send = 0; try_send < 5; try_send++)
	{
		if (rak_blues.start_req((char *)"card.motion.sync"))
		{
			rak_blues.add_bool_entry((char *)"stop", true);
			if (rak_blues.send_req())
			{
				request_success = true;
				break;
			}
		}
	}
	if (!request_success)
	{
		MYLOG("BLUES", "card.motion.sync request failed");
	}
	request_success = false;

	// Disable motion tracking (just in case)
	for (int try_send = 0; try_send < 5; try_send++)
	{
		if (rak_blues.start_req((char *)"card.motion.track"))
		{
			rak_blues.add_bool_entry((char *)"stop", true);
			if (rak_blues.send_req())
			{
				request_success = true;
				break;
			}
		}
	}
	if (!request_success)
	{
		MYLOG("BLUES", "card.motion.track request failed");
	}

	// Get the ProductUID from the saved settings
	// If no settings are found, use NoteCard internal settings!
	if (read_blues_settings())
	{
		request_success = false;

		MYLOG("BLUES", "Found saved settings, override NoteCard internal settings!");
		if (memcmp(g_blues_settings.product_uid, "com.my-company.my-name", 22) == 0)
		{
			MYLOG("BLUES", "No Product ID saved");
			AT_PRINTF(":EVT NO PUID");
			memcpy(g_blues_settings.product_uid, PRODUCT_UID, 33);
		}

		MYLOG("BLUES", "Set Product ID and connection mode");
		for (int try_send = 0; try_send < 5; try_send++)
		{
			if (rak_blues.start_req((char *)"hub.set"))
			{
				rak_blues.add_string_entry((char *)"product", g_blues_settings.product_uid);
				if (g_blues_settings.conn_continous)
				{
					rak_blues.add_string_entry((char *)"mode", (char *)"continuous");
				}
				else
				{
					rak_blues.add_string_entry((char *)"mode", (char *)"minimum");
				}
				// Set sync time to the sensor read time
				rak_blues.add_int32_entry((char *)"seconds", (g_lorawan_settings.send_repeat_time / 1000));
				rak_blues.add_bool_entry((char *)"heartbeat", true);

				if (rak_blues.send_req())
				{
					request_success = true;
					break;
				}
			}
			delay(100);
		}
		if (!request_success)
		{
			MYLOG("BLUES", "hub.set request failed");
			return false;
		}
		request_success = false;

		MYLOG("BLUES", "Set SIM and APN");
		for (int try_send = 0; try_send < 5; try_send++)
		{
			if (rak_blues.start_req((char *)"card.wireless"))
			{
				rak_blues.add_string_entry((char *)"mode", (char *)"auto");

				switch (g_blues_settings.sim_usage)
				{
				case 0:
					// USING BLUES eSIM CARD
					rak_blues.add_string_entry((char *)"method", (char *)"primary");
					break;
				case 1:
					// USING EXTERNAL SIM CARD only
					rak_blues.add_string_entry((char *)"apn", g_blues_settings.ext_sim_apn);
					rak_blues.add_string_entry((char *)"method", (char *)"secondary");
					break;
				case 2:
					// USING EXTERNAL SIM CARD as primary
					rak_blues.add_string_entry((char *)"apn", g_blues_settings.ext_sim_apn);
					rak_blues.add_string_entry((char *)"method", (char *)"dual-secondary-primary");
					break;
				case 3:
					// USING EXTERNAL SIM CARD as secondary
					rak_blues.add_string_entry((char *)"apn", g_blues_settings.ext_sim_apn);
					rak_blues.add_string_entry((char *)"method", (char *)"dual-primary-secondary");
					break;
				}

				if (rak_blues.send_req())
				{
					request_success = true;
					break;
				}
			}
		}
		if (!request_success)
		{
			MYLOG("BLUES", "card.wireless request failed");
			return false;
		}
		request_success = false;

		// Enable motion trigger
		for (int try_send = 0; try_send < 5; try_send++)
		{
			if (rak_blues.start_req((char *)"card.motion.mode"))
			{
				rak_blues.add_bool_entry((char *)"start", true);

				// // Set sensitivity 25Hz, +/- 16G range, 7.8 milli-G sensitivity
				// rak_blues.add_int32_entry((char *)"sensitivity", 1);
				// Set sensitivity 1.6Hz, +/- 2G range, 1 milli-G sensitivity
				rak_blues.add_int32_entry((char *)"sensitivity", -1);

				if (rak_blues.send_req())
				{
					request_success = true;
					break;
				}
			}
			delay(100);
		}
		if (!request_success)
		{
			MYLOG("BLUES", "card.motion.mode request failed");
			return false;
		}
		request_success = false;

		// Enable GNSS mode
		for (int try_send = 0; try_send < 5; try_send++)
		{
			if (blues_switch_gnss_mode(false))
			{
				request_success = true;
				break;
			}

			request_success = false;
			MYLOG("BLUES", "card.location.mode delete last location");
			// Clear last GPS location
			for (int try_send = 0; try_send < 5; try_send++)
			{
				rak_blues.start_req((char *)"card.location.mode");
				rak_blues.add_bool_entry((char *)"delete", true);
				if (rak_blues.send_req())
				{
					request_success = true;
					break;
				}
			}
		}
		if (!request_success)
		{
			MYLOG("BLUES", "card.location.mode delete last location request failed");
			return false;
		}
		request_success = false;

		/// \todo reset attn signal needs rework
		if (g_blues_settings.motion_trigger)
		{
			for (int try_send = 0; try_send < 5; try_send++)
			{
				if (rak_blues.start_req((char *)"card.attn"))
				{
					rak_blues.add_string_entry((char *)"mode", (char *)"disarm");
					if (rak_blues.send_req())
					{
						request_success = true;
						break;
					}
				}
			}
			if (!request_success)
			{
				MYLOG("BLUES", "card.attn disarm request failed");
				return false;
			}
			request_success = false;
		}
		else
		{
			MYLOG("BLUES", "Motion trigger disabled");
		}

		/// \todo reset attn signal needs rework
		if (!blues_enable_attn(true))
		{
			MYLOG("BLUES", "blues_enable_attn enable failed");
			return false;
		}
	}
	else
	{
		MYLOG("BLUES", "No saved Blues NoteCard settings, read existing settings");
		for (int try_send = 0; try_send < 3; try_send++)
		{
			if (rak_blues.start_req((char *)"card.wireless"))
			{
				if (rak_blues.send_req())
				{
					if (rak_blues.has_entry((char *)"apn"))
					{
						rak_blues.get_string_entry((char *)"apn", g_blues_settings.ext_sim_apn, 256);
						MYLOG("BLUES", "Got APN %s", g_blues_settings.ext_sim_apn);
					}
					else
					{
						MYLOG("BLUES", "No APN from NoteCard");
						// no entry, assume no APN
						g_blues_settings.ext_sim_apn[0] = 0;
					}
					if (rak_blues.has_entry((char *)"method"))
					{
						MYLOG("BLUES", "Got Method from NoteCard");
						char method_str[256];
						rak_blues.get_string_entry((char *)"method", method_str, 256);
						if (strcmp(method_str, "primary") == 0)
						{
							g_blues_settings.sim_usage = 0;
						}
						else if (strcmp(method_str, "secondary") == 0)
						{
							g_blues_settings.sim_usage = 1;
						}
						else if (strcmp(method_str, "dual-secondary-primary") == 0)
						{
							g_blues_settings.sim_usage = 2;
						}
						else if (strcmp(method_str, "dual-primary-secondary") == 0)
						{
							g_blues_settings.sim_usage = 3;
						}
						else
						{
							// no match, assume primary
							g_blues_settings.sim_usage = 0;
						}
					}
					else
					{
						MYLOG("BLUES", "No Method from NoteCard");
						// no entry, assume primary
						g_blues_settings.sim_usage = 0;
					}
					request_success = true;
					break;
				}
				else
				{
					MYLOG("BLUES", "Send request failed : card.wireless");
				}
			}
			else
			{
				MYLOG("BLUES", "Start request failed");
			}
		}
		request_success = false;

		for (int try_send = 0; try_send < 3; try_send++)
		{
			if (rak_blues.start_req((char *)"hub.get"))
			{
				if (rak_blues.send_req())
				{
					if (rak_blues.has_entry((char *)"product"))
					{
						MYLOG("BLUES", "Got Product from NoteCard");
						rak_blues.get_string_entry((char *)"product", g_blues_settings.product_uid, 256);
					}
					else
					{
						MYLOG("BLUES", "No Product from NoteCard");
						// no entry, assume no UID set
						g_blues_settings.product_uid[0] = 0;
					}
					if (rak_blues.has_entry((char *)"mode"))
					{
						MYLOG("BLUES", "Got Mode from NoteCard");
						char mode_str[256];
						rak_blues.get_string_entry((char *)"mode", mode_str, 256);
						if (strcmp(mode_str, "minimum") == 0)
						{
							g_blues_settings.conn_continous = false;
						}
						else if (strcmp(mode_str, "continous") == 0)
						{
							g_blues_settings.conn_continous = true;
						}
						else if (strcmp(mode_str, "periodic") == 0)
						{
							g_blues_settings.conn_continous = true;
						}
						else
						{
							// no match, assume continous
							g_blues_settings.conn_continous = true;
						}
					}
					else
					{
						MYLOG("BLUES", "No Mode from NoteCard");
						// no match, assume continous
						g_blues_settings.conn_continous = true;
					}
					request_success = true;
					break;
				}
				else
				{
					MYLOG("BLUES", "Send request failed : hub.get");
				}
			}
			else
			{
				MYLOG("BLUES", "Start request failed");
			}
		}
	}

#if IS_V2 == 1
	request_success = false;
	// Only for V2 cards, setup the WiFi network
	MYLOG("BLUES", "Set WiFi");
	for (int try_send = 0; try_send < 5; try_send++)
	{
		if (rak_blues.start_req((char *)"card.wifi"))
		{
			rak_blues.add_string_entry((char *)"ssid", (char *)"-");
			rak_blues.add_string_entry((char *)"password", (char *)"-");
			rak_blues.add_string_entry((char *)"name", (char *)"-");
			rak_blues.add_string_entry((char *)"org", (char *)"");
			rak_blues.add_bool_entry((char *)"start", false);

			if (rak_blues.send_req())
			{
				request_success = true;
				break;
			}
		}
	}
	if (!request_success)
	{
		MYLOG("BLUES", "card.wifi request failed");
		return false;
	}
#endif
	return true;
}

/**
 * @brief Send a data packet to NoteHub.IO
 *
 * @param data Payload as byte array (CayenneLPP formatted)
 * @param data_len Length of payload
 * @return true if note could be sent to NoteCard
 * @return false if note send failed
 */
bool blues_send_payload(uint8_t *data, uint16_t data_len)
{
	char payload_b86[255];
	bool request_success = false;

	for (int try_send = 0; try_send < 5; try_send++)
	{
		if (rak_blues.start_req((char *)"note.add"))
		{
			rak_blues.add_string_entry((char *)"file", (char *)"data.qo");
			rak_blues.add_bool_entry((char *)"sync", true);
			char node_id[24];
			sprintf(node_id, "%02x%02x%02x%02x%02x%02x%02x%02x",
					g_lorawan_settings.node_device_eui[0], g_lorawan_settings.node_device_eui[1],
					g_lorawan_settings.node_device_eui[2], g_lorawan_settings.node_device_eui[3],
					g_lorawan_settings.node_device_eui[4], g_lorawan_settings.node_device_eui[5],
					g_lorawan_settings.node_device_eui[6], g_lorawan_settings.node_device_eui[7]);
			rak_blues.add_nested_string_entry((char *)"body", (char *)"dev_eui", node_id);

			rak_blues.myJB64Encode(payload_b86, (const char *)data, data_len);

			rak_blues.add_string_entry((char *)"payload", payload_b86);

			MYLOG("BLUES", "Finished parsing");

			if (rak_blues.send_req())
			{
				AT_PRINTF("+EVT:TX_CELL_OK");
				request_success = true;
				break;
			}
		}
	}
	if (!request_success)
	{
		MYLOG("BLUES", "Send request failed");
		AT_PRINTF("+EVT:TX_CELL_FAIL");
		return false;
	}

	return request_success;
}
/**
 * @brief Request NoteHub status, only for debug purposes
 *
 */
void blues_hub_status(void)
{
	bool request_success = false;
	for (int try_send = 0; try_send < 5; try_send++)
	{
		rak_blues.start_req((char *)"hub.status");
		if (rak_blues.send_req())
		{
			request_success = true;
			break;
		}
	}
	if (!request_success)
	{
		MYLOG("BLUES", "hub.status request failed");
	}
}

/**
 * @brief 	Switch GNSS between continuous and periodic mode
 *
 * @param continuous_on true for continuous mode, false for periodic mode
 * @return true if switch was successful
 * @return false if switch failed
 */
bool blues_switch_gnss_mode(bool continuous_on)
{
	bool request_success = false;
	MYLOG("BLUES", "Set location mode %s", continuous_on ? "continuous" : "off");
	for (int try_send = 0; try_send < 5; try_send++)
	{
		if (rak_blues.start_req((char *)"card.location.mode"))
		{
			if (continuous_on)
			{
				// Continous GNSS mode
				rak_blues.add_string_entry((char *)"mode", (char *)"continuous");
			}
			else
			{
				// GNSS off
				rak_blues.add_string_entry((char *)"mode", (char *)"off");
			}
			// Set location acquisition time to the sensor read time
			// MYLOG("BLUES", "Set location period %d", (g_lorawan_settings.send_repeat_time / 1000 / 2));
			// rak_blues.add_int32_entry((char *)"seconds", (g_lorawan_settings.send_repeat_time / 1000 / 2));
			if (rak_blues.send_req())
			{
				request_success = true;
				break;
			}
		}
	}
	if (!request_success)
	{
		MYLOG("BLUES", "card.location.mode request failed");
		return false;
	}
	return true;
}

/**
 * @brief Get the location information from the NoteCard
 *
 * @return true if a location could be acquired
 * @return false if request failed or no location is available
 */
bool blues_get_location(void)
{
	bool result = false;
	bool got_gnss_location = false;
	bool request_success = false;
	char str_value[128];
	uint32_t last_gnss_update = (uint32_t)millis();
	uint32_t current_card_time = last_gnss_update;

	for (int try_send = 0; try_send < 5; try_send++)
	{
		rak_blues.start_req((char *)"card.location");
		if (rak_blues.send_req())
		{
			// Check if the location is confirmed or an old location
			if (rak_blues.has_entry((char *)"status"))
			{
				if (rak_blues.get_string_entry((char *)"status", str_value, 128))
				{
					String gnss_status = String(str_value);
					MYLOG("BLUES", "gnss_status >>%s<<", gnss_status.c_str());
					if (gnss_status.indexOf("search") > 0)
					{
						MYLOG("BLUES", "GNSS is searching!");
					}
					if (gnss_status.indexOf("inactive") > 0)
					{
						MYLOG("BLUES", "GNSS is inactive!");
					}
					if (gnss_status.indexOf("updated") > 0)
					{
						MYLOG("BLUES", "GNSS is updated!");
					}
				}
			}
			if (rak_blues.has_entry((char *)"lat") && rak_blues.has_entry((char *)"lon"))
			{
				float blues_latitude;
				float blues_longitude;
				float blues_altitude = 0;
				if (rak_blues.get_float_entry((char *)"lat", blues_latitude))
				{
					if (rak_blues.get_float_entry((char *)"lon", blues_longitude))
					{
						got_gnss_location = true;

						if ((blues_latitude == 0.0) && (blues_longitude == 0.0))
						{
							MYLOG("BLUES", "No valid GPS data, report no location");
						}
						else
						{
							MYLOG("BLUES", "Got location Lat %.6f Long %0.6f", blues_latitude, blues_longitude);
							g_solution_data.addGNSS_6(LPP_CHANNEL_GPS, (int32_t)(blues_latitude * 10000000), (int32_t)(blues_longitude * 10000000), (int32_t)blues_altitude);
							g_solution_data.addPresence(LPP_CHANNEL_GPS_TOWER, false);
							result = true;
						}
					}

					if (rak_blues.has_entry((char *)"time"))
					{
						if (rak_blues.get_uint32_entry((char *)"time", last_gnss_update))
						{
							MYLOG("BLUES", "Last GNSS update was %ld", last_gnss_update);
						}
					}
				}
			}
		}

		request_success = true;
		break;
	}

	if (!request_success)
	{
		MYLOG("BLUES", "card.location request failed");
	}

	// Blink green LED if we found a GNSS location
	if (got_gnss_location)
	{
		digitalWrite(LED_GREEN, HIGH);
		blink_green.setPeriod(500);
		blink_green.start();
	}
	else
	{
		blink_green.stop();
		digitalWrite(LED_GREEN, LOW);
	}
	request_success = false;

	for (int try_send = 0; try_send < 5; try_send++)
	{
		rak_blues.start_req((char *)"card.time");
		if (rak_blues.send_req())
		{
			if (rak_blues.has_entry((char *)"lat") && rak_blues.has_entry((char *)"lon"))
			{
				if (rak_blues.has_entry((char *)"country"))
				{
					rak_blues.get_string_entry((char *)"country", str_value, 20);
					// Try to set LoRaWAN band automatically
					if (strcmp(str_value, "PH") == 0)
					{
						MYLOG("BLUES", "Found PH");
						if (g_lorawan_settings.lora_region != 10)
						{
							MYLOG("BLUES", "Switch to band 10");
							g_lorawan_settings.lora_region = 10;
							init_lorawan(true);
						}
					}
					else if (strcmp(str_value, "JP") == 0)
					{
						MYLOG("BLUES", "Found JP");
						if (g_lorawan_settings.lora_region != 8)
						{
							MYLOG("BLUES", "Switch to band 8");
							g_lorawan_settings.lora_region = 8;
							init_lorawan(true);
						}
					}
					else if (strcmp(str_value, "US") == 0)
					{
						MYLOG("BLUES", "Found US");
						if (g_lorawan_settings.lora_region != 5)
						{
							MYLOG("BLUES", "Switch to band 5");
							g_lorawan_settings.lora_region = 5;
							init_lorawan(true);
						}
					}
					else if (strcmp(str_value, "AU") == 0)
					{
						MYLOG("BLUES", "Found AU");
						if (g_lorawan_settings.lora_region != 6)
						{
							MYLOG("BLUES", "Switch to band 6");
							g_lorawan_settings.lora_region = 6;
							init_lorawan(true);
						}
					}
					else if ((strcmp(str_value, "DE") == 0) ||
							 (strcmp(str_value, "FR") == 0) ||
							 (strcmp(str_value, "IT") == 0) ||
							 (strcmp(str_value, "NL") == 0) ||
							 (strcmp(str_value, "GB") == 0))
					{
						MYLOG("BLUES", "Found Europe");
						if (g_lorawan_settings.lora_region != 4)
						{
							MYLOG("BLUES", "Switch to band 4");
							g_lorawan_settings.lora_region = 4;
							init_lorawan(true);
						}
					}
				}

				float blues_latitude;
				float blues_longitude;
				float blues_altitude = 0;

				// If no location from GNSS use the tower location
				if (!got_gnss_location)
				{
					if (rak_blues.get_float_entry((char *)"lat", blues_latitude))
					{
						if (rak_blues.get_float_entry((char *)"lon", blues_longitude))
						{

							if ((blues_latitude == 0.0) && (blues_longitude == 0.0))
							{
								MYLOG("BLUES", "No valid GPS data, report no location");
							}
							else
							{
								MYLOG("BLUES", "Got tower location Lat %.6f Long %0.6f", blues_latitude, blues_longitude);
								g_solution_data.addGNSS_6(LPP_CHANNEL_GPS, (int32_t)(blues_latitude * 10000000), (int32_t)(blues_longitude * 10000000), (int32_t)blues_altitude);
								g_solution_data.addPresence(LPP_CHANNEL_GPS_TOWER, true);
								result = true;
							}
						}
					}
				}

				if (rak_blues.has_entry((char *)"time"))
				{
					if (rak_blues.get_uint32_entry((char *)"time", current_card_time))
					{
						MYLOG("BLUES", "Last card time was %ld", current_card_time);
					}
				}
			}
			request_success = true;
			break;
		}
	}
	if (!request_success)
	{
		MYLOG("BLUES", "card.time request failed");
	}
	return result;
}

void blues_card_restore(void)
{
	for (int try_send = 0; try_send < 5; try_send++)
	{
		if (rak_blues.start_req((char *)"hub.status"))
		{
			rak_blues.add_bool_entry((char *)"delete", true);
			rak_blues.add_bool_entry((char *)"connected", true);
			if (rak_blues.send_req())
			{
				break;
			}
		}
	}
}

/**
 * @brief Enable ATTN interrupt
 * 		At the moment enables only the alarm on motion
 *
 * @param motion true enable motion interrupt, false enable location interrupt
 * @return true if ATTN could be enabled
 * @return false if ATTN could not be enabled
 */
bool blues_enable_attn(bool motion)
{
	bool request_success = false;

	// Disarm before making changes
	blues_disable_attn();

	if (motion)
	{
		MYLOG("BLUES", "Enable ATTN on motion");
		for (int try_send = 0; try_send < 5; try_send++)
		{
			if (rak_blues.start_req((char *)"card.attn"))
			{
				rak_blues.add_string_entry((char *)"mode", (char *)"motion");
				if (rak_blues.send_req(g_at_query_buf, ATQUERY_SIZE))
				{
					MYLOG("BLUES", "card.attn mode returned: %s", g_at_query_buf);
					request_success = true;
					break;
				}
			}
		}
		if (!request_success)
		{
			MYLOG("BLUES", "card.attn request failed");
			return false;
		}
		request_success = false;

		MYLOG("BLUES", "Arm ATTN on motion");
		for (int try_send = 0; try_send < 5; try_send++)
		{
			if (rak_blues.start_req((char *)"card.attn"))
			{
				rak_blues.add_string_entry((char *)"mode", (char *)"arm");
				if (rak_blues.send_req())
				{
					request_success = true;
					break;
				}
			}
		}
		if (!request_success)
		{
			MYLOG("BLUES", "card.attn request failed");
			return false;
		}
		delay(250);
		MYLOG("BLUES", "Attach interrupt on motion");
		detachInterrupt(WB_IO5);
		attachInterrupt(WB_IO5, blues_attn_cb, RISING);
	}
	else
	{
		request_success = false;
		MYLOG("BLUES", "Enable ATTN on location");
		for (int try_send = 0; try_send < 5; try_send++)
		{
			if (rak_blues.start_req((char *)"card.attn"))
			{
				rak_blues.add_string_entry((char *)"mode", (char *)"location");
				if (rak_blues.send_req(g_at_query_buf, ATQUERY_SIZE))
				{
					MYLOG("BLUES", "card.attn mode returned: %s", g_at_query_buf);
					request_success = true;
					break;
				}
			}
		}
		if (!request_success)
		{
			MYLOG("BLUES", "card.attn request failed");
			return false;
		}

		request_success = false;
		MYLOG("BLUES", "Arm ATTN on location");
		for (int try_send = 0; try_send < 5; try_send++)
		{
			if (rak_blues.start_req((char *)"card.attn"))
			{
				rak_blues.add_string_entry((char *)"mode", (char *)"arm");
				if (rak_blues.send_req())
				{
					request_success = true;
					break;
				}
			}
		}
		if (!request_success)
		{
			MYLOG("BLUES", "card.attn request failed");
			return false;
		}
		MYLOG("BLUES", "Attach interrupt on location");
		detachInterrupt(WB_IO5);
		attachInterrupt(WB_IO5, blues_attn_cb, RISING);
	}
	return true;
}

/**
 * @brief Disable ATTN interrupt
 *
 * @return true if ATTN could be disabled
 * @return false if ATTN could not be disabled
 */
bool blues_disable_attn(void)
{
	MYLOG("BLUES", "Disable ATTN");
	detachInterrupt(WB_IO5);

	bool request_success = false;
	for (int try_send = 0; try_send < 5; try_send++)
	{
		if (rak_blues.start_req((char *)"card.attn"))
		{
			rak_blues.add_string_entry((char *)"mode", (char *)"disarm,-all");
			if (rak_blues.send_req())
			{
				request_success = true;
				break;
			}
		}
	}
	if (!request_success)
	{
		MYLOG("BLUES", "card.attn request failed");
	}
	return request_success;
}

char attn_msg[256];
/**
 * @brief Get the reason for the ATTN interrupt
 *  /// \todo work in progress
 * @return uint8_t reason
 * 			0 = unknown reason
 *			1 = motion
 *			2 = location fix
 *			3 = motion & location fix
 */
uint8_t blues_attn_reason(void)
{
	bool request_success = false;
	uint8_t result = 0;
	for (int try_send = 0; try_send < 5; try_send++)
	{
		if (rak_blues.start_req((char *)"card.attn"))
		{
			if (rak_blues.send_req(g_at_query_buf, ATQUERY_SIZE))
			{
				MYLOG("BLUES", "card.attn mode returned: %s", g_at_query_buf);
				request_success = true;
				break;
			}
		}
	}
	if (request_success)
	{
		MYLOG("BLUES", "card.attn check returned: %s", g_at_query_buf);
		if (rak_blues.has_entry((char *)"files"))
		{
			rak_blues.get_string_entry_from_array((char *)"files", attn_msg, 255);
			MYLOG("BLUES", "card.attn files: %s", attn_msg);
			String motion_str = "motion";
			String location_str = "location";
			String response_str = String(attn_msg);

			if (response_str.indexOf(motion_str) != -1)
			{
				MYLOG("BLUES", "card.attn for MOTION");
				result += 1;
			}
			if (response_str.indexOf(location_str) != -1)
			{
				MYLOG("BLUES", "card.attn for LOCATION");
				result += 2;
			}
		}
		else
		{
			MYLOG("BLUES", "card.attn files missing");
		}
	}
	else
	{
		MYLOG("BLUES", "Request creation failed");
	}

	return result;
}

/**
 * @brief Callback for ATTN interrupt
 *       Wakes up the app_handler with an BLUES_ATTN event
 *
 */
void blues_attn_cb(void)
{
	api_wake_loop(BLUES_ATTN);
}

/**
 * @brief Check connection to cellular network
 *
 * @return true if connection is/was established
 * @return false if no connection
 */
bool blues_hub_connected(void)
{
	bool request_success = false;
	bool cellular_connected = false;
	for (int try_send = 0; try_send < 5; try_send++)
	{
		if (rak_blues.start_req((char *)"card.wireless"))
		{
			if (rak_blues.send_req())
			{
				if (rak_blues.has_entry((char *)"net"))
				{
					if (rak_blues.has_nested_entry((char *)"net", (char *)"band"))
					{
						cellular_connected = true;
					}
					request_success = true;
					break;
				}
			}
		}
	}
	if (!request_success)
	{
		MYLOG("BLUES", "card.wireless request failed");
		return false;
	}
	return cellular_connected;
}