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

#include <cstdio>
#include <cerrno>

// --- PluginManager ---

PluginManager::PluginManager()
	: plugin_code("\n\nfunction loadPlugins() {\n")
{
	config.build_version = VERSION;
	config.build_describe = VERSION_DESCRIBE;
}

void PluginManager::setContext(const std::string &ctx) { config.context = ctx; }
void PluginManager::setWebControl(const std::string &url) { config.webcontrol_http = url; }
void PluginManager::setShareLoc(bool b) { config.share_location = b; }
void PluginManager::setMsgSave(bool b) { config.save_messages = b; }
void PluginManager::setRealtime(bool b) { config.realtime = b; }
void PluginManager::setLog(bool b) { config.log_enabled = b; }
void PluginManager::setDecoder(bool b) { config.decoder = b; }
void PluginManager::setSharing(bool sharing, bool sharing_uuid)
{
	config.sharing = sharing;
	config.sharing_uuid = sharing_uuid;
}

void PluginManager::setStation(const std::string &name)
{
	// Caller passes an already-JSON-quoted string ("\"MyStation\""); strip
	// quotes for storage so the JSON writer at render time can re-quote.
	if (name.length() >= 2 && name.front() == '"' && name.back() == '"')
		config.station = name.substr(1, name.length() - 2);
	else
		config.station = name;
}

void PluginManager::setReceivers(const std::vector<std::unique_ptr<ReceiverTracker>> &states)
{
	config.receivers.clear();
	if (states.size() > 1)
		for (int i = 0; i < (int)states.size(); i++)
			config.receivers.emplace_back(i, states[i]->label);
}

void PluginManager::addPlugin(const std::string &arg)
{
	int version = 0;
	std::string author, description;

	plugin_code += "\n//=============\n//" + arg + "\n\n";
	try
	{
		std::string s = Util::Helper::readFile(arg);

		JSON::Parser parser(JSON_DICT_SETTING);
		std::string firstline = s.substr(0, s.find('\n'));
		if (firstline.length() > 2 && firstline[0] == '/' && firstline[1] == '/')
			firstline = firstline.substr(2);
		else
			throw std::runtime_error("Plugin does not start with // followed by JSON description.");

		JSON::Document j = parser.parse(firstline);

		for (const auto &p : j.getMembers())
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
			case AIS::KEY_SETTING_DESC:
				description = p.Get().getString();
				break;
			default:
				throw std::runtime_error("Unknown key in plugin JSON field");
				break;
			}
		}
		Info() << "Adding plugin (" + arg + "). Description: \"" << description << "\", Author: \"" << author << "\", version " << version;
		// v3: pre-CSP (inline-string handlers, no longer run under strict CSP).
		// v4: function-callback contract for addShipcardItem etc.
		// v5: ES-module script.js — overrides go through AISCatcher hooks.
		if (version < 3 || version > 5)
			throw std::runtime_error("Version not supported, expected 3..5, got " + std::to_string(version));
		if (version == 3)
			Warning() << "Plugin \"" << arg << "\" declares version 3 (pre-CSP). It may render but inline-string handlers will not run under strict CSP. Consider updating to version 5.";
		config.plugins_loaded.emplace_back(arg, version);
		std::string safe_arg = JSON::Writer::escape(arg);
		plugin_code += "\ntry{\n" + s + "\n} catch (error) {\nshowDialog(\"Error in Plugin \" + " + safe_arg + ", \"Plugins contain error: \" + error + \"</br>Consider updating plugins or disabling them.\"); }\n";
	}
	catch (const std::exception &e)
	{
		Warning() << "Server: Plugin \"" + arg + "\" ignored - JS plugin error : " << e.what();
		config.plugin_errors.push_back(arg + ": " + e.what());
	}
}

void PluginManager::addPluginCode(const std::string &code)
{
	plugin_code += code;
}

