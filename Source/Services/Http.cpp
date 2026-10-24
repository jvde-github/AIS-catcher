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
#include "Parse.h"

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

	Response Response::json(std::string body, int status)
	{
		Response r;
		r.status = status;
		r.body = std::move(body);
		return r;
	}

	Response Response::json(std::shared_ptr<const std::string> body, int status)
	{
		Response r;
		r.status = status;
		r.shared = std::move(body);
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

	IsoTime isoTimeText(std::time_t t)
	{
		IsoTime out;
		struct tm tm;
#ifdef _WIN32
		gmtime_s(&tm, &t);
#else
		gmtime_r(&t, &tm);
#endif
		if (!std::strftime(out.text, sizeof(out.text), "%Y-%m-%dT%H:%M:%SZ", &tm))
			out.text[0] = 0;
		return out;
	}

	std::string isoTime(std::time_t t)
	{
		return isoTimeText(t).text;
	}

	void beginList(JSON::Writer &w)
	{
		w.beginObject().key("data").beginArray();
	}

	Response badLimit()
	{
		return Response::problem(400, "Bad request", "limit must be between 1 and " + std::to_string(LIST_MAX) + ".");
	}

	Response badCursor()
	{
		return Response::problem(400, "Bad request", "cursor must be what a previous page gave in meta.next or meta.poll.");
	}

	Response gone(const std::string &why)
	{
		return Response::problem(410, "Gone", why + " Start again without cursor.");
	}

	void beginMeta(JSON::Writer &w, std::size_t count, const std::string &next)
	{
		w.endArray().key("meta").beginObject().kv("count", (long long)count).kv("complete", next.empty());
		if (next.empty())
			w.kv_null("next");
		else
			w.kv("next", next);
	}

	void endList(JSON::Writer &w, std::size_t count, const std::string &next)
	{
		beginMeta(w, count, next);
		w.endObject().endObject();
	}

	void textOrNull(JSON::Writer &w, const char *key, const std::string &v)
	{
		if (v.find_first_not_of(' ') == std::string::npos)
			w.kv_null(key);
		else
			w.kv(key, v);
	}

	std::string cursor(const std::string &tag, const std::vector<long long> &parts)
	{
		std::string out = tag;
		for (long long p : parts)
			out += "_" + std::to_string(p);
		return out;
	}

	bool parseCursor(const std::string &s, const std::string &tag, std::size_t n, std::vector<long long> &out)
	{
		out.clear();
		if (s.compare(0, tag.size(), tag) != 0)
			return false;
		std::size_t pos = tag.size();
		while (pos < s.size() && out.size() < n)
		{
			if (s[pos] != '_')
				return false;
			const std::size_t end = s.find('_', pos + 1);
			const std::string part = s.substr(pos + 1, end == std::string::npos ? std::string::npos : end - pos - 1);
			long long v = 0;
			if (!Util::Parse::Digits(part, v))
				return false;
			out.push_back(v);
			pos = end == std::string::npos ? s.size() : end;
		}
		return pos == s.size() && out.size() == n;
	}

	bool parseTime(const std::string &s, std::time_t &out)
	{
		long long seconds = 0;
		if (Util::Parse::Digits(s, seconds, 10))
		{
			out = (std::time_t)seconds;
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
}
