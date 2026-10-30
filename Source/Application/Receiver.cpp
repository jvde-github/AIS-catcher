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

#include "Receiver.h"
#include "Convert.h"
#include "Helper.h"
#include "Logger.h"

//--------------------------------------------

Setting &Receiver::applyKey(AIS::Keys key, const std::string &arg)
{
	switch (key)
	{
	case AIS::KEY_SETTING_VERBOSE:
		verbose = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_SENSITIVITY_HIGH:
		Warning() << "Receiver: \"sensitivity_high\" is deprecated and will be removed, use \"engines\": [ { \"type\": \"v1_high\" } ] instead";
		high_sensitivity = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_CHANNEL:
		setChannel(arg, "");
		break;
	case AIS::KEY_SETTING_NMEA_CHANNEL:
		setChannel(channelModeString(), arg);
		break;
	case AIS::KEY_SETTING_META:
		// exactly these tags, as -M has it
		clearTags();
		setTags(arg);
		break;
	case AIS::KEY_SETTING_TIMING:
		timing_ = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_SERIAL:
		deviceManager.setSerial(arg);
		break;
	case AIS::KEY_SETTING_INPUT:
	{
		Type type = Type::NONE;
		if (!Util::Parse::DeviceType(arg, type))
			throw std::runtime_error("unknown input type: " + arg);
		deviceManager.setInputType(type);
	}
	break;
	case AIS::KEY_SETTING_SAMPLE_RATE:
		sample_rate = Util::Parse::Integer(arg, 12500, 12288000);
		break;
	case AIS::KEY_SETTING_BANDWIDTH:
		bandwidth = Util::Parse::Integer(arg, 0, 20000000);
		break;
	case AIS::KEY_SETTING_FREQOFFSET:
		ppm = Util::Parse::Integer(arg, -150, 150);
		break;
	case AIS::KEY_SETTING_LAT:
		station_lat = Util::Parse::Float(arg);
		break;
	case AIS::KEY_SETTING_LON:
		station_lon = Util::Parse::Float(arg);
		break;
	case AIS::KEY_SETTING_DESCRIPTION:
	case AIS::KEY_SETTING_DESC:
		description = arg;
		break;
	case AIS::KEY_SETTING_STATION:
		station = Util::Parse::Integer(arg, 0, 2147483647);
		break;
	default:
		throw std::runtime_error(getName() + ": unknown setting \"" + AIS::KeyMap[key][JSON_DICT_SETTING] + "\"");
	}
	return *this;
}

int Receiver::tagBit(char c)
{
	switch (toupper(c))
	{
	case 'M':
		return 4;
	case 'T':
		return 2;
	case 'D':
		return 1;
	default:
		return 0;
	}
}

void Receiver::setTags(const std::string &s)
{
	for (char c : s)
	{
		const int bit = tagBit(c);
		if (bit)
			tag.mode |= bit;
		else
			Warning() << "Unknown tag '" + std::string(1, c) + "' defined on command line ignored [D / T / M]";
	}
}

void Receiver::removeTags(const std::string &s)
{
	for (char c : s)
	{
		const int bit = tagBit(c);
		if (bit)
			tag.mode &= ~bit;
		else
			throw std::runtime_error("illegal tag '" + std::string(1, c) + "' defined on command line [D / T / M]");
	}
}

std::string Receiver::tagString() const
{
	return std::string(tag.mode & tagBit('D') ? "D" : "") + (tag.mode & tagBit('T') ? "T" : "") + (tag.mode & tagBit('M') ? "M" : "");
}