void PluginManager::addStyle(const std::string &arg)
{
	Info() << "Server: adding plugin (CSS): " << arg;
	config.plugins_loaded.emplace_back("CSS: " + arg, 0);

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
		config.plugin_errors.push_back(arg + ": " + e.what());
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

std::string PluginManager::render() const
{
	std::string out;

	// 1. Emit the structured config blob. Single source of truth for all
	//    server-side state the frontend needs at startup.
	out += "window.__SERVER_CONFIG__ = ";
	{
		JSON::Writer w(out);
		w.beginObject()
			.key("build")
			.beginObject()
			.kv("version", config.build_version)
			.kv("describe", config.build_describe)
			.endObject()
			.kv("context", config.context)
			.kv("station", config.station)
			.kv("webcontrol_http", config.webcontrol_http)
			.key("features")
			.beginObject()
			.kv("share_location", config.share_location)
			.kv("save_messages", config.save_messages)
			.kv("realtime", config.realtime)
			.kv("log", config.log_enabled)
			.kv("decoder", config.decoder)
			.kv("about_md", aboutPresent)
			.kv("sharing", config.sharing)
			.kv("sharing_uuid", config.sharing_uuid)
			.endObject()
			.key("receivers")
			.beginArray();
		for (const auto &r : config.receivers)
			w.beginObject().kv("idx", r.first).kv("label", r.second).endObject();
		w.endArray()
			.key("plugins")
			.beginObject()
			.key("loaded")
			.beginArray();
		for (const auto &p : config.plugins_loaded)
			w.beginObject().kv("name", p.first).kv("version", p.second).endObject();
		w.endArray().key("errors").beginArray();
		for (const auto &e : config.plugin_errors)
			w.val(e);
		w.endArray().endObject().endObject();
		w.finish();
	}
	out += ";\n";

	// 2. Plugin function body and trailing
	out += plugin_code + "}\n";

	return out;
}

// --- SSEStreamer ---

void SSEStreamer::Receive(const JSON::JSON *data, int len, TAG &tag)
{
	if (!server)
		return;

	std::time_t now = std::time(nullptr);

	for (int j = 0; j < len; j++)
	{
		AIS::Message *m = (AIS::Message *)data[j].binary;
		char channel = m->getChannel();

		if (!m->sentences().empty())
		{
			std::string json;
			JSON::Writer w(json);
			w.beginObject()
				.kv("mmsi", m->mmsi())
				.kv("timestamp", (long long)now)
				.kv("channel", {&channel, 1})
				.kv("type", m->type())
				.kv("shipname", tag.shipname)
				.key("nmea").beginArray();

			for (const auto &s : m->sentences())
			{
				std::string nmea = s;

				std::string::size_type end = nmea.rfind(',');
				if (end == std::string::npos)
					continue;

				std::string::size_type start = nmea.rfind(',', end - 1);
				if (start == std::string::npos)
					continue;

				std::string::size_type field_len = end - start - 1;

				if (field_len == 0)
					continue;

				if (obfuscate)
				{
					for (int i = 0; i < 3; i++)
					{
						idx = (idx + 1) % field_len;
						nmea[start + 1 + idx] = '*';
					}
				}

				w.val(nmea);
			}
			w.endArray();
			w.endObject();
			w.finish();
			server->sendSSE(1, "nmea", json);
		}

		if (tag.lat != 0 && tag.lon != 0)
		{
			std::string json;
			JSON::Writer w(json);
			w.beginObject()
				.kv("mmsi", m->mmsi())
				.kv("channel", {&channel, 1})
				.kv("lat", tag.lat)
				.kv("lon", tag.lon)
				.endObject();
			w.finish();
			server->sendSSE(2, "nmea", json);
		}
	}
}

WebViewer::WebViewer() : Setting("WebViewer"),
	os(JSON::Writer::escape(Util::Helper::getOS())),
	hardware(JSON::Writer::escape(Util::Helper::getHardware()))
{
	states.push_back(std::unique_ptr<ReceiverTracker>(new ReceiverTracker("All")));
}

std::string WebViewer::decodeNMEAtoJSON(const std::string &nmea_input, bool enhanced)
{
	// Decoder class with full pipeline: NMEA -> Message -> JSON -> Array
	class NMEADecoder : public StreamIn<JSON::JSON>
	{
	public:
		AIS::NMEA nmea_decoder;
		AIS::JSONAIS json_converter;
		JSON::Serializer *builder;
		JSON::Writer *writer;
		size_t message_count;
		const size_t MAX_OUTPUT_SIZE;

		NMEADecoder(JSON::Serializer *b, JSON::Writer *w) : builder(b), writer(w), message_count(0), MAX_OUTPUT_SIZE(1024 * 1024)
		{
			nmea_decoder >> json_converter;
			json_converter.out.Connect(this);
		}

		void Receive(const JSON::JSON *data, int len, TAG &tag) override
		{
			for (int i = 0; i < len; i++)
			{
				if (writer->written() > MAX_OUTPUT_SIZE)
					throw std::runtime_error("Output size limit exceeded");

				builder->stringify(data[i], *writer);
				message_count++;
			}
		}
	};

	std::string result;
	result.reserve(4096);
	JSON::Writer w(result);
	w.beginArray();

	JSON::Serializer builder(JSON_DICT_FULL);
	builder.setStringifyEnhanced(enhanced);
	NMEADecoder decoder(&builder, &w);

	std::string input = nmea_input + "\n";
	RAW raw = {Format::TXT, (void *)input.c_str(), (int)input.length()};
	TAG tag;
	decoder.nmea_decoder.Receive(&raw, 1, tag);

	w.endArray();
	w.finish();
	return result;
}

// Raw NMEA never contains '%' or '+', so decoding is safe whether or not
// the client percent-encoded its input.
static std::string urlDecode(const std::string &in)
{
	std::string out;
	out.reserve(in.size());

	for (std::size_t i = 0; i < in.size(); i++)
	{
		if (in[i] == '+')
			out += ' ';
		else if (in[i] == '%' && i + 2 < in.size() && Util::Convert::isHexDigit(in[i + 1]) && Util::Convert::isHexDigit(in[i + 2]))
		{
			out += (char)((Util::Convert::hexDigitValue(in[i + 1]) << 4) | Util::Convert::hexDigitValue(in[i + 2]));
			i += 2;
		}
		else
			out += in[i];
	}
	return out;
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
	states[0]->clear();
}

// --- BackupManager ---

void BackupManager::run()
{
	Info() << "Server: starting backup service every " << interval << " minutes.";

	while (running)
	{
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait_for(lock, std::chrono::minutes(interval), [this]
					{ return !running.load(); });

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
		{
			// must hold mtx so the store cannot slip between the run thread's
			// predicate check and its wait, which would stall join() a full interval
			std::lock_guard<std::mutex> lock(mtx);
			running = false;
		}
		cv.notify_all();
		if (thread.joinable())
			thread.join();
	}
}

bool BackupManager::save()
{
	if (filename.empty() || !tracker)
		return false;

	const std::string tmp = filename + ".tmp";

	try
	{
		std::ofstream outfile(tmp, std::ios::binary | std::ios::trunc);

		if (!outfile.is_open())
		{
			Error() << "Server: cannot open backup file for writing: "
					<< tmp << " (" << std::strerror(errno) << ")";
			return false;
		}

		outfile.exceptions(std::ios::failbit | std::ios::badbit);

		if (!tracker->save(outfile))
		{
			outfile.close();
			std::remove(tmp.c_str());
			return false;
		}

		outfile.close();
	}
	catch (const std::ios_base::failure &e)
	{
		Error() << "Server: write error on " << tmp << " (" << std::strerror(errno) << ")";
		std::remove(tmp.c_str());
		return false;
	}
	catch (const std::exception &e)
	{
		Error() << e.what();
		std::remove(tmp.c_str());
		return false;
	}
	catch (...)
	{
		Error() << "Server: unknown error writing " << tmp;
		std::remove(tmp.c_str());
		return false;
	}

#ifdef _WIN32
	std::remove(filename.c_str());
#endif
	if (std::rename(tmp.c_str(), filename.c_str()) != 0)
	{
		Error() << "Server: cannot rename " << tmp << " -> " << filename
				<< " (" << std::strerror(errno) << ")";
		std::remove(tmp.c_str());
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
	if (!counter.Load(f) || !hist_second.Load(f) || !hist_minute.Load(f) || !hist_hour.Load(f) || !hist_day.Load(f))
		return false;

	if (f.peek() != EOF && !ships.Load(f))
		Warning() << "Backup: ship positions could not be restored from backup (format change?).";

	return true;
}

void ReceiverTracker::writeHistoryJSON(JSON::Writer &w)
{
	w.beginObject();
	w.key("second");
	hist_second.writeJSON(w);
	w.key("minute");
	hist_minute.writeJSON(w);
	w.key("hour");
	hist_hour.writeJSON(w);
	w.key("day");
	hist_day.writeJSON(w);
	w.endObject();
}

void ReceiverTracker::writeCountersJSON(JSON::Writer &w)
{
	w.key("total");
	counter.writeJSON(w);
	w.key("session");
	counter_session.writeJSON(w);
	w.key("last_day");
	hist_day.writeLastStatJSON(w);
	w.key("last_hour");
	hist_hour.writeLastStatJSON(w);
	w.key("last_minute");
	hist_minute.writeLastStatJSON(w);
	w.kv("msg_rate", hist_second.getAverage());
	w.kv("vessel_count", ships.getCount());
	w.kv("vessel_max", ships.getMaxCount());
}

std::string ReceiverTracker::toHistoryJSON()
{
	std::string s;
	JSON::Writer w(s);
	writeHistoryJSON(w);
	w.finish();
	return s;
}

std::string ReceiverTracker::toCountersJSON()
{
	std::string s;
	JSON::Writer w(s);
	writeCountersJSON(w);
	w.finish();
	return s;
}

void ReceiverTracker::writeSummary(std::ostream &out)
{
	auto tidy = [](std::string s) -> std::string {
		size_t pos;
		while ((pos = s.find("<br>")) != std::string::npos)  s.replace(pos, 4, ", ");
		while ((pos = s.find("<br/>")) != std::string::npos) s.replace(pos, 5, ", ");
		while (!s.empty() && (s.back() == ' ' || s.back() == ',' ||
		                       s.back() == '\n' || s.back() == '\r'))
			s.pop_back();
		while (!s.empty() && (s.front() == ' ' || s.front() == ','))
			s.erase(s.begin());
		return s;
	};

	out << "=== state: " << (label.empty() ? "(unnamed)" : label) << " ===\n";

	std::string p = tidy(product), v = tidy(vendor), sn = tidy(serial), sr = tidy(sample_rate);
	if (!p.empty())
	{
		out << "  device:   " << p;
		bool has_extra = (!v.empty() && v != "-") || (!sn.empty() && sn != "-") || !sr.empty();
		if (has_extra)
		{
			out << " [";
			bool first = true;
			auto sep = [&]() { if (!first) out << ", "; first = false; };
			if (!v.empty() && v != "-")  { sep(); out << v; }
			if (!sn.empty() && sn != "-") { sep(); out << "sn=" << sn; }
			if (!sr.empty())              { sep(); out << "rate=" << sr; }
			out << "]";
		}
		out << "\n";
	}
	std::string mn = tidy(model_name);
	if (!mn.empty())
		out << "  model:    " << mn << "\n";

	out << "  vessels:  " << ships.getCount()
	    << "   msg/s: " << hist_second.getAverage() << "\n";
	counter_session.print(out, "  ");
}

void ReceiverTracker::setDevice(Device::Device *device)
{
	product = device->getProduct();
	vendor = device->getVendor().empty() ? "-" : device->getVendor();
	serial = device->getSerial().empty() ? "-" : device->getSerial();
	sample_rate = device->getRateDescription();

	if (serial == ".")
		label = "Console";
	else
	{
		label = product;
		if (serial != "-")
			label += " " + serial;
	}
}

void ReceiverTracker::appendDevice(Device::Device *device, const std::string &newline)
{
	product += device->getProduct() + newline;
	vendor += (device->getVendor().empty() ? "-" : device->getVendor()) + newline;
	serial += (device->getSerial().empty() ? "-" : device->getSerial()) + newline;
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
	if (idx < 0 || idx >= (int)states.size())
		return states[0].get();
	return states[idx].get();
}

void WebViewer::connect(const std::vector<std::unique_ptr<Receiver>> &receivers)
{
	const std::string newline = "<br>";

	bool multi = receivers.size() > 1 && !filter.hasIDFilter() && groups_in == 0xFFFFFFFFFFFFFFFF;

	// re-entrant for managed mode where the viewer outlives engine runs:
	// device/model descriptions are rebuilt from scratch on every connect
	states[0]->product.clear();
	states[0]->vendor.clear();
	states[0]->serial.clear();
	states[0]->sample_rate.clear();
	states[0]->model_name.clear();

	std::size_t first_new = states.size();

	for (int k = 0; k < (int)receivers.size(); k++)
	{
		Receiver &r = *receivers[k];
		ReceiverTracker *per = nullptr;

		bool rec_details = false;
		for (int j = 0; j < r.Count(); j++)
		{
			if (r.Output(j).canConnect(groups_in))
			{
				if (!rec_details)
				{
					auto *device = r.getDeviceManager().getDevice();

					if (multi)
					{
						states.push_back(std::unique_ptr<ReceiverTracker>(new ReceiverTracker()));
						per = states.back().get();
						per->setDevice(device);
					}

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

	// trackers added after start() (managed-mode engine restart) still need
	// their internal streams wired
	if (run)
	{
		for (std::size_t i = first_new; i < states.size(); i++)
		{
			states[i]->setup();
			states[i]->wireStreams();
		}
		pluginManager.setReceivers(states);
	}

	Debug() << "Mutex: WebViewer sinks self-lock (DB/PlaneDB), raw_counter atomic (" << receivers.size() << " receivers)";

	raw_counter.setFilter(filter);
	engine_connected = true;
}

// Managed mode: unwire the per-run engine while the server keeps running.
// The aggregate tracker (states[0]) stays, so ship history, statistics and
// live SSE connections survive engine restarts.
void WebViewer::disconnectEngine()
{
	engine_connected = false;
	msg_channels = nullptr;
	setCommFeed(nullptr);

	if (states.size() > 1)
	{
		states.erase(states.begin() + 1, states.end());
		pluginManager.setReceivers(states);
	}
}

void WebViewer::setDeviceDescription(const std::string &product, const std::string &vendor, const std::string &serial)
{
	pending_product = product;
	pending_vendor = vendor;
	pending_serial = serial;

	if (!product.empty())
		states[0]->product = product;
	if (!vendor.empty())
		states[0]->vendor = vendor;
	if (!serial.empty())
		states[0]->serial = serial;
}

void WebViewer::connect(AIS::Model &model, Connection<JSON::JSON> &json, Device::Device &device)
{
	states[0]->setDevice(&device);
	states[0]->label = "All";
	states[0]->model_name = model.getName();

	// Android supplies USB product/vendor/serial out-of-band via setDeviceDescription().
	if (!pending_product.empty())
		states[0]->product = pending_product;
	if (!pending_vendor.empty())
		states[0]->vendor = pending_vendor;
	if (!pending_serial.empty())
		states[0]->serial = pending_serial;

	states[0]->connectJSON(json);
	device >> raw_counter;

	states[0]->applyConfig(tracking, filter);
	raw_counter.setFilter(filter);
	engine_connected = true;
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

	pluginManager.setSharing(comm_feed != nullptr,
							  comm_feed && comm_feed->hasUUID());

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

	if (supportPrometheus)
		states[0]->connectSink(dataPrometheus);

	if (firstport && lastport)
	{
		int port;
		for (port = firstport; port <= lastport; port++)
			if (HTTPServer::start(port))
				break;

		if (port > lastport)
			throw std::runtime_error("Cannot open port in range [" + std::to_string(firstport) + "," + std::to_string(lastport) + "]");

		bound_port = port;
	}
	else if (port_set)
	{
		// port 0: OS assigns a free port, actual port in listening_port
		if (!HTTPServer::start(0))
			throw std::runtime_error("Cannot open OS-assigned port");

		bound_port = listening_port;
	}
	else
		throw std::runtime_error("HTML server ports not specified");

	Info() << "HTML Server running at port " << std::to_string(bound_port);

	time_start = time(nullptr);

	pluginManager.setReceivers(states);

	run = true;

	backup.start();
}

void WebViewer::close()
{
	stopThread();
	logger.Stop();

	run = false;
	backup.stop();

	if (!backup.getFilename().empty() && !backup.save())
	{
		Error() << "Statistics - cannot write file: " << backup.getFilename();
	}

	if (stats_on_close)
	{
		std::ostringstream ss;
		ss << "\n";
		for (auto &s : states)
		{
			s->writeSummary(ss);
			ss << "\n";
		}
		Info() << ss.str();
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
	JSON::Writer w(content);

	w.beginObject();
	s->writeCountersJSON(w);
	w.kv("tcp_clients", numberOfClients());
	w.kv("sharing", comm_feed != nullptr);
	w.kv("sharing_uuid", comm_feed != nullptr && comm_feed->hasUUID());
	w.kv("engine_running", engine_connected);
	if (tracking.latlon_share && tracking.lat != LAT_UNDEFINED && tracking.lon != LON_UNDEFINED)
	{
		std::string link = "https://www.aiscatcher.org/?&zoom=10&lat=" + std::to_string(tracking.lat) + "&lon=" + std::to_string(tracking.lon);
		w.kv("sharing_link", link);
	}
	else
	{
		w.kv("sharing_link", "https://www.aiscatcher.org");
	}

	w.kv_raw("station", station);
	w.kv_raw("station_link", station_link);
	w.kv("sample_rate", s->sample_rate);
	w.kv("msg_rate", s->getMsgRate());
	w.kv("vessel_count", s->getCount());
	w.kv("vessel_max", s->getMaxCount());
	w.kv("product", s->product);
	w.kv("vendor", s->vendor);
	w.kv("serial", s->serial);
	w.kv("model", s->model_name);
	w.kv("build_date", __DATE__);
	w.kv("build_version", VERSION);
	w.kv("build_describe", VERSION_DESCRIBE);
	w.kv("run_time", std::to_string((long int)time(nullptr) - (long int)time_start));
	w.kv("memory", (unsigned long long)Util::Helper::getMemoryConsumption());
	w.kv_raw("os", os);
	w.kv_raw("hardware", hardware);

	w.key("outputs").beginArray();
	if (msg_channels)
	{
		for (auto &o : *msg_channels)
			o->writeJSON(w);
	}
	w.endArray();
	w.kv("received", (unsigned long long)raw_counter.received());
	w.endObject();

	w.finish();
	return content;
}

std::string WebViewer::buildMultiPathJSON(ReceiverTracker *s, const std::string &query)
{
	std::stringstream ss(query);
	std::string mmsi_str;
	std::string content;
	JSON::Writer w(content);
	w.beginObject();
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
				char keybuf[12];
				int n = snprintf(keybuf, sizeof(keybuf), "%d", mmsi);
				w.key(keybuf, n).raw_val(s ? s->getPathJSON(mmsi) : std::string("{}"));
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
	w.endObject();
	w.finish();
	return content;
}

// --- Route table ---

const WebViewer::Route WebViewer::routes[] = {
	// JSON API routes (application/json)
	{"/api/ships.json", nullptr, "application/json",
	 [](WebViewer *, ReceiverTracker *s, const std::string &)
	 { return s ? s->getShipsJSON() : std::string("{}"); }, true},
	{"/ships.json", nullptr, "application/json",
	 [](WebViewer *, ReceiverTracker *s, const std::string &)
	 { return s ? s->getShipsJSON() : std::string("{}"); }, true},
	{"/api/ships_full.json", nullptr, "application/json",
	 [](WebViewer *, ReceiverTracker *s, const std::string &)
	 { return s ? s->getShipsJSON(true) : std::string("{}"); }, true},
	{"/api/ships_array.json", nullptr, "application/json",
	 [](WebViewer *w, ReceiverTracker *s, const std::string &a)
	 { std::time_t since = w->parseSinceParam(a);
	   return s ? s->getShipsJSONcompact(since) : std::string("{}"); }, true},
	{"/api/planes_array.json", nullptr, "application/json",
	 [](WebViewer *w, ReceiverTracker *, const std::string &a)
	 { std::time_t since = w->parseSinceParam(a);
	   return w->planes.getCompactArray(false, since); }, true},
	{"/api/binmsgs.json", nullptr, "application/json",
	 [](WebViewer *w, ReceiverTracker *s, const std::string &a)
	 { std::time_t since = w->parseSinceParam(a);
	   return s ? s->getBinaryMessagesJSON(since) : std::string("{}"); }, true},
	{"/api/history_full.json", nullptr, "application/json",
	 [](WebViewer *, ReceiverTracker *s, const std::string &)
	 { return s ? s->toHistoryJSON() : std::string("{}"); }, true},
	{"/api/stat.json", nullptr, "application/json",
	 [](WebViewer *w, ReceiverTracker *s, const std::string &)
	 { return s ? w->buildStatJSON(s) : std::string("{}"); }, true},
	{"/stat.json", nullptr, "application/json",
	 [](WebViewer *w, ReceiverTracker *s, const std::string &)
	 { return s ? w->buildStatJSON(s) : std::string("{}"); }, true},
	{"/api/path.json", nullptr, "application/json",
	 [](WebViewer *w, ReceiverTracker *s, const std::string &a)
	 { return w->buildMultiPathJSON(s, a); }, true},
	{"/api/allpath.json", nullptr, "application/json",
	 [](WebViewer *w, ReceiverTracker *s, const std::string &a)
	 {
		 if (!s)
			 return std::string("{}");
		 std::time_t since = w->parseSinceParam(a);
		 return since > 0 ? s->getAllPathJSONSince(since) : s->getAllPathJSON();
	 }, true},
	{"/api/path.geojson", nullptr, "application/json",
	 [](WebViewer *, ReceiverTracker *s, const std::string &a)
	 {
		 if (!s)
			 return std::string("{}");
		 int mmsi = parseMMSI(a);
		 return mmsi > 0 ? s->getPathGeoJSON(mmsi) : std::string("{}");
	 }, true},
	{"/api/allpath.geojson", nullptr, "application/json",
	 [](WebViewer *, ReceiverTracker *s, const std::string &)
	 { return s ? s->getAllPathGeoJSON() : std::string("{}"); }, true},
	{"/api/message", nullptr, "application/json",
	 [](WebViewer *, ReceiverTracker *s, const std::string &a)
	 {
		 if (!s)
			 return std::string("{\"error\":\"No data available\"}");
		 int mmsi = parseMMSI(a);
		 if (mmsi <= 0)
			 return std::string("{\"error\":\"Invalid MMSI\"}");
		 std::string msg = s->getMessage(mmsi);
		 return msg.empty() ? std::string("{\"error\":\"Message not found\"}") : msg;
	 }, true},
	{"/api/vessel", nullptr, "application/json",
	 [](WebViewer *, ReceiverTracker *s, const std::string &a)
	 {
		 if (!s)
			 return std::string("{\"error\":\"No data available\"}");
		 int mmsi = parseMMSI(a);
		 if (mmsi <= 0)
			 return std::string("{\"error\":\"Invalid MMSI\"}");
		 std::string vessel = s->getShipJSON(mmsi);
		 return vessel == "{}" ? std::string("{\"error\":\"Vessel not found\"}") : vessel;
	 }, true},
	{"/api/decode", &WebViewer::showdecoder, "application/json",
	 [](WebViewer *, ReceiverTracker *, const std::string &a)
	 {
		 try
		 {
			 if (a.empty() || a.size() > 1024)
				 return std::string("{\"error\":\"Input size limit exceeded\"}");
			 std::string result = decodeNMEAtoJSON(urlDecode(a), true);
			 return result == "[]" ? std::string("{\"error\":\"No valid AIS messages decoded\"}") : result;
		 }
		 catch (const std::exception &e)
		 {
			 Error() << "Decoder error: " << e.what();
			 return std::string("{\"error\":\"Decoding failed\"}");
		 }
	 }, true},

	// Conditional GeoJSON/KML routes
	{"/geojson", &WebViewer::GeoJSON, "application/json",
	 [](WebViewer *, ReceiverTracker *s, const std::string &)
	 { return s ? s->getGeoJSON() : std::string("{}"); }, true},
	{"/allpath.geojson", &WebViewer::GeoJSON, "application/json",
	 [](WebViewer *, ReceiverTracker *s, const std::string &)
	 { return s ? s->getAllPathGeoJSON() : std::string("{}"); }, true},
	{"/kml", &WebViewer::KML, "application/vnd.google-earth.kml+xml",
	 [](WebViewer *, ReceiverTracker *s, const std::string &)
	 { return s ? s->getKML() : std::string(); }, true},

	// Prometheus metrics
	{"/metrics", &WebViewer::supportPrometheus, "text/plain",
	 [](WebViewer *w, ReceiverTracker *, const std::string &)
	 { return w->dataPrometheus.toPrometheus(); }, true},

	// Frontend assets
	{"/custom/plugins.js", nullptr, "application/javascript",
	 [](WebViewer *w, ReceiverTracker *, const std::string &)
	 { return w->pluginManager.render(); }, false},
	{"/custom/config.css", nullptr, "text/css",
	 [](WebViewer *w, ReceiverTracker *, const std::string &)
	 { return w->pluginManager.getStylesheets(); }, false},
	{"/about.md", nullptr, "text/markdown",
	 [](WebViewer *w, ReceiverTracker *, const std::string &)
	 { return w->pluginManager.getAbout(); }, false},

	{nullptr, nullptr, nullptr, nullptr, false}};

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
		Response(c, rt->content_type, rt->handler(this, s, a), use_zlib && gzip, false, rt->cors);
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
		if (s)
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
						Response(c, contentType, (char *)data.data(), data.size(), use_zlib && gzip, true);
						return;
					}
				}
			}
			Response(c, "text/plain", std::string("Tile not found"), false, false, false, 404);
			return;
		}
		Response(c, "text/plain", std::string("Invalid Tile Request"), false, false, false, 400);
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
			// 404 silently — browsers probe /.well-known/, favicon variants,
			// /robots.txt etc. and would flood the log.
			HTTPServer::Request(c, r, false);
		}
	}
}

