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

#include "Screen.h"
#include "Logger.h"
#include "Convert.h"

#include <cstdio>

#ifdef _WIN32
#include <io.h>
#define AISC_ISATTY(fd) _isatty(fd)
#define AISC_FILENO(f)  _fileno(f)
#else
#include <unistd.h>
#define AISC_ISATTY(fd) isatty(fd)
#define AISC_FILENO(f)  fileno(f)
#endif

namespace IO
{
	void ScreenOutput::Connect(Receiver &r)
	{
		if ((fmt == MessageFormat::BINARY_NMEA || fmt == MessageFormat::COMMUNITY_HUB)
		    && AISC_ISATTY(AISC_FILENO(stdout)))
		{
			Error() << "Screen: refusing " << Util::Convert::toString(fmt)
			        << " output to terminal (would emit raw binary). "
			           "Redirect stdout (e.g. ' > out.bin') or use a file/network output.";
			fmt = MessageFormat::SILENT;
		}
		OutputMessage::Connect(r);
	}
}
