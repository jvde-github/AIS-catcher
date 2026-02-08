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

#include "AIS-catcher.h"
#include "JSONConfig.h"
#include "CommunityStreamer.h"
#include "Helper.h"
#include "Receiver.h"
#include "WebViewer.h"
#include "Screen.h"
#include "File.h"
#include "MsgOut.h"

#include "JSON/JSON.h"
#include "JSON/Parser.h"
#include "JSON/StringBuilder.h"

#include "Device/FileRAW.h"
#include "Device/FileWAV.h"
#include "Device/RTLSDR.h"
#include "Device/AIRSPYHF.h"
#include "Device/HACKRF.h"
#include "Device/RTLTCP.h"
#include "Device/AIRSPY.h"
#include "Device/SDRPLAY.h"
#include "Device/SpyServer.h"
#include "Device/SoapySDR.h"
#include "Device/ZMQ.h"
#include "Device/HYDRASDR.h"

namespace JSONConfig
{
	// Forward declarations
	static bool isActiveObject(const JSON::Value &pd);
	static void setSettingsFromJSON(const JSON::Value &pd, Setting &s);
	static void setServerfromJSON(const JSON::Value &pd, ApplicationState &app);
	static void setHTTPfromJSON(const JSON::Property &pd, ApplicationState &app);
	static void setUDPfromJSON(const JSON::Property &pd, ApplicationState &app);
	static void setTCPfromJSON(const JSON::Property &pd, ApplicationState &app);
	static void setMQTTfromJSON(const JSON::Property &pd, ApplicationState &app);
	static void setTCPListenerfromJSON(const JSON::Property &pd, ApplicationState &app);
	static void setModelfromJSON(const JSON::Property &p, ApplicationState &app);
	static void setReceiverFromArray(const JSON::Property &pd, ApplicationState &app);
	static void setReceiverfromJSON(const std::vector<JSON::Property> &props, bool unspecAllowed, ApplicationState &app);
	static void setSharing(const std::vector<JSON::Property> &props, ApplicationState &app);

	static bool isActiveObject(const JSON::Value &pd)
	{

		if (!pd.isObject())
			throw std::runtime_error("expected JSON \"object\"");

		for (const JSON::Property &p : pd.getObject().getProperties())
		{
			if (p.Key() == AIS::KEY_SETTING_ACTIVE)
			{
				return Util::Parse::Switch(p.Get().to_string());
			}
		}
		return true;
	}

	static void setSettingsFromJSON(const JSON::Value &pd, Setting &s)
	{

		for (const JSON::Property &p : pd.getObject().getProperties())
		{
			if (p.Key() < 0 || p.Key() >= AIS::KeyMap.size())
				continue;

			if (p.Key() != AIS::KEY_SETTING_ACTIVE)
			{
				s.Set(AIS::KeyMap[p.Key()][JSON_DICT_SETTING], p.Get().to_string());
			}
		}
	}

	static void setServerfromJSON(const JSON::Value &pd, ApplicationState &app)
	{

		if (pd.isArray())
		{
			for (const auto &v : pd.getArray())
			{
				if (!isActiveObject(v))
					continue;
				app.servers.push_back(std::unique_ptr<WebViewer>(new WebViewer()));
				setSettingsFromJSON(v, *app.servers.back());
				app.receivers.back()->setTags("DTM");
				app.servers.back()->active() = true;
			}
		}
		else
		{
			if (!isActiveObject(pd))
				return;

			app.servers.push_back(std::unique_ptr<WebViewer>(new WebViewer()));
			setSettingsFromJSON(pd, *app.servers.back());
			app.receivers.back()->setTags("DTM");
			app.servers.back()->active() = true;
		}
	}

	static void setHTTPfromJSON(const JSON::Property &pd, ApplicationState &app)
	{

		if (!pd.Get().isArray())
			throw std::runtime_error("HTTP settings need to be an \"array\" of \"objects\". in config file");

		for (const auto &v : pd.Get().getArray())
		{
			if (!isActiveObject(v))
				continue;

			app.json.push_back(std::unique_ptr<IO::OutputJSON>(new IO::HTTPStreamer()));
			IO::OutputJSON &h = *app.json.back();

			setSettingsFromJSON(v, h);
			app.receivers.back()->setTags("DT");
		}
	}

