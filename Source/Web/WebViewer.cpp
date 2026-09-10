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
#include "PlaceCatalogue.h"
#include "Device/Device.h"
#include "Helper.h"
#include "JSONAIS.h"
#include "Logger.h"
#include "NMEA.h"
#include "WebDB.h"

// per poll; a viewer that has been away does not need the whole ring

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

// --- SSEStreamer ---

void SSEStreamer::Receive(const JSON::JSON *data, int len, TAG &tag) {
  if (!server)
    return;

  const bool want_nmea = server->sseSubscribed(IO::SSE::NMEA);
  const bool want_signal = server->sseSubscribed(IO::SSE::SIGNAL);
  if (!want_nmea && !want_signal)
    return;

  std::time_t now = std::time(nullptr);

  for (int j = 0; j < len; j++) {
    AIS::Message *m = (AIS::Message *)data[j].binary;
    char channel = m->getChannel();

    if (want_nmea && !m->sentences().empty()) {
      std::string json;
      JSON::Writer w(json);
      w.beginObject()
          .kv("mmsi", m->mmsi())
          .kv("timestamp", (long long)now)
          .kv("channel", {&channel, 1})
          .kv("type", m->type())
          .kv("shipname", tag.shipname)
          .key("nmea")
          .beginArray();

      for (const auto &s : m->sentences()) {
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

        if (obfuscate) {
          unsigned pos = idx;
          for (int i = 0; i < 3; i++) {
            pos = (pos + 1) % field_len;
            nmea[start + 1 + pos] = '*';
          }
          idx = pos;
        }

        w.val(nmea);
      }
      w.endArray();
      w.endObject();
      w.finish();
      server->sendSSE(IO::SSE::NMEA, "nmea", json);
    }

    if (want_signal && isValidCoord(tag.lat, tag.lon)) {
      std::string json;
      JSON::Writer w(json);
      w.beginObject()
          .kv("mmsi", m->mmsi())
          .kv("channel", {&channel, 1})
          .kv("lat", tag.lat)
          .kv("lon", tag.lon)
          .endObject();
      w.finish();
      server->sendSSE(IO::SSE::SIGNAL, "nmea", json);
    }
  }
}

WebViewer::WebViewer()
    : Setting("WebViewer"), os(JSON::Writer::escape(Util::Helper::getOS())),
      hardware(JSON::Writer::escape(Util::Helper::getHardware())) {
  states.push_back(
      std::unique_ptr<ReceiverTracker>(new ReceiverTracker("All")));
}

