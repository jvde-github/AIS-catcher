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

#include "WebViewer.h"
#include "WebDB.h"
#include "NMEA.h"
#include "JSONAIS.h"
#include "Helper.h"

IO::OutputMessage *commm_feed = nullptr;

void SSEStreamer::Receive(const JSON::JSON *data, int len, TAG &tag)
{
	if (server)
	{
		AIS::Message *m = (AIS::Message *)data[0].binary;
		std::time_t now = std::time(nullptr);

		if (!m->NMEA.empty())
		{
			std::string nmea_array = "[";
			bool first = true;

			for (const auto &s : m->NMEA)
			{
				std::string nmea = s;

				int end = nmea.rfind(',');
				if (end == std::string::npos)
					continue;

				int start = nmea.rfind(',', end - 1);
				if (start == std::string::npos)
					continue;

				int len = end - start - 1;

				if (len == 0)
					continue;

				if (obfuscate)
				{
					for (int i = 0; i < 3; i++)
					{
						idx = (idx + 1) % len;
						nmea[MIN(start + 1 + idx, nmea.length() - 1)] = '*';
					}
				}

				if (!first)
					nmea_array += ",";
				nmea_array += "\"" + nmea + "\"";
				first = false;
			}
			nmea_array += "]";

			std::string shipname_str;
			std::string shipname_escaped(tag.shipname);
			JSON::StringBuilder::stringify(shipname_escaped, shipname_str);

			std::string json = "{\"mmsi\":" + std::to_string(m->mmsi()) +
							   ",\"timestamp\":" + std::to_string(now) +
							   ",\"channel\":\"" + m->getChannel() +
							   "\",\"type\":" + std::to_string(m->type()) +
							   ",\"shipname\":" + shipname_str +
							   ",\"nmea\":" + nmea_array + "}";
			server->sendSSE(1, "nmea", json);
		}

		if (tag.lat != 0 && tag.lon != 0)
		{
			std::string json = "{\"mmsi\":" + std::to_string(m->mmsi()) + ",\"channel\":\"" + m->getChannel() + "\",\"lat\":" + std::to_string(tag.lat) + ",\"lon\":" + std::to_string(tag.lon) + "}";
			server->sendSSE(2, "nmea", json);
		}
	}
}

WebViewer::WebViewer()
{
	params = "const build_string = '" + std::string(VERSION_DESCRIBE) + "';\nconst build_version = '" + std::string(VERSION) + "';\n\n";
	plugin_code = "\n\nfunction loadPlugins() {\n";
	plugins = "let plugins = '';\nlet server_message = '';\n";
	os.clear();
	JSON::StringBuilder::stringify(Util::Helper::getOS(), os);
	hardware.clear();
	JSON::StringBuilder::stringify(Util::Helper::getHardware(), hardware);
}

std::string WebViewer::decodeNMEAtoJSON(const std::string &nmea_input, bool enhanced)
{
	// Decoder class with full pipeline: NMEA -> Message -> JSON -> Array
	class NMEADecoder : public StreamIn<JSON::JSON>
	{
	public:
		AIS::NMEA nmea_decoder;
		AIS::JSONAIS json_converter;
		JSON::StringBuilder *builder;
		std::string result;
		bool first;
		size_t message_count;
		const size_t MAX_OUTPUT_SIZE;

		NMEADecoder(JSON::StringBuilder *b) : builder(b), first(true), message_count(0), MAX_OUTPUT_SIZE(1024 * 1024)
		{
			nmea_decoder >> json_converter;
			json_converter.out.Connect(this);

			result = "[";
			result.reserve(4096);
		}

		void Receive(const JSON::JSON *data, int len, TAG &tag) override
		{
			for (int i = 0; i < len; i++)
			{
				if (result.size() > MAX_OUTPUT_SIZE)
					throw std::runtime_error("Output size limit exceeded");

				if (!first)
					result += ",";

				first = false;
				builder->stringify(data[i], result);
				message_count++;
			}
		}

		std::string decode(const std::string &nmea_input)
		{
			RAW raw = {Format::TXT, (void *)nmea_input.c_str(), (int)nmea_input.length()};
			TAG tag;
			nmea_decoder.Receive(&raw, 1, tag);
			result += "]";
			return result;
		}
	};

	JSON::StringBuilder builder(&AIS::KeyMap, JSON_DICT_FULL);
	builder.setStringifyEnhanced(enhanced);
	NMEADecoder decoder(&builder);
	return decoder.decode(nmea_input);
}

std::vector<std::string> WebViewer::parsePath(const std::string &url)
{
	std::vector<std::string> path;
	std::string pattern = "/";

	auto pos = url.find(pattern);
	if (pos != 0)
		return path;

	std::string remainder = url.substr(pattern.length());
	std::stringstream ss(remainder);
	std::string segment;

	while (std::getline(ss, segment, '/'))
	{
		if (!segment.empty())
		{
			path.push_back(segment);
		}
	}

	return path;
}

