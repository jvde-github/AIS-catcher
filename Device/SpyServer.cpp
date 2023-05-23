/*
	Copyright(c) 2021-2023 jvde.github@gmail.com

	spyserver_client (https://github.com/miweber67/spyserver_client)
	Copyright(c) miweber67
	spy_server client was used as reference implementation for the protocol.

	SPY Server protocol structures and constants
	Copyright (C) 2017 Youssef Touil youssef@live.com

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

#include <cstring>

#include "SpyServer.h"

namespace Device {

	//---------------------------------------
	// Device SPYSERVER

	// TO DO: implement formats (currently sets CS16 as output but accepts most return types),
	//        channels, digital gain and recovery in case we get off sync with headers if needed

	void SpyServer::Open(uint64_t h) {
		std::cerr << "Connecting to SpyServer..." << std::endl;
		if (!client.connect(host, port, false, timeout))
			throw std::runtime_error("SPYSERVER: cannot open connection.");

		if (!sendHandshake()) {
			client.disconnect();
			throw std::runtime_error("SPYSERVER: cannot send handshake");
		}

		// read two messages and check if device and sync info
		if (!(processHeader() && processHeader() && ((status & 3) == 3))) {
			client.disconnect();
			throw std::runtime_error("SPYSERVER: error receiving messages from server to start stream.");
		}

		uint32_t distance = device_info.MaximumSampleRate;
		uint32_t new_rate = 0;

		for (int i = device_info.MinimumIQDecimation; i <= device_info.DecimationStageCount; i++) {
			int rate = device_info.MaximumSampleRate >> i;
			int d = abs((int)rate - (int)sample_rate);

			if (rate >= 96000) {
				_sample_rates.push_back(std::pair<uint32_t, uint32_t>(rate, i));
				if (d < distance) {
					new_rate = rate;
					distance = d;
				}
			}
		}
		sample_rate = new_rate;
	}

	void SpyServer::Close() {
		client.disconnect();
		Device::Close();
	}

	void SpyServer::Play() {
		Device::Play();

		fifo.Init(16 * 16384, 8);
		lost = false;

		applySettings();

		async_thread = std::thread(&SpyServer::RunAsync, this);
		run_thread = std::thread(&SpyServer::Run, this);

		sendSetting(SETTING_STREAMING_ENABLED, { 1 });

		SleepSystem(10);
	}

	bool SpyServer::read(char* data, int size) {
		int maxzero = 2;

		while (size > 0 && maxzero >= 0) {
			int len = client.read(data, size, timeout);
			if (len < 0) return false;
			if (len == 0) maxzero--;
			data += len;
			size -= len;
		}
		return size == 0;
	}

	bool SpyServer::processHeader() {
		// read header
		if (!read((char*)&header, sizeof(MessageHeader))) {
			std::cerr << "SPYSERVER: no data received." << std::endl;
			return false;
		}

		// check header
		if ((header.ProtocolID & 0xFFFF0000) != (SPYSERVER_PROTOCOL_VERSION & 0xFFFF0000) || header.BodySize > SPYSERVER_MAX_MESSAGE_BODY_SIZE) {
			std::cerr << "SPYSERVER: protocol ID not supported (" << (header.ProtocolID >> 24 & 0xFF) << "." << (header.ProtocolID >> 16 & 0xFF) << ")" << std::endl;
			return false;
		}

		// action
		switch (header.MessageType) {
		case MSG_TYPE_DEVICE_INFO:
			if (!read((char*)&device_info, sizeof(DeviceInfo)))
				return false;
			status |= 1;
			printDevice();
			remainingBytes = 0;
			return true;

		case MSG_TYPE_CLIENT_SYNC:
			if (!read((char*)&client_sync, sizeof(ClientSync)))
				return false;
			status |= 2;
			printSync();
			remainingBytes = 0;
			return true;

		case MSG_TYPE_UINT8_IQ:
			Device::setFormat(Format::CU8);
			remainingBytes = header.BodySize;
			return true;

		case MSG_TYPE_INT16_IQ:
			Device::setFormat(Format::CS16);
			remainingBytes = header.BodySize;
			return true;

		case MSG_TYPE_FLOAT_IQ:
			Device::setFormat(Format::CF32);
			remainingBytes = header.BodySize;
			return true;

		default:
			std::cerr << "SPYSERVER: unknown message type received." << std::endl;
			return false;
		}
		return true;
	}

	void SpyServer::applySettings() {
		sendSetting(SETTING_STREAMING_MODE, { STREAM_MODE_IQ_ONLY });
		sendSetting(SETTING_IQ_DIGITAL_GAIN, { 0x0 });
		Device::setFormat(Format::CS16);
		sendStreamFormat();
		setFreq(getCorrectedFrequency());
		setRate(sample_rate);
		if (tuner_gain != 0) setGain(tuner_gain);
	}

	void SpyServer::setGain(FLOAT32 gain) {
		if (client_sync.CanControl)
			sendSetting(SETTING_GAIN, { (uint32_t)gain });
		else
			std::cerr << "SPYSERVER: server does not give gain control." << std::endl;
	}

	bool SpyServer::sendSetting(uint32_t type, const std::vector<uint32_t>& params) {
		std::vector<uint8_t> bytes((1 + params.size()) * sizeof(uint32_t));
		uint32_t* int_ptr = (uint32_t*)bytes.data();

		int_ptr[0] = type;
		for (int i = 0; i < params.size(); i++)
			int_ptr[i + 1] = params[i];

		return sendCommand(CMD_SET_SETTING, bytes);
	}


	bool SpyServer::sendCommand(uint32_t cmd, const std::vector<uint8_t>& args) {
		std::vector<char> bytes(sizeof(CommandHeader) + args.size());

		CommandHeader* ch = (CommandHeader*)bytes.data();
		ch->CommandType = cmd;
		ch->BodySize = args.size();

		std::memcpy(bytes.data() + sizeof(CommandHeader), args.data(), args.size());
		int len = client.send(bytes.data(), bytes.size());
		if (len != bytes.size()) return false;

		SleepSystem(100);
		return true;
	}

	bool SpyServer::sendHandshake() {
		std::string software = "AIS-catcher";
		uint32_t version = SPYSERVER_PROTOCOL_VERSION;

		std::vector<uint8_t> args(sizeof(uint32_t) + software.size());

		std::memcpy(&args[0], &version, sizeof(uint32_t));
		std::memcpy(&args[0] + sizeof(uint32_t), software.c_str(), software.size());

		return sendCommand(CMD_HELLO, args);
	}

	void SpyServer::Stop() {
		if (Device::isStreaming()) {
			Device::Stop();
			fifo.Halt();

			if (async_thread.joinable()) async_thread.join();
			if (run_thread.joinable()) run_thread.join();

			sendSetting(SETTING_STREAMING_ENABLED, { 0 });
		}
	}

	void SpyServer::RunAsync() {
		std::vector<char> data(BUFFER_SIZE);

		while (isStreaming()) {
			if (remainingBytes == 0) {
				if (!processHeader()) {
					std::cerr << "SPYSERVER: no valid message received." << std::endl;
					lost = true;
				}
			}

			if (remainingBytes) {
				int len = client.read(data.data(), remainingBytes, timeout);

				if (len <= 0) {
					std::cerr << "SPYSERVER: error receiving data from remote host. Cancelling. " << std::endl;
					lost = true;
					break;
				}
				else {
					if (isStreaming() && !fifo.Push(data.data(), len)) std::cerr << "SPYSERVER: buffer overrun." << std::endl;
					remainingBytes -= len;
				}
			}
		}
	}

	void SpyServer::sendStreamFormat() {
		switch (getFormat()) {
		case Format::CS16:
			sendSetting(SETTING_IQ_FORMAT, { STREAM_FORMAT_INT16 });
			break;
		case Format::CU8:
			sendSetting(SETTING_IQ_FORMAT, { STREAM_FORMAT_UINT8 });
			break;
		case Format::CF32:
			sendSetting(SETTING_IQ_FORMAT, { STREAM_FORMAT_FLOAT });
			break;
		default:
			throw std::runtime_error("SPYSERVER: format not supported.");
		}
	}

	void SpyServer::Run() {
		while (isStreaming()) {
			if (fifo.Wait()) {
				RAW r = { getFormat(), fifo.Front(), fifo.BlockSize() };
				Send(&r, 1, tag);
				fifo.Pop();
			}
			else {
				if (isStreaming()) std::cerr << "SPYSERVER: timeout." << std::endl;
			}
		}
	}

	void SpyServer::printDevice() {
		std::cerr << "Device info:" << std::endl;
		std::cerr << "  Serial: " << device_info.DeviceSerial << " DeviceType: " << device_info.DeviceType << " MaximumSampleRate: " << device_info.MaximumSampleRate << " MaximumBandwidth: " << device_info.MaximumBandwidth << std::endl;
		std::cerr << "  DecimationStageCount: " << device_info.DecimationStageCount << " GainStageCount: " << device_info.GainStageCount << " MaximumGainIndex: " << device_info.MaximumGainIndex << std::endl;
		std::cerr << "  Minimum/Maximum Frequency: " << device_info.MinimumFrequency << "/" << device_info.MaximumFrequency << " resolution: " << device_info.Resolution << std::endl;
		std::cerr << "  MinimumIQDecimation: " << device_info.MinimumIQDecimation << " ForcedIQFormat: " << device_info.ForcedIQFormat << std::endl;
	}

	void SpyServer::printSync() {
		std::cerr << "Client:" << std::endl;
		std::cerr << "  CanControl: " << client_sync.CanControl << " Gain: " << client_sync.Gain << " DeviceCenterFrequency: " << client_sync.DeviceCenterFrequency << std::endl;
		std::cerr << "  IQCenterFrequency: " << client_sync.IQCenterFrequency << "  Minimum/Maximum Frequency: " << client_sync.MinimumIQCenterFrequency << "/" << client_sync.MaximumIQCenterFrequency << " resolution: " << device_info.Resolution << std::endl;
	}

	bool SpyServer::setRate(uint32_t rate) {
		int idx = -1;

		for (int i = 0; i < _sample_rates.size(); i++) {
			if (_sample_rates[i].first == rate) {
				idx = i;
				break;
			}
		}

		if (idx == -1) {
			std::cerr << "SPYSERVER: sample rate not supported by server. Supported rates:" << std::endl;
			for (auto r : _sample_rates) {
				std::cerr << " " << r.first;
			}
			std::cerr << std::endl;
			throw std::runtime_error("SPYSERVER: rate not supported.");
		}

		sendSetting(SETTING_IQ_DECIMATION, { _sample_rates[idx].second });
		sendStreamFormat();

		return true;
	}


	bool SpyServer::setFreq(uint32_t f) {
		if (f < device_info.MinimumFrequency || f > device_info.MaximumFrequency) {
			throw std::runtime_error("SPYSERVER: server does not support required frequency.");
		}

		if (client_sync.CanControl == 0) {
			if ((f < client_sync.MinimumIQCenterFrequency || f > client_sync.MaximumIQCenterFrequency)) {
				throw std::runtime_error("SPYSERVER: cannot set frequency (outside of band).");
			}
		}

		sendSetting(SETTING_IQ_FREQUENCY, { f });
		sendStreamFormat();

		return true;
	}

	void SpyServer::getDeviceList(std::vector<Description>& DeviceList) {
		DeviceList.push_back(Description("SPYSERVER", "SPYSERVER", "SPYSERVER", (uint64_t)0, Type::SPYSERVER));
	}

	Setting& SpyServer::Set(std::string option, std::string arg) {
		Util::Convert::toUpper(option);

		if (option == "GAIN") {
			tuner_gain = Util::Parse::Float(arg, 0, 50);
		}
		else if (option == "HOST") {
			host = arg;
		}
		else if (option == "PORT") {
			port = arg;
		}
		else
			Device::Set(option, arg);

		return *this;
	}

	std::string SpyServer::Get() {
		return Device::Get() + " host " + host + " port " + port + " gain " + std::to_string(tuner_gain);
	}
}
