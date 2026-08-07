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

#include "TemplateString.h"
#include "Message.h"
#include "../JSON/Writer.h"

namespace Util
{
	void TemplateString::set(const std::string &t)
	{
		struct Token
		{
			const char *name;
			Segment::Kind kind;
			size_t reserve;
		};

		static const Token TOKENS[] = {
			{"mmsi", Segment::MMSI, 10},
			{"ppm", Segment::PPM, 12},
			{"station", Segment::STATION, 11},
			{"type", Segment::TYPE, 2},
			{"repeat", Segment::REPEAT, 1},
			{"channel", Segment::CHANNEL, 1},
			{"rxtimeux", Segment::RXTIMEUX, 11},
		};

		tpl = t;
		segments.clear();
		estimated_size = 0;
		has_substitution = false;

		auto push_literal = [&](size_t pos, size_t n)
		{
			if (!n)
				return;
			if (!segments.empty() && segments.back().kind == Segment::LITERAL)
				segments.back().literal.append(t, pos, n);
			else
				segments.push_back({Segment::LITERAL, t.substr(pos, n)});
			estimated_size += n;
		};

		size_t i = 0;
		while (i < t.size())
		{
			size_t open = t.find('%', i);
			if (open == std::string::npos)
			{
				push_literal(i, t.size() - i);
				break;
			}
			push_literal(i, open - i);

			size_t close = t.find('%', open + 1);
			const Token *tok = nullptr;

			if (close != std::string::npos)
				for (const Token &c : TOKENS)
					if (t.compare(open + 1, close - open - 1, c.name) == 0)
					{
						tok = &c;
						break;
					}

			if (!tok)
			{
				// Unterminated or unknown token: emit this '%' and rescan, so a
				// stray literal '%' cannot consume the opener of the next one.
				push_literal(open, 1);
				i = open + 1;
				continue;
			}

			segments.push_back({tok->kind, std::string()});
			estimated_size += tok->reserve;
			has_substitution = true;
			i = close + 1;
		}
	}

	void TemplateString::write(const TAG &tag, const AIS::Message &msg, std::string &out) const
	{
		out.clear();

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
				if (tag.ppm != PPM_UNDEFINED)
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

	std::string TemplateString::get(const TAG &tag, const AIS::Message &msg) const
	{
		std::string out;
		write(tag, msg, out);
		return out;
	}
}