bool WebViewer::parseMBTilesURL(const std::string &url, std::string &layerID, int &z, int &x, int &y)
{
	std::string pattern = "/tiles/";

	layerID.clear();

	std::string segment;
	std::vector<std::string> segments = parsePath(url);

	if (segments.size() != 5 && segments[0] != "tiles")
		return false;

	try
	{
		layerID = segments[1];
		z = std::stoi(segments[2]);
		x = std::stoi(segments[3]);
		y = std::stoi(segments[4]);

		return true;
	}
	catch (...)
	{
		return false;
	}
}

void WebViewer::addMBTilesSource(const std::string &filepath, bool overlay)
{
#if HASSQLITE
	auto source = std::make_shared<MBTilesSupport>();
	if (source->open(filepath))
	{
		mapSources.push_back(source);
		plugin_code += source->generatePluginCode(overlay);
	}
	else
	{
		Error() << "Failed to load MBTiles from: " << filepath;
	}
#endif
}

void WebViewer::addFileSystemTilesSource(const std::string &directoryPath, bool overlay)
{
	auto source = std::make_shared<FileSystemTiles>();
	if (source->open(directoryPath))
	{
		mapSources.push_back(source);
		plugin_code += source->generatePluginCode(overlay);
	}
	else
	{
		Error() << "Failed to load FileSystemTiles from: " << directoryPath;
	}
}

bool WebViewer::Save()
{
	try
	{
		std::ofstream infile(backup_filename, std::ios::binary);

		if (!infile.is_open())
		{
			Error() << "Server: cannot open backup file for writing: "
					<< backup_filename << " (" << std::strerror(errno) << ")";
			return false;
		}

		infile.exceptions(std::ios::failbit | std::ios::badbit); // will throw on write failure

		if (!states[0]->save(infile))
			return false;

		infile.close();
	}
	catch (const std::ios_base::failure &e)
	{
		Error() << "Server: write error on " << backup_filename << " (" << std::strerror(errno) << ")";
		return false;
	}
	catch (const std::exception &e)
	{
		Error() << e.what();
		return false;
	}
	catch (...)
	{
		Error() << "Server: unknown error writing " << backup_filename;
		return false;
	}
	return true;
}

void WebViewer::Clear()
{
	if (states.empty())
		return;
	states[0]->clear();
}

bool WebViewer::Load()
{

	if (backup_filename.empty())
		return false;

	Info() << "Server: reading statistics from " << backup_filename;
	try
	{
		std::ifstream infile(backup_filename, std::ios::binary);

		if (!states[0]->load(infile))
		{
			Warning() << "Server: Could not load statistics from backup";
			return false;
		}

		infile.close();
	}
	catch (const std::exception &e)
	{
		Error() << e.what();
		return false;
	}

	return true;
}

void WebViewer::addPlugin(const std::string &arg)
{
	int version = 0;
	std::string author, description;

	plugins += "console.log('plugin:" + arg + "');";
	plugins += "plugins += 'JS: " + arg + "\\n';";
	plugins += "\n\n//=============\n//" + arg + "\n\n";
	try
	{
		std::string s = Util::Helper::readFile(arg);

		JSON::Parser parser(&AIS::KeyMap, JSON_DICT_SETTING);
		std::string firstline = s.substr(0, s.find('\n'));
		if (firstline.length() > 2 && firstline[0] == '/' && firstline[1] == '/')
			firstline = firstline.substr(2);
		else
			throw std::runtime_error("Plugin does not start with // followed by JSON description.");

		std::shared_ptr<JSON::JSON> j = parser.parse(firstline);

		for (const auto &p : j->getProperties())
		{
			switch (p.Key())
			{
			case AIS::KEY_SETTING_VERSION:
				version = p.Get().getInt();
				break;
			case AIS::KEY_SETTING_AUTHOR:
				author = p.Get().getString();
				break;
			case AIS::KEY_SETTING_DESCRIPTION:
				description = p.Get().getString();
				break;
			default:
				throw std::runtime_error("Unknown key in plugin JSON field");
				break;
			}
		}
		Info() << "Adding plugin (" + arg + "). Description: \"" << description << "\", Author: \"" << author << "\", version " << version;
		if (version != 3)
			throw std::runtime_error("Version not supported, expected 3, got " + std::to_string(version));
		plugin_code += "\ntry{\n" + s + "\n} catch (error) {\nshowDialog(\"Error in Plugin " + arg + "\", \"Plugins contain error: \" + error + \"</br>Consider updating plugins or disabling them.\"); }\n";
	}
	catch (const std::exception &e)
	{

		plugins += "// FAILED\n";
		Warning() << "Server: Plugin \"" + arg + "\" ignored - JS plugin error : " << e.what();
		plugins += "server_message += \"Plugin error (" + arg + ") " + std::string(e.what()) + "\\n\"\n";
	}
}

