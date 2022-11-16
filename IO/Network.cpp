/*
	Copyright(c) 2021-2022 jvde.github@gmail.com

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

namespace IO {


	void HTTP::startServer() {
#ifdef HASCURL
		if (!running) {

			running = true;
			terminate = false;

			run_thread = std::thread(&HTTP::process, this);
			std::cerr << "HTTP: start server (" << url << ")." << std::endl;
		}
#else
		throw "HTTP: not implemented, please recompile with libcurl support.";
#endif
	}

	void HTTP::stopServer() {
#ifdef HASCURL
		if (running) {

			running = false;
			terminate = true;
			run_thread.join();

			std::cerr << "HTTP: stop server (" << url << ")." << std::endl;
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
			std::cerr << "HTTP: server response - " << response << std::endl;

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
			msg += "{\n\t\"protocol\": \"jsonaiscatcher\",";
			msg += "\n\t\"encodetime\": \"" + Util::Convert::toTimeStr(now) + "\",";
			msg += "\n\t\"stationid\": \"" + jsonify(stationid) + "\",";
			msg += "\n\t\"receiver\":\n\t\t{";
			msg += "\n\t\t\"description\": \"AIS-catcher " VERSION "\",";
			msg += "\n\t\t\"version\": " + std::to_string(VERSION_NUMBER) + ",";
			msg += "\n\t\t\"engine\": \"" + jsonify(model) + "\",";
			msg += "\n\t\t\"setting\": \"" + jsonify(model_setting) + "\"";
			msg += "\n\t\t},";
			msg += "\n\t\"device\":\n\t\t{";
			msg += "\n\t\t\"product\": \"" + jsonify(product) + "\",";
			msg += "\n\t\t\"vendor\": \"" + jsonify(vendor) + "\",";
			msg += "\n\t\t\"serial\": \"" + jsonify(serial) + "\",";
			msg += "\n\t\t\"setting\": \"" + jsonify(device_setting) + "\"";
			msg += "\n\t\t},";
			msg += "\n\t\"msgs\": [";

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
			msg += "\n\t\"groups\": [";
			msg += "\n\t{";
			msg += "\n\t\t\"path\": [{ \"name\": \"" + jsonify(stationid) + "\", \"url\" : \"" + jsonify(url) + "\" }],";

			msg += "\n\t\t\"msgs\": [";

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

	void HTTP::Set(std::string option, std::string arg) {
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
		else if (option == "DEVICE_SETTING") {
			device_setting = arg;
		}
		else {
			Util::Convert::toUpper(arg);

			if (option == "GZIP") {
				gzip = Util::Parse::Switch(arg);
				if (gzip && !zip.installed())
					throw "HTTP: ZLIB not installed";
			}
			else if (option == "RESPONSE") {
				show_response = Util::Parse::Switch(arg);
			}
			else if (option == "PROTOCOL") {

				if (arg == "AISCATCHER") {
					setMap(JSON_DICT_FULL);
					protocol = PROTOCOL::AISCATCHER;
				}
				else if (arg == "MINIMAL") {
					setMap(JSON_DICT_MINIMAL);
					protocol = PROTOCOL::AISCATCHER;
				}
				else if (arg == "LIST") {
					setMap(JSON_DICT_FULL);
					protocol = PROTOCOL::LIST;
				}
				else if (arg == "APRS") {
					setMap(JSON_DICT_APRS);
					protocol = PROTOCOL::APRS;
				}
				else
					throw "HTTP: error - unknown protocol";
			}
			else
				throw "HTTP: Invalid setting.";
		}
#else
		throw "HTTP: not implemented, please recompile with libcurl support.";
#endif
	}
	UDP::UDP() {
#ifdef _WIN32
		WSADATA wsaData;

		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			throw "Cannot initialize Winsocket.";
			return;
		}
#endif
	}

	UDP::~UDP() {
		closeConnection();

#ifdef _WIN32
		WSACleanup();
#endif
	}

	void UDP::Receive(const AIS::Message* data, int len, TAG& tag) {
		if (sock != -1)
			for (int i = 0; i < len; i++)
				for (const auto& s : data[i].NMEA)
					sendto(sock, (s + "\r\n").c_str(), (int)s.length() + 2, 0, address->ai_addr,
						   (int)address->ai_addrlen);
	}

	void UDP::openConnection(const std::string& host, const std::string& port) {
		if (sock != -1) {
			throw "UDP: internal error, socket already defined.";
			return;
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
			throw "UDP network address and/or port not valid.";
			return;
		}

		sock = socket(address->ai_family, address->ai_socktype, address->ai_protocol);

		if (sock == -1) {
			throw "Error creating socket for UDP.";
		}
	}

	void UDP::closeConnection() {
		if (sock != -1) {
			closesocket(sock);
			sock = -1;
		}
	}

	void TCP::Receive(const AIS::Message* data, int len, TAG& tag) {

		for (int i = 0; i < len; i++)
			for (const auto& s : data[i].NMEA)
				con.send((s + "\r\n").c_str(), (int)s.length() + 2);
	}

	void TCP::openConnection(const std::string& host, const std::string& port) {
		if (!con.connect(host, port))
			throw "TCP: cannot connect to server.";
	}

	void TCP::closeConnection() {
		con.disconnect();
	}
}
