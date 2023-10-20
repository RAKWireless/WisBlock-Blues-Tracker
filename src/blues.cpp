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

/** Flag if GNSS is in continuous or periodic mode */
bool gnss_continuous = true;

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
	// Get the ProductUID from the saved settings
	// If no settings are found, use NoteCard internal settings!
	if (read_blues_settings())
	{
		bool request_success = false;

		MYLOG("BLUES", "Found saved settings, override NoteCard internal settings!");
		if (memcmp(g_blues_settings.product_uid, "com.my-company.my-name", 22) == 0)
		{
			MYLOG("BLUES", "No Product ID saved");
			AT_PRINTF(":EVT NO PUID");
			memcpy(g_blues_settings.product_uid, PRODUCT_UID, 33);
		}

		MYLOG("BLUES", "Set Product ID and connection mode");
		for (int try_send = 0; try_send < 3; try_send++)
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
				// // Set sync time to 20 times the sensor read time
				// add_int32_entry((char *)"seconds", (g_lorawan_settings.send_repeat_time * 20 / 1000));
				// add_bool_entry((char *)"heartbeat", true);

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

		// Enable motion trigger
		for (int try_send = 0; try_send < 3; try_send++)
		{
			if (rak_blues.start_req((char *)"card.motion.mode"))
			{
				rak_blues.add_bool_entry((char *)"start", true);

				// Set sensitivity 25Hz, +/- 16G range, 7.8 milli-G sensitivity
				rak_blues.add_int32_entry((char *)"sensitivity", 1);
				// Set sensitivity 1.6Hz, +/- 2G range, 1 milli-G sensitivity
				// add_int32_entry((char *)"sensitivity", -1);

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

		// Enable GNSS continuous mode
		for (int try_send = 0; try_send < 3; try_send++)
		{
			if (blues_switch_gnss_mode(false))
			{
				request_success = true;
				break;
			}

			request_success = false;
			MYLOG("BLUES", "card.location.mode delete last location");
			// Clear last GPS location
			for (int try_send = 0; try_send < 3; try_send++)
			{
				rak_blues.start_req((char *)"card.location.mode");
				rak_blues.add_bool_entry((char *)"delete", true);
				if (rak_blues.send_req())
				{
					request_success = true;
					break;
				}
			}
			if (!request_success)
			{
				MYLOG("BLUES", "card.location.mode delete last location request failed");
			}
		}
		if (!request_success)
		{
			MYLOG("BLUES", "card.location.mode request failed");
			return false;
		}
		request_success = false;

		/// \todo reset attn signal needs rework
		pinMode(WB_IO5, INPUT);
		if (g_blues_settings.motion_trigger)
		{
			if (rak_blues.start_req((char *)"card.attn"))
			{
				rak_blues.add_string_entry((char *)"mode", (char *)"disarm");
				if (!rak_blues.send_req())
				{
					MYLOG("BLUES", "card.attn request failed");
				}

				/// \todo reset attn signal needs rework
				// if (!blues_enable_attn())
				// {
				// 	MYLOG("BLUES", "card.attn enable failed");
				// 	return false;
				// }
			}
		}
		else
		{
			MYLOG("BLUES", "card.attn request failed");
			// return false;
		}

		MYLOG("BLUES", "Set SIM and APN");
		for (int try_send = 0; try_send < 3; try_send++)
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

#if IS_V2 == 1
		// Only for V2 cards, setup the WiFi network
		MYLOG("BLUES", "Set WiFi");
		for (int try_send = 0; try_send < 3; try_send++)
		{
			if (rak_blues.start_req((char *)"card.wifi"))
			{
				rak_blues.add_string_entry((char *)"ssid", (char *)"-");
				rak_blues.add_string_entry((char *)"password", (char *)"-");
				rak_blues.add_string_entry((char *)"name", (char *)"RAK-");
				rak_blues.add_string_entry((char *)"org", (char *)"RAK-PH");
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
		request_success = false;
#endif
	}

	for (int try_send = 0; try_send < 3; try_send++)
	{
		if (rak_blues.start_req((char *)"card.version"))
		{
			if (rak_blues.send_req())
			{
				break;
			}
		}
	}
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
		if (!rak_blues.send_req())
		{
			MYLOG("BLUES", "Send request failed");
			return false;
		}
		return true;
	}
	return false;
}

/**
 * @brief Request NoteHub status, only for debug purposes
 *
 */
void blues_hub_status(void)
{
	bool request_success = false;
	for (int try_send = 0; try_send < 3; try_send++)
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

bool blues_switch_gnss_mode(bool continuous_on)
{
	bool request_success = false;
	if (continuous_on != gnss_continuous)
	{
		MYLOG("BLUES", "Change of GNSS mode, switch off first");
		// Enable motion trigger
		for (int try_send = 0; try_send < 3; try_send++)
		{
			if (rak_blues.start_req((char *)"card.location.mode"))
			{
				// GNSS mode off
				rak_blues.add_string_entry((char *)"mode", (char *)"off");
			}
			if (rak_blues.send_req())
			{
				request_success = true;
				break;
			}
		}
		if (!request_success)
		{
			MYLOG("BLUES", "card.location.mode request failed");
			return false;
		}
	}

	request_success = false;
	MYLOG("BLUES", "Set location mode %s", continuous_on ? "continuous" : "periodic");
	for (int try_send = 0; try_send < 3; try_send++)
	{
		if (rak_blues.start_req((char *)"card.location.mode"))
		{
			if (continuous_on)
			{
				gnss_continuous = true;
				// Continous GNSS mode
				rak_blues.add_string_entry((char *)"mode", (char *)"continuous");
				rak_blues.add_int32_entry((char *)"threshold", 0);
			}
			else
			{
				gnss_continuous = false;
				// Periodic GNSS mode
				rak_blues.add_string_entry((char *)"mode", (char *)"periodic");
				rak_blues.add_int32_entry((char *)"threshold", 0);
			}
			// Set location acquisition time to the sensor read time
			rak_blues.add_int32_entry((char *)"seconds", (g_lorawan_settings.send_repeat_time / 1000 / 2));
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

// /** Counter for GNSS inactive or searching before we switch to tower location */
uint16_t old_location = 0;

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
	bool got_gnss_time = false;
	bool got_card_time = false;

	for (int try_send = 0; try_send < 3; try_send++)
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
						old_location = 6;
					}
					// if (gnss_status.indexOf("inactive") > 0)
					// {
					// 	MYLOG("BLUES", "GNSS is inactive!");
					// 	old_location++;
					// }
					if (gnss_status.indexOf("updated") > 0)
					{
						MYLOG("BLUES", "GNSS is updated!");
						old_location = 0;
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
							if (old_location <= 5)
							{

								MYLOG("BLUES", "Got location Lat %.6f Long %0.6f", blues_latitude, blues_longitude);
								g_solution_data.addGNSS_6(LPP_CHANNEL_GPS, (int32_t)(blues_latitude * 10000000), (int32_t)(blues_longitude * 10000000), (int32_t)blues_altitude);
								g_solution_data.addPresence(LPP_CHANNEL_GPS_TOWER, false);
								// if (gnss_continuous)
								// {
								// 	// We got a location, switch to periodic mode
								// 	blues_switch_gnss_mode(false);
								// }
							}
							else
							{
								MYLOG("BLUES", "Got location, but it was not updated");
							}
							result = true;
						}

						if (rak_blues.has_entry((char *)"time"))
						{
							if (rak_blues.get_uint32_entry((char *)"time", last_gnss_update))
							{
								MYLOG("BLUES", "Last GNSS update was %ld", last_gnss_update);
								got_gnss_time = true;
							}
						}
					}
				}
			}

			// // If the location was not updated for the last 5 requests, use tower location instead
			// if (old_location > 5)
			// {
			// 	got_gnss_location = false;
			// }
			request_success = true;
			break;
		}
	}
	if (!request_success)
	{
		MYLOG("BLUES", "card.location request failed");
	}

	request_success = false;
	// if (!result)
	// {
	// 	if (!gnss_continuous)
	// 	{
	// 		// Switch GNSS to continuous to get a location
	// 		blues_switch_gnss_mode(false);
	// 	}
	// }

	for (int try_send = 0; try_send < 3; try_send++)
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
						got_card_time = true;
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

	// Check the age of the GNSS location
	if (got_card_time && got_gnss_time)
	// if (got_gnss_location)
	{
		if ((current_card_time - last_gnss_update) > (g_lorawan_settings.send_repeat_time / 1000 / 2))
		{
			request_success = false;
			MYLOG("BLUES", "Last GNSS location is older than acquisition time (%ld), delete old location", (current_card_time - last_gnss_update));
			// Clear last GPS location
			for (int try_send = 0; try_send < 3; try_send++)
			{
				rak_blues.start_req((char *)"card.location.mode");
				rak_blues.add_bool_entry((char *)"delete", true);
				if (rak_blues.send_req())
				{
					request_success = true;
					break;
				}
			}
			if (!request_success)
			{
				MYLOG("BLUES", "card.location.mode delete last location request failed");
			}
		}
	}
	return result;
}

