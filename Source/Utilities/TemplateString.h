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

#include <cstdint>
#include <string>
#include <vector>

struct TAG;
namespace AIS
{
	class Message;
}

namespace Util
{
	class TemplateString
	{
	public:
		TemplateString() = default;
		TemplateString(const std::string &t) { set(t); }

		void set(const std::string &t);
		std::string get(const TAG &tag, const AIS::Message &msg) const;

		void write(const TAG &tag, const AIS::Message &msg, std::string &out) const;

		const std::string &getTemplate() const { return tpl; }
		bool isTemplate() const { return has_substitution; }

	private:
		struct Segment
		{
			enum Kind : uint8_t { LITERAL, MMSI, PPM, STATION, TYPE, REPEAT, CHANNEL, RXTIMEUX };
			Kind kind;
			std::string literal;
		};

		std::string tpl;
		std::vector<Segment> segments;
		size_t estimated_size = 0;
		bool has_substitution = false;
	};
}