void WebViewer::BackupService()
{
	try
	{
		Info() << "Server: starting backup service every " << backup_interval << " minutes.";
		while (true)
		{
			std::unique_lock<std::mutex> lock(m);

			if (cv.wait_for(lock, std::chrono::minutes(backup_interval), [&]
							{ return !run; }))
			{
				break;
			}

			if (!Save())
				Error() << "Server failed to write backup:" << backup_filename;
		}
	}
	catch (std::exception &e)
	{
		Error() << "WebClient BackupService: " << e.what();
		std::terminate();
	}

	Info() << "Server: stopping backup service.";
}

void ReceiverState::applyConfig(float lat, float lon, bool latlon_share, bool server_mode,
								bool msg_save, bool use_GPS, uint32_t own_mmsi,
								int time_history, int cutoff, const AIS::Filter &f)
{
	ships.setLat(lat);
	ships.setLon(lon);
	ships.setShareLatLon(latlon_share);
	ships.setServerMode(server_mode);
	ships.setMsgSave(msg_save);
	ships.setUseGPS(use_GPS);
	ships.setOwnMMSI(own_mmsi);
	ships.setTimeHistory(time_history);
	ships.setFilter(f);
	hist_second.setCutoff(cutoff);
	hist_minute.setCutoff(cutoff);
	hist_hour.setCutoff(cutoff);
	hist_day.setCutoff(cutoff);
	counter.setCutOff(cutoff);
	counter_session.setCutOff(cutoff);
}

void ReceiverState::setup()
{
	ships.setup();
}

void ReceiverState::wireStreams()
{
	ships >> hist_day;
	ships >> hist_hour;
	ships >> hist_minute;
	ships >> hist_second;
	ships >> counter;
	ships >> counter_session;
}

void ReceiverState::clear()
{
	counter.Clear();
	counter_session.Clear();
	hist_second.Clear();
	hist_minute.Clear();
	hist_hour.Clear();
	hist_day.Clear();
}

void ReceiverState::reset()
{
	counter_session.Clear();
	hist_second.Clear();
	hist_minute.Clear();
	hist_hour.Clear();
	hist_day.Clear();
	ships.setup();
}

bool ReceiverState::save(std::ofstream &f)
{
	if (!counter.Save(f))
		return false;
	if (!hist_second.Save(f))
		return false;
	if (!hist_minute.Save(f))
		return false;
	if (!hist_hour.Save(f))
		return false;
	if (!hist_day.Save(f))
		return false;
	if (!ships.Save(f))
		return false;
	return true;
}

bool ReceiverState::load(std::ifstream &f)
{
	if (!counter.Load(f))
		return false;
	if (!hist_second.Load(f))
		return false;
	if (!hist_minute.Load(f))
		return false;
	if (!hist_hour.Load(f))
		return false;
	if (!hist_day.Load(f))
		return false;
	if (f.peek() != EOF)
		if (!ships.Load(f))
			return false;
	return true;
}

std::string ReceiverState::toHistoryJSON()
{
	std::string j = "{";
	j += "\"second\":" + hist_second.toJSON();
	j += ",\"minute\":" + hist_minute.toJSON();
	j += ",\"hour\":" + hist_hour.toJSON();
	j += ",\"day\":" + hist_day.toJSON();
	j += "}";
	return j;
}

std::string ReceiverState::toCountersJSON()
{
	std::string j;
	j += "\"total\":" + counter.toJSON() + ",";
	j += "\"session\":" + counter_session.toJSON() + ",";
	j += "\"last_day\":" + hist_day.lastStatToJSON() + ",";
	j += "\"last_hour\":" + hist_hour.lastStatToJSON() + ",";
	j += "\"last_minute\":" + hist_minute.lastStatToJSON() + ",";
	j += "\"msg_rate\":" + std::to_string(hist_second.getAverage()) + ",";
	j += "\"vessel_count\":" + std::to_string(ships.getCount()) + ",";
	j += "\"vessel_max\":" + std::to_string(ships.getMaxCount());
	return j;
}

void ReceiverState::setDevice(Device::Device *device)
{
	JSON::StringBuilder::stringify(device->getProduct(), product, false);
	JSON::StringBuilder::stringify(device->getVendor().empty() ? "-" : device->getVendor(), vendor, false);
	JSON::StringBuilder::stringify(device->getSerial().empty() ? "-" : device->getSerial(), serial, false);
	sample_rate = device->getRateDescription();

	if (serial == ".")
		label = "Console";
	else
	{
		label = device->getProduct();
		if (serial != "-")
			label += " " + serial;
	}
}

void ReceiverState::appendDevice(Device::Device *device, const std::string &newline)
{
	std::string prod, vnd, ser;
	JSON::StringBuilder::stringify(device->getProduct(), prod, false);
	JSON::StringBuilder::stringify(device->getVendor().empty() ? "-" : device->getVendor(), vnd, false);
	JSON::StringBuilder::stringify(device->getSerial().empty() ? "-" : device->getSerial(), ser, false);

	product += prod + newline;
	vendor += vnd + newline;
	serial += ser + newline;
	sample_rate += device->getRateDescription() + newline;
}

