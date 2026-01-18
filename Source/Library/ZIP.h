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

#define windowBits 15
#define GZIP_ENCODING 16
#endif

class ZIP
{

#ifdef HASZLIB
	z_stream strm;
#endif

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
#ifdef HASZLIB

		try
		{
			strm.zalloc = Z_NULL;
			strm.zfree = Z_NULL;
			strm.opaque = Z_NULL;

			if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowBits | GZIP_ENCODING, 8, Z_DEFAULT_STRATEGY) < 0)
				return false;

			strm.next_in = (unsigned char *)data;
			strm.avail_in = len;

			uLong bound = deflateBound(&strm, len);
			output.resize(bound);

			strm.avail_out = bound;
			strm.next_out = (unsigned char *)output.data();

			int result = deflate(&strm, Z_FINISH);
			deflateEnd(&strm);

			if (result != Z_STREAM_END)
				throw std::runtime_error("ZLIB: deflate did not complete");

			output.resize(strm.total_out);
			return true;
		}
		catch (...)
		{
			output.clear();
			return false;
		}
#else
		return false;
#endif
	}
};