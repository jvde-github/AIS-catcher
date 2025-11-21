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

#include <cstring>

#include "MsgOut.h"
#include "Receiver.h"

namespace IO
{

	void OutputMessage::ConnectMessage(Receiver &r)
	{

		for (int j = 0; j < r.Count(); j++)
		{
			StreamIn<AIS::Message> *um = (StreamIn<AIS::Message> *)&*this;
			if (r.Output(j).canConnect(um->getGroupsIn()))
				r.Output(j).Connect(um);
		}
	}

	void OutputMessage::ConnectJSON(Receiver &r)
	{

		for (int j = 0; j < r.Count(); j++)
		{
			StreamIn<JSON::JSON> *um = (StreamIn<JSON::JSON> *)&*this;
			if (r.Output(j).canConnect(um->getGroupsIn()))
				r.OutputJSON(j).Connect(um);
		}
	}

	void OutputMessage::Connect(Receiver &r)
	{

		if (fmt == MessageFormat::JSON_FULL || fmt == MessageFormat::JSON_ANNOTATED || fmt == MessageFormat::JSON_SPARSE)
			ConnectJSON(r);
		else
			ConnectMessage(r);

		for (int j = 0; j < r.Count(); j++)
		{

			StreamIn<AIS::GPS> *ug = (StreamIn<AIS::GPS> *)&*this;
			if (r.OutputGPS(j).canConnect(ug->getGroupsIn()))
				r.OutputGPS(j).Connect(ug);
		}
	}

	void OutputJSON::Connect(Receiver &r)
	{

		for (int j = 0; j < r.Count(); j++)
		{
			StreamIn<JSON::JSON> *um = (StreamIn<JSON::JSON> *)&*this;
			if (r.Output(j).canConnect(um->getGroupsIn()))
				r.OutputJSON(j).Connect(um);

			StreamIn<AIS::GPS> *ug = (StreamIn<AIS::GPS> *)&*this;
			if (r.OutputGPS(j).canConnect(ug->getGroupsIn()))
				r.OutputGPS(j).Connect(ug);
		}
	}

}