int WebViewer::parseReceiver(const std::string &query)
{
	auto pos = query.find("receiver=");
	if (pos == std::string::npos)
		return 0;
	try
	{
		return std::stoi(query.substr(pos + 9));
	}
	catch (...)
	{
		return 0;
	}
}

ReceiverState *WebViewer::getState(int idx)
{
	if (states.empty())
		return nullptr;
	if (idx < 0 || idx >= (int)states.size())
		return states[0].get();
	return states[idx].get();
}

void WebViewer::connect(const std::vector<std::unique_ptr<Receiver>> &receivers)
{
	const std::string newline = "<br>";

	bool multi = receivers.size() > 1 && !filter.hasIDFilter() && groups_in == 0xFFFFFFFFFFFFFFFF;

	states.push_back(std::unique_ptr<ReceiverState>(new ReceiverState()));
	states[0]->label = "All";

	for (int k = 0; k < (int)receivers.size(); k++)
	{
		Receiver &r = *receivers[k];
		ReceiverState *per = nullptr;

		if (multi)
		{
			states.push_back(std::unique_ptr<ReceiverState>(new ReceiverState()));
			per = states.back().get();
		}

		bool rec_details = false;
		for (int j = 0; j < r.Count(); j++)
		{
			if (r.Output(j).canConnect(groups_in))
			{
				if (!rec_details)
				{
					auto *device = r.getDeviceManager().getDevice();

					if (per)
						per->setDevice(device);

					states[0]->appendDevice(device, newline);
				}

				states[0]->model_name += r.Model(j)->getName() + newline;
				if (per)
					per->model_name += r.Model(j)->getName() + newline;

				states[0]->connectJSON(r.OutputJSON(j));
				if (per)
					per->connectJSON(r.OutputJSON(j));

				states[0]->connectGPS(r.OutputGPS(j));
				r.OutputADSB(j).Connect((StreamIn<Plane::ADSB> *)&planes);

				*r.getDeviceManager().getDevice() >> raw_counter;
			}
		}
	}

	for (auto &s : states)
		s->applyConfig(cfg_lat, cfg_lon, cfg_latlon_share, cfg_server_mode,
					   cfg_msg_save, cfg_use_GPS, cfg_own_mmsi,
					   cfg_time_history, cfg_cutoff, filter);

	if (states.size() > 1)
	{
		Info() << "Server: tracking " << states.size() - 1 << " receiver(s) with separate states:";
		for (int i = 0; i < (int)states.size(); i++)
			Info() << "  [" << i << "] " << states[i]->label;
	}
}

void WebViewer::connect(AIS::Model &m, Connection<JSON::JSON> &json, Device::Device &device)
{
	if (m.Output().out.canConnect(groups_in))
	{
		if (states.empty())
		{
			states.push_back(std::unique_ptr<ReceiverState>(new ReceiverState()));
			states[0]->label = "All";
			states[0]->applyConfig(cfg_lat, cfg_lon, cfg_latlon_share, cfg_server_mode,
								   cfg_msg_save, cfg_use_GPS, cfg_own_mmsi,
								   cfg_time_history, cfg_cutoff, filter);
		}

		states[0]->connectJSON(json);
		device >> raw_counter;

		states[0]->sample_rate = device.getRateDescription();
		JSON::StringBuilder::stringify(device.getProduct(), states[0]->product, false);
		JSON::StringBuilder::stringify(device.getVendor().empty() ? "-" : device.getVendor(), states[0]->vendor, false);
		JSON::StringBuilder::stringify(device.getSerial().empty() ? "-" : device.getSerial(), states[0]->serial, false);
		states[0]->model_name = m.getName();
	}
}

void WebViewer::Reset()
{
	for (auto &s : states)
	{
		s->reset();
	}
	raw_counter.Reset();
	time_start = time(nullptr);
}

void WebViewer::start()
{
	for (auto &s : states)
		s->setup();

	if (backup_filename.empty())
		Clear();
	else if (!Load())
	{
		Error() << "Statistics - cannot read file.";
		Clear();
	}

	if (realtime)
	{
		if (!states.empty())
			states[0]->connectSink(sse_streamer);
		sse_streamer.setSSE(this);
	}

	if (showlog)
	{
		logger.setSSE(this);
		logger.Start();
	}

	for (auto &s : states)
	{
		s->wireStreams();
	}

	if (supportPrometheus && !states.empty())
		states[0]->connectSink(dataPrometheus);

	if (firstport && lastport)
	{
		for (port = firstport; port <= lastport; port++)
			if (HTTPServer::start(port))
				break;

		if (port > lastport)
			throw std::runtime_error("Cannot open port in range [" + std::to_string(firstport) + "," + std::to_string(lastport) + "]");

		Info() << "HTML Server running at port " << std::to_string(port);
	}
	else
		throw std::runtime_error("HTML server ports not specified");

	time_start = time(nullptr);

	// Embed static receiver list into plugins.js
	if (states.size() > 1)
	{
		params += "const server_receivers = [";
		for (int i = 0; i < (int)states.size(); i++)
		{
			if (i)
				params += ",";
			params += "{\"idx\":" + std::to_string(i) + ",\"label\":";
			JSON::StringBuilder::stringify(states[i]->label, params);
			params += "}";
		}
		params += "];\n";
	}
	else
	{
		params += "const server_receivers = null;\n";
	}

	run = true;

	if (backup_interval > 0)
	{
		if (backup_filename.empty())
		{
			Warning() << "Server: backup of statistics requested without providing backup filename.";
		}
		else
		{
			backup_thread = std::thread(&WebViewer::BackupService, this);
			thread_running = true;
		}
	}
}

