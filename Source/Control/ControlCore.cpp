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

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <utility>
#include <vector>
#ifndef _WIN32
#include <climits>
#include <unistd.h>
#endif

#include "Common.h"
#include "Config.h"
#include "Convert.h"
#include "ControlCore.h"
#include "TCPServer.h"
#include "DeviceManager.h"
#include "Device/Serial.h"
#include "Engine.h"
#include "Helper.h"
#include "Keys.h"
#include "Logger.h"
#include "Parse.h"
#include "Parser.h"
#include "SHA256.h"
#include "Writer.h"
#include "PlaceCatalogue.h"
#ifdef _WIN32
// after the project's headers: winsock2.h has to come before windows.h
#include <windows.h>
#endif

ControlCore::~ControlCore() {}

// The file as people read and edit it: one key to a line, so the reader's line numbers mean something
static std::string indented(const std::string &json) {
  std::string out;
  out.reserve(json.size() * 2);
  int depth = 0;
  bool in_string = false;
  const auto newline = [&] {
    out += '\n';
    out.append(depth * 2, ' ');
  };
  for (std::size_t i = 0; i < json.size(); i++) {
    const char c = json[i];
    if (in_string) {
      out += c;
      if (c == '\\' && i + 1 < json.size())
        out += json[++i];
      else if (c == '"')
        in_string = false;
      continue;
    }
    switch (c) {
    case ' ': case '\t': case '\r': case '\n':
      break;
    case '"':
      in_string = true;
      out += c;
      break;
    case '{': case '[': {
      out += c;
      std::size_t next = json.find_first_not_of(" \t\r\n", i + 1);
      if (next != std::string::npos && (json[next] == '}' || json[next] == ']')) {
        out += json[next];
        i = next;
      } else {
        depth++;
        newline();
      }
      break;
    }
    case '}': case ']':
      depth--;
      newline();
      out += c;
      break;
    case ',':
      out += c;
      newline();
      break;
    case ':':
      out += ": ";
      break;
    default:
      out += c;
    }
  }
  return out;
}

ControlCore::ControlCore(const std::string &file, int port_override,
                         const std::string &bind)
    : config_file(file), bind_address(bind) {
  if (!std::ifstream(config_file).good())
    createDefaultConfig();
  if (port_override > 0)
    control_port = port_override;

  // read once and checked as a save is: the panel only writes files that
  // pass, so one that does not was edited by hand and stops the start
  const std::string json = Util::Helper::readFile(config_file);
  std::string error;
  if (!validate(json, error))
    throw std::runtime_error("Control: " + config_file + ": " + error);

  readManagedFields(json);
  refreshPlaces(json);
  addMainDefaults();
  announceSetupCode();

  auto_retry = desired;
}

// the machine's name, so a fresh station is not called "My Station"
static std::string machineName() {
  char name[256] = {0};
#ifdef _WIN32
  DWORD size = sizeof(name);
  if (!GetComputerNameA(name, &size))
    return "";
#else
  if (gethostname(name, sizeof(name) - 1) != 0)
    return "";
#endif
  std::string s(name);
  // a local domain adds nothing for visitors
  const auto dot = s.find('.');
  if (dot != std::string::npos && dot > 0)
    s.erase(dot);
  return s;
}

// the statistics backup beside the configuration, as a full path so the
// settings show where it lives (/etc/AIS-catcher for an installed service)
static std::string statsPath(const std::string &config_file) {
  const auto slash = config_file.find_last_of("/\\");
  const std::string dir = slash == std::string::npos ? "." : config_file.substr(0, slash);
  const std::string name =
      (slash == std::string::npos ? config_file : config_file.substr(slash + 1)) + ".stats";
#ifdef _WIN32
  char full[_MAX_PATH];
  if (_fullpath(full, dir.c_str(), sizeof(full)))
    return std::string(full) + "\\" + name;
#else
  char full[PATH_MAX];
  if (realpath(dir.c_str(), full))
    return std::string(full) + "/" + name;
#endif
  return name;
}

// what a fresh configuration starts from; the main server, the tracking backup and the default station come
// with addMainDefaults(), as for an imported one
void ControlCore::createDefaultConfig() {
  const std::string content = "{ \"config\": \"aiscatcher\", \"version\": 1, \"engine\": \"off\", \"sharing\": true, "
                              "\"control\": { \"wizard\": true } }\n";
  std::string error;
  if (!writeFileAtomic(config_file, content, error))
    throw std::runtime_error("Control: cannot create config file \"" +
                             config_file + "\": " + error);

  Info() << "Control: created default config file " << config_file;
}

// The engine switch and the panel's own section, from a file that was checked
void ControlCore::readManagedFields(const std::string &json) {
  JSON::Parser parser(JSON_DICT_SETTING);
  JSON::Document doc = parser.parse(json);

  for (const auto &m : doc.getMembers()) {
    switch (m.Key()) {
    case AIS::KEY_SETTING_ENGINE:
      desired = Util::Parse::Switch(m.Get().to_string());
      break;
    case AIS::KEY_SETTING_CONTROL:
      if (!m.Get().isObject())
        throw std::runtime_error("Control: " + config_file + ": \"control\" needs to be an object");

      applyAuthFields(m.Get());
      break;
    }
  }
}

