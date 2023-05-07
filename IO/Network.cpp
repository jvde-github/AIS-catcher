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

#include <cstring>

#include "AIS-catcher.h"
#include "Network.h"
#include "Utilities.h"

namespace IO {

#ifdef _WIN32
	static class WSA {
	public:
		WSA() {
			WSADATA wsaData;
			WSAStartup(MAKEWORD(2, 2), &wsaData);
		}

		~WSA() { WSACleanup(); }
	} _wsa;
#endif

	void HTTP::Start() {
#ifdef HASCURL
		if (!running) {

			running = true;
			terminate = false;

			run_thread = std::thread(&HTTP::process, this);
			std::cerr << "HTTP: start thread (" << url << "), filter: " << Util::Convert::toString(filter.isOn());
			if (filter.isOn()) std::cerr << ", Allowed: " << filter.getAllowed() << ".";
			std::cerr << std::endl;
		}
#else
		throw std::runtime_error("HTTP: not implemented, please recompile with libcurl support.");
#endif
	}

	void HTTP::Stop() {
#ifdef HASCURL
		if (running) {

			running = false;
			terminate = true;
			run_thread.join();

			std::cerr << "HTTP: stop thread (" << url << ")." << std::endl;
		}
#endif
	}

#ifdef HASCURL

	// curl callback
	size_t HTTP::curl_cb(char* contents, size_t size, size_t nmemb, char* s) {
		int len = MIN(size * nmemb, 1023);

		std::memcpy(s, contents, len);
		s[len] = '\0';
		return len;
	}

