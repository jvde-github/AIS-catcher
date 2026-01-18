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

namespace Util
{
	void TemplateString::set(const std::string &t)
	{
		tpl = t;
	}

	std::string TemplateString::get(const TAG &tag, const AIS::Message &msg) const
	{
		std::string out;
		out.reserve(tpl.size() + 64);

		int i = 0, n = tpl.size();

		while (i < n)
		{
			if (tpl[i] != '%')
			{
				out.push_back(tpl[i++]);
			}
			else
			{
				int start = i + 1;
				int end = tpl.find('%', start);
				if (end == std::string::npos)
				{
					out.push_back('%');
					i = start;
				}
				else
				{
					std::string key = tpl.substr(start, end - start);
					i = end + 1;

					// substitutions:
					if (key == "mmsi")
					{
						out += std::to_string(msg.mmsi());
					}
					else if (key == "ppm")
					{
						out += std::to_string(tag.ppm);
					}
					else if (key == "station")
					{
						out += std::to_string(msg.getStation());
					}
					else if (key == "type")
					{
						out += std::to_string(msg.type());
					}
					else if (key == "repeat")
					{
						out += std::to_string(msg.repeat());
					}
					else if (key == "channel")
					{
						out.push_back(msg.getChannel());
					}
					else if (key == "rxtimeux")
					{
						out += std::to_string(msg.getRxTimeUnix());
					}
					else
					{
						// unknown key → re-emit literally
						out.push_back('%');
						out += key;
						out.push_back('%');
					}
				}
			}
		}
		return out;
	}
}
