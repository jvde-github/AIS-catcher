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

#include "Common.h"
#include "JSON.h"
#include "HTTPServer.h"

class SSEStreamer : public StreamIn<JSON::JSON>
{
	IO::HTTPServer *server = nullptr;

	unsigned index = 0;
	bool obfuscate = true;

	void obfuscateNMEA(std::string &nmea);

public:
	virtual ~SSEStreamer() = default;

	void Receive(const JSON::JSON *data, int len, TAG &tag);
	void setSSE(IO::HTTPServer *s) { server = s; }
	void setObfuscate(bool o) { obfuscate = o; }
};