	void HTTP::send(const std::string& msg, const std::string& copyname) {
		CURL* ch;
		CURLcode r;

		struct curl_slist* headers = NULL;
		long retcode = 200;
		struct curl_httppost *post = NULL, *last = NULL;

		bool multipart = PROTOCOL::APRS == protocol;

		response[0] = '\0';

		headers = curl_slist_append(NULL, "Expect:");
		if (gzip) {
			zip.zip(msg);
			headers = curl_slist_append(headers, "Content-encoding: gzip");
		}

		if (multipart)
			curl_formadd(&post, &last, CURLFORM_COPYNAME, copyname.c_str(), CURLFORM_CONTENTTYPE, "application/json", CURLFORM_PTRCONTENTS, msg.c_str(), CURLFORM_END);
		else {
			headers = curl_slist_append(headers, "Content-Type: application/json");
		}

		if (!(ch = curl_easy_init())) {
			std::cerr << "HTTP: cannot initialize curl." << std::endl;
			return;
		}

		if (!headers)
			std::cerr << "HTTP: append for expect header failed" << std::endl;
		else
			try {

				if (!multipart) {
					if ((r = curl_easy_setopt(ch, CURLOPT_POSTFIELDS, gzip ? zip.getOutputPtr() : msg.c_str()))) throw r;
					if ((r = curl_easy_setopt(ch, CURLOPT_POSTFIELDSIZE, (long)(gzip ? zip.getOutputLength() : msg.length())))) throw r;
				}
				else if ((r = curl_easy_setopt(ch, CURLOPT_HTTPPOST, post)))
					throw r;

				if ((r = curl_easy_setopt(ch, CURLOPT_URL, url.c_str()))) throw r;
				if ((r = curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers))) throw r;
				if ((r = curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, curl_cb))) throw r;
				if ((r = curl_easy_setopt(ch, CURLOPT_WRITEDATA, response))) throw r;
				if ((r = curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 1L))) throw r;
				if (!userpwd.empty() && (r = curl_easy_setopt(ch, CURLOPT_USERPWD, userpwd.c_str()))) throw r;
				if ((r = curl_easy_setopt(ch, CURLOPT_VERBOSE, 0L))) throw r;
				if ((r = curl_easy_setopt(ch, CURLOPT_TIMEOUT, (long)TIMEOUT))) throw r;

				if ((r = curl_easy_perform(ch))) throw r;
				if ((r = curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &retcode))) throw r;
			}
			catch (CURLcode cc) {
				std::cerr << "HTTP: error - " << curl_easy_strerror(cc) << " (" << url << ")" << std::endl;
			}

		curl_easy_cleanup(ch);

		if (headers) curl_slist_free_all(headers);
		if (multipart) curl_formfree(post);

		if (retcode != 200) {
			std::cerr << "HTTP: server " << url << " returned " << retcode << std::endl;
		}

		if (show_response)
			std::cerr << "HTTP: server " << url << " response - " << response << std::endl;

		return;
	}

	void HTTP::post() {
		if (!queue.size()) return;

		std::list<std::string> send_list;

		{
			const std::lock_guard<std::mutex> lock(queue_mutex);
			send_list.splice(send_list.begin(), queue);
		}

		msg.clear();
		std::time_t now = std::time(0);

		if (protocol == PROTOCOL::AISCATCHER) {
			msg += "{\n\t\"protocol\": \"" + protocol_string + "\",";
			msg += "\n\t\"encodetime\": \"" + Util::Convert::toTimeStr(now) + "\",";
			msg += "\n\t\"stationid\": ";
			builder.stringify(stationid, msg);
			msg += ",";
			msg += "\n\t\"receiver\":\n\t\t{\n\t\t\"description\": \"AIS-catcher " VERSION "\",";
			msg += "\n\t\t\"version\": " + std::to_string(VERSION_NUMBER) + ",\n\t\t\"engine\": ";
			builder.stringify(model, msg);
			msg += ",\n\t\t\"setting\": ";
			builder.stringify(model_setting, msg);
			msg += "\n\t\t},\n\t\"device\":\n\t\t{\n\t\t\"product\": ";
			builder.stringify(product, msg);
			msg += ",\n\t\t\"vendor\": ";
			builder.stringify(vendor, msg);
			msg += ",\n\t\t\"serial\": ";
			builder.stringify(serial, msg);
			msg += ",\n\t\t\"setting\": ";
			builder.stringify(device_setting, msg);
			msg += "\n\t\t},\n\t\"msgs\": [";

			char delim = ' ';
			for (auto it = send_list.begin(); it != send_list.end(); ++it) {
				msg = msg + delim + "\n\t\t" + *it;
				delim = ',';
			}

			msg += "\n\t]\n}\n";

			send(msg, "");
		}
		else if (PROTOCOL::AIRFRAMES == protocol) {
			msg += "{\n\t\"app\": {\n\t\t\"name\": \"AIS-Catcher\",\n\t\t\"ver\": \"" VERSION "\"";
			msg += "\n\t},\n\t\"source\": {\n\t\t\"transport\": \"vhf\",\n\t\t\"protocol\": \"ais\",\n\t\t\"station_id\": ";
			builder.stringify(stationid, msg);
			msg += ",\n\t\t\"lat\": " + std::to_string(lat);
			msg += ",\n\t\t\"lon\": " + std::to_string(lon);
			msg += "\n\t\t},\n\t\"msgs\": [";

			char delim = ' ';
			for (auto it = send_list.begin(); it != send_list.end(); ++it) {
				msg = msg + delim + "\n\t\t" + *it;
				delim = ',';
			}

			msg += "\n\t]\n}\n";
			send(msg, "");
		}
		else if (PROTOCOL::APRS == protocol) {
			msg += "{\n\t\"protocol\": \"jsonais\",";
			msg += "\n\t\"encodetime\": \"" + Util::Convert::toTimeStr(now) + "\",";
			msg += "\n\t\"groups\": [\n\t{\n\t\t\"path\": [{ \"name\": ";
			builder.stringify(stationid, msg);
			msg += ", \"url\" : ";
			builder.stringify(url, msg);
			msg += " }],\n\t\t\"msgs\": [";

			char delim = ' ';
			for (auto it = send_list.begin(); it != send_list.end(); ++it) {
				msg = msg + delim + "\n\t\t\t" + *it;
				delim = ',';
			}

			msg += "\n\t\t]\n\t}]\n}";

			send(msg, "jsonais");
		}
		else if (PROTOCOL::LIST == protocol) {

			for (auto it = send_list.begin(); it != send_list.end(); ++it) {
				msg += std::string(*it) + "\n";
			}
			send(msg, "");
		}
	}

	void HTTP::process() {
		int i = 0;

		while (!terminate) {

			for (int i = 0; i < INTERVAL; i++) {
				SleepSystem(1000);
				if (terminate) break;
			}
			if (!url.empty()) post();
		}
	}