	static void setUDPfromJSON(const JSON::Property &pd, ApplicationState &app)
	{

		if (!pd.Get().isArray())
			throw std::runtime_error("UDP settings need to be an \"array\" of \"objects\" in config file.");

		for (const auto &v : pd.Get().getArray())
		{
			if (!isActiveObject(v))
				continue;

			app.msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::UDPStreamer()));
			IO::OutputMessage &o = *app.msg.back();
			setSettingsFromJSON(v, o);
		}
	}

	static void setTCPfromJSON(const JSON::Property &pd, ApplicationState &app)
	{

		if (!pd.Get().isArray())
			throw std::runtime_error("TCP settings need to be an \"array\" of \"objects\" in config file.");

		for (const auto &v : pd.Get().getArray())
		{
			if (!isActiveObject(v))
				continue;

			app.msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::TCPClientStreamer()));
			IO::OutputMessage &tcp = *app.msg.back();
			setSettingsFromJSON(v, tcp);
		}
	}

	static void setMQTTfromJSON(const JSON::Property &pd, ApplicationState &app)
	{
		if (!pd.Get().isArray())
			throw std::runtime_error("MQTT settings need to be an \"array\" of \"objects\" in config file.");

		for (const auto &v : pd.Get().getArray())
		{
			if (!isActiveObject(v))
				continue;

			app.msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::MQTTStreamer()));
			IO::OutputMessage &mqtt = *app.msg.back();
			setSettingsFromJSON(v, mqtt);
		}
	}
	static void setTCPListenerfromJSON(const JSON::Property &pd, ApplicationState &app)
	{

		if (!pd.Get().isArray())
			throw std::runtime_error("TCP Listener settings need to be an \"array\" of \"objects\" in config file.");

		for (const auto &v : pd.Get().getArray())
		{
			if (!isActiveObject(v))
				continue;

			app.msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::TCPlistenerStreamer()));
			IO::OutputMessage &tcp = *app.msg.back();
			tcp.Set("TIMEOUT", "0");
			setSettingsFromJSON(v, tcp);
		}
	}

	static void setModelfromJSON(const JSON::Property &p, ApplicationState &app)
	{

		if (!isActiveObject(p.Get()))
			return;

		app.receivers.back()->addModel(2);
		setSettingsFromJSON(p.Get(), *app.receivers.back()->Model(app.receivers.back()->Count() - 1));
	}

	static void setReceiverFromArray(const JSON::Property &pd, ApplicationState &app)
	{
		if (!pd.Get().isArray())
			throw std::runtime_error("Receiver settings need to be an \"array\" of \"objects\" in config file.");

		for (const auto &v : pd.Get().getArray())
		{
			if (!isActiveObject(v))
				continue;

			setReceiverfromJSON(v.getObject().getProperties(), false, app);
		}
	}

	static void setReceiverfromJSON(const std::vector<JSON::Property> &props, bool unspecAllowed, ApplicationState &app)
	{
		std::string config, serial, input;
		// pass 1
		for (const auto &p : props)
		{
			switch (p.Key())
			{
			case AIS::KEY_SETTING_SERIAL:
				serial = p.Get().to_string();
				break;
			case AIS::KEY_SETTING_INPUT:
				input = p.Get().to_string();
				break;
			case AIS::KEY_SETTING_ACTIVE:
				if (!Util::Parse::Switch(p.Get().to_string()))
					return;
				break;
			}
		}

		if (!unspecAllowed)
		{
			if (serial.empty() && input.empty())
				throw std::runtime_error("receiver needs to have a serial or input specified in Config.");
		}

		if ((!serial.empty() || !input.empty()))
		{
			if (++app.nrec > 1)
				app.receivers.push_back(std::unique_ptr<Receiver>(new Receiver()));

			app.receivers.back()->getDeviceManager().SerialNumber() = serial;
		}

		if (!input.empty())
		{
			if (!Util::Parse::DeviceType(input, app.receivers.back()->getDeviceManager().InputType()))
			{
				throw std::runtime_error("\"" + input + "\" is unknown input type in config file");
			}
		}

		// pass 2
		for (const auto &p : props)
		{
			switch (p.Key())
			{
			case AIS::KEY_SETTING_VERBOSE:
				app.receivers.back()->verbose = Util::Parse::Switch(p.Get().to_string());
				break;
			case AIS::KEY_SETTING_MODEL:
				setModelfromJSON(p, app);
				break;
			case AIS::KEY_SETTING_META:
				app.receivers.back()->setTags(p.Get().to_string());
				break;
			case AIS::KEY_SETTING_OWN_MMSI:
				app.own_mmsi = p.Get().getInt();
				break;
			case AIS::KEY_SETTING_RTLSDR:
				if (!isActiveObject(p.Get()))
					continue;
				setSettingsFromJSON(p.Get(), app.receivers.back()->getDeviceManager().RTLSDR());
				break;
			case AIS::KEY_SETTING_RTLTCP:
				if (!isActiveObject(p.Get()))
					continue;
				setSettingsFromJSON(p.Get(), app.receivers.back()->getDeviceManager().RTLTCP());
				break;
			case AIS::KEY_SETTING_AIRSPY:
				if (!isActiveObject(p.Get()))
					continue;
				setSettingsFromJSON(p.Get(), app.receivers.back()->getDeviceManager().AIRSPY());
				break;
			case AIS::KEY_SETTING_AIRSPYHF:
				if (!isActiveObject(p.Get()))
					continue;
				setSettingsFromJSON(p.Get(), app.receivers.back()->getDeviceManager().AIRSPYHF());
				break;
			case AIS::KEY_SETTING_SDRPLAY:
				if (!isActiveObject(p.Get()))
					continue;
				setSettingsFromJSON(p.Get(), app.receivers.back()->getDeviceManager().SDRPLAY());
				break;
			case AIS::KEY_SETTING_WAVFILE:
				if (!isActiveObject(p.Get()))
					continue;
				setSettingsFromJSON(p.Get(), app.receivers.back()->getDeviceManager().WAV());
				break;
			case AIS::KEY_SETTING_SERIALPORT:
				if (!isActiveObject(p.Get()))
					continue;
				setSettingsFromJSON(p.Get(), app.receivers.back()->getDeviceManager().SerialPort());
				break;
			case AIS::KEY_SETTING_HACKRF:
				if (!isActiveObject(p.Get()))
					continue;
				setSettingsFromJSON(p.Get(), app.receivers.back()->getDeviceManager().HACKRF());
				break;
			case AIS::KEY_SETTING_HYDRASDR:
				if (!isActiveObject(p.Get()))
					continue;
				setSettingsFromJSON(p.Get(), app.receivers.back()->getDeviceManager().HYDRASDR());
				break;
			case AIS::KEY_SETTING_UDPSERVER:
				if (!isActiveObject(p.Get()))
					continue;
				setSettingsFromJSON(p.Get(), app.receivers.back()->getDeviceManager().UDP());
				break;
			case AIS::KEY_SETTING_SOAPYSDR:
				if (!isActiveObject(p.Get()))
					continue;
				setSettingsFromJSON(p.Get(), app.receivers.back()->getDeviceManager().SOAPYSDR());
				break;
			case AIS::KEY_SETTING_NMEA2000:
				if (!isActiveObject(p.Get()))
					continue;
				setSettingsFromJSON(p.Get(), app.receivers.back()->getDeviceManager().N2KSCAN());
				break;
			case AIS::KEY_SETTING_FILE:
				if (!isActiveObject(p.Get()))
					continue;
				setSettingsFromJSON(p.Get(), app.receivers.back()->getDeviceManager().RAW());
				break;
			case AIS::KEY_SETTING_ZMQ:
				if (!isActiveObject(p.Get()))
					continue;
				setSettingsFromJSON(p.Get(), app.receivers.back()->getDeviceManager().ZMQ());
				break;
			case AIS::KEY_SETTING_SPYSERVER:
				if (!isActiveObject(p.Get()))
					continue;
				setSettingsFromJSON(p.Get(), app.receivers.back()->getDeviceManager().SpyServer());
				break;
			default:
				break;
			}
		}
	}

	void parse(const std::string &filename, ApplicationState &app)
	{

		if (!filename.empty())
		{
			parseString(Util::Helper::readFile(filename), app);
		}
	}

	static void setSharing(const std::vector<JSON::Property> &props, ApplicationState &app)
	{

		bool xchange = false;
		std::string uuid;

		for (const JSON::Property &p : props)
		{
			if (p.Key() == AIS::KEY_SETTING_SHARING)
			{
				xchange = Util::Parse::Switch(p.Get().to_string());
			}
			else if (p.Key() == AIS::KEY_SETTING_SHARING_KEY)
				uuid = p.Get().to_string();
		}

		if (xchange && !app.community_feed)
		{
			app.msg.push_back(std::unique_ptr<IO::OutputMessage>(new IO::CommunityStreamer()));
			app.community_feed = app.msg.back().get();
		}
		if (!uuid.empty() && app.community_feed)
			app.community_feed->Set("UUID", uuid);
	}

	void parseString(const std::string &str, ApplicationState &app)
	{
		std::string config, serial, input;
		int version = 0;

		JSON::Parser parser(&AIS::KeyMap, JSON_DICT_SETTING);
		std::shared_ptr<JSON::JSON> json = parser.parse(str);

		// loop over all properties
		const std::vector<JSON::Property> &props = json->getProperties();

		// pass 1
		for (const auto &p : props)
		{
			switch (p.Key())
			{
			case AIS::KEY_SETTING_CONFIG:
				config = p.Get().to_string();
				break;
			case AIS::KEY_SETTING_VERSION:
				version = Util::Parse::Integer(p.Get().to_string());
				break;
			}
		}

		if (version < 1 || version > 1 || config != "aiscatcher")
			throw std::runtime_error("version and/or format of config file not supported (required version <=1)");

		setReceiverfromJSON(props, true, app);
		setSharing(props, app);

		// pass 2
		for (const auto &p : props)
		{
			switch (p.Key())
			{
			// fields that are already processed for completeness
			case AIS::KEY_SETTING_CONFIG:
			case AIS::KEY_SETTING_VERSION:
			case AIS::KEY_SETTING_VERBOSE:
			case AIS::KEY_SETTING_SERIAL:
			case AIS::KEY_SETTING_INPUT:
			case AIS::KEY_SETTING_MODEL:
			case AIS::KEY_SETTING_META:
			case AIS::KEY_SETTING_RTLSDR:
			case AIS::KEY_SETTING_RTLTCP:
			case AIS::KEY_SETTING_AIRSPY:
			case AIS::KEY_SETTING_AIRSPYHF:
			case AIS::KEY_SETTING_SERIALPORT:
			case AIS::KEY_SETTING_OWN_MMSI:
			case AIS::KEY_SETTING_SDRPLAY:
			case AIS::KEY_SETTING_NMEA2000:
			case AIS::KEY_SETTING_WAVFILE:
			case AIS::KEY_SETTING_HACKRF:
			case AIS::KEY_SETTING_HYDRASDR:
			case AIS::KEY_SETTING_UDPSERVER:
			case AIS::KEY_SETTING_SOAPYSDR:
			case AIS::KEY_SETTING_FILE:
			case AIS::KEY_SETTING_ZMQ:
			case AIS::KEY_SETTING_SPYSERVER:
			case AIS::KEY_SETTING_SHARING:
			case AIS::KEY_SETTING_SHARING_KEY:
				break;
			case AIS::KEY_SETTING_UDP:
				setUDPfromJSON(p, app);
				break;
			case AIS::KEY_SETTING_TCP:
				setTCPfromJSON(p, app);
				break;
			case AIS::KEY_SETTING_MQTT:
				setMQTTfromJSON(p, app);
				break;
			case AIS::KEY_SETTING_TCP_LISTENER:
				setTCPListenerfromJSON(p, app);
				break;
			case AIS::KEY_SETTING_SERVER:
				setServerfromJSON(p.Get(), app);
				break;
			case AIS::KEY_SETTING_HTTP:
				setHTTPfromJSON(p, app);
				break;
			case AIS::KEY_SETTING_RECEIVER:
				setReceiverFromArray(p, app);
				break;
			case AIS::KEY_SETTING_SCREEN:
				app.screen.setScreen(p.Get().to_string());
				break;
			case AIS::KEY_SETTING_VERBOSE_TIME:
				app.screen.verboseUpdateTime = Util::Parse::Integer(p.Get().to_string(), 1, 300);
				break;
			default:
				if (p.Key() >= 0 && p.Key() < AIS::KeyMap.size())
					throw std::runtime_error("Config file: field \"" + AIS::KeyMap[p.Key()][JSON_DICT_SETTING] + "\" in main section is not allowed.");
				else
					throw std::runtime_error("Config file: unknown field in main section is not allowed.");
			}
		}
	}

} // namespace JSONConfig