void ControlCore::startEngine() {
  command_seq++;
  {
    std::lock_guard<std::mutex> lock(mtx);
    resetRetry();
    if (desired)
      return;
    desired = true;
    restart_pending = false;
    status_epoch++;
  }
  persistEngineField(true);
  cv.notify_all();
}

void ControlCore::stopEngine() {
  command_seq++;
  {
    std::lock_guard<std::mutex> lock(mtx);
    desired = false;
    restart_pending = false;
    resetRetry();
    status_epoch++;
  }
  persistEngineField(false);
  StopRequest();
}

void ControlCore::restartEngine() {
  command_seq++;
  bool running;
  {
    std::lock_guard<std::mutex> lock(mtx);
    desired = true;
    running = (state == EngineState::Running);
    restart_pending = running;
    resetRetry();
    auto_retry = running;
    status_epoch++;
  }
  persistEngineField(true);

  if (running)
    StopRequest();
  else
    cv.notify_all();
}

ControlCore::EngineState ControlCore::getEngineState() {
  std::lock_guard<std::mutex> lock(mtx);
  return state;
}

long long ControlCore::getUptime() {
  std::lock_guard<std::mutex> lock(mtx);

  if (state != EngineState::Running)
    return 0;

  return (long long)(std::time(nullptr) - engine_start_time);
}

std::string ControlCore::getConfig() {
  std::lock_guard<std::mutex> lock(file_mtx);
  return Util::Helper::readFile(config_file);
}

// what the panel shows: the file without what proves the password
std::string ControlCore::getPanelConfig() {
  std::lock_guard<std::mutex> lock(file_mtx);
  JSON::Parser parser(JSON_DICT_SETTING);
  JSON::Document doc = parser.parse(Util::Helper::readFile(config_file));
  if (JSON::Value *control = doc.root[AIS::KEY_SETTING_CONTROL])
    if (control->isObject())
      removeAuth(control->getObject());
  std::string out;
  JSON::Serializer serializer(JSON_DICT_SETTING);
  serializer.stringify(doc.root, out);
  return out;
}

static bool isSameFile(const std::string &a, const std::string &b) {
#ifdef _WIN32
  return a == b;
#else
  char ra[PATH_MAX], rb[PATH_MAX];
  if (!realpath(a.c_str(), ra) || !realpath(b.c_str(), rb))
    return a == b;
  return strcmp(ra, rb) == 0;
#endif
}

static std::string configFolder(const std::string &config_file) {
  const auto slash = config_file.find_last_of("/\\");
  return slash == std::string::npos ? "." : config_file.substr(0, slash);
}

// Whether a path, as its reader would open it, is the folder or in it: links
// and ".." resolved, and for a file not there yet, by the folder it would be in.
static bool inFolder(const std::string &path, const std::string &folder) {
#ifdef _WIN32
  char full[_MAX_PATH], base[_MAX_PATH];
  if (!_fullpath(full, path.c_str(), sizeof(full)) || !_fullpath(base, folder.c_str(), sizeof(base)))
    return false;
  const size_t n = strlen(base);
  return _strnicmp(full, base, n) == 0 && (full[n] == 0 || full[n] == '\\' || full[n] == '/');
#else
  char full[PATH_MAX], base[PATH_MAX];
  if (!realpath(folder.c_str(), base))
    return false;
  if (!realpath(path.c_str(), full)) {
    const auto slash = path.find_last_of('/');
    if (!realpath(slash == std::string::npos ? "." : (slash == 0 ? "/" : path.substr(0, slash).c_str()), full))
      return false;
  }
  const size_t n = strlen(base);
  return strncmp(full, base, n) == 0 && (full[n] == 0 || full[n] == '/');
#endif
}

static bool hasExtension(const std::string &path, const char *extension) {
  const size_t n = strlen(extension);
  return path.size() > n && path.compare(path.size() - n, n, extension) == 0;
}

typedef std::vector<std::pair<int, std::string>> PathValues;

// the values naming a file or a folder under a section, at any depth; a text
// value is a list, as its reader takes it
static void pathValues(const JSON::Value &v, PathValues &out) {
  if (v.isArray()) {
    for (const auto &e : v.getArray())
      pathValues(e, out);
  } else if (v.isObject()) {
    for (const auto &m : v.getObject().getMembers())
      switch (m.Key()) {
      case AIS::KEY_SETTING_ABOUT:
      case AIS::KEY_SETTING_STYLE:
      case AIS::KEY_SETTING_PLUGIN:
      case AIS::KEY_SETTING_PLUGIN_DIR:
      case AIS::KEY_SETTING_MBTILES:
      case AIS::KEY_SETTING_FSTILES:
      case AIS::KEY_SETTING_MBOVERLAY:
      case AIS::KEY_SETTING_FSOVERLAY:
      case AIS::KEY_SETTING_FILE:
      case AIS::KEY_SETTING_CERT:
      case AIS::KEY_SETTING_KEY:
      case AIS::KEY_SETTING_PLACES: {
        std::vector<std::string> parts;
        Util::Parse::Split(JSON::text(m.Get()), ',', parts);
        for (const auto &p : parts)
          if (!p.empty())
            out.emplace_back(m.Key(), p);
        break;
      }
      default:
        pathValues(m.Get(), out);
      }
  }
}

