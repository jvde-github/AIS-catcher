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

#include "CommunityStreamer.h"

// Community feed server configuration
#define AISCATCHER_URL "185.77.96.227"
#define AISCATCHER_PORT "4242"

namespace IO
{
	CommunityStreamer::CommunityStreamer() : TCPClientStreamer()
	{
		// Set default configuration for community feed
		Set("HOST", AISCATCHER_URL);
		Set("PORT", AISCATCHER_PORT);
		Set("MSGFORMAT", "COMMUNITY_HUB");
		Set("FILTER", "on");
		Set("GPS", "off");
		Set("REMOVE_EMPTY", "on");
		Set("KEEP_ALIVE", "on");
		Set("OWN_INTERVAL", "10");
		Set("INCLUDE_SAMPLE_START", "on");
	}
}
