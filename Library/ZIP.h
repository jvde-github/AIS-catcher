/*
	Copyright(c) 2021-2025 jvde.github@gmail.com

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


#define windowBits	  15
#define GZIP_ENCODING 16
#endif

class ZIP {

#ifdef HASZLIB
	const int CHUNKSIZE = 0x1000;
	z_stream strm;
#endif

	std::vector<unsigned char> output;

public:
	static bool installed() {
#ifdef HASZLIB
		return true;
#else
		return false;
#endif
	}

	int getOutputLength() { return output.size(); }
	void* getOutputPtr() { return output.data(); }

#ifdef HASZLIB
	void init() {

		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;

		if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowBits | GZIP_ENCODING, 8, Z_DEFAULT_STRATEGY) < 0) {
			throw std::runtime_error("ZLIB: error cannot initiate stream.");
		}
	}

	void end() {
		deflateEnd(&strm);
	}
#endif
	void zip(const std::string& input) {
#ifdef HASZLIB

		int idx = 0;
		bool done = false;

		init();

		strm.next_in = (unsigned char*)input.c_str();
		strm.avail_in = input.length();

		while (!done) {

			if (output.size() < idx + CHUNKSIZE) output.resize(idx + CHUNKSIZE);

			strm.avail_out = CHUNKSIZE;
			strm.next_out = (unsigned char*)(output.data() + idx);
			if (deflate(&strm, Z_FINISH) < 0)
				throw std::runtime_error("ZLIB: unexpected problem with ZLIB");

			done = strm.avail_out != 0;
			idx += CHUNKSIZE;
		}

		end();

		output.resize(strm.total_out);
#endif
	}

	void zip(char* data, int len) {
#ifdef HASZLIB

		int idx = 0;
		bool done = false;

		init();

		strm.next_in = (unsigned char*)data;
		strm.avail_in = len;

		while (!done) {

			if (output.size() < idx + CHUNKSIZE) output.resize(idx + CHUNKSIZE);

			strm.avail_out = CHUNKSIZE;
			strm.next_out = (unsigned char*)(output.data() + idx);
			if (deflate(&strm, Z_FINISH) < 0)
				throw std::runtime_error("ZLIB: unexpected problem with ZLIB");

			done = strm.avail_out != 0;
			idx += CHUNKSIZE;
		}

		end();

		output.resize(strm.total_out);
#endif
	}
};
