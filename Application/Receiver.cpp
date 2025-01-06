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

#include "Receiver.h"
#include "Utilities.h"
#include "JSON/StringBuilder.h"

#include "Statistics.h"
//--------------------------------------------

std::vector<Device::Description> Receiver::device_list;

void Receiver::refreshDevices(void)
{
	device_list.clear();

	RTLSDR().getDeviceList(device_list);
	AIRSPYHF().getDeviceList(device_list);
	AIRSPY().getDeviceList(device_list);
	SDRPLAY().getDeviceList(device_list);
	HACKRF().getDeviceList(device_list);
	SOAPYSDR().getDeviceList(device_list);
}

void Receiver::setTags(const std::string &s)
{
	for (char c : s)
	{
		switch (toupper(c))
		{
		case 'M':
			tag.mode |= 4;
			break;
		case 'T':
			tag.mode |= 2;
			break;
		case 'D':
			tag.mode |= 1;
			break;
		default:
			Warning() << "Unknown tag '" + std::string(1,c) + "' defined on command line [D / T /M]";
		}
	}
}

void Receiver::removeTags(const std::string &s)
{
	for (char c : s)
	{
		switch (toupper(c))
		{
		case 'M':
			tag.mode &= ~4;
			break;
		case 'T':
			tag.mode &= ~2;
			break;
		case 'D':
			tag.mode &= ~1;
			break;
		default:
			throw std::runtime_error("illegal tag '" + std::string(1, c) + "' defined on command line [D / T / M]");
		}
	}
}

void Receiver::setChannel(std::string mode, std::string NMEA)
{

	Util::Convert::toUpper(mode);
	Util::Convert::toUpper(NMEA);

	if (mode == "AB")
	{
		ChannelMode = AIS::Mode::AB;
		if (NMEA.empty())
			NMEA = "AB";
	}
	else if (mode == "CD")
	{
		ChannelMode = AIS::Mode::CD;
		if (NMEA.empty())
			NMEA = "CD";
	}
	else if (mode == "X")
	{
		ChannelMode = AIS::Mode::X;
		if (NMEA.empty())
		{
			NMEA = "XX";
		}
		else if (NMEA.length() == 1)
		{
			NMEA += NMEA;
		}
	}
	else
		throw std::runtime_error("channel mode needs to be AB, CD or X");

	if (NMEA.length() != 2)
		throw std::runtime_error("invalid NMEA channel designation: " + NMEA);

	for (int i = 0; i < NMEA.length(); i++)
		if (!std::isalpha(NMEA[i]) && !std::isdigit(NMEA[i]) && NMEA[i] != '?')
			throw std::runtime_error("invalid NMEA channel designation: " + std::string(1, NMEA[i]));

	ChannelNMEA = NMEA;
}

// Set up Device
Device::Device *Receiver::getDeviceByType(Type type)
{
	switch (type)
	{
	case Type::WAVFILE:
		return &_WAV;
	case Type::RAWFILE:
		return &_RAW;
	case Type::RTLTCP:
		return &_RTLTCP;
	case Type::SPYSERVER:
		return &_SpyServer;
	case Type::SERIALPORT:
		return &_SerialPort;
	case Type::UDP:
		return &_UDP;
#ifdef HASNMEA2000
	case Type::N2K:
		return &_N2KSCAN;
#endif
#ifdef HASZMQ
	case Type::ZMQ:
		return &_ZMQ;
#endif
#ifdef HASAIRSPYHF
	case Type::AIRSPYHF:
		return &_AIRSPYHF;
#endif
#ifdef HASAIRSPY
	case Type::AIRSPY:
		return &_AIRSPY;
#endif
#ifdef HASSDRPLAY
	case Type::SDRPLAY:
		return &_SDRPLAY;
#endif
#ifdef HASRTLSDR
	case Type::RTLSDR:
		return &_RTLSDR;
#endif
#ifdef HASHACKRF
	case Type::HACKRF:
		return &_HACKRF;
#endif
#ifdef HASSOAPYSDR
	case Type::SOAPYSDR:
		return &_SOAPYSDR;
#endif
	default:
		return nullptr;
	}
}

