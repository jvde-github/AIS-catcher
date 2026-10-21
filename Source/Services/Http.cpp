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


#include "Http.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>

#include "Convert.h"

namespace Services
{
	std::string urlEncode(const std::string &v)
	{
		static const char *hex = "0123456789ABCDEF";
		std::string out;
		for (unsigned char c : v)
		{
			if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == ',')
				out += (char)c;
			else
			{
				out += '%';
				out += hex[c >> 4];
				out += hex[c & 15];
			}
		}
		return out;
	}

	std::string urlDecode(const std::string &in)
	{
		std::string out;
		out.reserve(in.size());
		for (std::size_t i = 0; i < in.size(); i++)
		{
			if (in[i] == '+')
				out += ' ';
			else if (in[i] == '%' && i + 2 < in.size() && Util::Convert::isHexDigit(in[i + 1]) && Util::Convert::isHexDigit(in[i + 2]))
			{
				out += (char)((Util::Convert::hexDigitValue(in[i + 1]) << 4) | Util::Convert::hexDigitValue(in[i + 2]));
				i += 2;
			}
			else
				out += in[i];
		}
		return out;
	}

	std::string Request::param(const std::string &name) const
	{
		std::size_t pos = 0;
		while (pos < query.size())
		{
			std::size_t amp = query.find('&', pos);
			if (amp == std::string::npos)
				amp = query.size();
			std::size_t eq = query.find('=', pos);
			if (eq != std::string::npos && eq < amp && query.compare(pos, eq - pos, name) == 0)
				return urlDecode(query.substr(eq + 1, amp - eq - 1));
			pos = amp + 1;
		}
		return "";
	}

	std::vector<std::pair<std::string, std::string>> Request::params() const
	{
		std::vector<std::pair<std::string, std::string>> out;
		std::size_t pos = 0;
		while (pos < query.size())
		{
			std::size_t amp = query.find('&', pos);
			if (amp == std::string::npos)
				amp = query.size();
			std::size_t eq = query.find('=', pos);
			if (amp > pos)
			{
				if (eq != std::string::npos && eq < amp)
					out.push_back(std::make_pair(urlDecode(query.substr(pos, eq - pos)), urlDecode(query.substr(eq + 1, amp - eq - 1))));
				else
					out.push_back(std::make_pair(urlDecode(query.substr(pos, amp - pos)), std::string()));
			}
			pos = amp + 1;
		}
		return out;
	}

	bool Request::integer(const std::string &name, long long &out, long long fallback, long long min, long long max) const
	{
		const std::string v = param(name);
		if (v.empty())
		{
			out = fallback;
			return true;
		}
		char *end = nullptr;
		const long long n = std::strtoll(v.c_str(), &end, 10);
		if (!end || *end || n < min || n > max)
			return false;
		out = n;
		return true;
	}

	Response Response::json(const std::string &body, int status)
	{
		Response r;
		r.status = status;
		r.body = body;
		return r;
	}

	Response Response::problem(int status, const std::string &title, const std::string &detail)
	{
		Response r;
		r.status = status;
		r.type = "application/problem+json";
		JSON::Writer w(r.body);
		w.beginObject().kv("type", "about:blank").kv("title", title).kv("status", status).kv("detail", detail).endObject();
		w.finish();
		return r;
	}

	std::string isoTime(std::time_t t)
	{
		char buf[24];
		struct tm tm;
#ifdef _WIN32
		gmtime_s(&tm, &t);
#else
		gmtime_r(&t, &tm);
#endif
		std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
		return buf;
	}

	void beginList(JSON::Writer &w)
	{
		w.beginObject().key("data").beginArray();
	}

	void endList(JSON::Writer &w, std::size_t count, bool complete, const std::string &next)
	{
		w.endArray().key("meta").beginObject().kv("count", (long long)count).kv("complete", complete);
		if (next.empty())
			w.kv_null("next");
		else
			w.kv("next", next);
		w.endObject().endObject();
	}

	bool parseTime(const std::string &s, std::time_t &out)
	{
		if (!s.empty() && s.size() <= 10 && s.find_first_not_of("0123456789") == std::string::npos)
		{
			out = (std::time_t)std::atoll(s.c_str());
			return true;
		}
		int y, mo, d, h, mi, sec;
		char z = 0;
		if (s.size() != 20 || std::sscanf(s.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d%c", &y, &mo, &d, &h, &mi, &sec, &z) != 7 || z != 'Z' ||
			mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || sec > 60 || y < 1970)
			return false;
		// days since 1970 of a civil date
		y -= mo <= 2;
		const long era = y / 400, yoe = y - era * 400;
		const long doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
		const long days = era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
		out = (std::time_t)days * 86400 + h * 3600 + mi * 60 + sec;
		return true;
	}

	void endList(JSON::Writer &w, std::size_t count, bool complete)
	{
		w.endArray().key("meta").beginObject().kv("count", (long long)count).kv("complete", complete).kv_null("next").endObject().endObject();
	}
}
