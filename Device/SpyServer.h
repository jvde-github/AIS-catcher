/*
	Copyright(c) 2021-2023 jvde.github@gmail.com

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

#pragma once

#include "Device.h"

#include <fstream>
#include <iostream>

#include "TCP.h"


namespace Device {


	// spyserver protocol.
	//
	// SPY Server protocol structures and constants
	// Copyright (C) 2017 Youssef Touil youssef@live.com

#define SPYSERVER_PROTOCOL_VERSION (((2) << 24) | ((0) << 16) | (1700))

#define SPYSERVER_MAX_COMMAND_BODY_SIZE (256)
#define SPYSERVER_MAX_MESSAGE_BODY_SIZE (1 << 20)
#define SPYSERVER_MAX_DISPLAY_PIXELS	(1 << 15)
#define SPYSERVER_MIN_DISPLAY_PIXELS	(100)
#define SPYSERVER_MAX_FFT_DB_RANGE		(150)
#define SPYSERVER_MIN_FFT_DB_RANGE		(10)
#define SPYSERVER_MAX_FFT_DB_OFFSET		(100)

	enum DeviceType {
		DEVICE_INVALID = 0,
		DEVICE_AIRSPY_ONE = 1,
		DEVICE_AIRSPY_HF = 2,
		DEVICE_RTLSDR = 3,
	};

	enum CommandType {
		CMD_HELLO = 0,
		CMD_GET_SETTING = 1,
		CMD_SET_SETTING = 2,
		CMD_PING = 3,
	};

	enum SettingType {
		SETTING_STREAMING_MODE = 0,
		SETTING_STREAMING_ENABLED = 1,
		SETTING_GAIN = 2,

		SETTING_IQ_FORMAT = 100,	   // 0x64
		SETTING_IQ_FREQUENCY = 101,	   // 0x65
		SETTING_IQ_DECIMATION = 102,   // 0x66
		SETTING_IQ_DIGITAL_GAIN = 103, // 0x67

		SETTING_FFT_FORMAT = 200,		  // 0xc8
		SETTING_FFT_FREQUENCY = 201,	  // 0xc9
		SETTING_FFT_DECIMATION = 202,	  // 0xca
		SETTING_FFT_DB_OFFSET = 203,	  // 0xcb
		SETTING_FFT_DB_RANGE = 204,		  // 0xcc
		SETTING_FFT_DISPLAY_PIXELS = 205, // 0xcd
	};

	enum StreamType {
		STREAM_TYPE_STATUS = 0,
		STREAM_TYPE_IQ = 1,
		STREAM_TYPE_AF = 2,
		STREAM_TYPE_FFT = 4,
	};


	enum StreamingMode {
		STREAM_MODE_IQ_ONLY = STREAM_TYPE_IQ,				   // 0x01
		STREAM_MODE_AF_ONLY = STREAM_TYPE_AF,				   // 0x02
		STREAM_MODE_FFT_ONLY = STREAM_TYPE_FFT,				   // 0x04
		STREAM_MODE_FFT_IQ = STREAM_TYPE_FFT | STREAM_TYPE_IQ, // 0x05
		STREAM_MODE_FFT_AF = STREAM_TYPE_FFT | STREAM_TYPE_AF, // 0x06
	};

	enum StreamFormat {
		STREAM_FORMAT_INVALID = 0,
		STREAM_FORMAT_UINT8 = 1,
		STREAM_FORMAT_INT16 = 2,
		STREAM_FORMAT_INT24 = 3,
		STREAM_FORMAT_FLOAT = 4,
		STREAM_FORMAT_DINT4 = 5,
	};

	enum MessageType {
		MSG_TYPE_DEVICE_INFO = 0,
		MSG_TYPE_CLIENT_SYNC = 1,
		MSG_TYPE_PONG = 2,
		MSG_TYPE_READ_SETTING = 3,

		MSG_TYPE_UINT8_IQ = 100, // 0x64
		MSG_TYPE_INT16_IQ = 101, // 0x65
		MSG_TYPE_INT24_IQ = 102, // 0x66
		MSG_TYPE_FLOAT_IQ = 103, // 0x67

		MSG_TYPE_UINT8_AF = 200, // 0xc8
		MSG_TYPE_INT16_AF = 201, // 0xc9
		MSG_TYPE_INT24_AF = 202, // 0xca
		MSG_TYPE_FLOAT_AF = 203, // 0xcb

		MSG_TYPE_DINT4_FFT = 300, // 0x12C
		MSG_TYPE_UINT8_FFT = 301, // 0x12D
	};

	struct ClientHandshake {
		uint32_t ProtocolVersion;
		uint32_t ClientNameLength;
	};

	struct CommandHeader {
		uint32_t CommandType;
		uint32_t BodySize;
	};

	struct SettingTarget {
		uint32_t StreamType;
		uint32_t SettingType;
	};

	struct MessageHeader {
		uint32_t ProtocolID;
		uint32_t MessageType;
		uint32_t StreamType;
		uint32_t SequenceNumber;
		uint32_t BodySize;
	};

	struct DeviceInfo {
		uint32_t DeviceType;
		uint32_t DeviceSerial;
		uint32_t MaximumSampleRate;
		uint32_t MaximumBandwidth;
		uint32_t DecimationStageCount;
		uint32_t GainStageCount;
		uint32_t MaximumGainIndex;
		uint32_t MinimumFrequency;
		uint32_t MaximumFrequency;
		uint32_t Resolution;
		uint32_t MinimumIQDecimation;
		uint32_t ForcedIQFormat;
	};

	struct ClientSync {
		uint32_t CanControl;
		uint32_t Gain;
		uint32_t DeviceCenterFrequency;
		uint32_t IQCenterFrequency;
		uint32_t FFTCenterFrequency;
		uint32_t MinimumIQCenterFrequency;
		uint32_t MaximumIQCenterFrequency;
		uint32_t MinimumFFTCenterFrequency;
		uint32_t MaximumFFTCenterFrequency;
	};

	// End of Spyserver Protocol


	class SpyServer : public Device {
		FLOAT32 tuner_gain = 0.0;

		::TCP::Client client;

		std::string host = "localhost";
		std::string port = "1234";

		static const int BUFFER_SIZE = 16 * 16384;

		bool lost = false;
		uint32_t status = 0;
		std::thread async_thread;
		std::thread run_thread;
		int timeout = 2;

		void RunAsync();
		void Run();

		FIFO fifo;

		bool read(char* data, int len);

		void applySettings();

		bool sendCommand(uint32_t cmd, const std::vector<uint8_t>& args);
		bool sendHandshake();
		bool processHeader();
		int remainingBytes = 0;

		DeviceInfo device_info;
		ClientSync client_sync;
		MessageHeader header;

		std::vector<std::pair<double, uint32_t>> _sample_rates;

		bool sendSetting(uint32_t type, const std::vector<uint32_t>& params);
		void sendStreamFormat();
		bool setFreq(uint32_t f);
		bool setRate(uint32_t rate);
		void setGain(FLOAT32 gain);

		void printSync();
		void printDevice();

	public:
		SpyServer() : Device(Format::UNKNOWN, 288000) {}

		void OpenWithFileDescriptor(int) { Open(0); }

		// Control
		void Open(uint64_t);
		void Close();
		void Play();
		void Stop();

		bool isStreaming() { return Device::isStreaming() && !lost; }
		bool isCallback() { return true; }

		void getDeviceList(std::vector<Description>& DeviceList);

		// Settings
		Setting& Set(std::string option, std::string arg);
		std::string Get();

		std::string getProduct() { return "SPYSERVER"; }
		void setFormat(Format f) {}
	};
}
