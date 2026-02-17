/*
	Copyright(c) 2021-2026 jvde.github@gmail.com

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "Keys.h"

namespace AIS
{

	const std::vector<std::vector<std::string>> KeyMap = {
		{"class", "class", "class", "", "", ""},										// KEY_CLASS
		{"device", "device", "device", "", "", ""},										// KEY_DEVICE
		{"driver", "driver", "driver", "", "", ""},										// KEY_DRIVER
		{"error", "error", "error", "", "", ""},										// KEY_ERROR
		{"scaled", "", "scaled", "", "", ""},											// KEY_SCALED
		{"channel", "channel", "channel", "", "", ""},									// KEY_CHANNEL
		{"hardware", "hardware", "hardware", "", "", ""},								// KEY_HARDWARE
		{"ipv4", "ipv4", "ipv4", "", "", ""},											// KEY_IPV4
		{"signalpower", "signalpower", "signalpower", "", "", ""},						// KEY_SIGNAL_POWER
		{"ppm", "ppm", "ppm", "", "", ""},												// KEY_PPM
		{"rxtime", "rxtime", "rxtime", "rxtime", "", ""},								// KEY_RXTIME
		{"rxuxtime", "rxuxtime", "rxuxtime", "rxuxtime", "", ""},						// KEY_RXUXTIME
		{"nmea", "nmea", "nmea", "", "", ""},											// KEY_NMEA
		{"eta", "", "eta", "", "", ""},													// KEY_ETA
		{"shiptype_text", "", "shiptype_text", "", "", ""},								// KEY_SHIPTYPE_TEXT
		{"aid_type_text", "", "aid_type_text", "", "", ""},								// KEY_AID_TYPE_TEXT
		{"ssc", "", "ssc", "", "", ""},													// KEY_SAMPLE_START_COUNT
		{"sl", "", "sl", "", "", ""},													// KEY_SAMPLE_LENGTH
		{"station_id", "", "station_id", "", "", ""},									// KEY_STATION_ID
		{"version", "", "version", "", "", ""},											// KEY_VERSION
		{"", "", "", "", "about", ""},													// KEY_SETTING_ABOUT
		{"", "", "", "", "address", ""},												// KEY_SETTING_ADDRESS
		{"", "", "", "", "active", ""},													// KEY_SETTING_ACTIVE
		{"", "", "", "", "afc_wide", ""},												// KEY_SETTING_AFC_WIDE
		{"", "", "", "", "agc", ""},													// KEY_SETTING_AGC
		{"", "", "", "", "airspy", ""},													// KEY_SETTING_AIRSPY
		{"", "", "", "", "airspyhf", ""},												// KEY_SETTING_AIRSPYHF
		{"", "", "", "", "allow_type", ""},												// KEY_SETTING_ALLOW_TYPE
		{"", "", "", "", "antenna", ""},												// KEY_SETTING_ANTENNA
		{"", "", "", "", "author", ""},													// KEY_SETTING_AUTHOR
		{"", "", "", "", "backup", ""},													// KEY_SETTING_BACKUP
		{"", "", "", "", "bandwidth", ""},												// KEY_SETTING_BANDWIDTH
		{"", "", "", "", "baudrate", ""},												// KEY_SETTING_BAUDRATE
		{"", "", "", "", "biastee", ""},												// KEY_SETTING_BIASTEE
		{"", "", "", "", "binary", ""},													// KEY_SETTING_BINARY
		{"", "", "", "", "block_type", ""},												// KEY_SETTING_BLOCK_TYPE
		{"", "", "", "", "broadcast", ""},												// KEY_SETTING_BROADCAST
		{"", "", "", "", "buffer_count", ""},											// KEY_SETTING_BUFFER_COUNT
		{"", "", "", "", "callsign", ""},												// KEY_SETTING_CALLSIGN
		{"", "", "", "", "cdn", ""},													// KEY_SETTING_CDN
		{"", "", "", "", "channel", ""},												// KEY_SETTING_CHANNEL
		{"", "", "", "", "client_id", ""},												// KEY_SETTING_CLIENT_ID
		{"", "", "", "", "config", ""},													// KEY_SETTING_CONFIG
		{"", "", "", "", "context", ""},												// KEY_SETTING_CONTEXT
		{"", "", "", "", "crc_check", ""},												// KEY_SETTING_CRC_CHECK
		{"", "", "", "", "cutoff", ""},													// KEY_SETTING_CUTOFF
		{"", "", "", "", "decoder", ""},												// KEY_SETTING_DECODER
		{"", "", "", "", "description", ""},											// KEY_SETTING_DESCRIPTION
		{"", "", "", "", "device", ""},													// KEY_SETTING_DEVICE
		{"", "", "", "", "droop", ""},													// KEY_SETTING_DROOP
		{"", "", "", "", "dump", ""},													// KEY_SETTING_DUMP
		{"", "", "", "", "dump_file", ""},												// KEY_SETTING_DUMP_FILE
		{"", "", "", "", "endpoint", ""},												// KEY_SETTING_ENDPOINT
		{"", "", "", "", "file", ""},													// KEY_SETTING_FILE
		{"", "", "", "", "filter", ""},													// KEY_SETTING_FILTER
		{"", "", "", "", "own_interval", ""},											// KEY_SETTING_OWN_INTERVAL
		{"", "", "", "", "position_interval", ""},										// KEY_SETTING_POSITION_INTERVAL
		{"", "", "", "", "unique", ""},													// KEY_SETTING_UNIQUE
		{"", "", "", "", "format", ""},													// KEY_SETTING_FORMAT
		{"", "", "", "", "fp_ds", ""},													// KEY_SETTING_FP_DS
		{"", "", "", "", "freqoffset", ""},												// KEY_SETTING_FREQOFFSET
		{"", "", "", "", "gain", ""},													// KEY_SETTING_GAIN
		{"", "", "", "", "gain_mode", ""},												// KEY_SETTING_GAIN_MODE
		{"", "", "", "", "geojson", ""},												// KEY_SETTING_GEOJSON
		{"", "", "", "", "grdb", ""},													// KEY_SETTING_GRDB
		{"", "", "", "", "groups_in", ""},												// KEY_SETTING_GROUPS_IN
		{"", "", "", "", "gzip", ""},													// KEY_SETTING_GZIP
		{"", "", "", "", "hackrf", ""},													// KEY_SETTING_HACKRF
		{"", "", "", "", "history", ""},												// KEY_SETTING_HISTORY
		{"", "", "", "", "host", ""},													// KEY_SETTING_HOST
		{"", "", "", "", "http", ""},													// KEY_SETTING_HTTP
		{"", "", "", "", "hydrasdr", ""},												// KEY_SETTING_HYDRASDR
		{"", "", "", "", "id", ""},														// KEY_SETTING_ID
		{"", "", "", "", "input", ""},													// KEY_SETTING_INPUT
		{"", "", "", "", "interface", ""},												// KEY_SETTING_INTERFACE
		{"", "", "", "", "interval", ""},												// KEY_SETTING_INTERVAL
		{"", "", "", "", "json", ""},													// KEY_SETTING_JSON
		{"", "", "", "", "json_full", ""},												// KEY_SETTING_JSON_FULL
		{"", "", "", "", "keep_alive", ""},												// KEY_SETTING_KEEP_ALIVE
		{"", "", "", "", "kml", ""},													// KEY_SETTING_KML
		{"", "", "", "", "lat", ""},													// KEY_SETTING_LAT
		{"", "", "", "", "linearity", ""},												// KEY_SETTING_LINEARITY
		{"", "", "", "", "lna", ""},													// KEY_SETTING_LNA
		{"", "", "", "", "lnastate", ""},												// KEY_SETTING_LNASTATE
		{"", "", "", "", "lon", ""},													// KEY_SETTING_LON
		{"", "", "", "", "log", ""},													// KEY_SETTING_LOG
		{"", "", "", "", "mbtiles", ""},												// KEY_SETTING_MBTILES
		{"", "", "", "", "mboverlay", ""},												// KEY_SETTING_MBOVERLAY
		{"", "", "", "", "meta", ""},													// KEY_SETTING_META
		{"", "", "", "", "mixer", ""},													// KEY_SETTING_MIXER
		{"", "", "", "", "mode", ""},													// KEY_SETTING_MODE
		{"", "", "", "", "model", ""},													// KEY_SETTING_MODEL
		{"", "", "", "", "msg", ""},													// KEY_SETTING_MSG
		{"", "", "", "", "msgformat", ""},												// KEY_SETTING_MSGFORMAT
		{"", "", "", "", "msg_output", ""},												// KEY_SETTING_MSG_OUTPUT
		{"", "", "", "", "mqtt", ""},													// KEY_SETTING_MQTT
		{"", "", "", "", "nmea2000", ""},												// KEY_SETTING_NMEA2000
		{"", "", "", "", "nmea_refresh", ""},											// KEY_SETTING_NMEA_REFRESH
		{"", "", "", "", "origin", ""},													// KEY_SETTING_ORIGIN
		{"", "", "", "", "output", ""},													// KEY_SETTING_OUTPUT
		{"", "", "", "", "own_mmsi", ""},												// KEY_SETTING_OWN_MMSI
		{"", "", "", "", "password", ""},												// KEY_SETTING_PASSWORD
		{"", "", "", "", "persist", ""},												// KEY_SETTING_PERSIST
		{"", "", "", "", "plugin", ""},													// KEY_SETTING_PLUGIN
		{"", "", "", "", "plugin_dir", ""},												// KEY_SETTING_PLUGIN_DIR
		{"", "", "", "", "port", ""},													// KEY_SETTING_PORT
		{"", "", "", "", "port_min", ""},												// KEY_SETTING_PORT_MIN
		{"", "", "", "", "port_max", ""},												// KEY_SETTING_PORT_MAX
		{"", "", "", "", "preamp", ""},													// KEY_SETTING_PREAMP
		{"", "", "", "", "program", ""},												// KEY_SETTING_PROGRAM
		{"", "", "", "", "prome", ""},													// KEY_SETTING_PROME
		{"", "", "", "", "protocol", ""},												// KEY_SETTING_PROTOCOL
		{"", "", "", "", "protocols", ""},												// KEY_SETTING_PROTOCOLS
		{"", "", "", "", "ps_ema", ""},													// KEY_SETTING_PS_EMA
		{"", "", "", "", "realtime", ""},												// KEY_SETTING_REALTIME
		{"", "", "", "", "receiver", ""},												// KEY_SETTING_RECEIVER
		{"", "", "", "", "reset", ""},													// KEY_SETTING_RESET
		{"", "", "", "", "response", ""},												// KEY_SETTING_RESPONSE
		{"", "", "", "", "reuse_port", ""},												// KEY_SETTING_REUSE_PORT
		{"", "", "", "", "rtlagc", ""},													// KEY_SETTING_RTLAGC
		{"", "", "", "", "rtlsdr", ""},													// KEY_SETTING_RTLSDR
		{"", "", "", "", "rtltcp", ""},													// KEY_SETTING_RTLTCP
		{"", "", "", "", "sample_rate", ""},											// KEY_SETTING_SAMPLE_RATE
		{"", "", "", "", "screen", ""},													// KEY_SETTING_SCREEN
		{"", "", "", "", "sdrplay", ""},												// KEY_SETTING_SDRPLAY
		{"", "", "", "", "sensitivity", ""},											// KEY_SETTING_SENSITIVITY
		{"", "", "", "", "serial", ""},													// KEY_SETTING_SERIAL
		{"", "", "", "", "init_seq", ""},												// KEY_SETTING_SERIAL_INIT_SEQUENCE
		{"", "", "", "", "serialport", ""},												// KEY_SETTING_SERIALPORT
		{"", "", "", "", "server", ""},													// KEY_SETTING_SERVER
		{"", "", "", "", "share_loc", ""},												// KEY_SETTING_SHARE_LOC
		{"", "", "", "", "sharing", ""},												// KEY_SETTING_SHARING
		{"", "", "", "", "sharing_key", ""},											// KEY_SETTING_SHARING_KEY
		{"", "", "", "", "soapysdr", ""},												// KEY_SETTING_SOAPYSDR
		{"", "", "", "", "soxr", ""},													// KEY_SETTING_SOXR
		{"", "", "", "", "spyserver", ""},												// KEY_SETTING_SPYSERVER
		{"", "", "", "", "src", ""},													// KEY_SETTING_SRC
		{"", "", "", "", "station", ""},												// KEY_SETTING_STATION
		{"", "", "", "", "station_link", ""},											// KEY_SETTING_STATION_LINK
		{"", "", "", "", "stream", ""},													// KEY_SETTING_STREAM
		{"", "", "", "", "style", ""},													// KEY_SETTING_STYLE
		{"", "", "", "", "tcp", ""},													// KEY_SETTING_TCP
		{"", "", "", "", "tcp_listener", ""},											// KEY_SETTING_TCP_LISTENER
		{"", "", "", "", "test", ""},													// KEY_SETTING_TEST
		{"", "", "", "", "timeout", ""},												// KEY_SETTING_TIMEOUT
		{"", "", "", "", "threshold", ""},												// KEY_SETTING_THRESHOLD
		{"", "", "", "", "topic", ""},													// KEY_SETTING_TOPIC
		{"", "", "", "", "tuner", ""},													// KEY_SETTING_TUNER
		{"", "", "", "", "udp", ""},													// KEY_SETTING_UDP
		{"", "", "", "", "udpserver", ""},												// KEY_SETTING_UDPSERVER
		{"", "", "", "", "url", ""},													// KEY_SETTING_URL
		{"", "", "", "", "username", ""},												// KEY_SETTING_USERNAME
		{"", "", "", "", "userpwd", ""},												// KEY_SETTING_USERPWD
		{"", "", "", "", "uuid", ""},													// KEY_SETTING_UUID
		{"", "", "", "", "verbose", ""},												// KEY_SETTING_VERBOSE
		{"", "", "", "", "verbose_time", ""},											// KEY_SETTING_VERBOSE_TIME
		{"", "", "", "", "version", ""},												// KEY_SETTING_VERSION
		{"", "", "", "", "vga", ""},													// KEY_SETTING_VGA
		{"", "", "", "", "wavfile", ""},												// KEY_SETTING_WAVFILE
		{"", "", "", "", "qos", ""},													// KEY_SETTING_QOS
		{"", "", "", "", "zlib", ""},													// KEY_SETTING_ZLIB
		{"", "", "", "", "zmq", ""},													// KEY_SETTING_ZMQ
		{"accuracy", "", "accuracy", "", "", ""},										// KEY_ACCURACY
		{"ack_required", "", "ack_required", "", "", ""},								// KEY_ACK_REQUIRED
		{"addressed", "", "", "", "", ""},												// KEY_ADDRESSED
		{"ai_available", "", "", "", "", ""},											// KEY_AI_AVAILABLE
		{"aid_type", "", "", "", "", ""},												// KEY_AID_TYPE
		{"airtemp", "", "", "", "", ""},												// KEY_AIRTEMP
		{"ais_version", "", "", "", "", ""},											// KEY_AIS_VERSION
		{"alt", "", "", "", "", ""},													// KEY_ALT
		{"ana_int", "", "", "", "", ""},												// KEY_ANA_INT
		{"ana_ext1", "", "", "", "", ""},												// KEY_ANA_EXT1
		{"ana_ext2", "", "", "", "", ""},												// KEY_ANA_EXT2
		{"asm_battery_status", "", "", "", "", ""},										// KEY_ASM_BATTERY_STATUS
		{"asm_current_data", "", "", "", "", ""},										// KEY_ASM_CURRENT_DATA
		{"asm_light_status", "", "", "", "", ""},										// KEY_ASM_LIGHT_STATUS
		{"asm_off_position_status", "", "", "", "", ""},								// KEY_ASM_OFF_POSITION_STATUS
		{"asm_power_supply_type", "", "", "", "", ""},									// KEY_ASM_POWER_SUPPLY_TYPE
		{"asm_sub_app_id", "", "", "", "", ""},											// KEY_ASM_SUB_APP_ID
		{"asm_voltage_data", "", "", "", "", ""},										// KEY_ASM_VOLTAGE_DATA
		{"assigned", "", "", "", "", ""},												// KEY_ASSIGNED
		{"band", "", "", "", "", ""},													// KEY_BAND
		{"band_a", "", "", "", "", ""},													// KEY_BAND_A
		{"band_b", "", "", "", "", ""},													// KEY_BAND_B
		{"beam", "", "", "", "", ""},													// KEY_BEAM
		{"callsign", "", "callsign", "callsign", "", ""},								// KEY_CALLSIGN
		{"cdepth2", "", "", "", "", ""},												// KEY_CDEPTH2
		{"cdepth3", "", "", "", "", ""},												// KEY_CDEPTH3
		{"cdir", "", "", "", "", ""},													// KEY_CDIR
		{"cdir2", "", "", "", "", ""},													// KEY_CDIR2
		{"cdir3", "", "", "", "", ""},													// KEY_CDIR3
		{"channel_a", "", "", "", "", ""},												// KEY_CHANNEL_A
		{"channel_b", "", "", "", "", ""},												// KEY_CHANNEL_B
		{"cloud_amount_low", "", "", "", "", ""},										// KEY_CLOUD_AMOUNT_LOW
		{"cloud_base_height", "", "", "", "", ""},										// KEY_CLOUD_BASE_HEIGHT
		{"cloud_cover_total", "", "", "", "", ""},										// KEY_CLOUD_COVER_TOTAL
		{"cloud_type_high", "", "", "", "", ""},										// KEY_CLOUD_TYPE_HIGH
		{"cloud_type_low", "", "", "", "", ""},											// KEY_CLOUD_TYPE_LOW
		{"cloud_type_middle", "", "", "", "", ""},										// KEY_CLOUD_TYPE_MIDDLE
		{"country", "", "", "", "", ""},												// KEY_COUNTRY
		{"country_code", "", "", "", "", ""},											// KEY_COUNTRY_CODE
		{"course", "", "", "course", "", ""},											// KEY_COURSE
		{"course_q", "", "", "", "", ""},												// KEY_COURSE_Q
		{"cs", "", "", "", "", ""},														// KEY_CS
		{"cspeed", "", "", "", "", ""},													// KEY_CSPEED
		{"cspeed2", "", "", "", "", ""},												// KEY_CSPEED2
		{"cspeed3", "", "", "", "", ""},												// KEY_CSPEED3
		{"crew_count", "", "", "", "", ""},												// KEY_CREW_COUNT
		{"dac", "", "", "", "", ""},													// KEY_DAC
		{"data", "", "", "", "", ""},													// KEY_DATA
		{"day", "", "", "", "", ""},													// KEY_DAY
		{"dest_mmsi", "", "", "", "", ""},												// KEY_DEST_MMSI
		{"dest1", "", "", "", "", ""},													// KEY_DEST1
		{"dest2", "", "", "", "", ""},													// KEY_DEST2
		{"destination", "", "destination", "destination", "", ""},						// KEY_DESTINATION
		{"dewpoint", "", "", "", "", ""},												// KEY_DEWPOINT
		{"display", "", "", "", "", ""},												// KEY_DISPLAY
		{"draught", "", "", "draught", "", ""},											// KEY_DRAUGHT
		{"dsc", "", "", "", "", ""},													// KEY_DSC
		{"dte", "", "", "", "", ""},													// KEY_DTE
		{"epfd", "", "epfd", "", "", ""},												// KEY_EPFD
		{"epfd_text", "", "epfd_text", "", "", ""},										// KEY_EPFD_TEXT
		{"fid", "", "", "", "", ""},													// KEY_FID
		{"gnss", "", "", "", "", ""},													// KEY_GNSS
		{"hazard", "", "", "", "", ""},													// KEY_HAZARD
		{"heading", "", "heading", "heading", "", ""},									// KEY_HEADING
		{"heading_q", "", "", "", "", ""},												// KEY_HEADING_Q
		{"health", "", "", "", "", ""},													// KEY_HEALTH
		{"hour", "", "", "", "", ""},													// KEY_HOUR
		{"humidity", "", "", "", "", ""},												// KEY_HUMIDITY
		{"ice", "", "", "", "", ""},													// KEY_ICE
		{"ice_accretion_cause", "", "", "", "", ""},									// KEY_ICE_ACCRETION_CAUSE
		{"ice_accretion_rate", "", "", "", "", ""},										// KEY_ICE_ACCRETION_RATE
		{"ice_bearing", "", "", "", "", ""},											// KEY_ICE_BEARING
		{"ice_concentration", "", "", "", "", ""},										// KEY_ICE_CONCENTRATION
		{"ice_deposit_thickness", "", "", "", "", ""},									// KEY_ICE_DEPOSIT_THICKNESS
		{"ice_development", "", "", "", "", ""},										// KEY_ICE_DEVELOPMENT
		{"ice_situation", "", "", "", "", ""},											// KEY_ICE_SITUATION
		{"ice_type_amount", "", "", "", "", ""},										// KEY_ICE_TYPE_AMOUNT
		{"imo", "", "", "", "", ""},													// KEY_IMO
		{"increment1", "", "", "", "", ""},												// KEY_INCREMENT1
		{"increment2", "", "", "", "", ""},												// KEY_INCREMENT2
		{"increment3", "", "", "", "", ""},												// KEY_INCREMENT3
		{"increment4", "", "", "", "", ""},												// KEY_INCREMENT4
		{"interval", "", "", "", "", ""},												// KEY_INTERVAL
		{"lat", "", "lat", "lat", "", ""},												// KEY_LAT
		{"length", "", "length", "length", "", ""},										// KEY_LENGTH
		{"leveltrend", "", "", "", "", ""},												// KEY_LEVELTREND
		{"light", "", "", "", "", ""},													// KEY_LIGHT
		{"loaded", "", "", "", "", ""},													// KEY_LOADED
		{"lon", "", "lon", "lon", "", ""},												// KEY_LON
		{"maneuver", "", "", "", "", ""},												// KEY_MANEUVER
		{"minute", "", "", "", "", ""},													// KEY_MINUTE
		{"mmsi", "mmsi", "mmsi", "mmsi", "", ""},										// KEY_MMSI
		{"mmsi1", "", "", "", "", ""},													// KEY_MMSI1
		{"mmsi2", "", "", "", "", ""},													// KEY_MMSI2
		{"mmsi3", "", "", "", "", ""},													// KEY_MMSI3
		{"mmsi4", "", "", "", "", ""},													// KEY_MMSI4
		{"mmsiseq1", "", "", "", "", ""},												// KEY_MMSISEQ1
		{"mmsiseq2", "", "", "", "", ""},												// KEY_MMSISEQ2
		{"mmsiseq3", "", "", "", "", ""},												// KEY_MMSISEQ3
		{"mmsiseq4", "", "", "", "", ""},												// KEY_MMSISEQ4
		{"mmsi_text", "", "", "", "", ""},												// KEY_MSSI_TEXT
		{"model", "", "", "", "", ""},													// KEY_MODEL
		{"month", "", "", "", "", ""},													// KEY_MONTH
		{"mothership_mmsi", "", "", "", "", ""},										// KEY_MOTHERSHIP_MMSI
		{"msg22", "", "", "", "", ""},													// KEY_MSG22
		{"message_id", "", "", "", "", ""},												// KEY_MESSAGE_ID
		{"message", "message", "message", "", "", ""},									// KEY_MESSAGE
		{"name", "", "", "", "", ""},													// KEY_NAME
		{"ne_lat", "", "", "", "", ""},													// KEY_NE_LAT
		{"ne_lon", "", "", "", "", ""},													// KEY_NE_LON
		{"number1", "", "", "", "", ""},												// KEY_NUMBER1
		{"number2", "", "", "", "", ""},												// KEY_NUMBER2
		{"number3", "", "", "", "", ""},												// KEY_NUMBER3
		{"number4", "", "", "", "", ""},												// KEY_NUMBER4
		{"off_position", "", "", "", "", ""},											// KEY_OFF_POSITION
		{"offset1", "", "", "", "", ""},												// KEY_OFFSET1
		{"offset1_1", "", "", "", "", ""},												// KEY_OFFSET1_1
		{"offset1_2", "", "", "", "", ""},												// KEY_OFFSET1_2
		{"offset2", "", "", "", "", ""},												// KEY_OFFSET2
		{"offset2_1", "", "", "", "", ""},												// KEY_OFFSET2_1
		{"offset3", "", "", "", "", ""},												// KEY_OFFSET3
		{"offset4", "", "", "", "", ""},												// KEY_OFFSET4
		{"partno", "", "", "partno", "", ""},											// KEY_PARTNO
		{"passenger_count", "", "", "", "", ""},										// KEY_PASSENGER_COUNT
		{"persons", "", "", "", "", ""},												// KEY_PERSONS
		{"power", "", "", "", "", ""},													// KEY_POWER
		{"preciptype", "", "", "", "", ""},												// KEY_PRECIPTYPE
		{"present_weather", "", "", "", "", ""},										// KEY_PRESENT_WEATHER
		{"past_weather_1", "", "", "", "", ""},											// KEY_PAST_WEATHER_1
		{"past_weather_2", "", "", "", "", ""},											// KEY_PAST_WEATHER_2
		{"pressure_characteristic", "", "", "", "", ""},								// KEY_PRESSURE_CHARACTERISTIC
		{"pressure", "", "", "", "", ""},												// KEY_PRESSURE
		{"pressuretend", "", "", "", "", ""},											// KEY_PRESSURETEND
		{"precision", "", "", "", "", ""},												// KEY_PRECISION
		{"quiet", "", "", "", "", ""},													// KEY_QUIET
		{"radio", "", "", "", "", ""},													// KEY_RADIO
		{"raim", "", "", "", "", ""},													// KEY_RAIM
		{"received_stations", "", "", "", "", ""},										// KEY_RECEIVED_STATIONS
		{"regional", "", "", "", "", ""},												// KEY_REGIONAL
		{"repeat", "", "repeat", "", "", ""},											// KEY_REPEAT
		{"report_type", "", "", "", "", ""},											// KEY_REPORT_TYPE
		{"racon", "", "", "", "", ""},													// KEY_RACON
		{"reserved", "", "", "", "", ""},												// KEY_RESERVED
		{"retransmit", "", "", "", "", ""},												// KEY_RETRANSMIT
		{"requested_dac", "", "", "", "", ""},											// KEY_REQUESTED_DAC
		{"requested_fid", "", "", "", "", ""},											// KEY_REQUESTED_FID
		{"rel_wind_dir", "", "", "", "", ""},											// KEY_REL_WIND_DIR
		{"rel_wind_speed", "", "", "", "", ""},											// KEY_REL_WIND_SPEED
		{"salinity", "", "", "", "", ""},												// KEY_SALINITY
		{"seastate", "", "", "", "", ""},												// KEY_SEASTATE
		{"second", "", "", "", "", ""},													// KEY_SECOND
		{"seqno", "", "", "", "", ""},													// KEY_SEQNO
		{"serial", "", "", "", "", ""},													// KEY_SERIAL
		{"ship_type", "", "ship_type", "", "", ""},										// KEY_SHIP_TYPE
		{"shipname", "", "shipname", "shipname", "", ""},								// KEY_SHIPNAME
		{"shiptype", "", "shiptype", "shiptype", "", ""},								// KEY_SHIPTYPE
		{"shipboard_personnel_count", "", "", "", "", ""},								// KEY_SHIPBOARD_PERSONNEL_COUNT
		{"site_id", "", "", "", "", ""},												// KEY_SITE_ID
		{"sensor_description", "", "", "", "", ""},										// KEY_SENSOR_DESCRIPTION
		{"forecast_wspeed", "", "", "", "", ""},										// KEY_FORECAST_WSPEED
		{"forecast_wgust", "", "", "", "", ""},											// KEY_FORECAST_WGUST
		{"forecast_wdir", "", "", "", "", ""},											// KEY_FORECAST_WDIR
		{"forecast_day", "", "", "", "", ""},											// KEY_FORECAST_DAY
		{"forecast_hour", "", "", "", "", ""},											// KEY_FORECAST_HOUR
		{"forecast_minute", "", "", "", "", ""},										// KEY_FORECAST_MINUTE
		{"forecast_duration", "", "", "", "", ""},										// KEY_FORECAST_DURATION
		{"slot_number", "", "", "", "", ""},											// KEY_SLOT_NUMBER
		{"slot_timeout", "", "", "", "", ""},											// KEY_SLOT_TIMEOUT
		{"slot_offset", "", "", "", "", ""},											// KEY_SLOT_OFFSET
		{"spare", "", "", "", "", ""},													// KEY_SPARE
		{"speed", "", "speed", "speed", "", ""},										// KEY_SPEED
		{"speed_q", "", "", "", "", ""},												// KEY_SPEED_Q
		{"station_type", "", "", "", "", ""},											// KEY_STATION_TYPE
		{"station_name", "", "", "", "", ""},											// KEY_STATION_NAME
		{"status", "", "status", "status", "", ""},										// KEY_STATUS
		{"status_text", "", "status_text", "", "", ""},									// KEY_STATUS_TEXT
		{"stat_ext", "", "status_text", "", "", ""},									// KEY_STAT_EXT
		{"sw_lat", "", "", "", "", ""},													// KEY_SW_LAT
		{"sw_lon", "", "", "", "", ""},													// KEY_SW_LON
		{"swelldir", "", "", "", "", ""},												// KEY_SWELLDIR
		{"swellheight", "", "", "", "", ""},											// KEY_SWELLHEIGHT
		{"swellperiod", "", "", "", "", ""},											// KEY_SWELLPERIOD
		{"sync_state", "", "", "", "", ""},												// KEY_SYNC_STATE
		{"text", "", "", "", "", ""},													// KEY_TEXT
		{"text_sequence", "", "", "", "", ""},											// KEY_TEXT_SEQUENCE
		{"timeout1", "", "", "", "", ""},												// KEY_TIMEOUT1
		{"timeout2", "", "", "", "", ""},												// KEY_TIMEOUT2
		{"timeout3", "", "", "", "", ""},												// KEY_TIMEOUT3
		{"timeout4", "", "", "", "", ""},												// KEY_TIMEOUT4
		{"timestamp", "", "", "", "", ""},												// KEY_TIMESTAMP
		{"to_bow", "", "to_bow", "ref_front", "", ""},									// KEY_TO_BOW
		{"to_port", "", "to_port", "ref_left", "", ""},									// KEY_TO_PORT
		{"to_starboard", "", "to_starboard", "", "", ""},								// KEY_TO_STARBOARD
		{"to_stern", "", "to_stern", "", "", ""},										// KEY_TO_STERN
		{"turn", "", "turn", "", "", ""},												// KEY_TURN
		{"turn_unscaled", "", "turn_unscaled", "", "", ""},								// KEY_TURN_UNSCALED
		{"txrx", "", "", "", "", ""},													// KEY_TXRX
		{"type", "", "", "msgtype", "", ""},											// KEY_TYPE
		{"type1_1", "", "", "", "", ""},												// KEY_TYPE1_1
		{"type1_2", "", "", "", "", ""},												// KEY_TYPE1_2
		{"type2_1", "", "", "", "", ""},												// KEY_TYPE2_1
		{"utc_day", "", "", "", "", ""},												// KEY_UTC_DAY
		{"utc_hour", "", "", "", "", ""},												// KEY_UTC_HOUR
		{"utc_minute", "", "", "", "", ""},												// KEY_UTC_MINUTE
		{"uuid", "", "", "", "", ""},													// KEY_UUID
		{"vendorid", "", "", "vendorid", "", ""},										// KEY_VENDORID
		{"vin", "", "", "", "", ""},													// KEY_VIN
		{"virtual_aid", "", "", "", "", ""},											// KEY_VIRTUAL_AID
		{"vts_target_id_type", "", "", "", "", ""},										// KEY_VTS_TARGET_ID_TYPE
		{"vts_target_id", "", "", "", "", ""},											// KEY_VTS_TARGET_ID
		{"vts_target_lat", "", "", "", "", ""},											// KEY_VTS_TARGET_LAT
		{"vts_target_lon", "", "", "", "", ""},											// KEY_VTS_TARGET_LON
		{"vts_target_cog", "", "", "", "", ""},											// KEY_VTS_TARGET_COG
		{"vts_target_timestamp", "", "", "", "", ""},									// KEY_VTS_TARGET_TIMESTAMP
		{"vts_target_sog", "", "", "", "", ""},											// KEY_VTS_TARGET_SOG
		{"visgreater", "", "", "", "", ""},												// KEY_VISGREATER
		{"visibility", "", "", "", "", ""},												// KEY_VISIBILITY
		{"waterlevel", "", "", "", "", ""},												// KEY_WATERLEVEL
		{"water_level_type", "", "", "", "", ""},										// KEY_WATER_LEVEL_TYPE
		{"reference_datum", "", "", "", "", ""},										// KEY_REFERENCE_DATUM
		{"reading_type", "", "", "", "", ""},											// KEY_READING_TYPE
		{"wind_speed_avg", "", "", "", "", ""},											// KEY_WIND_SPEED_AVG
		{"wind_direction_avg", "", "", "", "", ""},										// KEY_WIND_DIRECTION_AVG
		{"wind_gust_speed", "", "", "", "", ""},										// KEY_WIND_GUST_SPEED
		{"air_temperature", "", "", "", "", ""},										// KEY_AIR_TEMPERATURE
		{"relative_humidity", "", "", "", "", ""},										// KEY_RELATIVE_HUMIDITY
		{"barometric_pressure", "", "", "", "", ""},									// KEY_BAROMETRIC_PRESSURE
		{"pressure_tendency", "", "", "", "", ""},										// KEY_PRESSURE_TENDENCY
		{"dew_point", "", "", "", "", ""},												// KEY_DEW_POINT
		{"water_temperature", "", "", "", "", ""},										// KEY_WATER_TEMPERATURE
		{"watertemp", "", "", "", "", ""},												// KEY_WATERTEMP
		{"water_flow", "", "", "", "", ""},												// KEY_WATER_FLOW
		{"wavedir", "", "", "", "", ""},												// KEY_WAVEDIR
		{"weather_report_type", "", "", "", "", ""},									// KEY_WEATHER_REPORT_TYPE
		{"waveheight", "", "", "", "", ""},												// KEY_WAVEHEIGHT
		{"waveperiod", "", "", "", "", ""},												// KEY_WAVEPERIOD
		{"wdir", "", "", "", "", ""},													// KEY_WDIR
		{"wgust", "", "", "", "", ""},													// KEY_WGUST
		{"wgustdir", "", "", "", "", ""},												// KEY_WGUSTDIR
		{"wspeed", "", "", "", "", ""},													// KEY_WSPEED
		{"year", "", "", "", "", ""},													// KEY_YEAR
		{"zonesize", "", "", "", "", ""},												// KEY_ZONESIZE
		{"altitude_unit", "altitude_unit", "altitude_unit", "", "", ""},				// KEY_ALT_UNIT
		{"velocity_type", "velocity_type", "velocity_type", "", "", ""},				// KEY_VELOCITY_TYPE
		{"ground_speed", "ground_speed", "ground_speed", "speed", "", ""},				// KEY_GROUND_SPEED
		{"vertical_rate", "vertical_rate", "vertical_rate", "", "", ""},				// KEY_VERTICAL_RATE
		{"true_airspeed", "true_airspeed", "true_airspeed", "", "", ""},				// KEY_TRUE_AIRSPEED
		{"indicated_airspeed", "indicated_airspeed", "indicated_airspeed", "", "", ""}, // KEY_INDICATED_AIRSPEED
		{"cpr_odd", "cpr_odd", "cpr_odd", "", "", ""},									// KEY_CPR_ODD
		{"cpr_lat", "cpr_lat", "cpr_lat", "lat", "", ""},								// KEY_CPR_LAT
		{"cpr_lon", "cpr_lon", "cpr_lon", "lon", "", ""},								// KEY_CPR_LON
		{"df", "df", "df", "", "", ""},													// KEY_DF
		{"df_text", "df_text", "df_text", "", "", ""},									// KEY_DF_TEXT
		{"signal", "signal", "signal", "", "", ""},										// KEY_SIGNAL
		{"vertical_status", "vertical_status", "vertical_status", "", "", ""},			// KEY_VS
		{"cross_link", "cross_link", "cross_link", "", "", ""},							// KEY_CC
		{"sensitivity", "sensitivity", "sensitivity", "", "", ""},						// KEY_SL
		{"reply_info", "reply_info", "reply_info", "", "", ""},							// KEY_RI
		{"flight_status", "flight_status", "flight_status", "", "", ""},				// KEY_FS
		{"downlink_request", "downlink_request", "downlink_request", "", "", ""},		// KEY_DR
		{"capability", "capability", "capability", "", "", ""},							// KEY_CA
		{"capability_text", "capability_text", "capability_text", "", "", ""},			// KEY_CA_TEXT
		{"address", "address", "address", "", "", ""},									// KEY_AA
		{"interrogator", "interrogator", "interrogator", "", "", ""},					// KEY_IID
		{"raw_message", "raw_message", "raw_message", "", "", ""},						// KEY_RAW_MESSAGE
		{"icao", "icao", "icao", "", "", ""},											// KEY_ICAO
		{"type_text", "type_text", "type_text", "", "", ""},							// KEY_TYPE_TEXT
		{"wake_vortex", "wake_vortex", "wake_vortex", "", "", ""},						// KEY_WAKE_VORTEX
	};

	const std::vector<std::string> LookupTable_aid_types = {
		"Default Type of Aid to Navigation not specified",
		"Reference point",
		"RACON (radar transponder marking a navigation hazard)",
		"Fixed offshore structure",
		"Spare Reserved for future use.",
		"Light without sectors",
		"Light with sectors",
		"Leading Light Front",
		"Leading Light Rear",
		"Beacon Cardinal N",
		"Beacon Cardinal E",
		"Beacon Cardinal S",
		"Beacon Cardinal W",
		"Beacon Port hand",
		"Beacon Starboard hand",
		"Beacon Preferred Channel port hand",
		"Beacon Preferred Channel starboard hand",
		"Beacon Isolated danger",
		"Beacon Safe water",
		"Beacon Special mark",
		"Cardinal Mark N",
		"Cardinal Mark E",
		"Cardinal Mark S",
		"Cardinal Mark W",
		"Port hand Mark",
		"Starboard hand Mark",
		"Preferred Channel Port hand",
		"Preferred Channel Starboard hand",
		"Isolated danger",
		"Safe Water",
		"Special Mark",
		"Light Vessel / LANBY / Rigs",
	};

	const std::vector<std::string> LookupTable_dte_types = {
		"Data Terminal Ready",
		"Data Terminal Not Ready",
	};

	const std::vector<std::string> LookupTable_message_error_types = {
		"None",
		"Undefined Error",
		"NMEA Checksum Error"};

	const std::vector<std::string> LookupTable_epfd_types = {
		"Undefined",
		"GPS",
		"GLONASS",
		"Combined GPS/GLONASS",
		"Loran-C",
		"Chayka",
		"Integrated navigation system",
		"Surveyed",
		"Galileo",
	};

	const std::vector<std::string> LookupTable_interval_types = {
		"As given by the autonomous mode",
		"10 Minutes",
		"6 Minutes",
		"3 Minutes",
		"1 Minute",
		"30 Seconds",
		"15 Seconds",
		"10 Seconds",
		"5 Seconds",
		"Next Shorter Reporting Interval",
		"Next Longer Reporting Interval",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
	};

	const std::vector<std::string> LookupTable_maneuver_types = {
		"Not available (default)",
		"No special maneuver",
		"Special maneuver (such as regional passing arrangement)",
	};

	const std::vector<std::string> LookupTable_nav_status = {
		"Under way using engine",
		"At anchor",
		"Not under command",
		"Restricted maneuverability",
		"Constrained by her draught",
		"Moored",
		"Aground",
		"Engaged in fishing",
		"Under way sailing",
		"Reserved for HSC",
		"Reserved for WIG",
		"Towing astern (regional)",
		"Pushing ahead or towing alongside (regional)",
		"Reserved",
		"AIS-SART is active",
		"Not defined",
	};

	const std::vector<std::string> LookupTable_precipation_types = {
		"Reserved",
		"Rain",
		"Thunderstorm",
		"Freezing rain",
		"Mixed/ice",
		"Snow",
		"Reserved",
		"N/A (default)",
	};

	const std::vector<std::string> LookupTable_seastate_types = {
		"Calm",
		"Light air",
		"Light breeze",
		"Gentle breeze",
		"Moderate breeze",
		"Fresh breeze",
		"Strong breeze",
		"High wind",
		"Gale",
		"Strong gale",
		"Storm",
		"Violent storm",
		"Hurricane force",
		"N/A (default)",
		"Reserved",
		"Reserved",
	};

	const std::vector<std::string> LookupTable_ship_types = {
		"Not available",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"Wing in ground (WIG) - all ships of this type",
		"Wing in ground (WIG) - Hazardous category A",
		"Wing in ground (WIG) - Hazardous category B",
		"Wing in ground (WIG) - Hazardous category C",
		"Wing in ground (WIG) - Hazardous category D",
		"Wing in ground (WIG) - Reserved",
		"Wing in ground (WIG) - Reserved",
		"Wing in ground (WIG) - Reserved",
		"Wing in ground (WIG) - Reserved",
		"Wing in ground (WIG) - Reserved",
		"Fishing",
		"Towing",
		"Towing: length exceeds 200m or breadth exceeds 25m",
		"Dredging or underwater ops",
		"Diving ops",
		"Military ops",
		"Sailing",
		"Pleasure Craft",
		"Reserved",
		"Reserved",
		"High speed craft (HSC) - all ships of this type",
		"High speed craft (HSC) - Hazardous category A",
		"High speed craft (HSC) - Hazardous category B",
		"High speed craft (HSC) - Hazardous category C",
		"High speed craft (HSC) - Hazardous category D",
		"High speed craft (HSC) - Reserved for future use",
		"High speed craft (HSC) - Reserved for future use",
		"High speed craft (HSC) - Reserved for future use",
		"High speed craft (HSC) - Reserved for future use",
		"High speed craft (HSC) - No additional information",
		"Pilot Vessel",
		"Search and Rescue vessel",
		"Tug",
		"Port Tender",
		"Anti-pollution equipment",
		"Law Enforcement",
		"Spare - Local Vessel",
		"Spare - Local Vessel",
		"Medical Transport",
		"Noncombatant ship according to RR Resolution No. 18",
		"Passenger - all ships of this type",
		"Passenger - Hazardous category A",
		"Passenger - Hazardous category B",
		"Passenger - Hazardous category C",
		"Passenger - Hazardous category D",
		"Passenger - Reserved for future use",
		"Passenger - Reserved for future use",
		"Passenger - Reserved for future use",
		"Passenger - Reserved for future use",
		"Passenger - No additional information",
		"Cargo - all ships of this type",
		"Cargo - Hazardous category A",
		"Cargo - Hazardous category B",
		"Cargo - Hazardous category C",
		"Cargo - Hazardous category D",
		"Cargo - Reserved for future use",
		"Cargo - Reserved for future use",
		"Cargo - Reserved for future use",
		"Cargo - Reserved for future use",
		"Cargo - No additional information",
		"Tanker - all ships of this type",
		"Tanker - Hazardous category A",
		"Tanker - Hazardous category B",
		"Tanker - Hazardous category C",
		"Tanker - Hazardous category D",
		"Tanker - Reserved for future use",
		"Tanker - Reserved for future use",
		"Tanker - Reserved for future use",
		"Tanker - Reserved for future use",
		"Tanker - No additional information",
		"Other Type - all ships of this type",
		"Other Type - Hazardous category A",
		"Other Type - Hazardous category B",
		"Other Type - Hazardous category C",
		"Other Type - Hazardous category D",
		"Other Type - Reserved for future use",
		"Other Type - Reserved for future use",
		"Other Type - Reserved for future use",
		"Other Type - Reserved for future use",
		"Other Type - no additional information",
	};

	const std::vector<std::string> LookupTable_station_types = {
		"All types of mobiles (default)",
		"Reserved for future use",
		"All types of Class B mobile stations",
		"SAR airborne mobile station",
		"Aid to Navigation station",
		"Class B shipborne mobile station (IEC62287 only)",
		"Regional use and inland waterways",
		"Regional use and inland waterways",
		"Regional use and inland waterways",
		"Regional use and inland waterways",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
		"Reserved for future use",
	};

	const std::vector<std::string> LookupTable_txrx_types = {
		"TxA/TxB, RxA/RxB (default)",
		"TxA, RxA/RxB",
		"TxB, RxA/RxB",
		"Reserved for Future Use",
	};

	const std::vector<KeyInfo> KeyInfoMap = {
		KeyInfo("", "AIS message class", nullptr),																			// KEY_CLASS
		KeyInfo("", "Device identifier", nullptr),																			// KEY_DEVICE
		KeyInfo("", "", nullptr),																							// KEY_DRIVER
		KeyInfo("", "", &LookupTable_message_error_types),																	// KEY_ERROR
		KeyInfo("", "", nullptr),																							// KEY_SCALED
		KeyInfo("", "", nullptr),																							// KEY_CHANNEL
		KeyInfo("", "", nullptr),																							// KEY_HARDWARE
		KeyInfo("", "", nullptr),																							// KEY_IPV4
		KeyInfo("dB", "Signal power level", nullptr),																		// KEY_SIGNAL_POWER
		KeyInfo("", "", nullptr),																							// KEY_PPM
		KeyInfo("", "", nullptr),																							// KEY_RXTIME
		KeyInfo("", "", nullptr),																							// KEY_RXUXTIME
		KeyInfo("", "", nullptr),																							// KEY_NMEA
		KeyInfo("", "", nullptr),																							// KEY_ETA
		KeyInfo("", "", nullptr),																							// KEY_SHIPTYPE_TEXT
		KeyInfo("", "", nullptr),																							// KEY_AID_TYPE_TEXT
		KeyInfo("", "", nullptr),																							// KEY_SAMPLE_START_COUNT
		KeyInfo("", "", nullptr),																							// KEY_SAMPLE_LENGTH
		KeyInfo("", "", nullptr),																							// KEY_STATION_ID
		KeyInfo("", "", nullptr),																							// KEY_VERSION
		KeyInfo("", "", nullptr),																							// KEY_SETTING_ABOUT
		KeyInfo("", "", nullptr),																							// KEY_SETTING_ADDRESS
		KeyInfo("", "", nullptr),																							// KEY_SETTING_ACTIVE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_AFC_WIDE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_AGC
		KeyInfo("", "", nullptr),																							// KEY_SETTING_AIRSPY
		KeyInfo("", "", nullptr),																							// KEY_SETTING_AIRSPYHF
		KeyInfo("", "", nullptr),																							// KEY_SETTING_ALLOW_TYPE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_ANTENNA
		KeyInfo("", "", nullptr),																							// KEY_SETTING_AUTHOR
		KeyInfo("", "", nullptr),																							// KEY_SETTING_BACKUP
		KeyInfo("", "", nullptr),																							// KEY_SETTING_BANDWIDTH
		KeyInfo("", "", nullptr),																							// KEY_SETTING_BAUDRATE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_BIASTEE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_BINARY
		KeyInfo("", "", nullptr),																							// KEY_SETTING_BLOCK_TYPE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_BROADCAST
		KeyInfo("", "", nullptr),																							// KEY_SETTING_BUFFER_COUNT
		KeyInfo("", "", nullptr),																							// KEY_SETTING_CALLSIGN
		KeyInfo("", "", nullptr),																							// KEY_SETTING_CDN
		KeyInfo("", "", nullptr),																							// KEY_SETTING_CHANNEL
		KeyInfo("", "", nullptr),																							// KEY_SETTING_CLIENT_ID
		KeyInfo("", "", nullptr),																							// KEY_SETTING_CONFIG
		KeyInfo("", "", nullptr),																							// KEY_SETTING_CONTEXT
		KeyInfo("", "", nullptr),																							// KEY_SETTING_CRC_CHECK
		KeyInfo("", "", nullptr),																							// KEY_SETTING_CUTOFF
		KeyInfo("", "", nullptr),																							// KEY_SETTING_DECODER
		KeyInfo("", "", nullptr),																							// KEY_SETTING_DESCRIPTION
		KeyInfo("", "", nullptr),																							// KEY_SETTING_DEVICE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_DROOP
		KeyInfo("", "", nullptr),																							// KEY_SETTING_ENDPOINT
		KeyInfo("", "", nullptr),																							// KEY_SETTING_FILE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_FILTER
		KeyInfo("seconds", "Minimum interval for own vessel messages", nullptr),											// KEY_SETTING_OWN_INTERVAL
		KeyInfo("seconds", "Minimum interval for position messages per MMSI", nullptr),										// KEY_SETTING_POSITION_INTERVAL
		KeyInfo("seconds", "Filter duplicate messages within interval", nullptr),											// KEY_SETTING_UNIQUE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_FORMAT
		KeyInfo("", "", nullptr),																							// KEY_SETTING_FP_DS
		KeyInfo("", "", nullptr),																							// KEY_SETTING_FREQOFFSET
		KeyInfo("", "", nullptr),																							// KEY_SETTING_GAIN
		KeyInfo("", "", nullptr),																							// KEY_SETTING_GAIN_MODE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_GEOJSON
		KeyInfo("", "", nullptr),																							// KEY_SETTING_GRDB
		KeyInfo("", "", nullptr),																							// KEY_SETTING_GROUPS_IN
		KeyInfo("", "", nullptr),																							// KEY_SETTING_GZIP
		KeyInfo("", "", nullptr),																							// KEY_SETTING_HACKRF
		KeyInfo("", "", nullptr),																							// KEY_SETTING_HISTORY
		KeyInfo("", "", nullptr),																							// KEY_SETTING_HOST
		KeyInfo("", "", nullptr),																							// KEY_SETTING_HTTP
		KeyInfo("", "", nullptr),																							// KEY_SETTING_HYDRASDR
		KeyInfo("", "", nullptr),																							// KEY_SETTING_ID
		KeyInfo("", "", nullptr),																							// KEY_SETTING_INPUT
		KeyInfo("", "", nullptr),																							// KEY_SETTING_INTERFACE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_INTERVAL
		KeyInfo("", "", nullptr),																							// KEY_SETTING_JSON
		KeyInfo("", "", nullptr),																							// KEY_SETTING_JSON_FULL
		KeyInfo("", "", nullptr),																							// KEY_SETTING_KEEP_ALIVE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_KML
		KeyInfo("", "", nullptr),																							// KEY_SETTING_LAT
		KeyInfo("", "", nullptr),																							// KEY_SETTING_LINEARITY
		KeyInfo("", "", nullptr),																							// KEY_SETTING_LNA
		KeyInfo("", "", nullptr),																							// KEY_SETTING_LNASTATE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_LON
		KeyInfo("", "", nullptr),																							// KEY_SETTING_LOG
		KeyInfo("", "", nullptr),																							// KEY_SETTING_MBTILES
		KeyInfo("", "", nullptr),																							// KEY_SETTING_MBOVERLAY
		KeyInfo("", "", nullptr),																							// KEY_SETTING_META
		KeyInfo("", "", nullptr),																							// KEY_SETTING_MIXER
		KeyInfo("", "", nullptr),																							// KEY_SETTING_MODE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_MODEL
		KeyInfo("", "", nullptr),																							// KEY_SETTING_MSG
		KeyInfo("", "", nullptr),																							// KEY_SETTING_MSGFORMAT
		KeyInfo("", "", nullptr),																							// KEY_SETTING_MSG_OUTPUT
		KeyInfo("", "", nullptr),																							// KEY_SETTING_MQTT
		KeyInfo("", "", nullptr),																							// KEY_SETTING_NMEA2000
		KeyInfo("", "", nullptr),																							// KEY_SETTING_NMEA_REFRESH
		KeyInfo("", "", nullptr),																							// KEY_SETTING_ORIGIN
		KeyInfo("", "", nullptr),																							// KEY_SETTING_OUTPUT
		KeyInfo("", "", nullptr),																							// KEY_SETTING_OWN_MMSI
		KeyInfo("", "", nullptr),																							// KEY_SETTING_PASSWORD
		KeyInfo("", "", nullptr),																							// KEY_SETTING_PERSIST
		KeyInfo("", "", nullptr),																							// KEY_SETTING_PLUGIN
		KeyInfo("", "", nullptr),																							// KEY_SETTING_PLUGIN_DIR
		KeyInfo("", "", nullptr),																							// KEY_SETTING_PORT
		KeyInfo("", "", nullptr),																							// KEY_SETTING_PORT_MIN
		KeyInfo("", "", nullptr),																							// KEY_SETTING_PORT_MAX
		KeyInfo("", "", nullptr),																							// KEY_SETTING_PREAMP
		KeyInfo("", "", nullptr),																							// KEY_SETTING_PROGRAM
		KeyInfo("", "", nullptr),																							// KEY_SETTING_PROME
		KeyInfo("", "", nullptr),																							// KEY_SETTING_PROTOCOL
		KeyInfo("", "", nullptr),																							// KEY_SETTING_PROTOCOLS
		KeyInfo("", "", nullptr),																							// KEY_SETTING_PS_EMA
		KeyInfo("", "", nullptr),																							// KEY_SETTING_REALTIME
		KeyInfo("", "", nullptr),																							// KEY_SETTING_RECEIVER
		KeyInfo("", "", nullptr),																							// KEY_SETTING_RESET
		KeyInfo("", "", nullptr),																							// KEY_SETTING_RESPONSE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_REUSE_PORT
		KeyInfo("", "", nullptr),																							// KEY_SETTING_RTLAGC
		KeyInfo("", "", nullptr),																							// KEY_SETTING_RTLSDR
		KeyInfo("", "", nullptr),																							// KEY_SETTING_RTLTCP
		KeyInfo("", "", nullptr),																							// KEY_SETTING_SAMPLE_RATE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_SCREEN
		KeyInfo("", "", nullptr),																							// KEY_SETTING_SDRPLAY
		KeyInfo("", "", nullptr),																							// KEY_SETTING_SENSITIVITY
		KeyInfo("", "", nullptr),																							// KEY_SETTING_SERIAL
		KeyInfo("", "", nullptr),																							// KEY_SETTING_SERIAL_INIT_SEQUENCE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_SERIALPORT
		KeyInfo("", "", nullptr),																							// KEY_SETTING_SERVER
		KeyInfo("", "", nullptr),																							// KEY_SETTING_SHARE_LOC
		KeyInfo("", "", nullptr),																							// KEY_SETTING_SHARING
		KeyInfo("", "", nullptr),																							// KEY_SETTING_SHARING_KEY
		KeyInfo("", "", nullptr),																							// KEY_SETTING_SOAPYSDR
		KeyInfo("", "", nullptr),																							// KEY_SETTING_SOXR
		KeyInfo("", "", nullptr),																							// KEY_SETTING_SPYSERVER
		KeyInfo("", "", nullptr),																							// KEY_SETTING_SRC
		KeyInfo("", "", nullptr),																							// KEY_SETTING_STATION
		KeyInfo("", "", nullptr),																							// KEY_SETTING_STATION_LINK
		KeyInfo("", "", nullptr),																							// KEY_SETTING_STREAM
		KeyInfo("", "", nullptr),																							// KEY_SETTING_STYLE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_TCP
		KeyInfo("", "", nullptr),																							// KEY_SETTING_TCP_LISTENER
		KeyInfo("", "", nullptr),																							// KEY_SETTING_TEST
		KeyInfo("", "", nullptr),																							// KEY_SETTING_TIMEOUT
		KeyInfo("", "", nullptr),																							// KEY_SETTING_THRESHOLD
		KeyInfo("", "", nullptr),																							// KEY_SETTING_TOPIC
		KeyInfo("", "", nullptr),																							// KEY_SETTING_TUNER
		KeyInfo("", "", nullptr),																							// KEY_SETTING_UDP
		KeyInfo("", "", nullptr),																							// KEY_SETTING_UDPSERVER
		KeyInfo("", "", nullptr),																							// KEY_SETTING_URL
		KeyInfo("", "", nullptr),																							// KEY_SETTING_USERNAME
		KeyInfo("", "", nullptr),																							// KEY_SETTING_USERPWD
		KeyInfo("", "", nullptr),																							// KEY_SETTING_UUID
		KeyInfo("", "", nullptr),																							// KEY_SETTING_VERBOSE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_VERBOSE_TIME
		KeyInfo("", "", nullptr),																							// KEY_SETTING_VERSION
		KeyInfo("", "", nullptr),																							// KEY_SETTING_VGA
		KeyInfo("", "", nullptr),																							// KEY_SETTING_WAVFILE
		KeyInfo("", "", nullptr),																							// KEY_SETTING_QOS
		KeyInfo("", "", nullptr),																							// KEY_SETTING_ZLIB
		KeyInfo("", "", nullptr),																							// KEY_SETTING_ZMQ
		KeyInfo("", "Position Accuracy; 1 indicates DGPS-quality (< 10m), 0 indicates unaugmented GNSS (> 10m).", nullptr), // KEY_ACCURACY
		KeyInfo("", "", nullptr),																							// KEY_ACK_REQUIRED
		KeyInfo("", "", nullptr),																							// KEY_ADDRESSED
		KeyInfo("", "", nullptr),																							// KEY_AI_AVAILABLE
		KeyInfo("", "Type of Aid to Navigation (e.g., Light, Buoy, Beacon).", &LookupTable_aid_types),						// KEY_AID_TYPE
		KeyInfo("Celsius", "Air temperature", nullptr),																		// KEY_AIRTEMP
		KeyInfo("", "AIS Version (0=[ITU1371], 1-3 = future editions).", nullptr),											// KEY_AIS_VERSION
		KeyInfo("meter", "Altitude", nullptr),																				// KEY_ALT
		KeyInfo("", "", nullptr),																							// KEY_ANA_INT
		KeyInfo("", "", nullptr),																							// KEY_ANA_EXT1
		KeyInfo("", "", nullptr),																							// KEY_ANA_EXT2
		KeyInfo("", "", nullptr),																							// KEY_ASM_BATTERY_STATUS
		KeyInfo("", "", nullptr),																							// KEY_ASM_CURRENT_DATA
		KeyInfo("", "", nullptr),																							// KEY_ASM_LIGHT_STATUS
		KeyInfo("", "", nullptr),																							// KEY_ASM_OFF_POSITION_STATUS
		KeyInfo("", "", nullptr),																							// KEY_ASM_POWER_SUPPLY_TYPE
		KeyInfo("", "", nullptr),																							// KEY_ASM_SUB_APP_ID
		KeyInfo("", "", nullptr),																							// KEY_ASM_VOLTAGE_DATA
		KeyInfo("", "Assigned-mode flag (0=Autonomous, 1=Assigned).", nullptr),												// KEY_ASSIGNED
		KeyInfo("", "Band flag (Can use any marine channel).", nullptr),													// KEY_BAND
		KeyInfo("", "", nullptr),																							// KEY_BAND_A
		KeyInfo("", "", nullptr),																							// KEY_BAND_B
		KeyInfo("", "", nullptr),																							// KEY_BEAM
		KeyInfo("", "Call Sign", nullptr),																					// KEY_CALLSIGN
		KeyInfo("", "Measurement Depth #2.", nullptr),																		// KEY_CDEPTH2
		KeyInfo("", "", nullptr),																							// KEY_CDEPTH3
		KeyInfo("", "Surface Current Direction (degrees from true north).", nullptr),										// KEY_CDIR
		KeyInfo("", "Current Direction #2.", nullptr),																		// KEY_CDIR2
		KeyInfo("", "", nullptr),																							// KEY_CDIR3
		KeyInfo("", "VHF Channel Number A.", nullptr),																		// KEY_CHANNEL_A
		KeyInfo("", "VHF Channel Number B.", nullptr),																		// KEY_CHANNEL_B
		KeyInfo("", "", nullptr),																							// KEY_CLOUD_AMOUNT_LOW
		KeyInfo("", "", nullptr),																							// KEY_CLOUD_BASE_HEIGHT
		KeyInfo("", "Total Cloud Cover (%).", nullptr),																		// KEY_CLOUD_COVER_TOTAL
		KeyInfo("", "", nullptr),																							// KEY_CLOUD_TYPE_HIGH
		KeyInfo("", "", nullptr),																							// KEY_CLOUD_TYPE_LOW
		KeyInfo("", "", nullptr),																							// KEY_CLOUD_TYPE_MIDDLE
		KeyInfo("", "", nullptr),																							// KEY_COUNTRY
		KeyInfo("", "", nullptr),																							// KEY_COUNTRY_CODE
		KeyInfo("degrees", "Course over Ground (COG)", nullptr),															// KEY_COURSE
		KeyInfo("", "", nullptr),																							// KEY_COURSE_Q
		KeyInfo("", "CS Unit flag (0=SOTDMA, 1=Carrier Sense).", nullptr),													// KEY_CS
		KeyInfo("", "Surface Current Speed (knots).", nullptr),																// KEY_CSPEED
		KeyInfo("", "Current Speed #2 (0.1 knot).", nullptr),																// KEY_CSPEED2
		KeyInfo("", "", nullptr),																							// KEY_CSPEED3
		KeyInfo("", "", nullptr),																							// KEY_CREW_COUNT
		KeyInfo("", "CS Unit flag (0=SOTDMA, 1=Carrier Sense).", nullptr),													// KEY_DAC
		KeyInfo("", "", nullptr),																							// KEY_DATA
		KeyInfo("", "Day (UTC)", nullptr),																					// KEY_DAY
		KeyInfo("", "Destination MMSI", nullptr),																			// KEY_DEST_MMSI
		KeyInfo("", "", nullptr),																							// KEY_DEST1
		KeyInfo("", "", nullptr),																							// KEY_DEST2
		KeyInfo("", "Destination", nullptr),																				// KEY_DESTINATION
		KeyInfo("celcius", "Dew Point", nullptr),																			// KEY_DEWPOINT
		KeyInfo("", "Display flag (0=No display, 1=Has display).", nullptr),												// KEY_DISPLAY
		KeyInfo("meters", "Draught", nullptr),																				// KEY_DRAUGHT
		KeyInfo("", "DSC Flag (1 = attached to VHF voice radio with DSC).", nullptr),										// KEY_DSC
		KeyInfo("", "DTE", &LookupTable_dte_types),																			// KEY_DTE
		KeyInfo("", "Electronic Position Fixing Device", &LookupTable_epfd_types),											// KEY_EPFD
		KeyInfo("", "Electronic Position Fixing Device", nullptr),															// KEY_EPFD_TEXT
		KeyInfo("", "Functional ID", nullptr),																				// KEY_FID
		KeyInfo("", "", nullptr),																							// KEY_GNSS
		KeyInfo("", "", nullptr),																							// KEY_HAZARD
		KeyInfo("degrees", "True Heading (HDG)", nullptr),																	// KEY_HEADING
		KeyInfo("", "", nullptr),																							// KEY_HEADING_Q
		KeyInfo("", "", nullptr),																							// KEY_HEALTH
		KeyInfo("", "Hour (UTC)", nullptr),																					// KEY_HOUR
		KeyInfo("percentage", "Relative Humidity (%).", nullptr),															// KEY_HUMIDITY
		KeyInfo("", "Ice status (Yes/No).", nullptr),																		// KEY_ICE
		KeyInfo("", "", nullptr),																							// KEY_ICE_ACCRETION_CAUSE
		KeyInfo("", "Rate of Ice Accretion.", nullptr),																		// KEY_ICE_ACCRETION_RATE
		KeyInfo("", "", nullptr),																							// KEY_ICE_BEARING
		KeyInfo("", "", nullptr),																							// KEY_ICE_CONCENTRATION
		KeyInfo("", "", nullptr),																							// KEY_ICE_DEPOSIT_THICKNESS
		KeyInfo("", "", nullptr),																							// KEY_ICE_DEVELOPMENT
		KeyInfo("", "", nullptr),																							// KEY_ICE_SITUATION
		KeyInfo("", "", nullptr),																							// KEY_ICE_TYPE_AMOUNT
		KeyInfo("", "IMO ship ID number.", nullptr),																		// KEY_IMO
		KeyInfo("", "", nullptr),																							// KEY_INCREMENT1
		KeyInfo("", "", nullptr),																							// KEY_INCREMENT2
		KeyInfo("", "", nullptr),																							// KEY_INCREMENT3
		KeyInfo("", "", nullptr),																							// KEY_INCREMENT4
		KeyInfo("", "Reporting Interval.", &LookupTable_interval_types),													// KEY_INTERVAL
		KeyInfo("degrees", "Latitude", nullptr),																			// KEY_LAT
		KeyInfo("meters", "Overall length of vessel", nullptr),																// KEY_LENGTH
		KeyInfo("", "Water Level Trend.", nullptr),																			// KEY_LEVELTREND
		KeyInfo("", "", nullptr),																							// KEY_LIGHT
		KeyInfo("", "", nullptr),																							// KEY_LOADED
		KeyInfo("degrees", "Longitude", nullptr),																			// KEY_LON
		KeyInfo("", "Maneuver Indicator", &LookupTable_maneuver_types),														// KEY_MANEUVER
		KeyInfo("", "Minute (UTC)", nullptr),																				// KEY_MINUTE
		KeyInfo("", "MMSI", nullptr),																						// KEY_MMSI
		KeyInfo("", "", nullptr),																							// KEY_MMSI1
		KeyInfo("", "", nullptr),																							// KEY_MMSI2
		KeyInfo("", "", nullptr),																							// KEY_MMSI3
		KeyInfo("", "", nullptr),																							// KEY_MMSI4
		KeyInfo("", "", nullptr),																							// KEY_MMSISEQ1
		KeyInfo("", "", nullptr),																							// KEY_MMSISEQ2
		KeyInfo("", "", nullptr),																							// KEY_MMSISEQ3
		KeyInfo("", "", nullptr),																							// KEY_MMSISEQ4
		KeyInfo("", "", nullptr),																							// KEY_MSSI_TEXT
		KeyInfo("", "Unit Model Code.", nullptr),																			// KEY_MODEL
		KeyInfo("", "UTC Month.", nullptr),																					// KEY_MONTH
		KeyInfo("", "MMSI of the mother ship (for auxiliary craft).", nullptr),												// KEY_MOTHERSHIP_MMSI
		KeyInfo("", "Message 22 flag (Unit can accept channel assignment).", nullptr),										// KEY_MSG22
		KeyInfo("", "", nullptr),																							// KEY_MESSAGE_ID
		KeyInfo("", "Freeform message string.", nullptr),																	// KEY_MESSAGE
		KeyInfo("", "Name of the Aid to Navigation.", nullptr),																// KEY_NAME
		KeyInfo("degrees", "North East Latitude", nullptr),																	// KEY_NE_LAT
		KeyInfo("degrees", "North East Longitude", nullptr),																// KEY_NE_LON
		KeyInfo("", "", nullptr),																							// KEY_NUMBER1
		KeyInfo("", "", nullptr),																							// KEY_NUMBER2
		KeyInfo("", "", nullptr),																							// KEY_NUMBER3
		KeyInfo("", "", nullptr),																							// KEY_NUMBER4
		KeyInfo("", "Off-Position Indicator (0=On position, 1=Off position).", nullptr),									// KEY_OFF_POSITION
		KeyInfo("", "", nullptr),																							// KEY_OFFSET1
		KeyInfo("", "", nullptr),																							// KEY_OFFSET1_1
		KeyInfo("", "", nullptr),																							// KEY_OFFSET1_2
		KeyInfo("", "", nullptr),																							// KEY_OFFSET2
		KeyInfo("", "", nullptr),																							// KEY_OFFSET2_1
		KeyInfo("", "", nullptr),																							// KEY_OFFSET3
		KeyInfo("", "", nullptr),																							// KEY_OFFSET4
		KeyInfo("", "Part Number (0 or 1) for Type 24 messages.", nullptr),													// KEY_PARTNO
		KeyInfo("", "", nullptr),																							// KEY_PASSENGER_COUNT
		KeyInfo("", "", nullptr),																							// KEY_PERSONS
		KeyInfo("", "", nullptr),																							// KEY_POWER
		KeyInfo("", "Precipitation Type (e.g., Rain, Snow).", &LookupTable_precipation_types),								// KEY_PRECIPTYPE
		KeyInfo("", "", nullptr),																							// KEY_PRESENT_WEATHER
		KeyInfo("", "", nullptr),																							// KEY_PAST_WEATHER_1
		KeyInfo("", "", nullptr),																							// KEY_PAST_WEATHER_2
		KeyInfo("", "", nullptr),																							// KEY_PRESSURE_CHARACTERISTIC
		KeyInfo("hPa", "Atmospheric pressure", nullptr),																	// KEY_PRESSURE
		KeyInfo("", "Pressure Tendency (0=steady, 1=decreasing, 2=increasing).", nullptr),									// KEY_PRESSURETEND
		KeyInfo("", "", nullptr),																							// KEY_PRECISION
		KeyInfo("minutes", "Quiet Time", nullptr),																			// KEY_QUIET
		KeyInfo("", "SOTDMA state", nullptr),																				// KEY_RADIO
		KeyInfo("", "RAIM flag", nullptr),																					// KEY_RAIM
		KeyInfo("", "", nullptr),																							// KEY_RECEIVED_STATIONS
		KeyInfo("", "", nullptr),																							// KEY_REGIONAL
		KeyInfo("", "Repeat Indicator", nullptr),																			// KEY_REPEAT
		KeyInfo("", "", nullptr),																							// KEY_REPORT_TYPE
		KeyInfo("", "", nullptr),																							// KEY_RACON
		KeyInfo("", "", nullptr),																							// KEY_RESERVED
		KeyInfo("", "Retransmit flag", nullptr),																			// KEY_RETRANSMIT
		KeyInfo("", "Designated Area Code", nullptr),																		// KEY_REQUESTED_DAC
		KeyInfo("", "", nullptr),																							// KEY_REQUESTED_FID
		KeyInfo("", "", nullptr),																							// KEY_REL_WIND_DIR
		KeyInfo("", "", nullptr),																							// KEY_REL_WIND_SPEED
		KeyInfo("percentage", "Salinity", nullptr),																			// KEY_SALINITY
		KeyInfo("", "Sea state (Beaufort Scale).", &LookupTable_seastate_types),											// KEY_SEASTATE
		KeyInfo("", "Second (UTC)", nullptr),																				// KEY_SECOND
		KeyInfo("", "Sequence Number", nullptr),																			// KEY_SEQNO
		KeyInfo("", "", nullptr),																							// KEY_SERIAL
		KeyInfo("", "", nullptr),																							// KEY_SHIP_TYPE
		KeyInfo("", "Vessel Name", nullptr),																				// KEY_SHIPNAME
		KeyInfo("", "Ship Type", &LookupTable_ship_types),																	// KEY_SHIPTYPE
		KeyInfo("", "", nullptr),																							// KEY_SHIPBOARD_PERSONNEL_COUNT
		KeyInfo("", "", nullptr),																							// KEY_SITE_ID
		KeyInfo("", "", nullptr),																							// KEY_SENSOR_DESCRIPTION
		KeyInfo("", "", nullptr),																							// KEY_FORECAST_WSPEED
		KeyInfo("", "", nullptr),																							// KEY_FORECAST_WGUST
		KeyInfo("", "", nullptr),																							// KEY_FORECAST_WDIR
		KeyInfo("", "", nullptr),																							// KEY_FORECAST_DAY
		KeyInfo("", "", nullptr),																							// KEY_FORECAST_HOUR
		KeyInfo("", "", nullptr),																							// KEY_FORECAST_MINUTE
		KeyInfo("", "", nullptr),																							// KEY_FORECAST_DURATION
		KeyInfo("", "", nullptr),																							// KEY_SLOT_NUMBER
		KeyInfo("", "", nullptr),																							// KEY_SLOT_TIMEOUT
		KeyInfo("", "", nullptr),																							// KEY_SLOT_OFFSET
		KeyInfo("", "", nullptr),																							// KEY_SPARE
		KeyInfo("knots", "Speed over Ground (SOG)", nullptr),																// KEY_SPEED
		KeyInfo("", "", nullptr),																							// KEY_SPEED_Q
		KeyInfo("", "Station Type (e.g., Base Station, Class B, SAR).", &LookupTable_station_types),						// KEY_STATION_TYPE
		KeyInfo("", "Station Name", nullptr),																				// KEY_STATION_NAME
		KeyInfo("", "Navigation Status", &LookupTable_nav_status),															// KEY_STATUS
		KeyInfo("", "", nullptr),																							// KEY_STATUS_TEXT
		KeyInfo("", "", nullptr),																							// KEY_STAT_EXT
		KeyInfo("degrees", "South West Latitude", nullptr),																	// KEY_SW_LAT
		KeyInfo("degrees", "South West Longitude", nullptr),																// KEY_SW_LON
		KeyInfo("", "Swell direction.", nullptr),																			// KEY_SWELLDIR
		KeyInfo("meter", "Swell height", nullptr),																			// KEY_SWELLHEIGHT
		KeyInfo("seconds", "Swell period", nullptr),																		// KEY_SWELLPERIOD
		KeyInfo("", "", nullptr),																							// KEY_SYNC_STATE
		KeyInfo("", "Tekst description", nullptr),																			// KEY_TEXT
		KeyInfo("", "", nullptr),																							// KEY_TEXT_SEQUENCE
		KeyInfo("", "", nullptr),																							// KEY_TIMEOUT1
		KeyInfo("", "", nullptr),																							// KEY_TIMEOUT2
		KeyInfo("", "", nullptr),																							// KEY_TIMEOUT3
		KeyInfo("", "", nullptr),																							// KEY_TIMEOUT4
		KeyInfo("", "", nullptr),																							// KEY_TIMESTAMP
		KeyInfo("meter", "Dimension to Bow", nullptr),																		// KEY_TO_BOW
		KeyInfo("meter", "Dimension to Port", nullptr),																		// KEY_TO_PORT
		KeyInfo("meter", "Dimension to Starboard", nullptr),																// KEY_TO_STARBOARD
		KeyInfo("meter", "Dimension to Stern", nullptr),																	// KEY_TO_STERN
		KeyInfo("degreres per minute", "Rate of Turn (ROT)", nullptr),														// KEY_TURN
		KeyInfo("", "", nullptr),																							// KEY_TURN_UNSCALED
		KeyInfo("", "Transmit/Receive mode.", &LookupTable_txrx_types),														// KEY_TXRX
		KeyInfo("", "Message Type", nullptr),																				// KEY_TYPE
		KeyInfo("", "", nullptr),																							// KEY_TYPE1_1
		KeyInfo("", "", nullptr),																							// KEY_TYPE1_2
		KeyInfo("", "", nullptr),																							// KEY_TYPE2_1
		KeyInfo("", "", nullptr),																							// KEY_UTC_DAY
		KeyInfo("", "", nullptr),																							// KEY_UTC_HOUR
		KeyInfo("", "", nullptr),																							// KEY_UTC_MINUTE
		KeyInfo("", "", nullptr),																							// KEY_UUID
		KeyInfo("", "Vendor ID", nullptr),																					// KEY_VENDORID
		KeyInfo("", "", nullptr),																							// KEY_VIN
		KeyInfo("", "Virtual Aid flag (0=Real, 1=Virtual/Simulated).", nullptr),											// KEY_VIRTUAL_AID
		KeyInfo("", "", nullptr),																							// KEY_VTS_TARGET_ID_TYPE
		KeyInfo("", "", nullptr),																							// KEY_VTS_TARGET_ID
		KeyInfo("", "", nullptr),																							// KEY_VTS_TARGET_LAT
		KeyInfo("", "", nullptr),																							// KEY_VTS_TARGET_LON
		KeyInfo("", "", nullptr),																							// KEY_VTS_TARGET_COG
		KeyInfo("", "", nullptr),																							// KEY_VTS_TARGET_TIMESTAMP
		KeyInfo("", "", nullptr),																							// KEY_VTS_TARGET_SOG
		KeyInfo("", "", nullptr),																							// KEY_VISGREATER
		KeyInfo("", "Horizontal Visibility (0.1nm).", nullptr),																// KEY_VISIBILITY
		KeyInfo("", "Water Level (deviation from local chart datum).", nullptr),											// KEY_WATERLEVEL
		KeyInfo("", "", nullptr),																							// KEY_WATER_LEVEL_TYPE
		KeyInfo("", "", nullptr),																							// KEY_REFERENCE_DATUM
		KeyInfo("", "", nullptr),																							// KEY_READING_TYPE
		KeyInfo("", "", nullptr),																							// KEY_WIND_SPEED_AVG
		KeyInfo("", "", nullptr),																							// KEY_WIND_DIRECTION_AVG
		KeyInfo("", "", nullptr),																							// KEY_WIND_GUST_SPEED
		KeyInfo("", "", nullptr),																							// KEY_AIR_TEMPERATURE
		KeyInfo("", "", nullptr),																							// KEY_RELATIVE_HUMIDITY
		KeyInfo("", "", nullptr),																							// KEY_BAROMETRIC_PRESSURE
		KeyInfo("", "", nullptr),																							// KEY_PRESSURE_TENDENCY
		KeyInfo("", "", nullptr),																							// KEY_DEW_POINT
		KeyInfo("", "", nullptr),																							// KEY_WATER_TEMPERATURE
		KeyInfo("Celsius", "Water temperature", nullptr),																	// KEY_WATERTEMP
		KeyInfo("", "", nullptr),																							// KEY_WATER_FLOW
		KeyInfo("", "Wave direction (degrees from true north).", nullptr),													// KEY_WAVEDIR
		KeyInfo("", "", nullptr),																							// KEY_WEATHER_REPORT_TYPE
		KeyInfo("meters", "Wave height", nullptr),																			// KEY_WAVEHEIGHT
		KeyInfo("seconds", "Wave period", nullptr),																			// KEY_WAVEPERIOD
		KeyInfo("degrees", "Wind direction", nullptr),																		// KEY_WDIR
		KeyInfo("knots", "Gust speed", nullptr),																			// KEY_WGUST
		KeyInfo("", "Wind Gust direction", nullptr),																		// KEY_WGUSTDIR
		KeyInfo("m/s", "Wind speed", nullptr),																				// KEY_WSPEED
		KeyInfo("", "Year (UTC)", nullptr),																					// KEY_YEAR
		KeyInfo("", "Size of transitional zone.", nullptr),																	// KEY_ZONESIZE
		KeyInfo("", "", nullptr),																							// KEY_ALT_UNIT
		KeyInfo("", "", nullptr),																							// KEY_VELOCITY_TYPE
		KeyInfo("", "", nullptr),																							// KEY_GROUND_SPEED
		KeyInfo("", "", nullptr),																							// KEY_VERTICAL_RATE
		KeyInfo("", "", nullptr),																							// KEY_TRUE_AIRSPEED
		KeyInfo("", "", nullptr),																							// KEY_INDICATED_AIRSPEED
		KeyInfo("", "", nullptr),																							// KEY_CPR_ODD
		KeyInfo("", "", nullptr),																							// KEY_CPR_LAT
		KeyInfo("", "", nullptr),																							// KEY_CPR_LON
		KeyInfo("", "", nullptr),																							// KEY_DF
		KeyInfo("", "", nullptr),																							// KEY_DF_TEXT
		KeyInfo("", "", nullptr),																							// KEY_SIGNAL
		KeyInfo("", "", nullptr),																							// KEY_VS
		KeyInfo("", "", nullptr),																							// KEY_CC
		KeyInfo("", "", nullptr),																							// KEY_SL
		KeyInfo("", "", nullptr),																							// KEY_RI
		KeyInfo("", "", nullptr),																							// KEY_FS
		KeyInfo("", "", nullptr),																							// KEY_DR
		KeyInfo("", "", nullptr),																							// KEY_CA
		KeyInfo("", "", nullptr),																							// KEY_CA_TEXT
		KeyInfo("", "", nullptr),																							// KEY_AA
		KeyInfo("", "", nullptr),																							// KEY_IID
		KeyInfo("", "", nullptr),																							// KEY_RAW_MESSAGE
		KeyInfo("", "", nullptr),																							// KEY_ICAO
		KeyInfo("", "", nullptr),																							// KEY_TYPE_TEXT
		KeyInfo("", "", nullptr),																							// KEY_WAKE_VORTEX
	};
}
