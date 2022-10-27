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
	int HTTP::send(struct curl_httppost* post) {

		// based on function "jsonout_post_single" in gnuais
		CURL* ch;
		CURLcode r;
		struct curl_slist* headers = NULL;
		long retcode = 200;

		if (!(ch = curl_easy_init())) {
			std::cerr << "HTTP: curl_easy_init() returned NULL" << std::endl;
			return 1;
		}

		do {
			headers = curl_slist_append(NULL, "Expect:");
			if (!headers) {
				std::cerr << "HTTP: curl_slist_append for Expect header failed" << std::endl;
				break;
			}

			if ((r = curl_easy_setopt(ch, CURLOPT_HTTPPOST, post))) {
				std::cerr << "HTTP: CURLOPT_HTTPPOST failed: %s" << curl_easy_strerror(r) << std::endl;
				break;
			}

			if ((r = curl_easy_setopt(ch, CURLOPT_URL, url.c_str()))) {
				std::cerr << "HTTP: CURLOPT_URL failed: " << curl_easy_strerror(r) << " (" << url << ")" << std::endl;
				break;
			}

			if ((r = curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers))) {
				std::cerr << "HTTP: URLOPT_HEADER failed: " << curl_easy_strerror(r) << " (" << url << ")" << std::endl;
				break;
			}

			if ((r = curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, &curl_wdata))) {
				std::cerr << "HTTP: CURLOPT_WRITEFUNCTION failed: " << curl_easy_strerror(r) << std::endl;
				break;
			}

			if ((r = curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 1))) {
				std::cerr << "HTTP: CURLOPT_NOPROGRESS failed: " << curl_easy_strerror(r) << std::endl;
				break;
			}
			if ((r = curl_easy_setopt(ch, CURLOPT_VERBOSE, 0))) {
				std::cerr << "HTTP: CURLOPT_VERBOSE failed: " << curl_easy_strerror(r) << std::endl;
				break;
			}

			if ((r = curl_easy_setopt(ch, CURLOPT_TIMEOUT, 2L))) {
				std::cerr << "HTTP: CURLOPT_TIMEOUT failed: " << curl_easy_strerror(r) << std::endl;
				break;
			}

			if ((r = curl_easy_perform(ch))) {
				std::cerr << "HTTP: perform failed: " << curl_easy_strerror(r) << " (" << url << ")" << std::endl;
				break;
			}

			if ((r = curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &retcode))) {
				std::cerr << "HTTP: CURLINFO_RESPONSE_CODE failed: " << curl_easy_strerror(r) << " (" << url << ")" << std::endl;
				break;
			}
		} while (0);

		curl_easy_cleanup(ch);

		if (headers)
			curl_slist_free_all(headers);

		if (retcode != 200) {
			std::cerr << "HTTP: server for " << url << " returned " << retcode << std::endl;
			r = (CURLcode)-1;
		}

		return (r);
	}


	void HTTP::post() {

		std::string post;
		std::list<std::string> send_list;

		{
			const std::lock_guard<std::mutex> lock(queue_mutex);
			send_list.splice(send_list.begin(), queue);
		}

		char delim = ' ';

		post = "{\n\t\"protocol\": \"jsonaiscatcher\",\n\t\"msgs\": [";

		for (auto it = send_list.begin(); it != send_list.end(); ++it) {
			post = post + delim + "\n\t\t" + *it;
			delim = ',';
		}
		post = post + "\n\t]\n}\n";

		struct curl_httppost *cpost = NULL, *last = NULL;
		curl_formadd(&cpost, &last, CURLFORM_COPYNAME, "jsonais", CURLFORM_CONTENTTYPE, "application/json", CURLFORM_PTRCONTENTS, post.c_str(), CURLFORM_END);
		send(cpost);
	}

	void HTTP::process() {

		int i = 0;

		while (!terminate) {
			SleepSystem(50);
			if (++i == 30000 / 50) {
				if (active) post();
				i = 0;
			}
		}
		if (active) post();
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

	TCP::TCP() {
	}

	TCP::~TCP() {
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
