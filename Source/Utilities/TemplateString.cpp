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

#include <cstring>

#include "TemplateString.h"
#include "Message.h"
#include "../JSON/Writer.h"

namespace Util
{
	void TemplateString::set(const std::string &t)
	{
		tpl = t;
		segments.clear();
		estimated_size = 0;
		has_substitution = false;

		auto push_literal = [&](const char *p, size_t n)
		{
			if (!n)
				return;
			if (!segments.empty() && segments.back().kind == Segment::LITERAL)
				segments.back().literal.append(p, n);
			else
			{
				segments.emplace_back();
				segments.back().kind = Segment::LITERAL;
				segments.back().literal.assign(p, n);
			}
			estimated_size += n;
		};

		auto push_sub = [&](Segment::Kind k, size_t reserve)
		{
			segments.emplace_back();
			segments.back().kind = k;
			estimated_size += reserve;
			has_substitution = true;
		};

		const size_t n = t.size();
		size_t i = 0;
		while (i < n)
		{
			if (t[i] != '%')
			{
				size_t j = t.find('%', i);
				if (j == std::string::npos)
				{
					push_literal(t.data() + i, n - i);
					break;
				}
				push_literal(t.data() + i, j - i);
				i = j;
				continue;
			}

			size_t end = t.find('%', i + 1);
			if (end == std::string::npos)
			{
				push_literal(t.data() + i, 1);
				i++;
				continue;
			}

			const char *kp = t.data() + i + 1;
			size_t kn = end - i - 1;

			auto eq = [&](const char *s, size_t l)
			{ return kn == l && std::memcmp(kp, s, l) == 0; };

			if (eq("mmsi", 4))           push_sub(Segment::MMSI, 10);
			else if (eq("ppm", 3))       push_sub(Segment::PPM, 12);
			else if (eq("station", 7))   push_sub(Segment::STATION, 11);
			else if (eq("type", 4))      push_sub(Segment::TYPE, 2);
			else if (eq("repeat", 6))    push_sub(Segment::REPEAT, 1);
			else if (eq("channel", 7))   push_sub(Segment::CHANNEL, 1);
			else if (eq("rxtimeux", 8))  push_sub(Segment::RXTIMEUX, 11);
			else                          push_literal(t.data() + i, end - i + 1);

			i = end + 1;
		}
	}

	void TemplateString::write(const TAG &tag, const AIS::Message &msg, std::string &out) const
	{
		out.clear();
		{
			JSON::Writer w(out, estimated_size + 16);
			for (const auto &s : segments)
			{
				switch (s.kind)
				{
				case Segment::LITERAL:
					w.append(s.literal.data(), s.literal.size());
					break;
				case Segment::MMSI:
					w.append_int((long long)msg.mmsi());
					break;
				case Segment::PPM:
					w.append_float((double)tag.ppm);
					break;
				case Segment::STATION:
					w.append_int((long long)msg.getStation());
					break;
				case Segment::TYPE:
					w.append_int((long long)msg.type());
					break;
				case Segment::REPEAT:
					w.append_int((long long)msg.repeat());
					break;
				case Segment::CHANNEL:
					w.append(msg.getChannel());
					break;
				case Segment::RXTIMEUX:
					w.append_int((long long)msg.getRxTimeUnix());
					break;
				}
			}
		}
	}

	std::string TemplateString::get(const TAG &tag, const AIS::Message &msg) const
	{
		std::string out;
		write(tag, msg, out);
		return out;
	}
}
