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

IO::OutputMessage *comm_feed = nullptr;

// --- PluginManager ---

PluginManager::PluginManager()
{
	params = "build_string = '" + std::string(VERSION_DESCRIBE) + "';\nbuild_version = '" + std::string(VERSION) + "';\ncontext='settings';\n\n";
	plugin_code = "\n\nfunction loadPlugins() {\n";
	plugin_preamble = "let plugins = '';\nlet server_message = '';\n";
}

void PluginManager::setContext(const std::string &ctx)
{
	plugin_preamble += "context=" + JSON::StringBuilder::stringify(ctx) + ";\n";
}

void PluginManager::setWebControl(const std::string &url)
{
	plugin_preamble += "webcontrol_http = " + JSON::StringBuilder::stringify(url) + ";\n";
}

void PluginManager::setShareLoc(bool b)
{
	plugin_preamble += "param_share_loc=" + std::string(b ? "true" : "false") + ";\n";
}

void PluginManager::setMsgSave(bool b)
{
	plugin_preamble += "message_save=" + std::string(b ? "true" : "false") + ";\n";
}

void PluginManager::setRealtime(bool b)
{
	plugin_preamble += "realtime_enabled = " + std::string(b ? "true" : "false") + ";\n";
}

void PluginManager::setLog(bool b)
{
	plugin_preamble += "log_enabled = " + std::string(b ? "true" : "false") + ";\n";
}

void PluginManager::setDecoder(bool b)
{
	plugin_preamble += "decoder_enabled = " + std::string(b ? "true" : "false") + ";\n";
}

void PluginManager::setReceivers(const std::vector<std::unique_ptr<ReceiverTracker>> &states)
{
	if (states.size() > 1)
	{
		params += "var server_receivers = [";
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
		params += "var server_receivers = null;\n";
	}
}

void PluginManager::addPlugin(const std::string &arg)
{
	int version = 0;
	std::string author, description;
	std::string safe_arg = JSON::StringBuilder::stringify(arg);

	plugin_preamble += "console.log('plugin:' + " + safe_arg + ");";
	plugin_preamble += "plugins += 'JS: ' + " + safe_arg + " + '\\n';";
	plugin_preamble += "\n\n//=============\n//" + arg + "\n\n";
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
		plugin_code += "\ntry{\n" + s + "\n} catch (error) {\nshowDialog(\"Error in Plugin \" + " + safe_arg + ", \"Plugins contain error: \" + error + \"</br>Consider updating plugins or disabling them.\"); }\n";
	}
	catch (const std::exception &e)
	{
		plugin_preamble += "// FAILED\n";
		Warning() << "Server: Plugin \"" + arg + "\" ignored - JS plugin error : " << e.what();
		plugin_preamble += "server_message += 'Plugin error: ' + " + safe_arg + " + ': ' + " + JSON::StringBuilder::stringify(std::string(e.what())) + " + '\\n';\n";
	}
}

void PluginManager::addPluginCode(const std::string &code)
{
	plugin_code += code;
}

void PluginManager::addStyle(const std::string &arg)
{
	Info() << "Server: adding plugin (CSS): " << arg;
	std::string safe_arg = JSON::StringBuilder::stringify(arg);
	plugin_preamble += "console.log('css:' + " + safe_arg + ");";
	plugin_preamble += "plugins += 'CSS: ' + " + safe_arg + " + '\\n';";

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
		plugin_preamble += "server_message += 'Plugin error: ' + " + safe_arg + " + ': ' + " + JSON::StringBuilder::stringify(std::string(e.what())) + " + '\\n';\n";
	}
}

void PluginManager::addPluginDir(const std::string &dir)
{
	const std::vector<std::string> &files_js = Util::Helper::getFilesWithExtension(dir, ".pjs");
	for (const auto &f : files_js)
		addPlugin(f);

	const std::vector<std::string> &files_ss = Util::Helper::getFilesWithExtension(dir, ".pss");
	for (const auto &f : files_ss)
		addStyle(f);

	if (files_ss.empty() && files_js.empty())
		Info() << "Server: no plugin files found in directory.";
}

void PluginManager::setAbout(const std::string &path)
{
	Info() << "Server: about context from " << path;
	about = Util::Helper::readFile(path);
	aboutPresent = true;
}