// what a receiver writes: a serial port's dump_file and a decoding model's dump,
// a path before "_A.wav" and "_B.wav"; a serial port's own "dump" is a switch
static void dumpValues(const JSON::JSON &section, bool serial, PathValues &out) {
  for (const auto &m : section.getMembers())
    if (m.Key() == AIS::KEY_SETTING_DUMP_FILE || (m.Key() == AIS::KEY_SETTING_DUMP && !serial)) {
      if (!JSON::text(m.Get()).empty())
        out.emplace_back(m.Key(), JSON::text(m.Get()));
    } else if (m.Get().isObject())
      dumpValues(m.Get().getObject(), serial || m.Key() == AIS::KEY_SETTING_SERIALPORT, out);
    else if (m.Get().isArray())
      for (const auto &entry : m.Get().getArray())
        if (entry.isObject())
          dumpValues(entry.getObject(), serial, out);
}

// the files and folders a configuration has this program serve or write; a
// receiver's input is only read for its samples
static PathValues pathValues(const JSON::JSON &root) {
  PathValues out;
  dumpValues(root, false, out);
  for (const auto &m : root.getMembers())
    switch (m.Key()) {
    case AIS::KEY_SETTING_TRACKING:
    case AIS::KEY_SETTING_SERVICES:
    case AIS::KEY_SETTING_SERVER:
    case AIS::KEY_SETTING_CONTROL:
      pathValues(m.Get(), out);
      break;
    case AIS::KEY_SETTING_FILE_OUTPUT:
      if (m.Get().isArray())
        for (const auto &entry : m.Get().getArray()) {
          const JSON::Value *file = entry.isObject() ? entry.getObject()[AIS::KEY_SETTING_FILE] : nullptr;
          if (file)
            out.emplace_back(AIS::KEY_SETTING_FILE_OUTPUT, file->to_string());
        }
      break;
    case AIS::KEY_SETTING_DB:
      if (m.Get().isArray())
        for (const auto &entry : m.Get().getArray()) {
          if (!entry.isObject())
            continue;
          // no conn_str: a CSV output's folder is the working folder
          std::string type = "postgres", conn = ".";
          for (const auto &p : entry.getObject().getMembers())
            if (p.Key() == AIS::KEY_SETTING_MODEL_TYPE)
              type = Util::Convert::toLowerCopy(p.Get().to_string());
            else if (p.Key() == AIS::KEY_SETTING_CONN_STR && !p.Get().to_string().empty())
              conn = p.Get().to_string();
          if (type == "csv" || type == "sqlite")
            out.emplace_back(AIS::KEY_SETTING_CONN_STR, conn);
        }
      break;
    default:
      break;
    }
  return out;
}

static std::string legacyConfigOf(const JSON::JSON &root) {
  std::string path;
  const JSON::Value *v = root[AIS::KEY_SETTING_CONTROL];
  if (v && v->isObject())
    for (const auto &c : v->getObject().getMembers())
      if (c.Key() == AIS::KEY_SETTING_LEGACY_CONFIG)
        path = c.Get().to_string();
  return path;
}

