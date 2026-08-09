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
#include <iostream>

#include "MsgOut.h"

namespace IO
{
	class ScreenOutput : public OutputMessage
	{
	private:
		bool readyToSend() override { return fmt != MessageFormat::SILENT; }

		void sendFormatted(const char *data, int len, const AIS::Message *, TAG &) override
		{
			std::cout.write(data, len);
		}

		// live sources flush per batch; bulk replays defer flushing to Stop()/buffer-full
		void batchDone(TAG &tag) override
		{
			if (!tag.replay)
				std::cout.flush();
		}

	public:
		int verboseUpdateTime = 3;
		ScreenOutput() : OutputMessage("Screen")
		{
			fmt = MessageFormat::FULL;
			line_suffix = "\n";
		}
		virtual ~ScreenOutput() {}

		void Stop() override { std::cout.flush(); }

		void setScreen(const std::string &str)
		{
			setOptionKey(AIS::KEY_SETTING_MSGFORMAT, str);
		}

		using StreamIn<AIS::Message>::Receive;
		using StreamIn<JSON::JSON>::Receive;
		using StreamIn<AIS::GPS>::Receive;

		void Connect(Receiver &r);
	};
}