std::string WebViewer::decodeNMEAtoJSON(const std::string &nmea_input,
                                        bool enhanced) {
  // Decoder class with full pipeline: NMEA -> Message -> JSON -> Array
  class NMEADecoder : public StreamIn<JSON::JSON> {
    enum { MAX_OUTPUT_SIZE = 1024 * 1024 };

  public:
    AIS::NMEA nmea_decoder;
    AIS::JSONAIS json_converter;
    JSON::Serializer *builder;
    JSON::Writer *writer;

    NMEADecoder(JSON::Serializer *b, JSON::Writer *w) : builder(b), writer(w) {
      nmea_decoder >> json_converter;
      json_converter.out.Connect(this);
    }

    void Receive(const JSON::JSON *data, int len, TAG &tag) override {
      for (int i = 0; i < len; i++) {
        if (writer->written() > MAX_OUTPUT_SIZE)
          throw std::runtime_error("Output size limit exceeded");

        builder->stringify(data[i], *writer);
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
static std::string urlDecode(const std::string &in) {
  std::string out;
  out.reserve(in.size());

  for (std::size_t i = 0; i < in.size(); i++) {
    if (in[i] == '+')
      out += ' ';
    else if (in[i] == '%' && i + 2 < in.size() &&
             Util::Convert::isHexDigit(in[i + 1]) &&
             Util::Convert::isHexDigit(in[i + 2])) {
      out += (char)((Util::Convert::hexDigitValue(in[i + 1]) << 4) |
                    Util::Convert::hexDigitValue(in[i + 2]));
      i += 2;
    } else
      out += in[i];
  }
  return out;
}

static long long toInt(const std::string &s, long long def = 0) {
  char *end = nullptr;
  long long v = std::strtoll(s.c_str(), &end, 10);
  return end == s.c_str() ? def : v;
}

// "/tiles/<layer>/<z>/<x>/<y>"
bool WebViewer::parseMBTilesURL(const std::string &url, std::string &layerID,
                                int &z, int &x, int &y) {
  layerID.clear();

  std::vector<std::string> segment;
  std::stringstream ss(url);
  std::string s;

  while (std::getline(ss, s, '/'))
    if (!s.empty())
      segment.push_back(s);

  if (segment.size() != 5 || segment[0] != "tiles")
    return false;

  z = (int)toInt(segment[2], -1);
  x = (int)toInt(segment[3], -1);
  y = (int)toInt(segment[4], -1);

  if (z < 0 || x < 0 || y < 0)
    return false;

  layerID = segment[1];
  return true;
}

long long WebViewer::queryInt(const std::string &query, const char *name) {
  return toInt(IO::HTTPRequest::queryParam(query, name));
}

ReceiverTracker *WebViewer::getState(int idx) {
  if (!settings.split || idx < 0 || idx >= (int)states.size())
    return states[0].get();
  return states[idx].get();
}

namespace {
// What a per-receiver tracker is called, and what identifies it across runs.
struct Names {
  std::string label;
  std::string key;
};

// The label drops the device name when a receiver runs several models, while
// the key always keeps both. Two receivers on one device can therefore share a
// key while showing different labels; they are near enough that handing the
// history of one to the other is acceptable, and the label is set from the
// receiver either way.
Names namesFor(Receiver &r, int j, std::size_t receiver_count) {
  Device::Device *device = r.getDeviceManager().getDevice();

  Names n;
  n.label = deviceLabel(device);
  if (r.Count() > 1)
    n.label = receiver_count > 1 ? n.label + " " + r.Model(j)->getName()
                                 : r.Model(j)->getName();
  n.key = device->getIdentity() + "|" + r.Model(j)->getName();

  return n;
}

std::unique_ptr<ReceiverTracker>
takeMatching(std::vector<std::unique_ptr<ReceiverTracker>> &previous,
             const std::string &key) {
  for (auto &old : previous)
    if (old && old->key == key)
      return std::move(old);

  return std::unique_ptr<ReceiverTracker>();
}

// Every model output the group mask lets through: k is the receiver index,
// j the model index within that receiver.
template <typename F>
void forEachConnectable(const std::vector<std::unique_ptr<Receiver>> &receivers,
                        uint64_t groups, F fn) {
  for (int k = 0; k < (int)receivers.size(); k++)
    for (int j = 0; j < receivers[k]->Count(); j++)
      if (receivers[k]->Output(j).canConnect(groups))
        fn(*receivers[k], j, k);
}
} // namespace

// The aggregate tracker takes every output, whichever receiver it came from.
void WebViewer::wireAggregate(
    const std::vector<std::unique_ptr<Receiver>> &receivers) {
  // the receiver whose device is already in the description, so a receiver
  // running several models only contributes it once
  int described = -1;

  forEachConnectable(
      receivers, configuration.values.groups_in,
      [&](Receiver &r, int j, int k) {
        Device::Device *device = r.getDeviceManager().getDevice();

        const bool first_of_device = described != k;
        if (first_of_device) {
          states[0]->appendDevice(device);
          described = k;
        }

        states[0]->appendModel(r.Model(j)->getName(), !first_of_device);
        states[0]->connectJSON(r.OutputJSON(j));
        states[0]->connectGPS(r.OutputGPS(j));
        states[0]->connectControl(r.OutputControl(j));
        r.OutputADSB(j).Connect((StreamIn<Plane::ADSB> *)&planes);

        *device >> raw_counter;
      });
}

// Zone filtering resolves here rather than in the caller: this is the only
// reader of groups_in, and running every time means a viewer that outlives the
// engine cannot keep a mask from a previous configuration.
void WebViewer::resolveZoneMask(
    const std::vector<std::unique_ptr<Receiver>> &receivers) {
  if (configuration.values.zones.empty())
    return;

  configuration.values.groups_in =
      resolveZones(receivers, configuration.values.zones);
  if (!configuration.values.groups_in)
    Warning() << "Viewer has zone filter but no matching receivers — will "
                 "receive nothing";
}

// One tracker per connectable output, each handed the tracker that served the
// same input last run when there is one. Walks the outputs twice: a name is
// only final once every other name is known, since duplicates get numbered.
void WebViewer::attachTrackers(
    const std::vector<std::unique_ptr<Receiver>> &receivers,
    std::vector<std::unique_ptr<ReceiverTracker>> &previous) {
  // Labels are numbered when two outputs would show the same one, which no
  // single output can tell on its own — hence the look ahead. Keys need no
  // such thing: identical inputs are indistinguishable, so takeMatching()
  // handing them out in order puts each tracker back where it was.
  std::vector<std::string> claimed;
  forEachConnectable(
      receivers, configuration.values.groups_in, [&](Receiver &r, int j, int) {
        claimed.push_back(namesFor(r, j, receivers.size()).label);
      });

  forEachConnectable(receivers, configuration.values.groups_in,
                     [&](Receiver &r, int j, int k) {
                       Names n = namesFor(r, j, receivers.size());

                       int sharing = 0;
                       for (const auto &c : claimed)
                         if (c == n.label)
                           sharing++;
                       if (sharing > 1)
                         n.label += " #" + std::to_string(k + 1);

                       std::unique_ptr<ReceiverTracker> tracker =
                           takeMatching(previous, n.key);
                       if (!tracker)
                         tracker.reset(new ReceiverTracker());

                       tracker->setDevice(r.getDeviceManager().getDevice());
                       tracker->label = n.label;
                       tracker->key = n.key;
                       tracker->model_name = {r.Model(j)->getName()};

                       tracker->connectJSON(r.OutputJSON(j));
                       tracker->connectGPS(r.OutputGPS(j));
                       tracker->connectControl(r.OutputControl(j));

                       states.push_back(std::move(tracker));
                     });
}

void WebViewer::attachEngine(
    const std::vector<std::unique_ptr<Receiver>> &receivers) {
  std::lock_guard<std::recursive_mutex> lock(state_mtx);

  resolveZoneMask(receivers);

  int connectable = 0;
  for (auto &rp : receivers)
    connectable += rp->Count();

  // one tracker per output only pays off when the outputs can be told apart
  const auto &requested = configuration.values;
  const bool multi = requested.split && connectable > 1 &&
                     !configuration.filter.hasIDFilter() &&
                     requested.groups_in == 0xFFFFFFFFFFFFFFFF;

  // trackers of the previous run: attachTrackers() takes the ones whose input
  // is still here, and the rest are dropped when this vector goes out of scope
  std::vector<std::unique_ptr<ReceiverTracker>> previous = beginAttach();

  wireAggregate(receivers);

  if (multi)
    attachTrackers(receivers, previous);

  Debug()
      << "Mutex: WebViewer sinks self-lock (DB/PlaneDB), raw_counter atomic ("
      << receivers.size() << " receivers)";

  endAttach();
}

std::vector<std::unique_ptr<ReceiverTracker>> WebViewer::beginAttach() {
  std::lock_guard<std::recursive_mutex> lock(state_mtx);

  std::vector<std::unique_ptr<ReceiverTracker>> previous;
  for (std::size_t i = 1; i < states.size(); i++)
    previous.push_back(std::move(states[i]));
  states.erase(states.begin() + 1, states.end());

  states[0]->product.clear();
  states[0]->vendor.clear();
  states[0]->serial.clear();
  states[0]->sample_rate.clear();
  states[0]->model_name.clear();
  states[0]->device_label.clear();

  return previous;
}

void WebViewer::endAttach() {
  std::lock_guard<std::recursive_mutex> lock(state_mtx);
  if (initialized)
    applySettings(ApplyMode::Attach);
  engine_attached = true;
}

void WebViewer::detachEngine() {
  std::lock_guard<std::recursive_mutex> lock(state_mtx);

  // Only the references into the engine that is going away. The trackers stay:
  // attachEngine() rebuilds them on the next run.
  engine_attached = false;
  msg_channels = nullptr;
  setCommFeed(nullptr);
}

// Android supplies USB product/vendor/serial out-of-band, after attachEngine()
// (which clears the aggregate tracker's description)
void WebViewer::setDeviceDescription(const std::string &product,
                                     const std::string &vendor,
                                     const std::string &serial) {
  std::lock_guard<std::recursive_mutex> lock(state_mtx);

  if (!product.empty())
    states[0]->product = {product};
  if (!vendor.empty())
    states[0]->vendor = {vendor};
  if (!serial.empty())
    states[0]->serial = {serial};
}

// Android: a single model on a single device, wired without a Receiver. Only
// the middle differs from the overload above — the prologue and epilogue are
// shared, so a change to either reaches this path too.
void WebViewer::attachEngine(AIS::Model &model, Connection<JSON::JSON> &json,
                             Device::Device &device) {
  std::lock_guard<std::recursive_mutex> lock(state_mtx);

  beginAttach();

  states[0]->setDevice(&device);
  states[0]->model_name = {model.getName()};

  states[0]->connectJSON(json);
  device >> raw_counter;

  endAttach();
}

void WebViewer::tick(std::time_t now) {
  std::lock_guard<std::recursive_mutex> lock(state_mtx);

  for (auto &s : states)
    s->tick(now);
}

void WebViewer::resetStatistics() {
  std::lock_guard<std::recursive_mutex> lock(state_mtx);

  for (auto &s : states) {
    s->reset();
  }
  raw_counter.Reset();
  time_start = time(nullptr);
}

void WebViewer::startServing() {
  std::lock_guard<std::recursive_mutex> lock(state_mtx);

  applySettings(ApplyMode::Start);

  // the HTTP server keeps running across a stop/start, so bind only once
  if (!bound_port) {
    if (!settings.port_set)
      throw std::runtime_error("HTML server ports not specified");

    if (!HTTPServer::start(settings.port))
      throw std::runtime_error(
          settings.port ? "Cannot open port " + std::to_string(settings.port)
                        : std::string("Cannot open OS-assigned port"));

    bound_port = listening_port;

    const auto &tc = settings.tracking;

    auto on = [](bool b) { return b ? "on" : "off"; };
    Info() << "Webviewer: port " << bound_port
           << (tc.track_time != 3600
                   ? ", track_time: " + std::to_string(tc.track_time) + "s"
                   : std::string())
           << (tc.track_memory > 0 ? ", track_memory: " +
                                         std::to_string(tc.track_memory) + " KB"
                                   : std::string())
           << (tc.cutoff > 0 ? ", cutoff: " + std::to_string(tc.cutoff)
                             : std::string())
           << (tc.server_mode ? ", server_mode: on" : "")
           << ", realtime: " << on(settings.realtime)
           << ", log: " << on(settings.showlog)
           << ", replay: " << on(settings.replay)
           << ", share_loc: " << on(tc.latlon_share)
           << (settings.showdecoder ? ", decoder: on" : "")
           << (settings.KML ? ", kml: on" : "")
           << (settings.GeoJSON ? ", geojson: on" : "")
           << (configuration.plugins.dirs().empty() &&
                       configuration.plugins.loaded().empty()
                   ? std::string()
                   : ", plugins: " +
                         std::to_string(configuration.plugins.loaded().size()))
           << (configuration.plugins.errors().empty()
                   ? std::string()
                   : " (" +
                         std::to_string(configuration.plugins.errors().size()) +
                         " failed)");

    time_start = time(nullptr);
  }

  serving = true;
}

void WebViewer::stopServing() {
  std::lock_guard<std::recursive_mutex> lock(state_mtx);

  serving = false;

  logger.Stop();
  backup.stop();
}

void WebViewer::shutdown() {
  stopServing();
  stopThread();

  // a viewer that never served has nothing to save or report
  if (!initialized)
    return;

  if (!backup.getFilename().empty() && !backup.save()) {
    Error() << "Statistics - cannot write file: " << backup.getFilename();
  }
}

// --- Route handler functions ---

int WebViewer::parseMMSI(const std::string &query) {
  long long mmsi = toInt(query, -1);
  return (mmsi >= 1 && mmsi <= 999999999) ? (int)mmsi : -1;
}

void WebViewer::writeOutputsJSON(JSON::Writer &w) {
  w.kv("tcp_clients", numberOfClients());
  w.key("outputs").beginArray();
  if (msg_channels) {
    for (auto &o : *msg_channels)
      o->writeJSON(w);
  }
  w.endArray();
}

std::string WebViewer::buildStatJSON(ReceiverTracker *s) {
  std::string content;
  JSON::Writer w(content);

  w.beginObject();
  s->writeCountersJSON(w);
  w.kv("sharing", comm_feed != nullptr);
  w.kv("sharing_uuid", comm_feed != nullptr && comm_feed->hasUUID());
  w.kv("engine_running", engine_attached);
  std::string link = "https://www.aiscatcher.org";
  if (settings.tracking.latlon_share &&
      settings.tracking.lat != LAT_UNDEFINED &&
      settings.tracking.lon != LON_UNDEFINED)
    link += "/?&zoom=10&lat=" + std::to_string(settings.tracking.lat) +
            "&lon=" + std::to_string(settings.tracking.lon);
  w.kv("sharing_link", link);

  w.kv("station", settings.station);
  w.kv("station_link", settings.station_link);
  w.kv("sample_rate", s->sample_rate);
  w.kv("msg_rate", s->getMsgRate());
  w.kv("vessel_count", s->getCount());
  w.kv("vessel_max", s->getMaxCount());
  w.kv("product", s->product);
  w.kv("vendor", s->vendor);
  w.kv("serial", s->serial);
  w.kv("model", s->model_name);
  w.kv("device_label", s->device_label);
  w.kv("build_date", __DATE__);
  w.kv("build_version", VERSION);
  w.kv("build_describe", VERSION_DESCRIBE);
  w.kv("run_time",
       std::to_string((long int)time(nullptr) - (long int)time_start));
  if (auto places = getPlaceCatalogue())
    places->writeStatus(w);
  w.kv("memory", (unsigned long long)Util::Helper::getMemoryConsumption());
  w.kv("track_time", settings.tracking.track_time);
  w.kv("track_memory", settings.tracking.track_memory > 0
                           ? settings.tracking.track_memory
                           : (settings.tracking.server_mode ? 4096 : 1024));
  w.kv_raw("os", os);
  w.kv_raw("hardware", hardware);

  writeOutputsJSON(w);
  w.kv("received", (unsigned long long)raw_counter.received());
  w.endObject();

  w.finish();
  return content;
}

// the viewer polls this every 10s for the community icon; stat.json is 100x it
std::string WebViewer::buildSharingStateJSON() {
  std::string content;
  JSON::Writer w(content);

  w.beginObject()
      .kv("config_version", viewerConfigVersion())
      .kv("sharing", comm_feed != nullptr)
      .kv("sharing_uuid", comm_feed != nullptr && comm_feed->hasUUID())
      .kv("engine_running", engine_attached)
      .endObject();

  w.finish();
  return content;
}

// Slim variant of stat.json for the control hub's data-flow tab, which only
// needs the per-output counters.
std::string WebViewer::buildOutputStatsJSON() {
  std::string content;
  JSON::Writer w(content);

  w.beginObject();
  writeOutputsJSON(w);
  w.endObject();

  w.finish();
  return content;
}

std::string WebViewer::buildMultiPathJSON(ReceiverTracker *s,
                                          const std::string &query) {
  std::stringstream ss(query);
  std::string mmsi_str;
  std::string content;
  JSON::Writer w(content);
  w.beginObject();
  int count = 0;
  const int MAX_MMSI_COUNT = 100;

  while (std::getline(ss, mmsi_str, ',')) {
    if (++count > MAX_MMSI_COUNT) {
      Error() << "Server - path MMSI count exceeds limit: " << MAX_MMSI_COUNT;
      break;
    }

    int mmsi = parseMMSI(mmsi_str);
    if (mmsi < 0) {
      Error() << "Server - path MMSI invalid: " << mmsi_str;
      continue;
    }

    w.key((unsigned)mmsi).raw_val(s->getPathJSON(mmsi));
  }
  w.endObject();
  w.finish();
  return content;
}

// --- Route table ---

// Replay history is served in fixed blocks addressed by index, so a stretch of
// time is always the same URL and no client can ask for a slightly different
// range that would miss the cache. Changing this invalidates every cached
// block, which is the intended effect.
static const std::time_t REPLAY_BLOCK = 600;
static const long long MAX_REPLAY_LOOKBACK = 7 * 24 * 3600;

// Checked before it is multiplied by anything: a negative index yields a
// negative `until`, which the path writer reads as "no upper bound".
static bool validReplayBlock(long long block) {
  return block > 0 && block <= (long long)(time(nullptr) / REPLAY_BLOCK);
}

// The tracker a handler is given is never null: getState() falls back to the
// aggregate, which exists for the lifetime of the viewer.
const WebViewer::Route WebViewer::routes[] = {
    // JSON API routes (application/json)
    {"/api/config.json", nullptr, "application/json",
     [](WebViewer *w, ReceiverTracker *, const std::string &) {
       return w->viewerConfigJSON();
     },
     true},
    {"/api/ships.json", nullptr, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &) {
       return s->getShipsJSON();
     },
     true},
    {"/ships.json", nullptr, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &) {
       return s->getShipsJSON();
     },
     true},
    {"/api/ships_full.json", nullptr, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &) {
       return s->getShipsJSON(true);
     },
     true},
    {"/api/ships_array.json", nullptr, "application/json",
     [](WebViewer *w, ReceiverTracker *s, const std::string &a) {
       return s->getShipsJSONcompact(queryInt(a, "since"),
                                     w->viewerConfigVersion());
     },
     true},
    {"/api/ships_table.json", nullptr, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &a) {
       return s->getShipsTableJSON(queryInt(a, "since"));
     },
     true},
    {"/api/planes.json", nullptr, "application/json",
     [](WebViewer *w, ReceiverTracker *, const std::string &) {
       return w->planes.getJSON();
     },
     true},
    {"/api/planes_array.json", nullptr, "application/json",
     [](WebViewer *w, ReceiverTracker *, const std::string &a) {
       return w->planes.getCompactArray(queryInt(a, "since"));
     },
     true},
    {"/api/binmsgs.json", nullptr, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &a) {
       return s->getBinaryMessagesJSON(
           queryInt(a, "since"),
           strtoull(IO::HTTPRequest::queryParam(a, "marker").c_str(), nullptr,
                    16),
           (uint32_t)queryInt(a, "mmsi"));
     },
     true},
    {"/api/mapobjects.json", nullptr, "application/json",
     [](WebViewer *w, ReceiverTracker *s, const std::string &a) {
       return s->database().getMapObjectsJSON((uint64_t)queryInt(a, "since"));
     },
     true},
    {"/api/object.json", nullptr, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &a) {
       return s->getObjectJSON(IO::HTTPRequest::queryParam(a, "key"));
     },
     true},
    {"/api/events.json", nullptr, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &a) {
       return s->getEventsJSON(
           strtoull(IO::HTTPRequest::queryParam(a, "since").c_str(), nullptr,
                    10),
           queryInt(a, "level"));
     },
     true},
    {"/api/history_full.json", nullptr, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &) {
       return s->toHistoryJSON();
     },
     true},
    {"/api/stat.json", nullptr, "application/json",
     [](WebViewer *w, ReceiverTracker *s, const std::string &) {
       return w->buildStatJSON(s);
     },
     true},
    {"/stat.json", nullptr, "application/json",
     [](WebViewer *w, ReceiverTracker *s, const std::string &) {
       return w->buildStatJSON(s);
     },
     true},
    {"/api/place.json", nullptr, "application/json",
     [](WebViewer *w, ReceiverTracker *, const std::string &q) {
       auto places = w->getPlaceCatalogue();
       const auto id = IO::HTTPRequest::queryParam(q, "id");
       // a missing or malformed id is no place, not place 0
       const bool digits = !id.empty() && id.size() <= 10 &&
                           id.find_first_not_of("0123456789") == std::string::npos;
       const auto number = digits ? std::strtoull(id.c_str(), nullptr, 10) : UINT32_MAX;
       return places && number < UINT32_MAX
         ? places->feature(static_cast<uint32_t>(number), IO::HTTPRequest::queryParam(q, "version")) : "{}";
     }, true},
    {"/api/places.json", nullptr, "application/json",
     [](WebViewer *w, ReceiverTracker *, const std::string &) {
       auto places = w->getPlaceCatalogue();
       return places ? places->collection()
                    : "{\"place_version\":\"\",\"objects\":[]}";
     },
     true},
    {"/api/sharing_state.json", nullptr, "application/json",
     [](WebViewer *w, ReceiverTracker *, const std::string &) {
       return w->buildSharingStateJSON();
     },
     true},
    {"/api/output_stats.json", nullptr, "application/json",
     [](WebViewer *w, ReceiverTracker *, const std::string &) {
       return w->buildOutputStatsJSON();
     },
     true},
    {"/api/path.json", nullptr, "application/json",
     [](WebViewer *w, ReceiverTracker *s, const std::string &a) {
       return w->buildMultiPathJSON(s, a);
     },
     true},
    {"/api/allpath.json", nullptr, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &a) {
       std::time_t since = (std::time_t)queryInt(a, "since");
       return since > 0 ? s->getAllPathJSONSince(since) : s->getAllPathJSON();
     },
     true},
    {"/api/replay_info.json", &WebViewer::Settings::replay, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &) {
       return s->getReplayInfoJSON(REPLAY_BLOCK);
     },
     true},
    {"/api/replay_ships.json", &WebViewer::Settings::replay, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &a) {
       return s->getReplayShipsJSON((std::time_t)queryInt(a, "since"),
                                    (std::time_t)queryInt(a, "lookback"));
     },
     true},
    {"/api/replay.json", &WebViewer::Settings::replay, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &a) {
       long long block = queryInt(a, "block");
       if (!validReplayBlock(block))
         return std::string("{}\n\n");

       std::time_t since = (std::time_t)(block * REPLAY_BLOCK);
       long long lookback = queryInt(a, "lookback");
       if (lookback < 0 || lookback > MAX_REPLAY_LOOKBACK)
         lookback = 0;

       return s->getReplayJSON(since, since + REPLAY_BLOCK - 1,
                               (std::time_t)lookback);
     },
     true,
     // A dwell inside the block can still grow for DWELL_GAP after it ends, so
     // caching waits that out; past it only eviction changes anything.
     [](const std::string &q) -> bool {
       long long block = queryInt(q, "block");
       return validReplayBlock(block) &&
              (block + 1) * REPLAY_BLOCK + (long long)PathStore::DWELL_GAP <=
                  (long long)time(nullptr);
     }},
    {"/api/path.geojson", nullptr, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &a) {
       int mmsi = parseMMSI(a);
       return mmsi > 0 ? s->getPathGeoJSON(mmsi) : std::string("{}");
     },
     true},
    {"/api/allpath.geojson", nullptr, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &) {
       return s->getAllPathGeoJSON();
     },
     true},
    {"/api/message", nullptr, "application/json",
     [](WebViewer *w, ReceiverTracker *s, const std::string &a) {
       int mmsi = parseMMSI(a);
       if (mmsi <= 0)
         return std::string("{\"error\":\"Invalid MMSI\"}");
       std::string msg = s->getMessage(mmsi);
       return msg.empty() ? std::string("{\"error\":\"Message not found\"}")
                          : w->decodeNMEAtoJSON(msg, false);
     },
     true},
    {"/api/vessel", nullptr, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &a) {
       int mmsi = parseMMSI(a);
       if (mmsi <= 0)
         return std::string("{\"error\":\"Invalid MMSI\"}");
       std::string vessel = s->getShipJSON(mmsi);
       return vessel == "{}" ? std::string("{\"error\":\"Vessel not found\"}")
                             : vessel;
     },
     true},
    {"/api/ship.json", nullptr, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &a) {
       long long mmsi = queryInt(a, "mmsi");
       if (mmsi < 1 || mmsi > 999999999)
         return std::string("{\"error\":\"Invalid MMSI\"}");
       std::string vessel = s->getShipJSON((int)mmsi);
       return vessel == "{}" ? std::string("{\"error\":\"Vessel not found\"}")
                             : vessel;
     },
     true},
    {"/api/ships_place.json", nullptr, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &a) {
       return placeShipsJSON(s, a);
     }, true},
    {"/api/ships_port.json", nullptr, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &a) {
       std::string code = IO::HTTPRequest::queryParam(a, "code");
       bool valid = !code.empty() && code.size() <= 8;
       for (char &ch : code) {
         if (ch >= 'a' && ch <= 'z')
           ch -= 'a' - 'A';
         if (!((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')))
           valid = false;
       }
       if (!valid)
         return std::string(
             "{\"error\":\"code must be 1-8 letters or digits\"}");
       return s->database().getPlaceShipsJSON(UINT32_MAX, "", code,
                                              "expected", 24);
     },
     true},
    {"/api/changes.json", nullptr, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &a) {
       int mmsi = parseMMSI(a);
       if (mmsi <= 0)
         return std::string("{\"error\":\"Invalid MMSI\"}");
       return s->getChangesJSON(mmsi);
     },
     true},
    {"/api/decode", &WebViewer::Settings::showdecoder, "application/json",
     [](WebViewer *, ReceiverTracker *, const std::string &a) {
       try {
         if (a.empty() || a.size() > 1024)
           return std::string("{\"error\":\"Input size limit exceeded\"}");
         std::string result = decodeNMEAtoJSON(urlDecode(a));
         return result == "[]"
                    ? std::string(
                          "{\"error\":\"No valid AIS messages decoded\"}")
                    : result;
       } catch (const std::exception &e) {
         Error() << "Decoder error: " << e.what();
         return std::string("{\"error\":\"Decoding failed\"}");
       }
     },
     true},

    // Conditional settings.GeoJSON/settings.KML routes
    {"/geojson", &WebViewer::Settings::GeoJSON, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &) {
       return s->getGeoJSON();
     },
     true},
    {"/allpath.geojson", &WebViewer::Settings::GeoJSON, "application/json",
     [](WebViewer *, ReceiverTracker *s, const std::string &) {
       return s->getAllPathGeoJSON();
     },
     true},
    {"/kml", &WebViewer::Settings::KML, "application/vnd.google-earth.kml+xml",
     [](WebViewer *, ReceiverTracker *s, const std::string &) {
       return s->getKML();
     },
     true},

    // Prometheus metrics
    {"/metrics", &WebViewer::Settings::supportPrometheus, "text/plain",
     [](WebViewer *w, ReceiverTracker *, const std::string &) {
       return w->dataPrometheus.toPrometheus();
     },
     true},

    // Frontend assets
    {"/custom/plugins.js", nullptr, "application/javascript",
     [](WebViewer *w, ReceiverTracker *, const std::string &) {
       return w->configuration.frontend.render(w->configuration.plugins);
     },
     false},
    {"/custom/config.css", nullptr, "text/css",
     [](WebViewer *w, ReceiverTracker *, const std::string &) {
       return w->configuration.plugins.getStylesheets();
     },
     false},
    {"/about.md", nullptr, "text/markdown",
     [](WebViewer *w, ReceiverTracker *, const std::string &) {
       return w->configuration.plugins.getAbout();
     },
     false},

    {nullptr, nullptr, nullptr, nullptr, false}};