std::string PluginManager::render(bool communityFeed) const
{
	return params + plugin_preamble + plugin_code + "}\nserver_version = false;\naboutMDpresent = " +
		   (aboutPresent ? "true" : "false") + ";\ncommunityFeed = " +
		   (communityFeed ? "true" : "false") + ";\n";
}

// --- SSEStreamer ---

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

				std::string::size_type end = nmea.rfind(',');
				if (end == std::string::npos)
					continue;

				std::string::size_type start = nmea.rfind(',', end - 1);
				if (start == std::string::npos)
					continue;

				std::string::size_type len = end - start - 1;

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

			std::string json = "{\"mmsi\":" + std::to_string(m->mmsi()) +
							   ",\"timestamp\":" + std::to_string(now) +
							   ",\"channel\":\"" + m->getChannel() +
							   "\",\"type\":" + std::to_string(m->type()) +
							   ",\"shipname\":" + JSON::StringBuilder::stringify(std::string(tag.shipname)) +
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
	os = JSON::StringBuilder::stringify(Util::Helper::getOS());
	hardware = JSON::StringBuilder::stringify(Util::Helper::getHardware());
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

	if (segments.size() != 5 || segments[0] != "tiles")
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
		pluginManager.addPluginCode(source->generatePluginCode(overlay));
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
		pluginManager.addPluginCode(source->generatePluginCode(overlay));
	}
	else
	{
		Error() << "Failed to load FileSystemTiles from: " << directoryPath;
	}
}

void WebViewer::Clear()
{
	if (states.empty())
		return;
	states[0]->clear();
}

// --- BackupManager ---

void BackupManager::run()
{
	Info() << "Server: starting backup service every " << interval << " minutes.";

	while (running)
	{
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait_for(lock, std::chrono::minutes(interval), [this] { return !running.load(); });

		if (running && !save())
			Error() << "Server: failed to write backup: " << filename;
	}

	Info() << "Server: stopping backup service.";
}

void BackupManager::start()
{
	if (interval <= 0 || !tracker)
		return;

	if (filename.empty())
	{
		Warning() << "Server: backup of statistics requested without providing backup filename.";
		return;
	}

	running = true;
	thread = std::thread(&BackupManager::run, this);
}

void BackupManager::stop()
{
	if (running.load())
	{
		running = false;
		cv.notify_all();
		if (thread.joinable())
			thread.join();
	}
}

bool BackupManager::save()
{
	if (filename.empty() || !tracker)
		return false;

	try
	{
		std::ofstream outfile(filename, std::ios::binary);

		if (!outfile.is_open())
		{
			Error() << "Server: cannot open backup file for writing: "
					<< filename << " (" << std::strerror(errno) << ")";
			return false;
		}

		outfile.exceptions(std::ios::failbit | std::ios::badbit);

		if (!tracker->save(outfile))
			return false;

		outfile.close();
	}
	catch (const std::ios_base::failure &e)
	{
		Error() << "Server: write error on " << filename << " (" << std::strerror(errno) << ")";
		return false;
	}
	catch (const std::exception &e)
	{
		Error() << e.what();
		return false;
	}
	catch (...)
	{
		Error() << "Server: unknown error writing " << filename;
		return false;
	}
	return true;
}

