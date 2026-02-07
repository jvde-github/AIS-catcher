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
#include "JSON/JSONBuilder.h"

IO::OutputMessage *commm_feed = nullptr;

// Common MIME type constants
static const std::string MIME_JSON = "application/json";
static const std::string MIME_TEXT = "application/text";
static const std::string MIME_JAVASCRIPT = "application/javascript";
static const std::string MIME_CSS = "text/css";
static const std::string MIME_PLAIN = "text/plain";
static const std::string MIME_MARKDOWN = "text/markdown";
static const std::string MIME_OCTET_STREAM = "application/octet-stream";
static const std::string MIME_KML = "application/vnd.google-earth.kml+xml";
static const std::string MIME_SVG = "image/svg+xml";
static const std::string MIME_PNG = "image/png";

WebViewer::WebViewer()
{
params = "build_string = '" + std::string(VERSION_DESCRIBE) + "';\nbuild_version = '" + std::string(VERSION) + "';\ncontext='settings';\n\n";
plugin_code = "\n\nfunction loadPlugins() {\n";
plugins = "let plugins = '';\nlet server_message = '';\n";
os = Util::Helper::getOS();
hardware = Util::Helper::getHardware();

initializeRoutes();
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

void WebViewer::sendError(IO::TCPServerConnection &c, const std::string &message, bool gzip)
{
server.Response(c, MIME_JSON, "{\"error\":\"" + message + "\"}", use_zlib & gzip);
}

std::vector<std::string> WebViewer::parsePath(const std::string &url)
{
std::vector<std::string> path;
static const std::string pattern = "/";

const auto pos = url.find(pattern);
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
static const std::string pattern = "/tiles/";

layerID.clear();

const std::vector<std::string> segments = parsePath(url);

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

void WebViewer::initializeRoutes()
{
// Register all routes in the table for O(1) lookup
routeTable["/api/stat.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAPIStats(c, gzip); };
routeTable["/stat.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAPIStats(c, gzip); };
routeTable["/api/ships.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAPIShips(c, gzip); };
routeTable["/ships.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAPIShips(c, gzip); };
routeTable["/api/ships_array.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAPIShipsArray(c, gzip); };
routeTable["/api/planes_array.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAPIPlanesArray(c, gzip); };
routeTable["/sb"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleBinaryShips(c, gzip); };
routeTable["/api/ships_full.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAPIShipsFull(c, gzip); };
routeTable["/api/binmsgs.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAPIBinaryMessages(c, gzip); };
routeTable["/custom/plugins.js"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handlePluginsJS(c, gzip); };
routeTable["/custom/config.css"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleConfigCSS(c, gzip); };
routeTable["/about.md"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAboutMD(c, gzip); };
routeTable["/api/path.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAPIPath(c, args, gzip); };
routeTable["/api/allpath.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAPIAllPath(c, gzip); };
routeTable["/api/path.geojson"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAPIPathGeoJSON(c, args, gzip); };
routeTable["/api/allpath.geojson"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAPIAllPathGeoJSON(c, gzip); };
routeTable["/api/message"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAPIMessage(c, args, gzip); };
routeTable["/api/decode"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAPIDecode(c, args, gzip); };
routeTable["/api/vessel"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAPIVessel(c, args, gzip); };
routeTable["/api/history_full.json"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleHistoryFull(c, gzip); };

// Conditional routes - handlers check their enable flags internally
routeTable["/kml"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleKML(c, gzip); };
routeTable["/metrics"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleMetrics(c, gzip); };
routeTable["/api/sse"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAPISSE(c, gzip); };
routeTable["/api/signal"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAPISignal(c, gzip); };
routeTable["/api/log"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAPILog(c, gzip); };
routeTable["/geojson"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleGeoJSON(c, gzip); };
routeTable["/allpath.geojson"] = [this](IO::TCPServerConnection &c, const std::string &args, bool gzip)
{ handleAllPathGeoJSON(c, gzip); };
}

void WebViewer::connect(Receiver &r)
{
	bool rec_details = false;
	const std::string newline = "<br>";
	for (int j = 0; j < r.Count(); j++)
		if (r.Output(j).canConnect(groups_in))
		{
			if (!rec_details)
			{
				auto *device = r.getDeviceManager().getDevice();

				sample_rate += device->getRateDescription() + "<br>";

				product += device->getProduct() + newline;
				vendor += (device->getVendor().empty() ? "-" : device->getVendor()) + newline;
				serial += (device->getSerial().empty() ? "-" : device->getSerial()) + newline;

				rec_details = true;
			}
			model += r.Model(j)->getName() + newline;

			r.OutputJSON(j).Connect((StreamIn<JSON::JSON> *)&ships);
			r.OutputGPS(j).Connect((StreamIn<AIS::GPS> *)&ships);
			r.OutputADSB(j).Connect((StreamIn<Plane::ADSB> *)&planes);

			*r.getDeviceManager().getDevice() >> raw_counter;
		}
}

void WebViewer::connect(AIS::Model &m, Connection<JSON::JSON> &json, Device::Device &device)
{
	if (m.Output().out.canConnect(groups_in))
	{

		json.Connect((StreamIn<JSON::JSON> *)&ships);
		device >> raw_counter;

		sample_rate = device.getRateDescription();
		setDeviceDescription(device.getProduct(), device.getVendor().empty() ? "-" : device.getVendor(), device.getSerial().empty() ? "-" : device.getSerial());
		model = m.getName();
	}
}

void WebViewer::Reset()
{
	counter_session.Clear();
	hist_second.Clear();
	hist_minute.Clear();
	hist_hour.Clear();
	hist_day.Clear();

	raw_counter.Reset();
	ships.setup();
	time_start = time(nullptr);
}

void WebViewer::start()
{
	ships.setup();

	if (backup_filename.empty())
		Clear();
	else if (!Load())
	{
		Error() << "Statistics - cannot read file.";
		Clear();
	}

	if (realtime)
	{
		ships >> sse_streamer;
		sse_streamer.setSSE(&server);
	}

	if (showlog)
	{
		logger.setSSE(&server);
		logger.Start();
	}

	ships >> hist_day;
	ships >> hist_hour;
	ships >> hist_minute;
	ships >> hist_second;

	ships >> counter;
	ships >> counter_session;

	if (supportPrometheus)
		ships >> dataPrometheus;

	if (firstport && lastport)
	{
		for (port = firstport; port <= lastport; port++)
			if (server.start(port))
				break;

		if (port > lastport)
			throw std::runtime_error("Cannot open port in range [" + std::to_string(firstport) + "," + std::to_string(lastport) + "]");

		Info() << "HTML Server running at port " << std::to_string(port);
	}
	else
		throw std::runtime_error("HTML server ports not specified");

	// Set up request handler
	server.setRequestHandler([this](IO::TCPServerConnection &c, const std::string &r, bool gzip)
							 { handleRequest(c, r, gzip); });

	time_start = time(nullptr);

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
		Error() << "Statistics - cannot write file.";
	}
}

void WebViewer::handleRequest(IO::TCPServerConnection &c, const std::string &response, bool gzip)
{
	std::string path, args;
	std::string::size_type pos = response.find('?');

	if (pos != std::string::npos)
	{
		path = response.substr(0, pos);
		args = response.substr(pos + 1);
	}
	else
	{
		path = response;
	}

	// Root redirect
	if (path == "/")
	{
		path = cdn.empty() ? "/index.html" : "/index_local.html";
	}

	// Try router table (O(1) lookup)
	auto it = routeTable.find(path);
	if (it != routeTable.end())
	{
		it->second(c, args, gzip);
		return;
	}

	// Handle prefix-based routes
	if (!cdn.empty() && path.find("/cdn/") == 0)
	{
		handleCDNFile(c, path, gzip);
	}
	else if (path.substr(0, 6) == "/tiles")
	{
		handleTiles(c, path, gzip);
	}
	// Default: static file handler
	else if (path.rfind("/", 0) == 0)
	{
		handleStaticFile(c, path.substr(1), gzip);
	}
}

