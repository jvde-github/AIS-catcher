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

#ifdef HASSDRPLAY

#include <cstdlib>
#include <cstring>
#include <mutex>

#include "SDRPLAY.h"

namespace Device {

	// API described here: https://www.sdrplay.com/docs/SDRplay_API_Specification_v3.01.pdf

	//---------------------------------------
	// Device SDRPLAY

	// The API service connection is opened on first use, not on construction,
	// and held until process exit. A failed open is retried on the next call,
	// so a service started after AIS-catcher is picked up.
	static std::mutex api_mtx;
	static bool api_open = false;

	static bool ensureAPI() {
		std::lock_guard<std::mutex> lock(api_mtx);

		if (!api_open && sdrplay_api_Open() == sdrplay_api_Success) {
			api_open = true;
			std::atexit([]() { sdrplay_api_Close(); });
		}
		return api_open;
	}

	SDRPLAY::SDRPLAY() : Device(Format::CF32, 2304000, Type::SDRPLAY, "SDRPLAY") {}

	void SDRPLAY::Open(uint64_t h) {
		if (!ensureAPI()) throw std::runtime_error("SDRPLAY: API v3.x not running");

		sdrplay_api_ErrT err;
		unsigned int DeviceCount;

		sdrplay_api_LockDeviceApi();
		sdrplay_api_DeviceT devices[SDRPLAY_MAX_DEVICES];
		sdrplay_api_GetDevices(devices, &DeviceCount, SDRPLAY_MAX_DEVICES);

		if (h >= DeviceCount) throw std::runtime_error("SDRPLAY: cannot open device, handle not available.");

		device = devices[h];

		// Tuner and mode must be set before SelectDevice. A free RSPduo is
		// claimed single-tuner on the requested tuner; when another app runs
		// as master only slave mode is on offer and the API dictates the
		// tuner and the sample clock.
		if (device.hwVer == SDRPLAY_RSPduo_ID) {
			sdrplay_api_TunerSelectT want = (tuner == 'B') ? sdrplay_api_Tuner_B : sdrplay_api_Tuner_A;

			if (device.rspDuoMode & sdrplay_api_RspDuoMode_Master) {
				device.tuner = want;
				device.rspDuoMode = sdrplay_api_RspDuoMode_Single_Tuner;
			}
			else if (device.tuner != want)
				Warning() << "SDRPLAY: RSPduo tuner " << tuner << " not free, using the tuner offered by the master.";
		}

		err = sdrplay_api_SelectDevice(&device);
		if (err != sdrplay_api_Success) {
			Error() << sdrplay_api_GetErrorString(err);
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

		chParams = (device.tuner == sdrplay_api_Tuner_B) ? deviceParams->rxChannelB : deviceParams->rxChannelA;
		if (chParams == NULL)
			throw std::runtime_error("SDRPLAY: cannot get channel parameters.");

		// a slave inherits the master's ADC clock and streams at 2 MHz
		if (isDuoSlave()) {
			Info() << "SDRPLAY: RSPduo slave of a " << device.rspDuoSampleFreq / 1e6 << " MHz master, sample rate 2000K.";
			sample_rate = 2000000;
		}

		chParams->ctrlParams.agc.enable = AGC ? sdrplay_api_AGC_CTRL_EN : sdrplay_api_AGC_DISABLE;
		chParams->tunerParams.gain.gRdB = gRdB;
		chParams->tunerParams.gain.LNAstate = LNAstate;

		Device::Open(h);
	}

	void SDRPLAY::Play() {
		fifo.Init(16 * 16384, 8);

		// devParams is NULL for a duo slave: the master owns the clock, and the
		// slave runs low-IF at the IF its master's rate implies
		if (deviceParams->devParams != NULL)
			deviceParams->devParams->fsFreq.fsHz = sample_rate;

		if (isDuoSlave())
			chParams->tunerParams.ifType = device.rspDuoSampleFreq == 8000000.0 ? sdrplay_api_IF_2_048 : sdrplay_api_IF_1_620;
		else
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
			Error() << "SDRPLAY: device disconnected";
			Stop();
		}
		else if (eventId == sdrplay_api_RspDuoModeChange && params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterDllDisappeared) {
			Error() << "SDRPLAY: RSPduo master disappeared";
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
				Error() << "SDRPLAY: buffer overrun.";
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
				Error() << "SDRPLAY: timeout.";
		}
	}

	void SDRPLAY::getDeviceList(std::vector<Description>& DeviceList) {
		unsigned int DeviceCount;
		if (!ensureAPI()) {
			Warning() << "SDRPLAY: API v3.x not running, no SDRplay devices available.";
			return;
		}

		float version = 0.0;
		if (sdrplay_api_ApiVersion(&version) != sdrplay_api_Success || (int)version != 3) {
			Warning() << "SDRPLAY: API version is not 3.x, no SDRplay devices available.";
			return;
		}

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
		case SDRPLAY_RSP2_ID:
			return "RSP2";
		case SDRPLAY_RSPduo_ID:
			return "RSPDUO";
		case SDRPLAY_RSPdx_ID:
			return "RSPDX";
		default:
			break;
		}
		return "UNKNOWN";
	}

	Setting& SDRPLAY::SetKey(AIS::Keys key, const std::string &arg) {
		switch (key) {
		case AIS::KEY_SETTING_AGC:
			AGC = Util::Parse::Switch(arg);
			break;
		case AIS::KEY_SETTING_LNASTATE:
			LNAstate = Util::Parse::Integer(arg, 0, 9);
			break;
		case AIS::KEY_SETTING_GRDB:
			gRdB = Util::Parse::Integer(arg, 0, 59);
			break;
		case AIS::KEY_SETTING_ANTENNA: {
			char a = arg.empty() ? 0 : (arg[0] & ~0x20);
			if (a == 'A' || a == 'B' || a == 'C')
				antenna = a;
			else
				throw std::runtime_error("SDRPLAY: invalid antenna.");
			break;
		}
		case AIS::KEY_SETTING_TUNER: {
			char t = arg.empty() ? 0 : arg[0];
			if (t == '1' || t == '2')
				t = 'A' + t - '1';
			else
				t &= ~0x20;
			if (t == 'A' || t == 'B')
				tuner = t;
			else
				throw std::runtime_error("SDRPLAY: invalid tuner.");
			break;
		}
		default:
			Device::SetKey(key, arg);
			break;
		}
		return *this;
	}

	std::string SDRPLAY::Get() {
		return Device::Get() + " agc " + Util::Convert::toString(AGC) + " lnastate " + std::to_string(LNAstate) + " grdb " + std::to_string(gRdB) + " ANTENNA " + antenna + " TUNER " + tuner + " ";
	}
}

#endif