// What a save from the panel may name: what the file has now, or a file of the
// configuration's folder of the kind the key reads. The panel's session should
// not reach every file this program can. The command line and a file edited by
// hand are not asked.
bool ControlCore::allowedPaths(const JSON::JSON &saved, std::string &error) {
  std::set<std::pair<int, std::string>> known;
  std::string legacy;
  try {
    JSON::Parser parser(JSON_DICT_SETTING);
    JSON::Document current = parser.parse(Util::Helper::readFile(config_file));
    const PathValues values = pathValues(current.root);
    known.insert(values.begin(), values.end());
    legacy = legacyConfigOf(current.root);
  } catch (const std::exception &) {
  }

  const std::string now = legacyConfigOf(saved);
  if (!now.empty() && now != legacy) {
    error = "\"legacy_config\" is set by the installer and cannot be changed here.";
    return false;
  }

  const std::string folder = configFolder(config_file);
  const PathValues values = pathValues(saved);
  // a server's private key: no other setting, which might serve or write its file, may name it
  std::vector<std::string> tls_keys;
  for (const auto &v : values)
    if (v.first == AIS::KEY_SETTING_KEY)
      tls_keys.push_back(Util::Helper::resolvePath(v.second, folder));
  for (const auto &v : known)
    if (v.first == AIS::KEY_SETTING_KEY)
      tls_keys.push_back(Util::Helper::resolvePath(v.second, folder));
  for (const auto &v : values) {
    if (known.count(v))
      continue;
    const std::string quoted = "\"" + AIS::settingName(v.first) + "\": \"" + v.second + "\"";
    if (v.first == AIS::KEY_SETTING_STYLE && !hasExtension(v.second, ".css") && !hasExtension(v.second, ".pss")) {
      error = quoted + " is not a .css or .pss file.";
      return false;
    }
    if (v.first == AIS::KEY_SETTING_ABOUT && !hasExtension(v.second, ".md")) {
      error = quoted + " is not a .md file.";
      return false;
    }
    if (v.first == AIS::KEY_SETTING_PLUGIN && !hasExtension(v.second, ".js") && !hasExtension(v.second, ".pjs")) {
      error = quoted + " is not a .js or .pjs file.";
      return false;
    }
    // the site's own are read from the configuration's folder, the others from the working folder
    const bool site_path = v.first == AIS::KEY_SETTING_FILE || v.first == AIS::KEY_SETTING_PLACES ||
                           v.first == AIS::KEY_SETTING_CERT || v.first == AIS::KEY_SETTING_KEY;
    const std::string path = site_path ? Util::Helper::resolvePath(v.second, folder) : v.second;
    if (!inFolder(path, folder)) {
      error = quoted + " is outside the configuration's folder (" + folder + "): a path elsewhere can only be written in the file by hand.";
      return false;
    }
    if (isSameFile(path, config_file)) {
      error = quoted + " is the configuration itself.";
      return false;
    }
    if (v.first != AIS::KEY_SETTING_KEY)
      for (const auto &k : tls_keys)
        if (isSameFile(path, k)) {
          error = quoted + " is a server's private key.";
          return false;
        }
  }
  return true;
}

bool ControlCore::setConfig(const std::string &json, std::string &error) {
  if (!validate(json, error))
    return false;

  std::lock_guard<std::mutex> lock(file_mtx);

  std::string out;
  try {
    JSON::Parser parser(JSON_DICT_SETTING);
    JSON::Document doc = parser.parse(json);
    if (!allowedPaths(doc.root, error))
      return false;
    // the password is changed by /api/password alone, whatever the document says
    putAuth(doc);
    JSON::Serializer serializer(JSON_DICT_SETTING);
    serializer.stringify(doc.root, out);
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }

  try {
    std::string ignore;
    writeFileAtomic(config_file + ".bak", Util::Helper::readFile(config_file),
                    ignore);
  } catch (const std::exception &) {
  }

  if (!writeFileAtomic(config_file, indented(out), error))
    return false;

  refreshAuthFields(out);
  refreshPlaces(out);

  config_dirty = true;

  return true;
}

// Reads the pre-managed-mode config file recorded by the installer in
// control.legacy_config; only that one path is ever served, never the
// active config itself.
bool ControlCore::readLegacyConfig(std::string &content) {
  std::string path;

  try {
    std::lock_guard<std::mutex> lock(file_mtx);

    JSON::Parser parser(JSON_DICT_SETTING);
    JSON::Document doc = parser.parse(Util::Helper::readFile(config_file));
    path = legacyConfigOf(doc.root);
  } catch (const std::exception &) {
    return false;
  }

  if (path.empty() || isSameFile(path, config_file))
    return false;

  try {
    content = Util::Helper::readFile(path);

    // syntactically valid JSON only; old configs may hold retired keys
    JSON::Parser parser(JSON_DICT_SETTING);
    parser.setSkipUnknown(true);
    parser.parse(content);
  } catch (const std::exception &) {
    return false;
  }

  return true;
}

void ControlCore::applyAuthFields(const JSON::Value &control) {
  bool wizard = false;
  LogLevel level = LogLevel::DEBUG;

  for (const auto &c : control.getObject().getMembers()) {
    if (c.Key() == AIS::KEY_SETTING_PASSWORD)
      password_hash = c.Get().to_string();
    else if (c.Key() == AIS::KEY_SETTING_SALT)
      password_salt = c.Get().to_string();
    else if (c.Key() == AIS::KEY_SETTING_ITERATIONS)
      password_iterations = std::atoi(c.Get().to_string().c_str());
    else if (c.Key() == AIS::KEY_SETTING_SETUP_CODE)
      setup_code = c.Get().to_string();
    else if (c.Key() == AIS::KEY_SETTING_TLS) {
      panel_tls = true;
      if (c.Get().isObject())
        for (const auto &t : c.Get().getObject().getMembers()) {
          if (t.Key() == AIS::KEY_SETTING_CERT)
            panel_tls_cert = t.Get().to_string();
          else if (t.Key() == AIS::KEY_SETTING_KEY)
            panel_tls_key = t.Get().to_string();
        }
    } else if (c.Key() == AIS::KEY_SETTING_WIZARD)
      wizard = Util::Parse::Switch(c.Get().to_string());
    else if (c.Key() == AIS::KEY_SETTING_LEVEL) {
      if (!Logger::parseLevel(c.Get().to_string(), level)) {
        Warning() << "Control: invalid log level \"" << c.Get().to_string()
                  << "\", using DEBUG";
        level = LogLevel::DEBUG;
      }
    }
  }

  wizard_flag = wizard;
  Logger::getInstance().setMinLevel(level);
}

