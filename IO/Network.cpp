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

#include "Network.h"

namespace IO {

#ifdef HASCURL

	void HTTP::send(const std::string& msg) {

		CURL* ch;
		struct curl_slist* headers = NULL;
		long retcode = 200;
		CURLcode r;
		struct curl_httppost *post = NULL, *last = NULL;

		response[0] = '\0';

		if (protocol == PROTOCOL::APRS)
			curl_formadd(&post, &last, CURLFORM_COPYNAME, "jsonais", CURLFORM_CONTENTTYPE, "application/json", CURLFORM_PTRCONTENTS, msg.c_str(), CURLFORM_END);
		else
			curl_formadd(&post, &last, CURLFORM_COPYNAME, "jsonaiscatcher", CURLFORM_CONTENTTYPE, "application/json", CURLFORM_PTRCONTENTS, msg.c_str(), CURLFORM_END);

		if (!(ch = curl_easy_init())) {
			std::cerr << "HTTP: cannot initialize curl." << std::endl;
			return;
		}

		headers = curl_slist_append(NULL, "Expect:");
		if (!headers)
			std::cerr << "HTTP: append for expect header failed" << std::endl;
		else
			try {

				if ((r = curl_easy_setopt(ch, CURLOPT_HTTPPOST, post))) throw r;
				if ((r = curl_easy_setopt(ch, CURLOPT_URL, url.c_str()))) throw r;
				if ((r = curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers))) throw r;
				if ((r = curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, curl_cb))) throw r;
				if ((r = curl_easy_setopt(ch, CURLOPT_WRITEDATA, response))) throw r;
				if ((r = curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 1))) throw r;
				if ((r = curl_easy_setopt(ch, CURLOPT_VERBOSE, 0))) throw r;
				if ((r = curl_easy_setopt(ch, CURLOPT_TIMEOUT, (long)TIMEOUT))) throw r;

				if ((r = curl_easy_perform(ch))) throw r;
				if ((r = curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &retcode))) throw r;
			}
			catch (CURLcode cc) {
				std::cerr << "HTTP: error - " << curl_easy_strerror(cc) << " (" << url << ")" << std::endl;
			}

		curl_easy_cleanup(ch);

		if (headers) curl_slist_free_all(headers);

		if (retcode != 200) {
			std::cerr << "HTTP: server " << url << " returned " << retcode << std::endl;
		}

		if (show_response)
			std::cerr << "HTTP: server response - " << response << std::endl;

		return;
	}


	void HTTP::post() {

		std::string post;
		std::list<std::string> send_list;

		{
			const std::lock_guard<std::mutex> lock(queue_mutex);
			send_list.splice(send_list.begin(), queue);
		}

		std::time_t now = std::time(0);

		if (protocol == PROTOCOL::AISCATCHER) {
			post = "{\n\t\"protocol\": \"jsonaiscatcher\",";
			post = post + "\n\t\"encodetime\": \"" + Util::Convert::toTimeStr(now) + "\",";
			post = post + "\n\t\"stationid\": \"" + stationid + "\",";
			// post = post + "\n\t\"device\": \"" + device + "\",";
			post = post + "\n\t\"decoder\": \"" + model + "\",";
			post = post + "\n\t\"receiver\": \"" + receiver + "\",";
			post = post + "\n\t\"msgs\": [";
			char delim = ' ';

			for (auto it = send_list.begin(); it != send_list.end(); ++it) {
				post = post + delim + "\n\t\t" + *it;
				delim = ',';
			}

			post = post + "\n\t]\n}\n";
		}
		else {
			post = "{\n\t\"protocol\": \"jsonais\",";
			post = post + "\n\t\"encodetime\": \"" + Util::Convert::toTimeStr(now) + "\",";
			post = post + "\n\t\"groups\": [";
			post = post + "\n\t{";
			post = post + "\n\t\t\"path\": [{ \"name\": \"" + stationid + "\", \"url\" : \"" + url + "\" }],";
			post = post + "\n\t\t\"msgs\": [";
			char delim = ' ';

			for (auto it = send_list.begin(); it != send_list.end(); ++it) {
				post = post + delim + "\n\t\t\t" + *it;
				delim = ',';
			}

			post = post + "\n\t\t]\n\t}]\n}";
		}


		send(post);
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
				for (const auto& s : data[i].sentence)
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
			for (const auto& s : data[i].sentence)
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