void blues_card_restore(void)
{
	rak_blues.start_req((char *)"hub.status");
	rak_blues.add_bool_entry((char *)"delete", true);
	rak_blues.add_bool_entry((char *)"connected", true);
	rak_blues.send_req();
}

/**
 * @brief Enable ATTN interrupt
 * 		At the moment enables only the alarm on motion
 *
 * @return true if ATTN could be enabled
 * @return false if ATTN could not be enabled
 */
bool blues_enable_attn(void)
{
	MYLOG("BLUES", "Enable ATTN on motion");
	if (rak_blues.start_req((char *)"card.attn"))
	{
		rak_blues.add_string_entry((char *)"mode", (char *)"motion");
		if (!rak_blues.send_req())
		{
			MYLOG("BLUES", "card.attn request failed");
			return false;
		}
	}
	else
	{
		MYLOG("BLUES", "Request creation failed");
	}
	attachInterrupt(WB_IO5, blues_attn_cb, RISING);

	MYLOG("BLUES", "Arm ATTN on motion");
	if (rak_blues.start_req((char *)"card.attn"))
	{
		rak_blues.add_string_entry((char *)"mode", (char *)"arm");
		if (!rak_blues.send_req())
		{
			MYLOG("BLUES", "card.attn request failed");
			return false;
		}
	}
	else
	{
		MYLOG("BLUES", "Request creation failed");
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
	MYLOG("BLUES", "Disable ATTN on motion");
	detachInterrupt(WB_IO5);

	if (rak_blues.start_req((char *)"card.attn"))
	{
		rak_blues.add_string_entry((char *)"mode", (char *)"disarm");
		if (!rak_blues.send_req())
		{
			MYLOG("BLUES", "card.attn request failed");
		}
	}
	else
	{
		MYLOG("BLUES", "Request creation failed");
	}
	if (rak_blues.start_req((char *)"card.attn"))
	{
		rak_blues.add_string_entry((char *)"mode", (char *)"-motion");
		if (!rak_blues.send_req())
		{
			MYLOG("BLUES", "card.attn request failed");
		}
	}
	else
	{
		MYLOG("BLUES", "Request creation failed");
	}

	return true;
}

/**
 * @brief Get the reason for the ATTN interrupt
 *  /// \todo work in progress
 * @return String reason /// \todo return value not final yet
 */
String blues_attn_reason(void)
{
	return "";
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