void ControlCore::refreshAuthFields(const std::string &json) {
  try {
    JSON::Parser parser(JSON_DICT_SETTING);
    JSON::Document doc = parser.parse(json);

    const JSON::Value *v = doc.root[AIS::KEY_SETTING_CONTROL];
    std::lock_guard<std::mutex> lock(mtx);
    if (v && v->isObject())
      applyAuthFields(*v);
    else {
      wizard_flag = false;
      Logger::getInstance().setMinLevel(LogLevel::DEBUG);
    }
  } catch (const std::exception &) {
  }
}

bool ControlCore::wizardPending() {
  std::lock_guard<std::mutex> lock(mtx);
  return wizard_flag;
}

bool ControlCore::hasPassword() {
  std::lock_guard<std::mutex> lock(mtx);
  return !password_hash.empty();
}

// no iterations in the file: a password from before, one round of SHA-256
static std::string hashPassword(const std::string &password, const std::string &salt, int iterations) {
  return iterations > 0 ? Util::SHA256::pbkdf2(password, salt, iterations) : Util::SHA256::hex(salt + password);
}

bool ControlCore::verifyPassword(const std::string &password) {
  std::string hash, salt;
  int iterations;
  {
    std::lock_guard<std::mutex> lock(mtx);
    hash = password_hash;
    salt = password_salt;
    iterations = password_iterations;
  }

  if (hash.empty())
    return false;

  const std::string computed = hashPassword(password, salt, iterations);
  if (computed.size() != hash.size())
    return false;

  unsigned char diff = 0;
  for (size_t i = 0; i < computed.size(); i++)
    diff |= (unsigned char)(computed[i] ^ hash[i]);

  if (diff != 0)
    return false;

  // a password from before moves to the current hash now that it is at hand
  if (iterations != PASSWORD_ITERATIONS)
    setPassword(password);
  return true;
}