void WebViewer::Request(IO::TCPServerConnection &c,
                        const IO::HTTPRequest &request, bool gzip) {
  std::lock_guard<std::recursive_mutex> lock(state_mtx);

  // between stop() and start() the settings are only half applied
  if (!serving) {
    Response(c, "text/plain", std::string("Viewer is applying settings."),
             false, false, false, 503);
    return;
  }

  std::string r = request.path();

  // the single argument a handler receives: the query string for a GET, the
  // body for a POST (/api/decode submits the NMEA that way)
  const std::string a = request.method == "POST" && !request.body.empty()
                            ? request.body
                            : request.query();

  if (r == "/")
    r = "/index.html";

  // Route table lookup
  for (const Route *rt = routes; rt->path; ++rt) {
    if (r != rt->path)
      continue;
    if (rt->flag && !(settings.*(rt->flag)))
      continue;

    ReceiverTracker *s = getState((int)queryInt(a, "receiver"));
    const bool may_cache = rt->cacheable && rt->cacheable(a);
    Response(c, rt->content_type, rt->handler(this, s, a),
             settings.use_zlib && gzip, may_cache, rt->cors);
    return;
  }

  // SSE routes (upgrade connection, not a normal response)
  if (r == "/api/sse" && settings.realtime) {
    upgradeSSE(c, 1u << IO::SSE::NMEA);
  } else if (r == "/api/signal" && settings.realtime) {
    upgradeSSE(c, 1u << IO::SSE::SIGNAL);
  } else if (r == "/api/log" && settings.showlog) {
    upgradeSSE(c, 1u << IO::SSE::VIEWER_LOG, "log",
               []() -> std::vector<std::string> {
                 return Logger::getInstance().getBacklogJSON(INT_MAX);
               });
  }
  // Prefix-match routes
  else if (r.substr(0, 6) == "/tiles") {
    int z, x, y;
    std::string layer;
    if (parseMBTilesURL(r, layer, z, x, y)) {
      for (const auto &source : configuration.mapSources) {
        if (source->getLayerID() != layer)
          continue;

        std::string contentType;
        const std::vector<unsigned char> &data =
            source->getTile(z, x, y, contentType);

        if (!data.empty()) {
          Response(c, contentType, (char *)data.data(), data.size(),
                   settings.use_zlib && gzip, true);
          return;
        }
      }
      Response(c, "text/plain", std::string("Tile not found"), false, false,
               false, 404);
      return;
    }
    Response(c, "text/plain", std::string("Invalid Tile Request"), false, false,
             false, 400);
    return;
  } else if (extra_request && extra_request(*this, c, r, a, gzip)) {
  }
  // Static files
  else if (r.rfind("/", 0) == 0) {
    std::string filename = r.substr(1);

    auto it = WebDB::files.find(filename);
    if (it != WebDB::files.end()) {
      const WebDB::FileData &file = it->second;
      ResponseRaw(c, file.mime_type, (char *)file.data, file.size, true,
                  std::string(file.mime_type) != "text/html");
    } else {
      // 404 silently — browsers probe /.well-known/, favicon variants,
      // /robots.txt etc. and would flood the log.
      NotFound(c);
    }
  } else
    NotFound(c);
}

std::string WebViewer::placeShipsJSON(ReceiverTracker *s, const std::string &query) {
  const auto param = [&](const char *key) { return IO::HTTPRequest::queryParam(query, key); };
  const auto idText = param("id"), hoursText = param("hours");
  auto digits = [](const std::string &v) { return !v.empty() && v.find_first_not_of("0123456789") == std::string::npos; };
  if ((!idText.empty() && (!digits(idText) || idText.size() > 10 || std::strtoull(idText.c_str(), nullptr, 10) >= UINT32_MAX)) ||
      (!hoursText.empty() && (!digits(hoursText) || hoursText.size() > 4)))
    return "{\"error\":\"Invalid place query\"}";
  auto code = param("code");
  if (code.size() > 8) return "{\"error\":\"Invalid port code\"}";
  for (char &c : code) {
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
      return "{\"error\":\"Invalid port code\"}";
  }
  return s->database().getPlaceShipsJSON(idText.empty() ? UINT32_MAX : std::strtoul(idText.c_str(), nullptr, 10),
      param("version"), code, param("tab").empty() ? "inside" : param("tab"),
      hoursText.empty() ? 24 : std::strtoul(hoursText.c_str(), nullptr, 10));
}
