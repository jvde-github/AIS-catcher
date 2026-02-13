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
#include "StringBuilder.h"
#include "Statistics.h"

//--------------------------------------------

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
			Warning() << "Unknown tag '" + std::string(1, c) + "' defined on command line ignored [D / T / M]";
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
void Receiver::setupDevice()
{
	int frequency = (ChannelMode == AIS::Mode::AB) ? 162000000 : 156800000;
	
	tag.version = VERSION_NUMBER;
	tag.station_lat = station_lat;
	tag.station_lon = station_lon;
	
	if (!deviceManager.openDevice(sample_rate, bandwidth, ppm, frequency, tag))
		throw std::runtime_error("cannot set up device");
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
	case 8:
		models.push_back(std::unique_ptr<AIS::Model>(new AIS::ModelBeast()));
		break;
	case 9:
		models.push_back(std::unique_ptr<AIS::Model>(new AIS::ModelExport()));
		break;
	case 10:
		models.push_back(std::unique_ptr<AIS::Model>(new AIS::ModelRAW1090()));
		break;
	default:
		throw std::runtime_error("Model not implemented in this version. Check in later.");
	}
	return models.back();
}

void Receiver::setupModel(int &group, int idx)
{
	receiver_index = idx;
	auto* device = deviceManager.getDevice();
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
		case Format::BEAST:
			addModel(8);
			break;
		case Format::RAW1090:
			addModel(10);
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
			(m->getClass() == AIS::ModelClass::BASESTATION && (device->getFormat() != Format::BASESTATION && device->getFormat() != Format::BEAST && device->getFormat() != Format::RAW1090)))
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
		models[i]->OutputADSB().out.setGroupOut(mask);
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

	auto* device = deviceManager.getDevice();
	device->Play();

	if (verbose)
	{
		std::stringstream ss;
		ss << "Device    : " << device->getProduct() << "\n"
		   << "Settings  : " << device->Get() << "\n";
		for (int i = 0; i < models.size(); i++)
			ss << "Model #" + std::to_string(receiver_index) + "-" + std::to_string(i) << " -> (Src: " << std::to_string(Util::Helper::lsb(models[i]->Output().out.getGroupOut()) + 1)
			   << ", Grp: " + std::to_string(models[i]->Output().out.getGroupOut()) + "): [" + models[i]->getName() + "] " + models[i]->Get() << "\n";

		Info() << ss.str();
	}
}

void Receiver::stop()
{
	auto* device = deviceManager.getDevice();
	if (device)
		device->Stop();
}

//-----------------------------------
// set up screen output


//-----------------------------------
// set up screen counters

void OutputStatistics::connect(Receiver &r)
{

	statistics.resize(r.Count());

	for (int i = 0; i < r.Count(); i++)
	{
		r.Output(i).Connect((StreamIn<AIS::Message> *)&statistics[i]);
		r.OutputADSB(i).Connect((StreamIn<Plane::ADSB> *)&statistics[i]);
	}
}

void OutputStatistics::start() {}
