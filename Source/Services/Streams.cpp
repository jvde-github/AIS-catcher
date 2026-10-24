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


#include "Streams.h"
#include "AIS-catcher.h"
#include "JSONAIS.h"
#include "Logger.h"
#include "NMEA.h"
#include "Writer.h"

#include <climits>
#include <ctime>
#include <stdexcept>

namespace Services
{
	void MessageStream::Receive(const JSON::JSON *data, int len, TAG &tag)
	{
		const bool want_nmea = server.sseSubscribed(IO::SSE::NMEA);
		const bool want_signal = server.sseSubscribed(IO::SSE::SIGNAL);
		if (!want_nmea && !want_signal)
			return;

		const std::time_t now = std::time(nullptr);
		for (int j = 0; j < len; j++)
		{
			const AIS::Message *m = (const AIS::Message *)data[j].binary;
			if (!m)
				continue;
			char channel = m->getChannel();

			if (want_nmea && !m->sentences().empty())
			{
				std::string json;
				JSON::Writer w(json);
				w.beginObject().kv("mmsi", m->mmsi()).kv("timestamp", (long long)now).kv("channel", {&channel, 1})
					.kv("type", m->type()).kv("shipname", tag.shipname).key("nmea").beginArray();
				for (const auto &s : m->sentences())
				{
					std::string nmea = s;
					// the payload is the field before the last comma
					const std::string::size_type end = nmea.rfind(',');
					if (end == std::string::npos)
						continue;
					const std::string::size_type start = nmea.rfind(',', end - 1);
					if (start == std::string::npos)
						continue;
					const std::string::size_type field_len = end - start - 1;
					if (field_len == 0)
						continue;
					// without the decoder three characters of it are hidden, a different three each time
					if (obfuscate)
					{
						unsigned pos = idx;
						for (int i = 0; i < 3; i++)
						{
							pos = (pos + 1) % field_len;
							nmea[start + 1 + pos] = '*';
						}
						idx = pos;
					}
					w.val(nmea);
				}
				w.endArray().endObject();
				w.finish();
				server.sendSSE(IO::SSE::NMEA, "nmea", json);
			}

			if (want_signal && isValidCoord(tag.lat, tag.lon))
			{
				std::string json;
				JSON::Writer w(json);
				w.beginObject().kv("mmsi", m->mmsi()).kv("channel", {&channel, 1}).kv("lat", tag.lat).kv("lon", tag.lon).endObject();
				w.finish();
				server.sendSSE(IO::SSE::SIGNAL, "nmea", json);
			}
		}
	}

	LogStream::LogStream(IO::HTTPServer &server)
	{
		IO::HTTPServer *s = &server;
		listener = Logger::getInstance().addLogListener([s](const LogMessage &msg) { s->sendSSE(IO::SSE::VIEWER_LOG, "log", msg.toJSON()); });
	}

	LogStream::~LogStream()
	{
		// the logger outlives the server: a listener left behind would call into freed memory
		if (listener != -1)
			Logger::getInstance().removeLogListener(listener);
	}

	namespace
	{
		// NMEA, into messages, into JSON: at most `left` of them
		class Decoder : public StreamIn<JSON::JSON>
		{
			enum { MAX_OUTPUT_SIZE = 1024 * 1024 };

		public:
			AIS::NMEA nmea;
			AIS::JSONAIS json;
			JSON::Serializer *builder;
			JSON::Writer *writer;
			int left;

			Decoder(JSON::Serializer *b, JSON::Writer *w, int limit) : builder(b), writer(w), left(limit)
			{
				nmea >> json;
				json.out.Connect(this);
			}

			void Receive(const JSON::JSON *data, int len, TAG &) override
			{
				for (int i = 0; i < len && left > 0; i++, left--)
				{
					if (writer->written() > MAX_OUTPUT_SIZE)
						throw std::runtime_error("Output size limit exceeded");
					builder->stringify(data[i], *writer);
				}
			}

			// `mode`: more of what a receiver's tag asks for, as 4 for the country of the MMSI
			void decode(const std::string &input, unsigned mode = 0)
			{
				std::string text = input + "\n";
				RAW raw = {Format::TXT, (void *)text.c_str(), (int)text.length()};
				TAG tag;
				tag.mode |= mode;
				nmea.Receive(&raw, 1, tag);
			}
		};
	}

	std::string decodeNMEA(const std::string &input, bool enhanced)
	{
		std::string result;
		result.reserve(4096);
		JSON::Writer w(result);
		w.beginArray();
		JSON::Serializer builder(JSON_DICT_FULL);
		builder.setStringifyEnhanced(enhanced);
		Decoder decoder(&builder, &w, INT_MAX);
		decoder.decode(input);
		w.endArray();
		w.finish();
		return result;
	}

	std::string decodeMessage(const std::string &input)
	{
		std::string result;
		JSON::Writer w(result);
		JSON::Serializer builder(JSON_DICT_FULL);
		Decoder decoder(&builder, &w, 1);
		decoder.decode(input, 4);
		w.finish();
		return result;
	}
}