void WebViewer::stopThread()
{
	if (thread_running)
	{
		{
			std::lock_guard<std::mutex> lock(m);
			run = false;
		}
		cv.notify_all();

		if (backup_thread.joinable())
			backup_thread.join();

		thread_running = false;
	}
}

void WebViewer::close()
{

	stopThread();

	if (!backup_filename.empty() && !Save())
	{
		Error() << "Statistics - cannot write file: " << backup_filename;
	}
}

void WebViewer::Request(IO::TCPServerConnection &c, const std::string &response, bool gzip)
{

	std::string r;
	std::string a;

	std::string::size_type pos = response.find('?');

	if (pos != std::string::npos)
	{
		r = response.substr(0, pos);
		a = response.substr(pos + 1, response.length());
	}
	else
	{
		r = response;
	}

	if (r == "/")
		r = "/index.html";

	if (r == "/kml" && KML)
	{
		ReceiverState *s = getState(parseReceiver(a));
		std::string content = s ? s->getKML() : "";
		Response(c, "application/vnd.google-earth.kml+xml", content, use_zlib & gzip);
	}
	else if (r == "/metrics")
	{
		if (supportPrometheus)
		{
			std::string content = dataPrometheus.toPrometheus();
			Response(c, "text/plain", content, use_zlib & gzip);
			dataPrometheus.Reset();
		}
	}
	else if (r == "/api/stat.json" || r == "/stat.json")
	{
		ReceiverState *s = getState(parseReceiver(a));
		if (!s)
		{
			Response(c, "application/json", "{}", use_zlib & gzip);
			return;
		}

		std::string content;

		content += "{" + s->toCountersJSON() + ",";
		content += "\"tcp_clients\":" + std::to_string(numberOfClients()) + ",";
		content += "\"sharing\":" + std::string(commm_feed ? "true" : "false") + ",";
		if (cfg_latlon_share && cfg_lat != LAT_UNDEFINED && cfg_lon != LON_UNDEFINED)
			content += "\"sharing_link\":\"https://www.aiscatcher.org/?&zoom=10&lat=" + std::to_string(cfg_lat) + "&lon=" + std::to_string(cfg_lon) + "\",";
		else
			content += "\"sharing_link\":\"https://www.aiscatcher.org\",";

		content += "\"station\":" + station + ",";
		content += "\"station_link\":" + station_link + ",";
		content += "\"sample_rate\":\"" + s->sample_rate + "\",";
		content += "\"msg_rate\":" + std::to_string(s->getMsgRate()) + ",";
		content += "\"vessel_count\":" + std::to_string(s->getCount()) + ",";
		content += "\"vessel_max\":" + std::to_string(s->getMaxCount()) + ",";
		content += "\"product\":\"" + s->product + "\",";
		content += "\"vendor\":\"" + s->vendor + "\",";
		content += "\"serial\":\"" + s->serial + "\",";
		content += "\"model\":\"" + s->model_name + "\",";
		content += "\"build_date\":\"" + std::string(__DATE__) + "\",";
		content += "\"build_version\":\"" + std::string(VERSION) + "\",";
		content += "\"build_describe\":\"" + std::string(VERSION_DESCRIBE) + "\",";
		content += "\"run_time\":\"" + std::to_string((long int)time(nullptr) - (long int)time_start) + "\",";
		content += "\"memory\":" + std::to_string(Util::Helper::getMemoryConsumption()) + ",";
		content += "\"os\":" + os + ",";
		content += "\"hardware\":" + hardware;

		content += ",\"outputs\":[";
		bool first = true;
		if (msg_channels)
		{
			for (auto &o : *msg_channels)
			{
				if (!first)
					content += ",";
				content += o->getJSON();
				first = false;
			}
		}
		content += "],\"received\":" + std::to_string(raw_counter.received) + "}";

		Response(c, "application/json", content, use_zlib & gzip);
	}
	else if (r == "/api/ships.json" || r == "/ships.json")
	{
		ReceiverState *s = getState(parseReceiver(a));
		std::string content = s ? s->getShipsJSON() : "{}";
		Response(c, "application/json", content, use_zlib & gzip);
	}
	else if (r == "/api/ships_array.json")
	{
		ReceiverState *s = getState(parseReceiver(a));
		std::string content = s ? s->getShipsJSONcompact() : "{}";
		Response(c, "application/json", content, use_zlib & gzip);
	}
	else if (r == "/api/planes_array.json")
	{
		std::string content = planes.getCompactArray();
		Response(c, "application/json", content, use_zlib & gzip);
	}
	else if (r == "/api/ships_full.json")
	{
		ReceiverState *s = getState(parseReceiver(a));
		std::string content = s ? s->getShipsJSON(true) : "{}";
		Response(c, "application/json", content, use_zlib & gzip);
	}
	else if (r == "/api/sse" && realtime)
	{
		upgradeSSE(c, 1);
	}
	else if (r == "/api/signal" && realtime)
	{
		upgradeSSE(c, 2);
	}
	else if (r == "/api/log" && showlog)
	{
		IO::SSEConnection *s = upgradeSSE(c, 3);
		auto l = Logger::getInstance().getLastMessages(25);
		for (auto &m : l)
		{
			s->SendEvent("log", m.toJSON());
		}
	}
	else if (r == "/api/binmsgs.json")
	{
		ReceiverState *s = getState(parseReceiver(a));
		std::string content = s ? s->getBinaryMessagesJSON() : "[]";
		Response(c, "application/json", content, use_zlib & gzip);
	}
	else if (r == "/custom/plugins.js")
	{
		Response(c, "application/javascript", params + plugins + plugin_code + "}\nconst server_version = false;\nconst context = '" + js_context + "';\nconst aboutMDpresent = " + (aboutPresent ? "true" : "false") + ";\nconst communityFeed = " + (commm_feed ? "true" : "false") + ";\n", use_zlib & gzip);
	}
	else if (r == "/custom/config.css")
	{
		Response(c, "text/css", stylesheets, use_zlib & gzip);
	}
	else if (r == "/about.md")
	{
		Response(c, "text/markdown", about, use_zlib & gzip);
	}
	else if (r == "/api/path.json")
	{
		ReceiverState *s = getState(parseReceiver(a));
		std::stringstream ss(a);
		std::string mmsi_str;
		std::string content = "{";
		int count = 0;
		const int MAX_MMSI_COUNT = 100; // Limit number of MMSIs to prevent DoS

		while (std::getline(ss, mmsi_str, ','))
		{
			if (++count > MAX_MMSI_COUNT)
			{
				Error() << "Server - path MMSI count exceeds limit: " << MAX_MMSI_COUNT;
				break;
			}

			try
			{
				int mmsi = std::stoi(mmsi_str);
				if (mmsi >= 1 && mmsi <= 999999999)
				{
					if (content.length() > 1)
						content += ",";
					content += "\"" + std::to_string(mmsi) + "\":" + (s ? s->getPathJSON(mmsi) : "{}");
				}
			}
			catch (const std::invalid_argument &)
			{
				Error() << "Server - path MMSI invalid: " << mmsi_str;
			}
			catch (const std::out_of_range &)
			{
				Error() << "Server - path MMSI out of range: " << mmsi_str;
			}
		}
		content += "}";
		Response(c, "application/json", content, use_zlib & gzip);
	}
	else if (r == "/api/allpath.json")
	{
		ReceiverState *s = getState(parseReceiver(a));
		std::string content = s ? s->getAllPathJSON() : "{}";
		Response(c, "application/json", content, use_zlib & gzip);
	}
	else if (r == "/api/path.geojson")
	{
		std::stringstream ss(a);
		std::string mmsi_str;

		if (std::getline(ss, mmsi_str))
		{
			try
			{
				int mmsi = std::stoi(mmsi_str);
				if (mmsi >= 1 && mmsi <= 999999999)
				{
					ReceiverState *s = getState(parseReceiver(a));
					std::string content = s ? s->getPathGeoJSON(mmsi) : "{}";
					Response(c, "application/json", content, use_zlib & gzip);
				}
				else
				{
					Response(c, "application/json", "{\"error\":\"Invalid MMSI range\"}", use_zlib & gzip);
				}
			}
			catch (const std::invalid_argument &)
			{
				Error() << "Server - path GeoJSON MMSI invalid: " << mmsi_str;
				Response(c, "application/json", "{\"error\":\"Invalid MMSI format\"}", use_zlib & gzip);
			}
			catch (const std::out_of_range &)
			{
				Error() << "Server - path GeoJSON MMSI out of range: " << mmsi_str;
				Response(c, "application/json", "{\"error\":\"MMSI out of range\"}", use_zlib & gzip);
			}
		}
		else
		{
			Response(c, "application/json", "{\"error\":\"No MMSI provided\"}", use_zlib & gzip);
		}
	}
	else if (r == "/api/allpath.geojson" || (r == "/allpath.geojson" && GeoJSON))
	{
		ReceiverState *s = getState(parseReceiver(a));
		std::string content = s ? s->getAllPathGeoJSON() : "{}";
		Response(c, "application/json", content, use_zlib & gzip);
	}
	else if (r == "/geojson" && GeoJSON)
	{
		ReceiverState *s = getState(parseReceiver(a));
		std::string content = s ? s->getGeoJSON() : "{}";
		Response(c, "application/json", content, use_zlib & gzip);
	}
	else if (r == "/api/message")
	{
		int mmsi = -1;
		std::stringstream ss(a);
		if (ss >> mmsi && mmsi >= 1 && mmsi <= 999999999)
		{
			ReceiverState *s = getState(parseReceiver(a));
			std::string content = s ? s->getMessage(mmsi) : "";
			Response(c, "application/text", content, use_zlib & gzip);
		}
		else
			Response(c, "application/text", "Message not availaible");
	}
	else if (r == "/api/decode")
	{
		if (!showdecoder)
		{
			Response(c, "application/json", std::string("{\"error\":\"Decoder is disabled\"}"), use_zlib & gzip);
			return;
		}

		try
		{
			if (a.empty() || a.size() > 1024)
				throw std::runtime_error("Input size limit exceeded");

			std::string result = decodeNMEAtoJSON(a, true);

			if (result == "[]")
			{
				Response(c, "application/json", std::string("{\"error\":\"No valid AIS messages decoded\"}"), use_zlib & gzip);
			}
			else
			{
				Response(c, "application/json", result, use_zlib & gzip);
			}
		}
		catch (const std::exception &e)
		{
			// Security: Don't expose internal exception details
			Error() << "Decoder error: " << e.what();
			Response(c, "application/json", std::string("{\"error\":\"Decoding failed\"}"), use_zlib & gzip);
		}
	}
	else if (r == "/api/vessel")
	{
		std::stringstream ss(a);
		int mmsi;
		if (ss >> mmsi && mmsi >= 1 && mmsi <= 999999999)
		{
			ReceiverState *s = getState(parseReceiver(a));
			std::string content = s ? s->getShipJSON(mmsi) : "{}";
			Response(c, "application/text", content, use_zlib & gzip);
		}
		else
		{
			Response(c, "application/text", "Vessel not available");
		}
	}
	else if (r == "/api/history_full.json")
	{
		ReceiverState *s = getState(parseReceiver(a));
		if (!s)
		{
			Response(c, "application/json", "{}", use_zlib & gzip);
			return;
		}

		std::string content = s->toHistoryJSON() + "\n\n";

		Response(c, "application/json", content, use_zlib & gzip);
	}
	else if (r.substr(0, 6) == "/tiles")
	{
		int z, x, y;
		std::string layer;
		if (parseMBTilesURL(r, layer, z, x, y))
		{
			for (const auto &source : mapSources)
			{
				if (source->getLayerID() != layer)
					continue;

				if (source->isValidTile(z, x, y))
				{
					std::string contentType;
					const std::vector<unsigned char> &data = source->getTile(z, x, y, contentType);

					if (!data.empty())
					{
						Response(c, contentType, (char *)data.data(), data.size(), use_zlib & gzip, true);
						return;
					}
				}
			}
			Response(c, "text/plain", std::string("Tile not found"), false, true);
			return;
		}
		Response(c, "text/plain", std::string("Invalid Tile Request"), false, true);
		return;
	}
	else if (r.rfind("/", 0) == 0)
	{
		std::string filename = r.substr(1);

		auto it = WebDB::files.find(filename);
		if (it != WebDB::files.end())
		{
			const WebDB::FileData &file = it->second;
			ResponseRaw(c, file.mime_type, (char *)file.data, file.size, true, std::string(file.mime_type) != "text/html");
		}
		else
		{
			Error() << "File not found: " << filename << std::endl;
			HTTPServer::Request(c, r, false);
		}
	}
}

