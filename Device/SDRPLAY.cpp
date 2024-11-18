/*
	Copyright(c) 2021-2024 jvde.github@gmail.com

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

#include "SDRPLAY.h"

namespace Device {

	// API described here: https://www.sdrplay.com/docs/SDRplay_API_Specification_v3.01.pdf

#ifdef HASSDRPLAY

	//---------------------------------------
	// Device SDRPLAY

	int SDRPLAY::API_count = 0;

	SDRPLAY::SDRPLAY() : Device(Format::CF32, 2304000, Type::SDRPLAY) {
		float version = 0.0;

		if (API_count++ == 0 && sdrplay_api_Open() != sdrplay_api_Success) {
			running = false;
			return;
		}
		if (sdrplay_api_ApiVersion(&version) != sdrplay_api_Success) {
			running = false;
			return;
		}
		if ((int)version != 3) {
			running = false;
			return;
		}

		running = true;
	}

	SDRPLAY::~SDRPLAY() {
		if (--API_count) sdrplay_api_Close();
	}

	void SDRPLAY::Open(uint64_t h) {
		sdrplay_api_ErrT err;
		unsigned int DeviceCount;

		sdrplay_api_LockDeviceApi();
		sdrplay_api_DeviceT devices[SDRPLAY_MAX_DEVICES];
		sdrplay_api_GetDevices(devices, &DeviceCount, SDRPLAY_MAX_DEVICES);

		if (DeviceCount < h) throw std::runtime_error("SDRPLAY: cannot open device, handle not available.");

		device = devices[h];

		err = sdrplay_api_SelectDevice(&device);
		if (err != sdrplay_api_Success) {
			Error() << sdrplay_api_GetErrorString(err) << std::endl;
			sdrplay_api_UnlockDeviceApi();
			throw std::runtime_error("SDRPLAY: cannot open device");
		}
		sdrplay_api_UnlockDeviceApi();

		if (sdrplay_api_GetDeviceParams(device.dev, &deviceParams) != sdrplay_api_Success) {
			throw std::runtime_error("SDRPLAY: cannot get device parameters.");
		}


		if (streaming) throw std::runtime_error("SDRPLAY: internal error, settings modified while streaming.");

		if (device.hwVer == SDRPLAY_RSPdx_ID) {
			switch (antenna) {
			case 'A':
				deviceParams->devParams->rspDxParams.antennaSel = sdrplay_api_RspDx_ANTENNA_A;
				break;
			case 'B':
				deviceParams->devParams->rspDxParams.antennaSel = sdrplay_api_RspDx_ANTENNA_B;
				break;
			case 'C':
				deviceParams->devParams->rspDxParams.antennaSel = sdrplay_api_RspDx_ANTENNA_C;
				break;
			default:
				throw std::runtime_error("SDRPLAY: Invalid antenna selection.");
			}
		}

		chParams = deviceParams->rxChannelA;

		chParams->ctrlParams.agc.enable = AGC ? sdrplay_api_AGC_CTRL_EN : sdrplay_api_AGC_DISABLE;
		chParams->tunerParams.gain.gRdB = gRdB;
		chParams->tunerParams.gain.LNAstate = LNAstate;

		Device::Open(h);
	}

	void SDRPLAY::Play() {
		fifo.Init(16 * 16384, 8);

		deviceParams->devParams->fsFreq.fsHz = sample_rate;
		chParams->tunerParams.ifType = sdrplay_api_IF_Zero;
		chParams->ctrlParams.decimation.enable = 0;
		chParams->ctrlParams.decimation.decimationFactor = 1;
		chParams->ctrlParams.decimation.wideBandSignal = 1;
		chParams->tunerParams.bwType = sdrplay_api_BW_0_200;
		chParams->tunerParams.rfFreq.rfHz = frequency;

		sdrplay_api_CallbackFnsT cbFns;
		cbFns.StreamACbFn = callback_static;
		cbFns.EventCbFn = callback_event_static;

		sdrplay_api_ErrT e = sdrplay_api_Init(device.dev, &cbFns, (void*)this);
		if (e != sdrplay_api_Success) throw std::runtime_error("SDRPLAY: cannot start device - " + std::string(sdrplay_api_GetErrorString(e)));

		Device::Play();

		run_thread = std::thread(&SDRPLAY::Run, this);

		SleepSystem(10);
	}

	void SDRPLAY::Stop() {
		if (isStreaming()) {
			Device::Stop();
			fifo.Halt();

			sdrplay_api_Uninit(device.dev);
			if (run_thread.joinable()) run_thread.join();
		}
	}

	void SDRPLAY::Close() {
		Device::Close();
		sdrplay_api_ReleaseDevice(&device);
	}


	void SDRPLAY::callback_static(short* xi, short* xq, sdrplay_api_StreamCbParamsT* params, unsigned int numSamples, unsigned int reset, void* cbContext) {
		((SDRPLAY*)cbContext)->callback(xi, xq, params, numSamples, reset);
	}

	void SDRPLAY::callback_event_static(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT* params, void* cbContext) {
		((SDRPLAY*)cbContext)->callback_event(eventId, tuner, params);
	}

	void SDRPLAY::callback_event(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT* params) {
		if (eventId == sdrplay_api_DeviceRemoved) {
			Error() << "SDRPLAY: device disconnected" << std::endl;
			Stop();
		}
	}

	void SDRPLAY::callback(short* xi, short* xq, sdrplay_api_StreamCbParamsT* params, unsigned int len, unsigned int reset) {
		if (output.size() < len) output.resize(len);

		if (isStreaming()) {
			for (int i = 0; i < len; i++) {
				output[i].real(xi[i] / 32768.0f);
				output[i].imag(xq[i] / 32768.0f);
			}

			if (!fifo.Push((char*)output.data(), len * sizeof(CFLOAT32)))
				Error() << "SDRPLAY: buffer overrun." << std::endl;
		}
	}

	void SDRPLAY::Run() {
		while (isStreaming()) {
			if (fifo.Wait()) {
				RAW r = { Format::CF32, fifo.Front(), fifo.BlockSize() };
				Send(&r, 1, tag);
				fifo.Pop();
			}
			else if (isStreaming())
				Error() << "SDRPLAY: timeout." << std::endl;
		}
	}

	void SDRPLAY::getDeviceList(std::vector<Description>& DeviceList) {
		unsigned int DeviceCount;
		if (!running) throw std::runtime_error("SDRPLAY: API v3.x not running");

		sdrplay_api_LockDeviceApi();
		sdrplay_api_DeviceT devices[SDRPLAY_MAX_DEVICES];
		sdrplay_api_GetDevices(devices, &DeviceCount, SDRPLAY_MAX_DEVICES);

		for (int i = 0; i < DeviceCount; i++)
			DeviceList.push_back(Description("SDRPLAY", getHardwareDescription(devices[i].hwVer), devices[i].SerNo, (uint64_t)i, Type::SDRPLAY));


		sdrplay_api_UnlockDeviceApi();
	}

	std::string SDRPLAY::getHardwareDescription(unsigned char hwver) {
		switch (hwver) {
		case SDRPLAY_RSP1_ID:
			return "RSP1";
		case SDRPLAY_RSP1A_ID:
			return "RSP1A";
		case SDRPLAY_RSPdx_ID:
			return "RSPDX";
		default:
			break;
		}
		return "UNKNOWN";
	}

#endif
	Setting& SDRPLAY::Set(std::string option, std::string arg) {
		Util::Convert::toUpper(option);

		if (option == "AGC") {
			AGC = Util::Parse::Switch(arg);
		}
		else if (option == "LNASTATE") {
			LNAstate = Util::Parse::Integer(arg, 0, 9);
		}
		else if (option == "GRDB") {
			gRdB = Util::Parse::Integer(arg, 0, 59);
		}
		else if (option == "ANTENNA") {
			if (antenna == 'A' || antenna == 'B') {
				antenna = arg[0];
			}
			else
				throw std::runtime_error("SDRPLAY: invalid antenna.");
		}
		else
			Device::Set(option, arg);

		return *this;
	}

	std::string SDRPLAY::Get() {
		return Device::Get() + " agc " + Util::Convert::toString(AGC) + " lnastate " + std::to_string(LNAstate) + " grdb " + std::to_string(gRdB) + " ANTENNA " + antenna + " ";
	}

}