void Receiver::setChannel(std::string mode, std::string NMEA)
{

	Util::Convert::toUpper(mode);
	Util::Convert::toUpper(NMEA);

	if (mode == "AB")
	{
		channel_mode = AIS::Mode::AB;
		if (NMEA.empty())
			NMEA = "AB";
	}
	else if (mode == "CD")
	{
		channel_mode = AIS::Mode::CD;
		if (NMEA.empty())
			NMEA = "CD";
	}
	else if (mode == "X")
	{
		channel_mode = AIS::Mode::X;
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

	channel_nmea = NMEA;
}

// Set up Device
void Receiver::setupDevice()
{
	int frequency = (channel_mode == AIS::Mode::AB) ? 162000000 : 156800000;

	tag.version = VERSION_NUMBER;
	tag.station_lat = station_lat;
	tag.station_lon = station_lon;
	tag.station_id = station;

	if (!deviceManager.openDevice(sample_rate, bandwidth, ppm, frequency, tag))
		throw std::runtime_error("Receiver: cannot set up device.");
}

// Set up model
int Receiver::defaultModel()
{
	switch (deviceManager.getInputFormat())
	{
	case Format::TXT:
		return AIS::MODEL_NMEA;
	case Format::N2K:
		return AIS::MODEL_N2K;
	case Format::BASESTATION:
		return AIS::MODEL_BASESTATION;
	case Format::BEAST:
		return AIS::MODEL_BEAST;
	case Format::RAW1090:
		return AIS::MODEL_RAW1090;
	default:
		return high_sensitivity ? AIS::MODEL_V1_HIGH : AIS::MODEL_V1_BASE;
	}
}

AIS::Model *Receiver::makeModel(int m)
{
	switch (m)
	{
	case 0:
		return new AIS::ModelStandard();
	case 1:
		return new AIS::ModelBase();
	case 2:
		return new AIS::ModelDefault();
	case 3:
		return new AIS::ModelDiscriminator();
	case 4:
		return new AIS::ModelChallenger();
	case 5:
		return new AIS::ModelNMEA();
	case 6:
		return new AIS::ModelN2K();
	case 7:
		return new AIS::ModelBaseStation();
	case 8:
		return new AIS::ModelBeast();
	case 9:
		return new AIS::ModelExport();
	case 10:
		return new AIS::ModelRAW1090();
	case 11:
		return new AIS::ModelEngineV2();
	default:
		throw std::runtime_error("Model not implemented in this version. Check in later.");
	}
}

AIS::Model &Receiver::addModel(int m, const std::vector<std::string> &zones)
{
	// -1: the default model for the input format
	if (m == -1)
		m = defaultModel();

	decoders.push_back(Decoder());
	Decoder &d = decoders.back();
	d.model.reset(makeModel(m));
	d.id = m;
	d.zones = zones;
	return *d.model;
}

void Receiver::setupModel(int &group, int idx)
{
	receiver_index = idx;
	auto *device = deviceManager.getDevice();
	// if nothing defined, create one model
	if (!decoders.size())
		addModel(-1);

	// ensure some basic compatibility between model and device
	for (const auto &d : decoders)
	{
		const auto &m = d.model;
		if ((m->getClass() == AIS::ModelClass::TXT && device->getFormat() != Format::TXT) ||
			(m->getClass() != AIS::ModelClass::TXT && device->getFormat() == Format::TXT) ||
			(m->getClass() == AIS::ModelClass::BASESTATION && (device->getFormat() != Format::BASESTATION && device->getFormat() != Format::BEAST && device->getFormat() != Format::RAW1090)))
			throw std::runtime_error("Decoding model and input format not consistent.");
	}

	// build the decoder models
	for (auto &d : decoders)
	{
		auto &m = d.model;
		// with stations configured, the station is the station_id its messages carry, unless the decoder names one
		if (stamp_station)
			m->setDefaultStation(station);
		m->setDesignation(channel_nmea);
		m->setMode(channel_mode);
		m->setOwnMMSI(own_mmsi);
		m->buildModel(channel_nmea[0], channel_nmea[1], device->getSampleRate(), timing_, device);
	}

	// set up JSON output channels, still unconnected
	for (auto &d : decoders)
		d.json.reset(new AIS::JSONAIS());

	// assign the output of each individual model to a separate group; bits 0-30
	// only, as group masks pass through Parse::Integer (long, 32-bit on Windows)
	// where bit 31 would overflow
	if (group + (int)decoders.size() >= 32)
		throw std::runtime_error("Receiver: too many models/receivers, group bit limit exceeded.");

	for (auto &d : decoders)
	{
		uint64_t mask = 1ULL << group;
		d.json->out.setGroupOut(mask);
		d.model->Output().out.setGroupOut(mask);
		d.model->OutputADSB().out.setGroupOut(mask);
		group++;
	}
}

void Receiver::connectCounters()
{
	for (auto &d : decoders)
		d.counter.reset(new IO::StreamCounter());

	for (auto &d : decoders)
	{
		d.model->Output().out.Connect((StreamIn<AIS::Message> *)d.counter.get());
		d.model->OutputADSB().out.Connect((StreamIn<Plane::ADSB> *)d.counter.get());
	}
}

uint64_t Receiver::messages()
{
	uint64_t count = 0;
	for (auto &d : decoders)
		count += d.counter->getCount();
	return count;
}

void Receiver::play()
{

	// connect the JSON output where and if needed
	for (auto &d : decoders)
	{
		if (d.json->out.isConnected())
		{
			d.model->Output() >> *d.json;
		}
	}

	auto *device = deviceManager.getDevice();
	device->Play();

	if (verbose)
		logSummary();
}

void Receiver::logSummary() const
{
	auto *device = deviceManager.getDevice();
	if (!device)
		return;

	Info() << "Device    : " << device->getProduct();
	Info() << "Settings  : " << device->Get();
	for (int i = 0; i < decoders.size(); i++)
	{
		const auto &m = decoders[i].model;
		Info() << "Model " + modelIndex(i) << " -> (Src: " << std::to_string(Util::Helper::lsb(m->Output().out.getGroupOut()) + 1)
			   << ", Grp: " + std::to_string(m->Output().out.getGroupOut()) + "): [" + m->getName() + "] channel " + channel_nmea + " " + m->Get();
	}
}

void Receiver::stop()
{
	auto *device = deviceManager.getDevice();
	if (device)
		device->Stop();
}

// the kind of device with its place among the receivers of that kind
static std::string kindLabel(const std::vector<std::unique_ptr<Receiver>> &receivers, std::size_t k)
{
	const Type type = receivers[k]->inputType();
	int nth = 0;
	for (std::size_t i = 0; i <= k; i++)
		nth += receivers[i]->inputType() == type;
	return std::string(DeviceManager::label(type)) + " #" + std::to_string(nth);
}

std::string receiverLabel(const std::vector<std::unique_ptr<Receiver>> &receivers, std::size_t k)
{
	Receiver &r = *receivers[k];
	if (!r.getDescription().empty())
		return r.getDescription();
	Device::Device *device = r.device();
	if (device && device->getSerial() == ".")
		return "Console";
	return kindLabel(receivers, k);
}

bool anyZoned(const std::vector<std::unique_ptr<Receiver>> &receivers)
{
	for (const auto &r : receivers)
		for (int i = 0; i < r->modelCount(); i++)
			if (r->hasZone(i))
				return true;
	return false;
}

// the group mask of every model output in no zone at all
static uint64_t unzonedGroups(const std::vector<std::unique_ptr<Receiver>> &receivers)
{
	uint64_t mask = 0;

	for (const auto &r : receivers)
		for (int i = 0; i < r->modelCount(); i++)
			if (!r->hasZone(i))
				mask |= r->getGroupMask(i);

	return mask;
}

// the group mask of every model output that is in one of the zones
static uint64_t zonedGroups(const std::vector<std::unique_ptr<Receiver>> &receivers, const std::vector<std::string> &zones)
{
	uint64_t mask = 0;

	for (const auto &r : receivers)
		for (int i = 0; i < r->modelCount(); i++)
			for (const auto &z : zones)
				if (r->inZone(i, z))
				{
					mask |= r->getGroupMask(i);
					break;
				}

	return mask;
}

uint64_t routeGroups(const std::vector<std::unique_ptr<Receiver>> &receivers, const std::vector<std::string> &zones, uint64_t groups_in, const std::string &what)
{
	uint64_t mask = groups_in;
	if (!zones.empty())
	{
		mask &= zonedGroups(receivers, zones);
		if (!mask)
		{
			std::string list;
			for (const auto &z : zones)
				list += (list.empty() ? "" : ", ") + z;
			Warning() << what << " receives nothing: no input is in its zone" << (zones.size() > 1 ? "s " : " ") << list;
		}
	}
	else if (anyZoned(receivers))
	{
		mask &= unzonedGroups(receivers);
		if (!mask)
			Warning() << what << " receives nothing: it has no zone, and every input has one";
	}
	return mask;
}