Setting &WebViewer::Set(std::string option, std::string arg)
{
	Util::Convert::toUpper(option);

	if (option == "PORT")
	{
		port_set = true;
		firstport = lastport = Util::Parse::Integer(arg, 1, 65535, option);
	}
	else if (option == "SERVER_MODE")
	{
		cfg_server_mode = Util::Parse::Switch(arg);
	}
	else if (option == "ZLIB")
	{
		use_zlib = Util::Parse::Switch(arg);
	}
	else if (option == "GROUPS_IN")
	{
		groups_in = Util::Parse::Integer(arg);
	}
	else if (option == "PORT_MIN")
	{
		port_set = true;
		firstport = Util::Parse::Integer(arg, 1, 65535, option);
		lastport = MAX(firstport, lastport);
	}
	else if (option == "PORT_MAX")
	{
		port_set = true;
		lastport = Util::Parse::Integer(arg, 1, 65535, option);
		firstport = MIN(firstport, lastport);
	}
	else if (option == "STATION")
	{
		station.clear();
		JSON::StringBuilder::stringify(arg, station);
	}
	else if (option == "STATION_LINK")
	{
		station_link.clear();
		JSON::StringBuilder::stringify(arg, station_link);
	}
	else if (option == "WEBCONTROL_HTTP")
	{
		plugins += "const webcontrol_http = '" + arg + "';\n";
	}
	else if (option == "LAT")
	{
		cfg_lat = Util::Parse::Float(arg);
		planes.setLat(cfg_lat);
	}
	else if (option == "CUTOFF")
	{
		cfg_cutoff = Util::Parse::Integer(arg, 0, 10000, option);
		dataPrometheus.setCutOff(cfg_cutoff);
	}
	else if (option == "SHARE_LOC")
	{
		cfg_latlon_share = Util::Parse::Switch(arg);
		plugins += "const param_share_loc=" + (cfg_latlon_share ? std::string("true;\n") : std::string("false;\n"));
	}
	else if (option == "IP_BIND")
	{
		setIP(arg);
	}
	else if (option == "CONTEXT")
	{
		js_context = arg;
	}
	else if (option == "MESSAGE" || option == "MSG")
	{
		cfg_msg_save = Util::Parse::Switch(arg);
		plugins += "const message_save=" + (cfg_msg_save ? std::string("true;\n") : std::string("false;\n"));
	}
	else if (option == "LON")
	{
		cfg_lon = Util::Parse::Float(arg);
		planes.setLon(cfg_lon);
	}
	else if (option == "USE_GPS")
	{
		cfg_use_GPS = Util::Parse::Switch(arg);
	}
	else if (option == "KML")
	{
		KML = Util::Parse::Switch(arg);
	}
	else if (option == "GEOJSON")
	{
		GeoJSON = Util::Parse::Switch(arg);
	}
	else if (option == "OWN_MMSI")
	{
		cfg_own_mmsi = Util::Parse::Integer(arg, 0, 999999999, option);
	}
	else if (option == "HISTORY")
	{
		cfg_time_history = Util::Parse::Integer(arg, 5, 12 * 3600, option);
	}
	else if (option == "FILE")
	{
		backup_filename = arg;
	}
	else if (option == "CDN")
	{
		Warning() << "CDN option is no longer supported - web libraries are now bundled.";
	}
	else if (option == "MBTILES")
	{
		addMBTilesSource(arg, false);
	}
	else if (option == "MBOVERLAY")
	{
		addMBTilesSource(arg, true);
	}
	else if (option == "FSTILES")
	{
		addFileSystemTilesSource(arg, false);
	}
	else if (option == "FSOVERLAY")
	{
		addFileSystemTilesSource(arg, true);
	}
	else if (option == "BACKUP")
	{
		backup_interval = Util::Parse::Integer(arg, 5, 2 * 24 * 60, option);
	}
	else if (option == "REALTIME")
	{
		realtime = Util::Parse::Switch(arg);
		if (realtime)
			plugins += "const realtime_enabled = true;\n";
		else
			plugins += "const realtime_enabled = false;\n";
	}
	else if (option == "LOG")
	{
		showlog = Util::Parse::Switch(arg);
		if (showlog)
			plugins += "const log_enabled = true;\n";
		else
			plugins += "const log_enabled = false;\n";
	}
	else if (option == "DECODER")
	{
		showdecoder = Util::Parse::Switch(arg);
		if (showdecoder)
			plugins += "const decoder_enabled = true;\n";
		else
			plugins += "const decoder_enabled = false;\n";

		sse_streamer.setObfuscate(!showdecoder);
	}
	else if (option == "PLUGIN")
	{
		addPlugin(arg);
	}
	else if (option == "STYLE")
	{
		Info() << "Server: adding plugin (CSS): " << arg;
		plugins += "console.log('css:" + arg + "');";
		plugins += "plugins += 'CSS: " + arg + "\\n';";

		stylesheets += "/* ================ */\n";
		stylesheets += "/* CSS plugin: " + arg + "*/\n";
		try
		{
			stylesheets += Util::Helper::readFile(arg) + "\n";
		}
		catch (const std::exception &e)
		{
			stylesheets += "/* FAILED */\r\n";
			Warning() << "Server: style plugin error - " << e.what();
			plugins += "server_message += \"Plugin error: " + std::string(e.what()) + "\\n\"\n";
		}
	}
	else if (option == "PLUGIN_DIR")
	{
		const std::vector<std::string> &files_js = Util::Helper::getFilesWithExtension(arg, ".pjs");
		for (auto f : files_js)
			Set("PLUGIN", f);

		const std::vector<std::string> &files_ss = Util::Helper::getFilesWithExtension(arg, ".pss");
		for (auto f : files_ss)
			Set("STYLE", f);

		if (!files_ss.size() && !files_js.size())
			Info() << "Server: no plugin files found in directory.";
	}
	else if (option == "ABOUT")
	{
		Info() << "Server: about context from " << arg;
		about = Util::Helper::readFile(arg);
		aboutPresent = true;
	}
	else if (option == "PROME")
	{
		supportPrometheus = Util::Parse::Switch(arg);
	}
	else if (option == "REUSE_PORT")
	{
		setReusePort(Util::Parse::Switch(arg));
	}
	else if (!filter.SetOption(option, arg))
	{
		throw std::runtime_error("unrecognized setting for HTML service: " + option + " " + arg);
	}
	else
	{
		raw_counter.setFilterOption(option, arg);
	}

	return *this;
}
