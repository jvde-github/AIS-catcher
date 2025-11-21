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
#include <iostream>

#include "MsgOut.h"

namespace IO
{
	class ScreenOutput : public OutputMessage
	{
	private:
		bool include_sample_start = false;

	public:
		int verboseUpdateTime = 3;
		ScreenOutput() : OutputMessage() { fmt = MessageFormat::FULL; }
		virtual ~ScreenOutput() {}
		
		void setScreen(const std::string &str)
		{
            setOption("MSGFORMAT", str);			
		}

		void Receive(const AIS::Message *data, int len, TAG &tag);
		void Receive(const JSON::JSON *data, int len, TAG &tag);
		void Receive(const AIS::GPS *data, int len, TAG &tag);

		Setting &Set(std::string option, std::string arg)
		{
			Util::Convert::toUpper(option);

			if (option == "GROUPS_IN")
			{
				StreamIn<AIS::Message>::setGroupsIn(Util::Parse::Integer(arg));
				StreamIn<JSON::JSON>::setGroupsIn(Util::Parse::Integer(arg));
				StreamIn<AIS::GPS>::setGroupsIn(Util::Parse::Integer(arg));
			}
			else if (option == "INCLUDE_SAMPLE_START")
			{
				include_sample_start = Util::Parse::Switch(arg);
			}
			else if (!OutputMessage::setOption(option, arg))
			{
				throw std::runtime_error("Screen output - unknown option: " + option);
			}

			return *this;
		}
	};
}