void Receiver::setupDevice()
{

	int idx = device_list.empty() ? -1 : 0;
	uint64_t handle = device_list.empty() ? 0 : device_list[0].getHandle();

	if (!serial.empty())
		Info() << "Searching for device with SN " << serial << ".";

	if (!serial.empty() || type != Type::NONE)
	{
		idx = -1;
		for (int i = 0; i < device_list.size(); i++)
		{
			bool serial_match = device_list[i].getSerial() == serial && (type == Type::NONE || type == device_list[i].getType());
			bool type_match = serial.empty() && (type == device_list[i].getType());

			if (serial_match || type_match)
			{
				idx = i;
				handle = device_list[i].getHandle();
				break;
			}
		}
		if (idx == -1)
		{

			if (!serial.empty())
				throw std::runtime_error("Cannot find device with specified parameters");

			idx = 0;
			handle = 0;
		}
	}

	if (type == Type::NONE && device_list.size() == 0)
		throw std::runtime_error("No devices available");

	if (type != Type::NONE)
		device = getDeviceByType(type);
	else
		device = getDeviceByType(device_list[idx].getType());

	if (device == 0)
		throw std::runtime_error("cannot set up device");

	device->Open(handle);

	if (ChannelMode == AIS::Mode::AB)
		device->setFrequency((int)(162000000));
	else
		device->setFrequency((int)(156800000));

	if (sample_rate)
		device->setSampleRate(sample_rate);
	if (ppm)
		device->Set("FREQOFFSET", std::to_string(ppm));
	if (bandwidth)
		device->Set("BW", std::to_string(bandwidth));

	tag.hardware = device->getProduct();
	tag.driver = device->getDriver();
	tag.version = VERSION_NUMBER;

	device->setTag(tag);
}

// Set up model
std::unique_ptr<AIS::Model> &Receiver::addModel(int m)
{

	switch (m)
	{
	case 0:
		models.push_back(std::unique_ptr<AIS::Model>(new AIS::ModelStandard()));
		break;
	case 1:
		models.push_back(std::unique_ptr<AIS::Model>(new AIS::ModelBase()));
		break;
	case 2:
		models.push_back(std::unique_ptr<AIS::Model>(new AIS::ModelDefault()));
		break;
	case 3:
		models.push_back(std::unique_ptr<AIS::Model>(new AIS::ModelDiscriminator()));
		break;
	case 4:
		models.push_back(std::unique_ptr<AIS::Model>(new AIS::ModelChallenger()));
		break;
	case 5:
		models.push_back(std::unique_ptr<AIS::Model>(new AIS::ModelNMEA()));
		break;
	case 6:
		models.push_back(std::unique_ptr<AIS::Model>(new AIS::ModelN2K()));
		break;
	case 7:
		models.push_back(std::unique_ptr<AIS::Model>(new AIS::ModelBaseStation()));
		break;
	default:
		throw std::runtime_error("Model not implemented in this version. Check in later.");
	}
	return models.back();
}

void Receiver::setupModel(int &group)
{
	// if nothing defined, create one model
	if (!models.size())
	{
		switch (device->getFormat())
		{
		case Format::TXT:
			addModel(5);
			break;
		case Format::N2K:
			addModel(6);
			break;
		case Format::BASESTATION:
			addModel(7);
			break;
		default:
			addModel(2);
			break;
		}
	}
	// ensure some basic compatibility between model and device
	for (const auto &m : models)
	{
		if ((m->getClass() == AIS::ModelClass::TXT && device->getFormat() != Format::TXT) ||
			(m->getClass() != AIS::ModelClass::TXT && device->getFormat() == Format::TXT) || 
			(m->getClass() == AIS::ModelClass::BASESTATION && device->getFormat() != Format::BASESTATION) )
			throw std::runtime_error("Decoding model and input format not consistent.");
	}

	// build the decoder models
	for (auto &m : models)
	{
		m->setDesignation(ChannelNMEA);
		m->setMode(ChannelMode);
		m->setOwnMMSI(own_mmsi);
		m->buildModel(ChannelNMEA[0], ChannelNMEA[1], device->getSampleRate(), timing, device);
	}

	// set up JSON output channels, still unconnected
	jsonais.clear();
	jsonais.resize(models.size());

	// assign the output of each individual model to a seperate group
	assert(group + models.size() < 32);

	for (int i = 0; i < models.size(); i++)
	{
		uint32_t mask = 1 << group;
		jsonais[i].out.setGroupOut(mask);
		models[i]->Output().out.setGroupOut(mask);
		group++;
	}
}

