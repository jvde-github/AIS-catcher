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


#pragma once

#include <string>

#include "HTTPServer.h"
#include "Stream.h"
#include "JSON/JSON.h"

namespace Services
{
	// What a viewer streams live: every message as it arrives (its payload
	// partly hidden unless the decoder is on), and where each was heard.
	class MessageStream : public StreamIn<JSON::JSON>
	{
	public:
		MessageStream(IO::HTTPServer &server, bool obfuscate) : server(server), obfuscate(obfuscate) {}
		void Receive(const JSON::JSON *data, int len, TAG &tag) override;

	private:
		IO::HTTPServer &server;
		bool obfuscate;
		unsigned idx = 0;
	};

	// The log, line by line, to the viewer's log tab
	class LogStream
	{
	public:
		explicit LogStream(IO::HTTPServer &server);
		~LogStream();

	private:
		int listener = -1;
	};

	// NMEA as the decoder tab shows it: each message it holds, decoded
	std::string decodeNMEA(const std::string &nmea, bool enhanced = true);
	// the first message the NMEA holds as one object; empty when it holds none
	std::string decodeMessage(const std::string &nmea);
}
