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

#include <vector>
#include <string>

#ifdef HASZLIB
#include <zlib.h>
#endif

class ZIP
{
	std::vector<unsigned char> output;

public:
	static bool installed()
	{
#ifdef HASZLIB
		return true;
#else
		return false;
#endif
	}

	size_t getOutputLength() const { return output.size(); }
	const char *getOutputPtr() const { return (const char *)output.data(); }

	bool zip(const std::string &input)
	{
		return zip(input.c_str(), input.length());
	}

	bool zip(const char *data, int len)
	{
		output.clear();
#ifdef HASZLIB
		if (len < 0)
			return false;

		z_stream strm = {};
		if (deflateInit2(&strm, Z_BEST_SPEED, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
			return false;

		strm.next_in = (unsigned char *)data;
		strm.avail_in = len;

		output.resize(deflateBound(&strm, len));
		strm.next_out = output.data();
		strm.avail_out = output.size();

		bool ok = deflate(&strm, Z_FINISH) == Z_STREAM_END;
		if (ok)
			output.resize(strm.total_out);
		else
			output.clear();

		deflateEnd(&strm);
		return ok;
#else
		return false;
#endif
	}
};