void Receiver::play()
{

	// connect the JSON output where and if needed
	for (int i = 0; i < jsonais.size(); i++)
	{
		if (jsonais[i].out.isConnected())
		{
			models[i]->Output() >> jsonais[i];
		}
	}

	device->Play();

	if (verbose)
	{
		std::stringstream ss;
		ss << "Device    : " << device->getProduct() << "\n"
		   << "Settings  : " << device->Get() << "\n";
		for (int i = 0; i < models.size(); i++)
			ss << "Model #" + std::to_string(i) << " -> (Src: " << std::to_string(Util::Helper::lsb(models[i]->Output().out.getGroupOut()) + 1)
			   << ", Grp: " + std::to_string(models[i]->Output().out.getGroupOut()) + "): [" + models[i]->getName() + "] " + models[i]->Get() << "\n";

		Info() << ss.str();
	}
}

void Receiver::stop()
{
	if (device)
		device->Stop();
}

//-----------------------------------
// set up screen output

void OutputScreen::setScreen(const std::string &str)
{
	switch (Util::Parse::Integer(str, 0, 5))
	{
	case 0:
		level = MessageFormat::SILENT;
		break;
	case 1:
		level = MessageFormat::NMEA;
		break;
	case 2:
		level = MessageFormat::FULL;
		break;
	case 3:
		level = MessageFormat::JSON_NMEA;
		break;
	case 4:
		level = MessageFormat::JSON_SPARSE;
		break;
	case 5:
		level = MessageFormat::JSON_FULL;
		break;
	default:
		throw std::runtime_error("unknown option for screen output: " + str);
	}
}
void OutputScreen::setScreen(MessageFormat o)
{
	level = o;
}

void OutputScreen::connect(Receiver &r)
{

	if (level == MessageFormat::NMEA || level == MessageFormat::JSON_NMEA || level == MessageFormat::FULL)
	{
		for (int j = 0; j < r.Count(); j++)
		{
			if (r.Output(j).canConnect(((StreamIn<AIS::Message>)msg2screen).getGroupsIn()))
				r.Output(j).Connect((StreamIn<AIS::Message> *)&msg2screen);

			if (r.OutputGPS(j).canConnect(((StreamIn<AIS::GPS>)msg2screen).getGroupsIn()))
				r.OutputGPS(j).Connect((StreamIn<AIS::GPS> *)&msg2screen);
		}

		msg2screen.setDetail(level);
	}
	else if (level == MessageFormat::JSON_SPARSE || level == MessageFormat::JSON_FULL)
	{
		for (int j = 0; j < r.Count(); j++)
		{
			if (r.OutputJSON(j).canConnect(((StreamIn<JSON::JSON>)json2screen).getGroupsIn()))
				r.OutputJSON(j).Connect((StreamIn<JSON::JSON> *)&json2screen);

			if (r.OutputGPS(j).canConnect(((StreamIn<AIS::GPS>)json2screen).getGroupsIn()))
				r.OutputGPS(j).Connect((StreamIn<AIS::GPS> *)&json2screen);
		}

		if (level == MessageFormat::JSON_SPARSE)
			json2screen.setMap(JSON_DICT_SPARSE);
	}
}

void OutputScreen::start() {}
//-----------------------------------
// set up screen counters

void OutputStatistics::connect(Receiver &r)
{

	statistics.resize(r.Count());

	for (int i = 0; i < r.Count(); i++)
		r.Output(i) >> statistics[i];
}

void OutputStatistics::start() {}