bool BackupManager::load()
{
	if (filename.empty() || !tracker)
		return false;

	Info() << "Server: reading statistics from " << filename;
	try
	{
		std::ifstream infile(filename, std::ios::binary);

		if (!infile.is_open())
		{
			Warning() << "Server: cannot open backup file for reading: "
					  << filename << " (" << std::strerror(errno) << ")";
			return false;
		}

		if (!tracker->load(infile))
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

void ReceiverTracker::applyConfig(const TrackingConfig &cfg, const AIS::Filter &f)
{
	ships.setLat(cfg.lat);
	ships.setLon(cfg.lon);
	ships.setShareLatLon(cfg.latlon_share);
	ships.setServerMode(cfg.server_mode);
	ships.setMsgSave(cfg.msg_save);
	ships.setUseGPS(cfg.use_GPS);
	ships.setOwnMMSI(cfg.own_mmsi);
	ships.setTimeHistory(cfg.time_history);
	ships.setFilter(f);

	if (cfg.cutoff > 0)
	{
		hist_second.setCutoff(cfg.cutoff);
		hist_minute.setCutoff(cfg.cutoff);
		hist_hour.setCutoff(cfg.cutoff);
		hist_day.setCutoff(cfg.cutoff);
		counter.setCutOff(cfg.cutoff);
		counter_session.setCutOff(cfg.cutoff);
	}
}

void ReceiverTracker::setup()
{
	ships.setup();
}

void ReceiverTracker::wireStreams()
{
	ships >> hist_day;
	ships >> hist_hour;
	ships >> hist_minute;
	ships >> hist_second;
	ships >> counter;
	ships >> counter_session;
}

void ReceiverTracker::clear()
{
	counter.Clear();
	counter_session.Clear();
	hist_second.Clear();
	hist_minute.Clear();
	hist_hour.Clear();
	hist_day.Clear();
}

void ReceiverTracker::reset()
{
	counter_session.Clear();
	hist_second.Clear();
	hist_minute.Clear();
	hist_hour.Clear();
	hist_day.Clear();
	ships.setup();
}

bool ReceiverTracker::save(std::ofstream &f)
{
	return counter.Save(f) && hist_second.Save(f) && hist_minute.Save(f) && hist_hour.Save(f) && hist_day.Save(f) && ships.Save(f);
}

bool ReceiverTracker::load(std::ifstream &f)
{
	return counter.Load(f) && hist_second.Load(f) && hist_minute.Load(f) && hist_hour.Load(f) && hist_day.Load(f) && (f.peek() == EOF || ships.Load(f));
}

std::string ReceiverTracker::toHistoryJSON()
{
	std::string j = "{";
	j += "\"second\":" + hist_second.toJSON();
	j += ",\"minute\":" + hist_minute.toJSON();
	j += ",\"hour\":" + hist_hour.toJSON();
	j += ",\"day\":" + hist_day.toJSON();
	j += "}";
	return j;
}

std::string ReceiverTracker::toCountersJSON()
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

void ReceiverTracker::setDevice(Device::Device *device)
{
	product = JSON::StringBuilder::stringify(device->getProduct(), false);
	vendor = JSON::StringBuilder::stringify(device->getVendor().empty() ? "-" : device->getVendor(), false);
	serial = JSON::StringBuilder::stringify(device->getSerial().empty() ? "-" : device->getSerial(), false);
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

void ReceiverTracker::appendDevice(Device::Device *device, const std::string &newline)
{
	product += JSON::StringBuilder::stringify(device->getProduct(), false) + newline;
	vendor += JSON::StringBuilder::stringify(device->getVendor().empty() ? "-" : device->getVendor(), false) + newline;
	serial += JSON::StringBuilder::stringify(device->getSerial().empty() ? "-" : device->getSerial(), false) + newline;
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

std::time_t WebViewer::parseSinceParam(const std::string &query)
{
	auto pos = query.find("since=");
	if (pos == std::string::npos)
		return 0;
	try
	{
		return (std::time_t)std::stoll(query.substr(pos + 6));
	}
	catch (...)
	{
		return 0;
	}
}

ReceiverTracker *WebViewer::getState(int idx)
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

	states.push_back(std::unique_ptr<ReceiverTracker>(new ReceiverTracker()));
	states[0]->label = "All";

	for (int k = 0; k < (int)receivers.size(); k++)
	{
		Receiver &r = *receivers[k];
		ReceiverTracker *per = nullptr;

		if (multi)
		{
			states.push_back(std::unique_ptr<ReceiverTracker>(new ReceiverTracker()));
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
				rec_details = true;
				}

				states[0]->model_name += r.Model(j)->getName() + newline;
				if (per)
					per->model_name += r.Model(j)->getName() + newline;

				states[0]->connectJSON(r.OutputJSON(j));
				if (per)
					per->connectJSON(r.OutputJSON(j));

				states[0]->connectGPS(r.OutputGPS(j));
				if (per)
					per->connectGPS(r.OutputGPS(j));
				r.OutputADSB(j).Connect((StreamIn<Plane::ADSB> *)&planes);

				*r.getDeviceManager().getDevice() >> raw_counter;
			}
		}
	}

	for (auto &s : states)
		s->applyConfig(tracking, filter);

}

void WebViewer::connect(AIS::Model &m, Connection<JSON::JSON> &json, Device::Device &device)
{
	if (m.Output().out.canConnect(groups_in))
	{
		if (states.empty())
		{
			states.push_back(std::unique_ptr<ReceiverTracker>(new ReceiverTracker()));
			states[0]->label = "All";
			states[0]->applyConfig(tracking, filter);
		}

		states[0]->connectJSON(json);
		device >> raw_counter;

		states[0]->sample_rate = device.getRateDescription();
		states[0]->product = JSON::StringBuilder::stringify(device.getProduct(), false);
		states[0]->vendor = JSON::StringBuilder::stringify(device.getVendor().empty() ? "-" : device.getVendor(), false);
		states[0]->serial = JSON::StringBuilder::stringify(device.getSerial().empty() ? "-" : device.getSerial(), false);
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

	backup.setTracker(states[0].get());

	if (backup.getFilename().empty())
		Clear();
	else if (!backup.load())
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
		int port;
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

	pluginManager.setReceivers(states);

	run = true;

	backup.start();
}

void WebViewer::close()
{
	run = false;
	backup.stop();

	if (!backup.getFilename().empty() && !backup.save())
	{
		Error() << "Statistics - cannot write file: " << backup.getFilename();
	}
}

// --- Route handler functions ---

int WebViewer::parseMMSI(const std::string &query)
{
	try
	{
		int mmsi = std::stoi(query);
		return (mmsi >= 1 && mmsi <= 999999999) ? mmsi : -1;
	}
	catch (...)
	{
		return -1;
	}
}

std::string WebViewer::buildStatJSON(ReceiverTracker *s)
{
	std::string content;
	content.reserve(2048);

	content += "{" + s->toCountersJSON() + ",";
	content += "\"tcp_clients\":" + std::to_string(numberOfClients()) + ",";
	content += "\"sharing\":" + std::string(comm_feed ? "true" : "false") + ",";
	if (tracking.latlon_share && tracking.lat != LAT_UNDEFINED && tracking.lon != LON_UNDEFINED)
		content += "\"sharing_link\":\"https://www.aiscatcher.org/?&zoom=10&lat=" + std::to_string(tracking.lat) + "&lon=" + std::to_string(tracking.lon) + "\",";
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

	return content;
}

std::string WebViewer::buildMultiPathJSON(ReceiverTracker *s, const std::string &query)
{
	std::stringstream ss(query);
	std::string mmsi_str;
	std::string content = "{";
	int count = 0;
	const int MAX_MMSI_COUNT = 100;

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
	return content;
}

// --- Route table ---

const WebViewer::Route WebViewer::routes[] = {
	// JSON API routes (application/json)
	{"/api/ships.json", nullptr, "application/json",
		[](WebViewer *, ReceiverTracker *s, const std::string &) { return s ? s->getShipsJSON() : std::string("{}"); }},
	{"/ships.json", nullptr, "application/json",
		[](WebViewer *, ReceiverTracker *s, const std::string &) { return s ? s->getShipsJSON() : std::string("{}"); }},
	{"/api/ships_full.json", nullptr, "application/json",
		[](WebViewer *, ReceiverTracker *s, const std::string &) { return s ? s->getShipsJSON(true) : std::string("{}"); }},
	{"/api/ships_array.json", nullptr, "application/json",
		[](WebViewer *, ReceiverTracker *s, const std::string &) { return s ? s->getShipsJSONcompact() : std::string("{}"); }},
	{"/api/planes_array.json", nullptr, "application/json",
		[](WebViewer *w, ReceiverTracker *, const std::string &) { return w->planes.getCompactArray(); }},
	{"/api/binmsgs.json", nullptr, "application/json",
		[](WebViewer *, ReceiverTracker *s, const std::string &) { return s ? s->getBinaryMessagesJSON() : std::string("[]"); }},
	{"/api/history_full.json", nullptr, "application/json",
		[](WebViewer *, ReceiverTracker *s, const std::string &) { return s ? s->toHistoryJSON() : std::string("{}"); }},
	{"/api/stat.json", nullptr, "application/json",
		[](WebViewer *w, ReceiverTracker *s, const std::string &) { return s ? w->buildStatJSON(s) : std::string("{}"); }},
	{"/stat.json", nullptr, "application/json",
		[](WebViewer *w, ReceiverTracker *s, const std::string &) { return s ? w->buildStatJSON(s) : std::string("{}"); }},
	{"/api/path.json", nullptr, "application/json",
		[](WebViewer *w, ReceiverTracker *s, const std::string &a) { return w->buildMultiPathJSON(s, a); }},
	{"/api/allpath.json", nullptr, "application/json",
		[](WebViewer *w, ReceiverTracker *s, const std::string &a) {
			if (!s) return std::string("{}");
			std::time_t since = w->parseSinceParam(a);
			return since > 0 ? s->getAllPathJSONSince(since) : s->getAllPathJSON();
		}},
	{"/api/path.geojson", nullptr, "application/json",
		[](WebViewer *, ReceiverTracker *s, const std::string &a) {
			if (!s) return std::string("{}");
			int mmsi = parseMMSI(a);
			return mmsi > 0 ? s->getPathGeoJSON(mmsi) : std::string("{}");
		}},
	{"/api/allpath.geojson", nullptr, "application/json",
		[](WebViewer *, ReceiverTracker *s, const std::string &) { return s ? s->getAllPathGeoJSON() : std::string("{}"); }},
	{"/api/message", nullptr, "application/json",
		[](WebViewer *, ReceiverTracker *s, const std::string &a) {
			if (!s) return std::string("{\"error\":\"No data available\"}");
			int mmsi = parseMMSI(a);
			if (mmsi <= 0) return std::string("{\"error\":\"Invalid MMSI\"}");
			std::string msg = s->getMessage(mmsi);
			return msg.empty() ? std::string("{\"error\":\"Message not found\"}") : msg;
		}},
	{"/api/vessel", nullptr, "application/json",
		[](WebViewer *, ReceiverTracker *s, const std::string &a) {
			if (!s) return std::string("{\"error\":\"No data available\"}");
			int mmsi = parseMMSI(a);
			if (mmsi <= 0) return std::string("{\"error\":\"Invalid MMSI\"}");
			std::string vessel = s->getShipJSON(mmsi);
			return vessel == "{}" ? std::string("{\"error\":\"Vessel not found\"}") : vessel;
		}},
	{"/api/decode", &WebViewer::showdecoder, "application/json",
		[](WebViewer *, ReceiverTracker *, const std::string &a) {
			try {
				if (a.empty() || a.size() > 1024) return std::string("{\"error\":\"Input size limit exceeded\"}");
				std::string result = decodeNMEAtoJSON(a, true);
				return result == "[]" ? std::string("{\"error\":\"No valid AIS messages decoded\"}") : result;
			} catch (const std::exception &e) {
				Error() << "Decoder error: " << e.what();
				return std::string("{\"error\":\"Decoding failed\"}");
			}
		}},

	// Conditional GeoJSON/KML routes
	{"/geojson", &WebViewer::GeoJSON, "application/json",
		[](WebViewer *, ReceiverTracker *s, const std::string &) { return s ? s->getGeoJSON() : std::string("{}"); }},
	{"/allpath.geojson", &WebViewer::GeoJSON, "application/json",
		[](WebViewer *, ReceiverTracker *s, const std::string &) { return s ? s->getAllPathGeoJSON() : std::string("{}"); }},
	{"/kml", &WebViewer::KML, "application/vnd.google-earth.kml+xml",
		[](WebViewer *, ReceiverTracker *s, const std::string &) { return s ? s->getKML() : std::string(); }},

	// Prometheus metrics
	{"/metrics", &WebViewer::supportPrometheus, "text/plain",
		[](WebViewer *w, ReceiverTracker *, const std::string &) {
			std::string r = w->dataPrometheus.toPrometheus();
			w->dataPrometheus.Reset();
			return r;
		}},

	// Frontend assets
	{"/custom/plugins.js", nullptr, "application/javascript",
		[](WebViewer *w, ReceiverTracker *, const std::string &) { return w->pluginManager.render(comm_feed != nullptr); }},
	{"/custom/config.css", nullptr, "text/css",
		[](WebViewer *w, ReceiverTracker *, const std::string &) { return w->pluginManager.getStylesheets(); }},
	{"/about.md", nullptr, "text/markdown",
		[](WebViewer *w, ReceiverTracker *, const std::string &) { return w->pluginManager.getAbout(); }},

	{nullptr, nullptr, nullptr, nullptr}
};

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

	// Route table lookup
	for (const Route *rt = routes; rt->path; ++rt)
	{
		if (r != rt->path)
			continue;
		if (rt->flag && !(this->*(rt->flag)))
			continue;

		ReceiverTracker *s = getState(parseReceiver(a));
		Response(c, rt->content_type, rt->handler(this, s, a), use_zlib & gzip);
		return;
	}

	// SSE routes (upgrade connection, not a normal response)
	if (r == "/api/sse" && realtime)
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
		for (auto &m : Logger::getInstance().getLastMessages(25))
			s->SendEvent("log", m.toJSON());
	}
	// Prefix-match routes
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
	// Static files
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
		tracking.server_mode = Util::Parse::Switch(arg);
	}
	else if (option == "ZLIB")
	{
		use_zlib = Util::Parse::Switch(arg);
	}
	else if (option == "GROUPS_IN")
	{
		groups_in = Util::Parse::Integer(arg);
	}
	else if (option == "ZONE")
	{
		Util::Parse::Split(arg, ',', zones);
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
		station = JSON::StringBuilder::stringify(arg);
	}
	else if (option == "STATION_LINK")
	{
		station_link = JSON::StringBuilder::stringify(arg);
	}
	else if (option == "WEBCONTROL_HTTP")
	{
		pluginManager.setWebControl(arg);
	}
	else if (option == "LAT")
	{
		tracking.lat = Util::Parse::Float(arg);
		planes.setLat(tracking.lat);
	}
	else if (option == "CUTOFF")
	{
		tracking.cutoff = Util::Parse::Integer(arg, 0, 10000, option);
		dataPrometheus.setCutOff(tracking.cutoff);
	}
	else if (option == "SHARE_LOC")
	{
		tracking.latlon_share = Util::Parse::Switch(arg);
		pluginManager.setShareLoc(tracking.latlon_share);
	}
	else if (option == "IP_BIND")
	{
		setIP(arg);
	}
	else if (option == "CONTEXT")
	{
		pluginManager.setContext(arg);
	}
	else if (option == "MESSAGE" || option == "MSG")
	{
		tracking.msg_save = Util::Parse::Switch(arg);
		pluginManager.setMsgSave(tracking.msg_save);
	}
	else if (option == "LON")
	{
		tracking.lon = Util::Parse::Float(arg);
		planes.setLon(tracking.lon);
	}
	else if (option == "USE_GPS")
	{
		tracking.use_GPS = Util::Parse::Switch(arg);
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
		tracking.own_mmsi = Util::Parse::Integer(arg, 0, 999999999, option);
	}
	else if (option == "HISTORY")
	{
		tracking.time_history = Util::Parse::Integer(arg, 5, 12 * 3600, option);
	}
	else if (option == "FILE")
	{
		backup.setFilename(arg);
	}
	else if (option == "CDN")
	{
		Warning() << "CDN option is no longer supported — web libraries are now bundled. Ignoring.";
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
		backup.setInterval(Util::Parse::Integer(arg, 5, 2 * 24 * 60, option));
	}
	else if (option == "REALTIME")
	{
		realtime = Util::Parse::Switch(arg);
		pluginManager.setRealtime(realtime);
	}
	else if (option == "LOG")
	{
		showlog = Util::Parse::Switch(arg);
		pluginManager.setLog(showlog);
	}
	else if (option == "DECODER")
	{
		showdecoder = Util::Parse::Switch(arg);
		pluginManager.setDecoder(showdecoder);
		sse_streamer.setObfuscate(!showdecoder);
	}
	else if (option == "PLUGIN")
	{
		pluginManager.addPlugin(arg);
	}
	else if (option == "STYLE")
	{
		pluginManager.addStyle(arg);
	}
	else if (option == "PLUGIN_DIR")
	{
		pluginManager.addPluginDir(arg);
	}
	else if (option == "ABOUT")
	{
		pluginManager.setAbout(arg);
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
