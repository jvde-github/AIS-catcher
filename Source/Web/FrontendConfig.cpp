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

#include "FrontendConfig.h"
#include "SHA256.h"

#include "Helper.h"
#include "Logger.h"
#include "Writer.h"
#include "JSON/Parser.h"

// --- FrontendConfig ---

FrontendConfig::FrontendConfig() {
  build_version = VERSION;
  build_describe = VERSION_DESCRIBE;
}

void FrontendConfig::setStation(const std::string &name) { station = name; }

void FrontendConfig::setReceivers(
    const std::vector<std::unique_ptr<ReceiverTracker>> &states) {
  receivers.clear();
  if (split && states.size() > 1)
    for (int i = 0; i < (int)states.size(); i++)
      receivers.emplace_back(i ? i : -1, states[i]->label); // -1 is All, as the page has it
}

std::string FrontendConfig::json(const PluginStore &plugins,
                                 std::string *revisionOut) const {
  std::string out;

  // 1. Emit the structured config blob. Single source of truth for all
  //    server-side state the frontend needs at startup.
  // Also served independently by api/config.json.
  const auto emit = [&](const std::string &version) {
    out.clear();
    JSON::Writer w(out);
    w.beginObject();
    if (!version.empty())
      w.kv("config_version", version);
    w.kv("plugin_version", plugins.version())
        .kv("about_version", Util::SHA256::hex(plugins.getAbout()))
        .key("maps")
        .beginArray();
    for (const auto &source : maps)
      w.raw_val(source);
    w.endArray().key("restart_required").beginArray();
    for (const auto &reason : restart_required)
      w.val(reason);
    w.endArray()
        .key("build")
        .beginObject()
        .kv("version", build_version)
        .kv("describe", build_describe)
        .endObject()
        .kv("context", context)
        .kv("station", station)
        .kv("webcontrol_http", webcontrol_http)
        .key("features")
        .beginObject()
        .kv("share_location", share_location)
        .kv("save_messages", save_messages)
        .kv("replay", replay)
        .kv("realtime", realtime)
        .kv("log", log_enabled)
        .kv("decoder", decoder)
        .kv("managed", managed)
        .kv("places", places)
        .kv("geojson", geojson)
        .kv("kml", kml)
        .kv("about_md", plugins.isAboutPresent())
        .kv("sharing", sharing)
        .kv("sharing_uuid", sharing_uuid)
        .endObject()
        .key("receivers")
        .beginArray();
    for (const auto &r : receivers)
      w.beginObject().kv("idx", r.first).kv("label", r.second).endObject();
    w.endArray().key("plugins").beginObject().key("loaded").beginArray();
    for (const auto &p : plugins.loaded())
      w.beginObject().kv("name", p.first).kv("version", p.second).endObject();
    w.endArray().key("errors").beginArray();
    for (const auto &e : plugins.errors())
      w.val(e);
    w.endArray().endObject().endObject();
    w.finish();
  };
  emit("");
  const auto version = Util::SHA256::hex(out + plugins.getAbout());
  if (revisionOut)
    *revisionOut = version;
  emit(version);
  return out;
}

std::string FrontendConfig::render(const PluginStore &plugins) const {
  return "window.__SERVER_CONFIG__ = " + json(plugins) + ";\n" +
         plugins.getCode() + "}\n";
}