Setting &WebViewer::SetKey(AIS::Keys key, const std::string &arg)
{
	switch (key)
	{
	case AIS::KEY_SETTING_PORT:
		port_set = true;
		firstport = lastport = Util::Parse::Integer(arg, 1, 65535);
		break;
	case AIS::KEY_SETTING_SERVER_MODE:
		tracking.server_mode = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_ZLIB:
		use_zlib = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_GROUPS_IN:
		groups_in = Util::Parse::Integer(arg);
		break;
	case AIS::KEY_SETTING_ZONE:
		Util::Parse::Split(arg, ',', zones);
		break;
	case AIS::KEY_SETTING_PORT_MIN:
		port_set = true;
		firstport = Util::Parse::Integer(arg, 1, 65535);
		lastport = MAX(firstport, lastport);
		break;
	case AIS::KEY_SETTING_PORT_MAX:
		port_set = true;
		lastport = Util::Parse::Integer(arg, 1, 65535);
		firstport = MIN(firstport, lastport);
		break;
	case AIS::KEY_SETTING_STATION:
		station = JSON::Writer::escape(arg);
		pluginManager.setStation(station);
		break;
	case AIS::KEY_SETTING_STATS_ON_CLOSE:
		stats_on_close = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_STATION_LINK:
		station_link = JSON::Writer::escape(arg);
		break;
	case AIS::KEY_SETTING_WEBCONTROL_HTTP:
		pluginManager.setWebControl(arg);
		break;
	case AIS::KEY_SETTING_FRAME_ANCESTORS:
		setFrameAncestors(arg);
		break;
	case AIS::KEY_SETTING_LAT:
		tracking.lat = Util::Parse::Float(arg);
		planes.setLat(tracking.lat);
		break;
	case AIS::KEY_SETTING_CUTOFF:
		tracking.cutoff = Util::Parse::Integer(arg, 0, 10000);
		dataPrometheus.setCutOff(tracking.cutoff);
		break;
	case AIS::KEY_SETTING_SHARE_LOC:
		tracking.latlon_share = Util::Parse::Switch(arg);
		pluginManager.setShareLoc(tracking.latlon_share);
		break;
	case AIS::KEY_SETTING_IP_BIND:
		setIP(arg);
		break;
	case AIS::KEY_SETTING_CONTEXT:
		pluginManager.setContext(arg);
		break;
	case AIS::KEY_SETTING_MESSAGE:
	case AIS::KEY_SETTING_MSG:
		tracking.msg_save = Util::Parse::Switch(arg);
		pluginManager.setMsgSave(tracking.msg_save);
		break;
	case AIS::KEY_SETTING_LON:
		tracking.lon = Util::Parse::Float(arg);
		planes.setLon(tracking.lon);
		break;
	case AIS::KEY_SETTING_USE_GPS:
		tracking.use_GPS = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_KML:
		KML = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_GEOJSON:
		GeoJSON = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_OWN_MMSI:
		tracking.own_mmsi = Util::Parse::Integer(arg, 0, 999999999);
		break;
	case AIS::KEY_SETTING_HISTORY:
		tracking.time_history = Util::Parse::Integer(arg, 5, 12 * 3600);
		break;
	case AIS::KEY_SETTING_FILE:
		backup.setFilename(arg);
		break;
	case AIS::KEY_SETTING_CDN:
		Warning() << "CDN option is no longer supported — web libraries are now bundled. Ignoring.";
		break;
	case AIS::KEY_SETTING_MBTILES:
		addMBTilesSource(arg, false);
		break;
	case AIS::KEY_SETTING_MBOVERLAY:
		addMBTilesSource(arg, true);
		break;
	case AIS::KEY_SETTING_FSTILES:
		addFileSystemTilesSource(arg, false);
		break;
	case AIS::KEY_SETTING_FSOVERLAY:
		addFileSystemTilesSource(arg, true);
		break;
	case AIS::KEY_SETTING_BACKUP:
		backup.setInterval(Util::Parse::Integer(arg, 5, 2 * 24 * 60));
		break;
	case AIS::KEY_SETTING_REALTIME:
		realtime = Util::Parse::Switch(arg);
		pluginManager.setRealtime(realtime);
		break;
	case AIS::KEY_SETTING_LOG:
		showlog = Util::Parse::Switch(arg);
		pluginManager.setLog(showlog);
		break;
	case AIS::KEY_SETTING_DECODER:
		showdecoder = Util::Parse::Switch(arg);
		pluginManager.setDecoder(showdecoder);
		sse_streamer.setObfuscate(!showdecoder);
		break;
	case AIS::KEY_SETTING_PLUGIN:
		pluginManager.addPlugin(arg);
		break;
	case AIS::KEY_SETTING_STYLE:
		pluginManager.addStyle(arg);
		break;
	case AIS::KEY_SETTING_PLUGIN_DIR:
		pluginManager.addPluginDir(arg);
		break;
	case AIS::KEY_SETTING_ABOUT:
		pluginManager.setAbout(arg);
		break;
	case AIS::KEY_SETTING_PROME:
		supportPrometheus = Util::Parse::Switch(arg);
		break;
	case AIS::KEY_SETTING_REUSE_PORT:
		setReusePort(Util::Parse::Switch(arg));
		break;
	default:
		if (!filter.SetOptionKey(key, arg))
			throw std::runtime_error(std::string("unrecognized setting for HTML service: ") + AIS::KeyMap[key][JSON_DICT_SETTING] + " " + arg);
		break;
	}

	return *this;
}