#endif

	Setting& HTTP::Set(std::string option, std::string arg) {
#ifdef HASCURL
		Util::Convert::toUpper(option);

		if (option == "URL") {
			url = arg;
		}
		else if (option == "USERPWD") {
			userpwd = arg;
		}
		else if (option == "STATIONID" || option == "ID" || option == "CALLSIGN") {
			stationid = arg;
		}
		else if (option == "INTERVAL") {
			INTERVAL = Util::Parse::Integer(arg, 1, 60 * 60 * 24);
		}
		else if (option == "TIMEOUT") {
			TIMEOUT = Util::Parse::Integer(arg, 1, 30);
		}
		else if (option == "MODEL") {
			model = arg;
		}
		else if (option == "MODEL_SETTING") {
			model_setting = arg;
		}
		else if (option == "PRODUCT") {
			product = arg;
		}
		else if (option == "VENDOR") {
			vendor = arg;
		}
		else if (option == "SERIAL") {
			serial = arg;
		}
		else if (option == "LAT") {
			lat = Util::Parse::Float(arg);
		}
		else if (option == "LON") {
			lon = Util::Parse::Float(arg);
		}
		else if (option == "DEVICE_SETTING") {
			device_setting = arg;
		}
		else {
			Util::Convert::toUpper(arg);

			if (option == "GZIP") {
				gzip = Util::Parse::Switch(arg);
				if (gzip && !zip.installed())
					throw std::runtime_error("HTTP: ZLIB not installed");
			}
			else if (option == "RESPONSE") {
				show_response = Util::Parse::Switch(arg);
			}
			else if (option == "PROTOCOL") {

				if (arg == "AISCATCHER") {
					builder.setMap(JSON_DICT_FULL);
					protocol_string = "jsonaiscatcher";
					protocol = PROTOCOL::AISCATCHER;
				}
				else if (arg == "MINIMAL") {
					builder.setMap(JSON_DICT_MINIMAL);
					protocol_string = "jsonaiscatcher";
					protocol = PROTOCOL::AISCATCHER;
				}
				else if (arg == "AIRFRAMES") {
					builder.setMap(JSON_DICT_MINIMAL);
					protocol_string = "airframes";
					protocol = PROTOCOL::AIRFRAMES;
					gzip = zip.installed();
					INTERVAL = 30;
				}
				else if (arg == "LIST") {
					builder.setMap(JSON_DICT_FULL);
					protocol = PROTOCOL::LIST;
				}
				else if (arg == "APRS") {
					builder.setMap(JSON_DICT_APRS);
					protocol = PROTOCOL::APRS;
				}

				else
					throw std::runtime_error("HTTP: error - unknown protocol");
			}
			else
				filter.Set(option, arg);
		}
#else
		throw std::runtime_error("HTTP: not implemented, please recompile with libcurl support.");
#endif
		return *this;
	}

	UDP::UDP() {}

	UDP::~UDP() {
		Stop();
	}

	void UDP::Receive(const AIS::Message* data, int len, TAG& tag) {
		if(reset > 0) {
			long now = (long) std::time(nullptr);
			if ((now - last_reconnect) > 60*reset) {

				std::cerr << "UDP: recreate socket (" << host << ":" << port << ")" << std::endl;

				closesocket(sock);
				sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);

				if (sock == -1) {
					std::cerr << "UDP: cannot create socket. Requesting termination.\n";
					StopRequest();
				}

				last_reconnect = now;
			}
		}
		
		if (sock != -1) {
			if (!JSON) {
				for (int i = 0; i < len; i++) {
					if (!filter.include(data[i])) continue;

					for (const auto& s : data[i].NMEA)
						SendTo((s + "\r\n").c_str());
				}
			}
			else {
				for (int i = 0; i < len; i++) {
					if (!filter.include(data[i])) continue;

					std::string str = "{\"class\":\"AIS\",\"device\":\"AIS-catcher\",\"channel\":\"";
					str += ((char)data[i].getChannel());
					str += std::string("\"");

					if (tag.mode & 2) {
						// str += ",\"rxtime\":\"" + data[i].getRxTime() + "\"";
						str += ",\"rxuxtime\":" + std::to_string(data[i].getRxTimeUnix());
					}
					if (tag.mode & 1) str += ",\"signalpower\":" + std::to_string(tag.level) + ",\"ppm\":" + std::to_string(tag.ppm);

					str += ",\"mmsi\":" + std::to_string(data[i].mmsi()) + ",\"type\":" + std::to_string(data[i].type());
					str += ",\"nmea\":[\"" + data[i].NMEA[0] + std::string("\"");

					for (int j = 1; j < data[i].NMEA.size(); j++) str += ",\"" + data[i].NMEA[j] + "\"";
					str += "]}\r\n";
					SendTo(str);
				}
			}
		}
	}

	void UDP::Start() {
		std::cerr << "UDP: open socket for host: " << host << ", port: " << port << ", filter: " << Util::Convert::toString(filter.isOn());
		if (filter.isOn()) std::cerr << ", allowed: {" << filter.getAllowed() << "}";
		std::cerr << ", JSON: " << Util::Convert::toString(JSON);
		if(reset > 0) std::cerr << ", reset: " << reset;
		std::cerr << std::endl;

		if (sock != -1) {
			throw std::runtime_error("UDP: internal error, socket already defined.");
		}

		struct addrinfo h;
		memset(&h, 0, sizeof(h));
		h.ai_family = AF_UNSPEC;
		h.ai_socktype = SOCK_DGRAM;
		h.ai_protocol = IPPROTO_UDP;
#ifndef _WIN32
		h.ai_flags = AI_ADDRCONFIG;
#endif

		int code = getaddrinfo(host.c_str(), port.c_str(), &h, &address);

		if (code != 0 || address == NULL) {
			std::string error = (const char*)gai_strerror(code);
			throw std::runtime_error("UDP network address and/or port not valid: " + host + " port " + port + " code: " + error);
		}

		sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);

		if (sock == -1) {
			throw std::runtime_error("cannot create socket for UDP " + host + " port " + port);
		}
		if(reset > 0)
			last_reconnect = (long) std::time(nullptr);
	}

	void UDP::Stop() {
		std::cerr << "UDP: close socket for host: " << host << ", port: " << port << std::endl;

		if (sock != -1) {
			closesocket(sock);
			sock = -1;
		}
		if (address != NULL) {
			freeaddrinfo(address);
			address = NULL;
    	}
	}

	Setting& UDP::Set(std::string option, std::string arg) {
		Util::Convert::toUpper(option);

		if (option == "HOST") {
			host = arg;
		}
		else if (option == "PORT") {
			port = arg;
		}
		else if (option == "JSON") {
			JSON = Util::Parse::Switch(arg);
		}
		else if (option == "RESET") {
			reset = Util::Parse::Integer(arg,1,24*60);
		}
		else
			return filter.Set(option, arg);

		return *this;
	}


	// TCP output to server

	void TCP::Receive(const AIS::Message* data, int len, TAG& tag) {
		if (!JSON) {
			for (int i = 0; i < len; i++) {
				if (!filter.include(data[i])) continue;

				for (const auto& s : data[i].NMEA) {
					if (SendTo((s + "\r\n").c_str()) < 0)
						if (!persistent) {
							std::cerr << "TCP feed: requesting termination.\n";
							StopRequest();
						}
				}
			}
		}
		else {
			for (int i = 0; i < len; i++) {
				if (!filter.include(data[i])) continue;

				std::string str = "{\"class\":\"AIS\",\"device\":\"AIS-catcher\",\"channel\":\"";
				str += ((char)data[i].getChannel());
				str += std::string("\"");

				if (tag.mode & 2) {
					// str += ",\"rxtime\":\"" + data[i].getRxTime() + "\"";
					str += ",\"rxuxtime\":" + std::to_string(data[i].getRxTimeUnix());
				}
				if (tag.mode & 1) str += ",\"signalpower\":" + std::to_string(tag.level) + ",\"ppm\":" + std::to_string(tag.ppm);

				str += ",\"mmsi\":" + std::to_string(data[i].mmsi()) + ",\"type\":" + std::to_string(data[i].type());
				str += ",\"nmea\":[\"" + data[i].NMEA[0] + std::string("\"");

				for (int j = 1; j < data[i].NMEA.size(); j++) str += ",\"" + data[i].NMEA[j] + "\"";
				str += "]}\r\n";

				if(SendTo(str) <0) 
					if (!persistent) {
						std::cerr << "TCP feed: requesting termination.\n";
						StopRequest();
					}
			}
		}
	}

	void TCP::Start() {
		std::cerr << "TCP feed: open socket for host: " << host << ", port: " << port << ", filter: " << Util::Convert::toString(filter.isOn());
		if (filter.isOn()) std::cerr << ", allowed: {" << filter.getAllowed() << "}";
		std::cerr << ", PERSIST: " << Util::Convert::toString(persistent);
		std::cerr << ", JSON: " << Util::Convert::toString(JSON) << ", status: ";

		if (tcp.connect(host, port, persistent, 0)) 
			std::cerr << "connected\n";
		else
			std::cerr << "pending\n";
	}

	void TCP::Stop() {
		tcp.disconnect();
	}

	Setting& TCP::Set(std::string option, std::string arg) {
		Util::Convert::toUpper(option);

		if (option == "HOST") {
			host = arg;
		}
		else if (option == "PORT") {
			port = arg;
		}
		else if (option == "JSON") {
			JSON = Util::Parse::Switch(arg);
		}
		else if (option == "PERSIST") {
			persistent = Util::Parse::Switch(arg);
		}
		else
			return filter.Set(option, arg);

		return *this;
	}
}
