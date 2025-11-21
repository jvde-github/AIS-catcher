/*
	Copyright(c) 2024 jvde.github@gmail.com

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


// Work in progress. This code is using the NMEA2000 library to properly manage address selection and heartbeat
// The functionality has only been tested on a virtual socketCAN network using CANboat
// The code is not yet ready for production use

// Sources:
// 	https://github.com/thomasonw/NMEA2000_socketCAN

#include <cstring>

#ifdef HASNMEA2000
#include "Common.h"
#include "JSONAIS.h"

#include <iostream>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#include <NMEA2000.h>
#include "N2KInterface.h"

using namespace std;

namespace N2K {

	N2KHubInterfaceHub N2KInterface;

	// This code is based and an extension of https://github.com/thomasonw/NMEA2000_socketCAN
	bool tNMEA2000_SKTCAN::CANOpen() {
		return OpenInterface(CANinterface);
	}

	bool tNMEA2000_SKTCAN::OpenInterface(const std::string& iface) {
		struct ifreq ifr = { 0 };
		struct sockaddr_can addr = { 0 };

		skt = socket(PF_CAN, SOCK_RAW, CAN_RAW);
		if (skt < 0) {
			Error() << "NMEA2000: cannot open CAN socket: " << CANinterface;
			return false;
		}

		strcpy(ifr.ifr_name, iface.c_str());

		if (ioctl(skt, SIOCGIFINDEX, &ifr) < 0) {
			Error() << "NMEA2000: ioctl failed on " << ifr.ifr_name << endl;
			return false;
		}

		addr.can_family = AF_CAN;
		addr.can_ifindex = ifr.ifr_ifindex;
		if (bind(skt, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
			Error() << "NMEA2000: cannot bind to socket." << endl;
			return false;
		}

		int r = fcntl(skt, F_GETFL, 0);
		if (r < 0 || fcntl(skt, F_SETFL, r | O_NONBLOCK) < 0)
			return false;

		return true;
	}

	bool tNMEA2000_SKTCAN::CANSendFrame(unsigned long id, unsigned char len, const unsigned char* buf, bool wait_sent) {
		struct can_frame frame_wr = { 0 };

		frame_wr.can_id = id | CAN_EFF_FLAG;
		frame_wr.can_dlc = len;
		memcpy(frame_wr.data, buf, 8);

		return (write(skt, &frame_wr, sizeof(frame_wr)) == sizeof(frame_wr));
	}


	bool tNMEA2000_SKTCAN::CANGetFrame(unsigned long& id, unsigned char& len, unsigned char* buf) {
		struct can_frame frame_rd;
		struct timeval tv = { 0, 0 };
		fd_set fds;

		FD_ZERO(&fds);
		FD_SET(skt, &fds);

		if (select((skt + 1), &fds, NULL, NULL, &tv) < 0)
			return false;

		if (FD_ISSET(skt, &fds)) {
			if (read(skt, &frame_rd, sizeof(frame_rd)) > 0) {
				memcpy(buf, frame_rd.data, 8);
				len = frame_rd.can_dlc;
				id = frame_rd.can_id;
				return true;
			}
		}
		return false;
	}

	// additions not in tNMEA2000
	void tNMEA2000_SKTCAN::waitForFrame(int milliseconds) {
		struct timeval tv = { 0, milliseconds * 1000 };
		fd_set fds;

		FD_ZERO(&fds);
		FD_SET(skt, &fds);

		select((skt + 1), &fds, NULL, NULL, &tv);
	}

	void tNMEA2000_SKTCAN::Close() {
		if (skt != -1) {
			close(skt);
		}
	}

	// N2KHubInterfaceHub - centralized input and output

	// static
	void N2KHubInterfaceHub::onMsgStatic(const tN2kMsg& N2kMsg) {
		N2KInterface.onMsg(N2kMsg);
	}

	void N2KHubInterfaceHub::onOpenStatic() {
		N2KInterface.onOpen();
	}

	void N2KHubInterfaceHub::onOpen() {
		connected = true;
		Info() << "NMEA2000: Connected" ;
	}

	// non-static
	void N2KHubInterfaceHub::onMsg(const tN2kMsg& N2kMsg) {

		if (input != nullptr)
			input->onMsg(N2kMsg);
	}

	const unsigned long SupportedMessages[] = { 129038L, 129793L, 129794L, 129798L, 129039L, 129040L, 129809L, 129810L, 129041L, 129802L, 129802L, 0 };

	void N2KHubInterfaceHub::Start() {

		if (!running && (output || input)) {
			NMEA2000.setNetwork(network);	

			if (input && output)
				Warning()<< "NMEA2000: warning input and output are both enabled. Device will not receive own messages." ;

			NMEA2000.SetProductInformation("00000002", 100, "AIS-catcher NMEA2000 plugin", "0.55", "0.55");
			NMEA2000.SetDeviceInformation(1, 195, 70, 2046);

			if (output)
				NMEA2000.ExtendTransmitMessages(SupportedMessages);
			if (input)
				NMEA2000.ExtendReceiveMessages(SupportedMessages);

			NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode, 23);
			NMEA2000.SetOnOpen(&N2KHubInterfaceHub::onOpenStatic);
			NMEA2000.SetMsgHandler(onMsgStatic);

			Info() << "Opening NMEA2000 network \"" + network + "\"..." ;

			if (!NMEA2000.Open()) {
				Info()  << "NMEA2000: not opened (yet).";
			}

			running = true;
			run_thread = std::thread(&N2KHubInterfaceHub::run, this);
		}
	}

	void N2KHubInterfaceHub::Stop() {
		NMEA2000.Close();
		if (running) {
			running = false;
			run_thread.join();
		}
	}

	void N2KHubInterfaceHub::sendMsg(const tN2kMsg& N2kMsg) {
		if (running) {
			std::lock_guard<std::mutex> lock(mtx);
			NMEA2000.SendMsg(N2kMsg);
		}
	}

	void N2KHubInterfaceHub::run() {
		while (running) {
			{
				std::unique_lock<std::mutex> lck(mtx);
				NMEA2000.ParseMessages();
			}
			NMEA2000.waitForFrame(250);
		}
	}
}

#endif