// A panel others can reach and no password yet: the first visitor has to show
// this code, which only who reads the machine's log has. Straight to the
// console, the journal: the log's buffer can be read from a viewer, and a log
// level can hide it.
void ControlCore::announceSetupCode() {
  if (!authRequired() || !password_hash.empty())
    return;

  if (setup_code.empty()) {
    // no 0/O, 1/I/L
    static const char letters[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
    std::random_device rd;
    std::uniform_int_distribution<int> pick(0, (int)sizeof(letters) - 2);
    for (int i = 0; i < 8; i++)
      setup_code += letters[pick(rd)];

    mutateConfig("keep the setup code", [&](JSON::Document &doc) {
      putAuth(doc);
      return true;
    });
  }
  std::cerr << "Control: to set the password, use setup code " << setup_code << std::endl;
}

bool ControlCore::checkSetupCode(std::string code) {
  Util::Convert::toUpper(code);
  std::lock_guard<std::mutex> lock(mtx);
  if (setup_code.empty() || code.size() != setup_code.size())
    return false;

  unsigned char diff = 0;
  for (size_t i = 0; i < code.size(); i++)
    diff |= (unsigned char)(code[i] ^ setup_code[i]);

  return diff == 0;
}

std::string ControlCore::randomHex(size_t length) {
  static const char *digits = "0123456789abcdef";

  std::random_device rd;
  std::string s;
  while (s.size() < length) {
    uint32_t r = rd();
    for (int j = 28; j >= 0 && s.size() < length; j -= 4)
      s += digits[(r >> j) & 0xF];
  }
  return s;
}

void ControlCore::setPassword(const std::string &password) {
  std::string salt = randomHex(32);
  std::string hash = hashPassword(password, salt, PASSWORD_ITERATIONS);

  {
    std::lock_guard<std::mutex> lock(mtx);
    password_salt = salt;
    password_hash = hash;
    password_iterations = PASSWORD_ITERATIONS;
    setup_code.clear();
  }

  mutateConfig("persist password", [&](JSON::Document &doc) {
    putAuth(doc);
    return true;
  });
}

bool ControlCore::mutateConfig(const char *what, const std::function<bool(JSON::Document &)> &fn,
                               std::string *error) {
  std::lock_guard<std::mutex> lock(file_mtx);
  std::string why;
  try {
    JSON::Parser parser(JSON_DICT_SETTING);
    JSON::Document doc = parser.parse(Util::Helper::readFile(config_file));

    if (!fn(doc))
      return true;

    std::string out;
    JSON::Serializer serializer(JSON_DICT_SETTING);
    serializer.stringify(doc.root, out);

    if (writeFileAtomic(config_file, indented(out), why))
      return true;
  } catch (const std::exception &e) {
    why = e.what();
  }
  Error() << "Control: cannot " << what << ": " << why;
  if (error)
    *error = why;
  return false;
}

void ControlCore::removeAuth(JSON::JSON &control) {
  control.Remove(AIS::KEY_SETTING_PASSWORD);
  control.Remove(AIS::KEY_SETTING_SALT);
  control.Remove(AIS::KEY_SETTING_ITERATIONS);
  control.Remove(AIS::KEY_SETTING_SETUP_CODE);
  control.Remove(AIS::KEY_SETTING_TLS);
}

// the document's "control" with the password and the setup code this process holds
void ControlCore::putAuth(JSON::Document &doc) {
  std::lock_guard<std::mutex> lock(mtx);
  if (password_hash.empty() && setup_code.empty() && !panel_tls && !doc.root[AIS::KEY_SETTING_CONTROL])
    return;

  JSON::JSON &control = doc.root.object(AIS::KEY_SETTING_CONTROL, doc.pool);
  removeAuth(control);
  if (!password_hash.empty()) {
    control.Set(AIS::KEY_SETTING_PASSWORD, password_hash, doc.pool);
    control.Set(AIS::KEY_SETTING_SALT, password_salt, doc.pool);
    if (password_iterations > 0) {
      JSON::Value iterations;
      iterations.setInt(password_iterations);
      control.Set(AIS::KEY_SETTING_ITERATIONS, iterations);
    }
  }
  if (!setup_code.empty())
    control.Set(AIS::KEY_SETTING_SETUP_CODE, setup_code, doc.pool);
  if (panel_tls) {
    JSON::JSON &tls = control.object(AIS::KEY_SETTING_TLS, doc.pool);
    tls.Set(AIS::KEY_SETTING_CERT, panel_tls_cert, doc.pool);
    tls.Set(AIS::KEY_SETTING_KEY, panel_tls_key, doc.pool);
  }
}

std::string ControlCore::configFolderPath() const {
  const std::string folder = configFolder(config_file);
#ifndef _WIN32
  char full[PATH_MAX];
  if (realpath(folder.c_str(), full))
    return full;
#endif
  return folder;
}

bool ControlCore::panelTLS(std::string &cert, std::string &key) {
  std::lock_guard<std::mutex> lock(mtx);
  if (!cli_tls_cert.empty()) {
    cert = cli_tls_cert;
    key = cli_tls_key;
    return true;
  }
  const std::string folder = configFolder(config_file);
  cert = panel_tls_cert.empty() ? "" : Util::Helper::resolvePath(panel_tls_cert, folder);
  key = panel_tls_key.empty() ? "" : Util::Helper::resolvePath(panel_tls_key, folder);
  return panel_tls;
}

bool ControlCore::panelTLSFields(std::string &cert, std::string &key) {
  std::lock_guard<std::mutex> lock(mtx);
  const bool fixed = !cli_tls_cert.empty();
  cert = fixed ? cli_tls_cert : panel_tls_cert;
  key = fixed ? cli_tls_key : panel_tls_key;
  return fixed;
}

void ControlCore::setPanelTLSFromCommandLine(const std::string &cert, const std::string &key) {
  std::lock_guard<std::mutex> lock(mtx);
  cli_tls_cert = cert;
  cli_tls_key = key;
}

bool ControlCore::setPanelTLS(const std::string &cert, const std::string &key, std::string &error) {
  std::string old_cert, old_key;
  if (panelTLSFields(old_cert, old_key)) {
    error = "HTTPS of the panel is set on the command line.";
    return false;
  }
  if (cert.empty() != key.empty()) {
    error = "HTTPS needs both the certificate and the private key.";
    return false;
  }
  if (!cert.empty()) {
    const std::string folder = configFolder(config_file);
    const std::string cert_path = Util::Helper::resolvePath(cert, folder);
    const std::string key_path = Util::Helper::resolvePath(key, folder);
    // as on a save: a path elsewhere can only be written in the file by hand, and stays if unchanged
    const std::string *const fields[][2] = {{&cert, &old_cert}, {&key, &old_key}};
    for (const auto &f : fields) {
      const std::string path = Util::Helper::resolvePath(*f[0], folder);
      if (*f[0] != *f[1] && !inFolder(path, folder)) {
        error = "\"" + *f[0] + "\" is outside the configuration's folder (" + folder + "): a path elsewhere can only be written in the file by hand.";
        return false;
      }
      if (isSameFile(path, config_file)) {
        error = "\"" + *f[0] + "\" is the configuration itself.";
        return false;
      }
    }
    // what does not load here would stop the next start
    error = IO::TCPServer::checkTLS(cert_path, key_path);
    if (!error.empty())
      return false;
  }

  const bool old_tls = !old_cert.empty();
  const auto keep = [&](bool on, const std::string &c, const std::string &k) {
    std::lock_guard<std::mutex> lock(mtx);
    panel_tls = on;
    panel_tls_cert = c;
    panel_tls_key = k;
  };
  keep(!cert.empty(), cert, key);
  if (mutateConfig("persist panel HTTPS", [&](JSON::Document &doc) {
        putAuth(doc);
        return true;
      }, &error))
    return true;
  keep(old_tls, old_cert, old_key);
  return false;
}

std::string ControlCore::getDeviceListJSON() {
  // hardware comes and goes between engine runs, so the list the UI shows must
  // be rescanned; enumerating can disturb a device being opened or streaming
  // (the SDRplay API in particular) and clears the claimed flags, so leave the
  // list alone for the whole engine session, device open through close
  std::lock_guard<std::mutex> lock(scan_mtx);

  if (!engine_busy) {
    if (!scanner)
      scanner.reset(new DeviceManager());

    scanner->refreshDevices();
  }

  return DeviceManager::getDeviceListJSON();
}

std::string ControlCore::getSerialListJSON() {
  std::string s;
  JSON::Writer w(s);
  w.beginArray();
  for (const auto &path : Device::SerialPort::getDevicePathsCopy())
    w.val(path);
  w.endArray().finish();
  return s;
}

std::string ControlCore::defaultStatsFile() const { return statsPath(config_file); }

bool ControlCore::validate(const std::string &json, std::string &error) {
  try {
    Engine scratch;
    Config c(scratch);
    c.setManaged(getControlPort(), getViewerPort(), getBindAddress());
    c.set(json);
    c.finish();
#ifdef HASAPI
    for (const auto &spec : scratch.site_config.servers)
      if (!spec.tls_cert.empty()) {
        const std::string problem = IO::TCPServer::checkTLS(spec.tls_cert, spec.tls_key);
        if (!problem.empty())
          throw std::runtime_error("the server on port " + std::to_string(spec.port) + ": " + problem);
      }
#endif
  } catch (const std::exception &e) {
    error = e.what();
    return false;
  }
  return true;
}

bool ControlCore::writeFileAtomic(const std::string &path,
                                  const std::string &content,
                                  std::string &error) {
  if (content.empty() || content.back() != '\n')
    return Util::Helper::writeFileAtomic(path, content + "\n", error);

  return Util::Helper::writeFileAtomic(path, content, error);
}

// A configuration without a main server under "services" nor a viewer
// section (a fresh one, or the installer's seed when it imports a legacy
// config) gets the main server with the map's defaults, the tracking backup
// beside the configuration, and the default station named after the machine.
// A viewer section from before stays for the panel to move.
void ControlCore::addMainDefaults() {
  mutateConfig("add main defaults", [&](JSON::Document &doc) {
    const JSON::Value *ctrl = doc.root[AIS::KEY_SETTING_CONTROL];
    if (ctrl && ctrl->isObject() && ctrl->getObject()[AIS::KEY_SETTING_VIEWER])
      return false;
    // a v0.70 file: its backup, position and name go with the panel's move of this section
    if (doc.root[AIS::KEY_SETTING_SERVER])
      return false;
    std::vector<JSON::Value> &services = doc.root.array(AIS::KEY_SETTING_SERVICES, doc.pool);
    for (const auto &entry : services) {
      const JSON::Value *main = entry.isObject() ? entry.getObject()[AIS::KEY_SETTING_MAIN] : nullptr;
      if (main && Util::Parse::Switch(main->to_string()))
        return false;
    }

    JSON::JSON *main = doc.pool.addObject();
    main->Add(AIS::KEY_SETTING_MAIN, true);
    main->Add(AIS::KEY_SETTING_REALTIME, true);
    main->Add(AIS::KEY_SETTING_DECODER, true);
    main->Add(AIS::KEY_SETTING_LOG, false);
    main->Add(AIS::KEY_SETTING_STATION_LINK, std::string("https://www.aiscatcher.org/stations"), doc.pool);
    JSON::Value entry;
    entry.setObject(main);
    services.insert(services.begin(), entry);

    JSON::JSON &tracking = doc.root.object(AIS::KEY_SETTING_TRACKING, doc.pool);
    if (!tracking[AIS::KEY_SETTING_FILE])
      tracking.Set(AIS::KEY_SETTING_FILE, statsPath(config_file), doc.pool);

    if (!doc.root[AIS::KEY_SETTING_STATIONS]) {
      JSON::JSON *station = doc.pool.addObject();
      station->Add(AIS::KEY_SETTING_ID, 0);
      const std::string host = machineName();
      if (!host.empty())
        station->Add(AIS::KEY_SETTING_NAME, host, doc.pool);
      station->Add(AIS::KEY_SETTING_SHARE_LOC, true);
      JSON::Value one;
      one.setObject(station);
      doc.root.array(AIS::KEY_SETTING_STATIONS, doc.pool).push_back(one);
    }
    return true;
  });
}

void ControlCore::persistEngineField(bool on) {
  mutateConfig("persist engine state", [on](JSON::Document &doc) {
    doc.root.Set(AIS::KEY_SETTING_ENGINE, on ? "on" : "off", doc.pool);
    return true;
  });
}

bool ControlCore::engineDesired() {
  std::lock_guard<std::mutex> lock(mtx);
  return desired;
}

uint32_t ControlCore::statusStamp() {
  std::lock_guard<std::mutex> lock(mtx);
  uint32_t bits =
      (state == EngineState::Running ? 1 : 0) | (desired ? 2 : 0) |
      ((desired && auto_retry && state != EngineState::Running) ? 4 : 0);
  return (status_epoch << 3) | bits;
}

bool ControlCore::engineRetrying() {
  std::lock_guard<std::mutex> lock(mtx);
  return desired && auto_retry && state != EngineState::Running;
}

void ControlCore::reportRunning() {
  std::lock_guard<std::mutex> lock(mtx);
  state = EngineState::Running;
  engine_start_time = std::time(nullptr);
  auto_retry = false;
  status_epoch++;
}

void ControlCore::reportStopped() {
  std::lock_guard<std::mutex> lock(mtx);
  bool was_running = state == EngineState::Running;
  bool healthy = was_running &&
                 std::time(nullptr) - engine_start_time >= RETRY_HEALTHY_UPTIME;
  state = EngineState::Stopped;
  status_epoch++;

  if (restart_pending)
    restart_pending = false;
  else if (desired) {
    if (was_running || auto_retry) {
      if (healthy)
        retry_delay = 0;
      retry_delay = retry_delay ? MIN(retry_delay * 2, RETRY_DELAY_MAX)
                                : RETRY_DELAY_FIRST;
      auto_retry = true;
      retry_pending = true;
    } else {
      desired = false;
      resetRetry();
    }
  }
}

void ControlCore::engineFailed() {
  std::lock_guard<std::mutex> lock(mtx);
  restart_pending = false;
}

void ControlCore::waitForCommand() {
  std::unique_lock<std::mutex> lock(mtx);
  cv.wait_for(lock, std::chrono::milliseconds(250), [this] { return desired; });
}

int ControlCore::consumeRetryDelay() {
  std::lock_guard<std::mutex> lock(mtx);
  if (!retry_pending)
    return 0;
  retry_pending = false;
  return retry_delay;
}

std::string ControlCore::defaultPlaceDirectory() const {
  const auto slash = config_file.find_last_of("/\\");
  return (slash == std::string::npos ? "." : config_file.substr(0, slash)) +
         "/places";
}

void ControlCore::refreshPlaces(const std::string &json) {
  std::string directory = defaultPlaceDirectory();
  bool enabled = false;
  try {
    JSON::Parser parser(JSON_DICT_SETTING);
    auto doc = parser.parse(json);
    const auto *tracking = doc.root[AIS::KEY_SETTING_TRACKING];
    const auto *path = tracking && tracking->isObject() ? tracking->getObject()[AIS::KEY_SETTING_PLACES] : nullptr;
    // a file from before has it under the viewer section
    const auto *control = doc.root[AIS::KEY_SETTING_CONTROL];
    const auto *viewer = control && control->isObject() ? control->getObject()[AIS::KEY_SETTING_VIEWER] : nullptr;
    if (!path && viewer && viewer->isObject())
      path = viewer->getObject()[AIS::KEY_SETTING_PLACES];
    if (path && path->isString() && !path->getString().empty()) {
      directory = path->getString();
      enabled = true;
    }
  } catch (const std::exception &e) {
    Warning() << "Places: " << e.what();
  }
  std::lock_guard<std::mutex> lock(place_mtx);
  if (!enabled)
    place_store.reset();
  else if (!place_store || directory != place_directory)
    place_store = std::make_shared<PlaceCatalogue>(directory);
  place_directory = directory;
  place_enabled = enabled;
}

std::shared_ptr<PlaceCatalogue> ControlCore::getPlaces() {
  std::lock_guard<std::mutex> lock(place_mtx);
  return place_store;
}
std::string ControlCore::placeSettings() {
  std::lock_guard<std::mutex> lock(place_mtx);
  std::string out;
  JSON::Writer w(out);
  w.beginObject().kv("enabled", place_enabled).kv("directory", place_directory).endObject().finish();
  return out;
}

bool ControlCore::enablePlaces(std::string &error) {
  const bool written = mutateConfig("enable places", [&](JSON::Document &doc) {
    JSON::JSON &tracking = doc.root.object(AIS::KEY_SETTING_TRACKING, doc.pool);
    const JSON::Value *path = tracking[AIS::KEY_SETTING_PLACES];
    if (path && path->isString() && !path->getString().empty())
      return false;
    tracking.Set(AIS::KEY_SETTING_PLACES, defaultPlaceDirectory(), doc.pool);
    return true;
  }, &error);
  if (!written)
    return false;
  refreshPlaces(getConfig());
  config_dirty = true;
  return true;
}
