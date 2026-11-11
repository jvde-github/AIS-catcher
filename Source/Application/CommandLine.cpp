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

#include <algorithm>
#include <iostream>
#include <fstream>
#include <string.h>
#include <memory>
#include <cctype>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "AIS-catcher.h"

#include "CommandLine.h"
#include "Receiver.h"
#include "Engine.h"
#include "Config.h"
#include "SiteConfig.h"
#include "JSON/Writer.h"
#include "Site.h"
#include "JSON.h"
#include "JSON/Parser.h"
#include "Helper.h"
#include "Logger.h"
#include "SHA256.h"
#include "ControlCore.h"

namespace CommandLine
{

// -------------------------------
// every option writes into a document in the configuration file's format, which the
// configuration reader applies as it applies a file; an option's settings are tried first on
// an object of the kind that takes them, so a mistake is reported at the option that made it

namespace
{
	using Keys = std::vector<AIS::Keys>;
	typedef JSON::JSON Object;

	// a key given twice is kept twice: this is the last one
	JSON::Value *last(Object &o, int key)
	{
		auto &members = o.getMembers();
		for (auto it = members.rbegin(); it != members.rend(); ++it)
			if (it->Key() == key)
				return &it->Get();
		return nullptr;
	}

	bool isActive(const Object &o)
	{
		for (const auto &m : o.getMembers())
			if (m.Key() == AIS::KEY_SETTING_ACTIVE)
				return Util::Parse::Switch(m.Get().to_string());
		return true;
	}

	std::string currentDirectory()
	{
		char cwd[4096];
#ifdef _WIN32
		return _getcwd(cwd, sizeof(cwd)) ? cwd : "";
#else
		return getcwd(cwd, sizeof(cwd)) ? cwd : "";
#endif
	}

#if defined(HASWEBVIEWER) || defined(HASDELIVERY)
	AIS::Keys lookup(const std::string &name, const std::string &where)
	{
		std::string upper = name;
		Util::Convert::toUpper(upper);
		AIS::Keys key;
		if (!AIS::lookupSettingKey(upper, key))
			throw std::runtime_error(where + ": unknown setting \"" + name + "\".");
		return key;
	}
#endif

	// the reader logs again what a trial object logs when it applies the setting:
	// while trying, the log keeps quiet
	struct Quiet
	{
		bool on;
		LogLevel level;
		Quiet(bool quiet = true) : on(quiet), level(Logger::getInstance().getMinLevel())
		{
			if (on)
				Logger::getInstance().setMinLevel(LogLevel::EMPTY);
		}
		~Quiet()
		{
			if (on)
				Logger::getInstance().setMinLevel(level);
		}
	};

}

// the key/value pairs after an option's positional arguments; -G's go to the log itself
static void parseSettings(const std::vector<std::string> &args, int ptr, Object *into, JSON::Pool &pool, Setting *apply = nullptr)
{
	const int argc = (int)args.size();
	std::string flag_context;
	if (ptr >= 0 && ptr < argc && args[ptr][0] == '-')
		flag_context = args[ptr];

	ptr++;

	while (ptr < argc - 1 && args[ptr][0] != '-')
	{
		std::string option = args[ptr++];
		std::string arg = args[ptr++];

		Util::Convert::toUpper(option);
		AIS::Keys key;

		try
		{
			if (AIS::lookupSettingKey(option, key))
			{
				if (apply)
					apply->SetKey(key, arg);
				if (into)
					into->Add(key, arg, pool);
			}
			else
			{
				std::string lower = option;
				Util::Convert::toLower(lower);
				std::string msg = apply && !apply->getName().empty() ? apply->getName() + ": " : "";
				msg += "unknown setting \"" + lower + "\".";
				throw std::runtime_error(msg);
			}
		}
		catch (const std::exception &e)
		{
			std::string msg = e.what();
			if (!flag_context.empty())
				msg += " (in " + flag_context + " " + option + " " + arg + ")";

			throw std::runtime_error(msg);
		}
	}
}

static bool isOption(const std::string &s)
{
	return s.length() >= 2 && s[0] == '-' && std::isalpha((unsigned char)s[1]);
}

#ifdef HASWEBVIEWER
static bool isNumber(const std::string &s)
{
	if (s.empty())
		return false;

	for (char c : s)
		if (!std::isdigit((unsigned char)c))
			return false;

	return true;
}
#endif

// Assign the positional arguments to keys, then hand any remaining key/value
// pairs to parseSettings. The caller has already asserted the argument count.
static void positional(const Keys &keys, const std::vector<std::string> &args, int ptr, int count, Object &into, JSON::Pool &pool)
{
	int i = 0;
	for (AIS::Keys k : keys)
		into.Add(k, args[ptr + 1 + i++], pool);

	if (count > (int)keys.size())
		parseSettings(args, ptr + (int)keys.size(), &into, pool);
}

static Keys leading(bool given, AIS::Keys key)
{
	return given ? Keys{key} : Keys();
}

static void Assert(bool b, const std::string &context, const std::string &msg = "")
{
	if (!b)
	{
		throw std::runtime_error("syntax error on command line with setting \"" + context + "\". " + msg + "\n");
	}
}

namespace
{
	struct Actions
	{
		bool list_devices = false, list_devices_json = false, list_support = false, list_options = false, no_run = false, new_key = false;
	};

	// the command line read into a document; -h, -l and -L are noted as actions, -G acts on the log
	class Translation
	{
		// every option and every -C file adds to it, in command line order
		JSON::Document tree;
		Object &doc = tree.root;
		JSON::Pool &pool = tree.pool;
		std::vector<std::unique_ptr<JSON::Document>> files;
		// until an input is named the first receiver is the default one, on whatever device is found
		bool named = false;
		// -v is every receiver's, one named after it too
		bool verbose = false;
		// the -N servers, joined to the document once all are read
		std::vector<Object *> viewers;
		bool ported = false;
		Config::Origins origins;
		// the last -C file's folder
		std::string base;

		// the engine the reader makes of the document so far, made again after every option
		// so a wrong one is refused where it stands
		std::unique_ptr<Engine> state_;

		void check(const std::string &where)
		{
			Quiet quiet;
			state_.reset();
			std::unique_ptr<Engine> e(new Engine());
			Config c(*e);
			try
			{
				c.set(tree);
			}
			catch (const std::exception &x)
			{
				const std::string msg = x.what();
				throw std::runtime_error(where.empty() || msg.compare(0, where.size(), where) == 0 ? msg : where + ": " + msg);
			}
			state_ = std::move(e);
		}

		// the object or the list under a key, the last one given; made when there is none
		Object &object(Object &in, AIS::Keys key)
		{
			JSON::Value *v = last(in, key);
			if (v && v->isObject())
				return v->getObject();
			Object *made = pool.addObject();
			in.Add(key, made);
			return *made;
		}
		std::vector<JSON::Value> &list(Object &in, AIS::Keys key)
		{
			JSON::Value *v = last(in, key);
			if (!v || !v->isArray())
			{
				in.Add(key, JSON::Value(pool.addArray()));
				v = &in.getMembers().back().Get();
			}
			return v->getArray();
		}
		Object &item(std::vector<JSON::Value> &in)
		{
			Object *made = pool.addObject();
			in.push_back(JSON::Value(made));
			return *made;
		}
		void put(Object &into, AIS::Keys key, const std::string &text) { into.Add(key, text, pool); }

		std::vector<JSON::Value> &receivers() { return list(doc, AIS::KEY_SETTING_RECEIVER); }

		// the receiver the reader made of rx
		Receiver &state(const Object &rx)
		{
			std::size_t n = 0;
			for (JSON::Value &r : receivers())
			{
				if (&r.getObject() == &rx)
					break;
				n += isActive(r.getObject());
			}
			return *state_->receivers[n];
		}

		static std::string label(const Object &server)
		{
			const auto &m = server.getMembers();
			return !m.empty() && m.front().Key() == AIS::KEY_SETTING_PORT ? "-N " + m.front().Get().to_string() : "-N";
		}

		// the channel given last counts, with its NMEA designation
		static void noChannel(Object &rx)
		{
			rx.Remove(AIS::KEY_SETTING_CHANNEL);
			rx.Remove(AIS::KEY_SETTING_NMEA_CHANNEL);
		}

#ifdef HASWEBVIEWER
		// KEYS name:scopes:sha256,... as the file's keys array; a bare hash is a data key named key
		JSON::Value keysFromText(const std::string &text, const std::string &where)
		{
			JSON::Value keys(pool.addArray());
			std::vector<std::string> entries;
			Util::Parse::Split(text, ',', entries);
			for (const auto &entry : entries)
			{
				std::vector<std::string> parts;
				Util::Parse::Split(entry, ':', parts);
				if (parts.size() != 1 && parts.size() != 3)
					throw std::runtime_error(where + ": KEYS takes name:scopes:sha256 entries separated by commas, scopes data or data+system.");
				const bool with_name = parts.size() >= 3;
				Object &key = item(keys.getArray());
				put(key, AIS::KEY_SETTING_NAME, with_name ? parts[0] : "key");
				put(key, AIS::KEY_SETTING_SHA256, parts[with_name ? 2 : 0]);
				std::vector<std::string> names;
				Util::Parse::Split(with_name ? parts[1] : "data", '+', names);
				std::vector<JSON::Value> &scopes = list(key, AIS::KEY_SETTING_SCOPES);
				for (const auto &n : names)
				{
					JSON::Value scope;
					scope.setString(pool.addString(n));
					scopes.push_back(scope);
				}
			}
			return keys;
		}
#endif

	public:
		std::vector<std::string> said;
		Actions actions;
		bool json = false;
		std::string json_file;

		Translation()
		{
			put(doc, AIS::KEY_SETTING_CONFIG, "aiscatcher");
			doc.Add(AIS::KEY_SETTING_VERSION, 1);
			put(item(receivers()), AIS::KEY_SETTING_INPUT, "none");
			check("");
		}

		~Translation()
		{
			Quiet quiet;
			state_.reset();
		}

		Object &current()
		{
			std::vector<JSON::Value> &all = receivers();
			for (auto it = all.rbegin(); it != all.rend(); ++it)
				if (isActive(it->getObject()))
					return it->getObject();
			return all.front().getObject();
		}

		// the first input named is the default receiver's, with what was set before it
		Object &newReceiver()
		{
			if (named)
				return item(receivers());
			named = true;
			Object &first = receivers().front().getObject();
			first.Remove(AIS::KEY_SETTING_INPUT);
			return first;
		}

		Object &block(Object &rx, AIS::Keys key)
		{
			JSON::Value *b = last(rx, key);
			if (b && b->isObject() && isActive(b->getObject()))
				return b->getObject();
			Object *made = pool.addObject();
			rx.Add(key, made);
			return *made;
		}

		void device(Type t, const Keys &keys, const std::vector<std::string> &args, int ptr, int count)
		{
			Object &rx = newReceiver();
			std::string name = Util::Parse::DeviceTypeString(t);
			Util::Convert::toLower(name);
			put(rx, AIS::KEY_SETTING_INPUT, name);
			positional(keys, args, ptr, count, block(rx, DeviceManager::keyForType(t)), pool);
		}

		void output(AIS::Keys kind, const Keys &keys, const std::vector<std::string> &args, int ptr, int count)
		{
			positional(keys, args, ptr, count, item(list(doc, kind)), pool);
		}

		void addEngine(Object &rx, int type)
		{
			put(item(list(rx, AIS::KEY_SETTING_ENGINES)), AIS::KEY_SETTING_MODEL_TYPE, Config::modelName(type));
		}

		Object &lastModelSettings(Object &rx)
		{
			std::vector<JSON::Value> &engines = list(rx, AIS::KEY_SETTING_ENGINES);
			for (auto e = engines.rbegin(); e != engines.rend(); ++e)
				if (e->isObject() && isActive(e->getObject()))
					return object(e->getObject(), AIS::KEY_SETTING_SETTINGS);
			throw std::runtime_error("internal error: no model for -go");
		}

		// the feed: -X or a file switches it on, by saying so or with a key; none switches it off
		bool share(bool stated, bool on, const std::string &key)
		{
			JSON::Value *sharing = last(doc, AIS::KEY_SETTING_SHARING);
			const bool feed = on || !key.empty() || (sharing && sharing->getBool()) || last(doc, AIS::KEY_SETTING_SHARING_KEY);
			if (sharing)
				sharing->setBool(feed);
			else if (stated)
				doc.Add(AIS::KEY_SETTING_SHARING, feed);
			if (!key.empty())
				put(doc, AIS::KEY_SETTING_SHARING_KEY, key);
			return feed;
		}

		// a file's receiver onto rx
		void receiver(Object &rx, const std::vector<JSON::Member> &members)
		{
			for (const auto &m : members)
				if (m.Key() == AIS::KEY_SETTING_CHANNEL)
					noChannel(rx);

			for (const auto &m : members)
				// an empty channel is none given
				if ((m.Key() != AIS::KEY_SETTING_CHANNEL && m.Key() != AIS::KEY_SETTING_NMEA_CHANNEL) || !m.Get().to_string().empty())
					rx.Add(m.Key(), m.Get());
		}

		// -C: the file's sections join the document, its files resolved against its folder
		void file(const std::string &path)
		{
			JSON::Parser parser(JSON_DICT_SETTING);
			const std::string content = Util::Helper::readFile(path);
			try
			{
				files.push_back(std::unique_ptr<JSON::Document>(new JSON::Document(parser.parse(content))));
			}
			catch (const std::exception &e)
			{
				throw std::runtime_error("-C " + path + ": " + e.what());
			}
			JSON::Document &parsed = *files.back();

			Config::preamble(parsed.getMembers());
			base = Config::folderOf(path);
			// a main-section receiver that names no input is the current one's, not the file's first
			bool unplaced = Config::normalise(parsed, base, said, false);

			bool tracked = false, shares = false, on = false;
			std::string key;
			const JSON::Value *zones = nullptr;
			for (const auto &m : parsed.getMembers())
			{
				const AIS::Keys name = (AIS::Keys)m.Key();
				const JSON::Value &v = m.Get();
				switch (name)
				{
				case AIS::KEY_SETTING_CONFIG:
				case AIS::KEY_SETTING_VERSION:
					break;
				case AIS::KEY_SETTING_RECEIVER:
					for (const auto &entry : v.getArray())
					{
						if (!Config::isActiveObject(entry))
							receivers().push_back(entry);
						else
							receiver(unplaced ? current() : newReceiver(), entry.getObject().getMembers());
						unplaced = false;
					}
					break;
				case AIS::KEY_SETTING_SHARING:
					shares = true;
					on = Util::Parse::Switch(v.to_string());
					break;
				case AIS::KEY_SETTING_SHARING_KEY:
					key = v.to_string();
					break;
				case AIS::KEY_SETTING_SHARING_ZONE:
					if (v.isArray() && !v.getArray().empty())
						zones = &v;
					break;
				case AIS::KEY_SETTING_TRACKING:
					// one "tracking" for all: the file's first adds to it
					if (!v.isObject())
						doc.Add(name, v);
					else if (!tracked)
					{
						tracked = true;
						Object &tracking = object(doc, name);
						for (const auto &t : v.getObject().getMembers())
							tracking.Add(t.Key(), t.Get());
					}
					break;
				default:
					// an array adds its entries to the document's one of that name
					if (v.isArray())
						for (const auto &entry : v.getArray())
							list(doc, name).push_back(entry);
					else if (name == AIS::KEY_SETTING_SERVER)
						list(doc, name).push_back(v);
					else
						doc.Add(name, v);
				}
			}

			// as the reader has it: a key that is no UUID counts only where sharing is on
			if ((shares || last(doc, AIS::KEY_SETTING_SHARING)) && !on && !key.empty() && !Util::Helper::isUUID(key))
			{
				said.push_back("Config file: \"sharing_key\" is not a UUID and sharing is off: ignored.");
				key.clear();
			}
			if (share(shares, on, key) && zones)
			{
				doc.Remove(AIS::KEY_SETTING_SHARING_ZONE);
				doc.Add(AIS::KEY_SETTING_SHARING_ZONE, *zones);
			}

			check("-C " + path);
		}

		void read(const std::vector<std::string> &args);

		// once every option is read: -v is every receiver's, and each -N a server after the files'
		void settle()
		{
			if (verbose)
				for (JSON::Value &rx : receivers())
					if (isActive(rx.getObject()))
						put(rx.getObject(), AIS::KEY_SETTING_VERBOSE, "on");
			if (viewers.empty())
				return;

			std::vector<JSON::Value> &servers = list(doc, AIS::KEY_SETTING_SERVER);
			origins.servers.assign(servers.size(), std::string());
			for (Object *server : viewers)
			{
				// its files are in the last file's folder
				Config::resolveFiles(*server, base, pool);
				origins.servers.push_back(label(*server));
				servers.push_back(JSON::Value(server));
			}
			viewers.clear();
		}

		// the document, with which of its servers are -N's; without, the copy -J writes
		std::string document(Config::Origins *from) const
		{
			JSON::Serializer serializer(JSON_DICT_SETTING);
			serializer.setExactFloats(true);
			std::string text;
			serializer.stringify(doc, text);
			if (from)
				*from = origins;
			else
			{
				JSON::Parser parser(JSON_DICT_SETTING);
				JSON::Document copy = parser.parse(text);
				std::vector<std::string> notes;
				Config::normalise(copy, currentDirectory(), notes);
				text.clear();
				serializer.stringify(copy.root, text);
			}
			return JSON::indented(text) + "\n";
		}
	};

	void Translation::read(const std::vector<std::string> &args)
	{
		const std::string MSG_NO_PARAMETER = "does not allow additional parameter.";
		const int argc = (int)args.size();
		int ptr = 1;

		while (ptr < argc)
		{
			Object &receiver = current();

			const std::string &param = args[ptr];
			Assert(param[0] == '-', param, "setting does not start with \"-\".");

			int count = 0;
			while (ptr + count + 1 < argc && !isOption(args[ptr + 1 + count]))
				count++;

			std::string arg1 = count >= 1 ? args[ptr + 1] : std::string();
			std::string arg2 = count >= 2 ? args[ptr + 2] : std::string();

			// -G sets the log itself
			Quiet quiet(param[1] != 'G');

			switch (param[1])
			{
			case 'G':
				Assert(count % 2 == 0, param, "requires parameters in key/value pairs");

				// the first SYSTEM on: the console goes quiet, the system log takes it all
				if (Logger::getInstance().hasConsoleListener())
				{
					for (int k = 0; k + 1 < count; k += 2)
					{
						std::string key = args[ptr + 1 + k];
						Util::Convert::toUpper(key);
						if (key == "SYSTEM" && Util::Parse::Switch(args[ptr + 2 + k]))
						{
							Logger::getInstance().removeConsoleListener();
							Logger::getInstance().setMinLevel(LogLevel::DEBUG);
							break;
						}
					}
				}
				parseSettings(args, ptr, nullptr, pool, &Logger::getInstance());
				break;
			case 's':
				Assert(count == 1, param, "does require one parameter [sample rate].");
				put(receiver, AIS::KEY_SETTING_SAMPLE_RATE, arg1);
				break;
			case 'm':
				Assert(count == 1, param, "requires one parameter [model number].");
				addEngine(receiver, Util::Parse::Integer(arg1, 0, AIS::MODEL_MAX));
				break;
			case 'M':
				Assert(count <= 1, param, "requires zero or one parameter [DT].");
				put(receiver, AIS::KEY_SETTING_META, arg1);
				break;
			case 'c':
				Assert(count <= 2 && count >= 1, param, "requires one or two parameter [AB/CD]].");
				noChannel(receiver);
				if (!arg1.empty())
					put(receiver, AIS::KEY_SETTING_CHANNEL, arg1);
				if (!arg2.empty())
					put(receiver, AIS::KEY_SETTING_NMEA_CHANNEL, arg2);
				break;
			case 'C':
				Assert(count == 1, param, "one parameter required: filename");

				if (!arg1.empty())
					file(arg1);
				break;
			case 'N':
#ifdef HASWEBVIEWER
				Assert(count > 0, param, "requires at least one parameter");
				{
					// -N port starts a server unless the last -N has none yet; -N key value adds to the last one
					const int port = isNumber(arg1) ? 1 : 0;
					Assert((count - port) % 2 == 0, param, "usage: -N [port] [key value]..., with a value that has spaces in quotes, e.g. -N 8100 station \"my station\".");
					if (viewers.empty() || (port && ported))
					{
						viewers.push_back(pool.addObject());
						ported = false;
					}
					Object &server = *viewers.back();
					if (port)
					{
						// first: a PORT among its settings counts over it
						server.getMembers().insert(server.getMembers().begin(), JSON::Member(AIS::KEY_SETTING_PORT, pool.addString(arg1)));
						ported = true;
					}
					for (int k = 1 + port; k + 1 <= count; k += 2)
					{
						const AIS::Keys key = lookup(args[ptr + k], "-N " + arg1);
						const std::string &value = args[ptr + k + 1];
						if (key == AIS::KEY_SETTING_PLACE_GEOMETRY)
							put(object(server, AIS::KEY_SETTING_VIEW), key, value);
						else if (key == AIS::KEY_SETTING_TLS_CERT)
							put(object(server, AIS::KEY_SETTING_TLS), AIS::KEY_SETTING_CERT, value);
						else if (key == AIS::KEY_SETTING_TLS_KEY)
							put(object(server, AIS::KEY_SETTING_TLS), AIS::KEY_SETTING_KEY, value);
						else if (key == AIS::KEY_SETTING_KEYS)
							server.Add(key, keysFromText(value, label(server)));
						else
							put(server, key, value);
					}
				}
#else
				throw std::runtime_error("Option -N needs a build with the web viewer (cmake -DWEBVIEWER=ON).");
#endif
				break;
			case 'S':
				Assert(count >= 1 && count % 2 == 1, param, "requires at least one parameter [port].");
				output(AIS::KEY_SETTING_TCP_LISTENER, {AIS::KEY_SETTING_PORT}, args, ptr, count);
				break;
			case 'f':
				output(AIS::KEY_SETTING_FILE_OUTPUT, leading(count % 2, AIS::KEY_SETTING_FILE), args, ptr, count);
				break;
			case 'v':
				Assert(count <= 1, param);
				if (param.length() == 3 && param[2] == '+')
				{
					// -v+ applies to last receiver only, no time parameter
					Assert(count == 0, param, "no parameters allowed with -v+");
					put(receiver, AIS::KEY_SETTING_VERBOSE, "on");
				}
				else
				{
					// -v or -v* applies to all receivers (after loop)
					Assert(param.length() == 2 || (param.length() == 3 && param[2] == '*'), param, "invalid verbose option");
					verbose = true;
					if (count == 1)
					{
						Util::Parse::Integer(arg1, 1, 3600);
						put(doc, AIS::KEY_SETTING_VERBOSE_TIME, arg1);
					}
				}
				break;
			case 'O':
				Assert(count == 1, param);
				doc.Add(AIS::KEY_SETTING_OWN_MMSI, (int64_t)Util::Parse::Integer(arg1, 1, 999999999));
				break;
			case 'T':
				Assert(count == 1 || (count == 2 && arg2 == "nomsg_only"), param, "timeout requires one parameter with optional \"nomsg_only\".");
				Util::Parse::Integer(arg1, 1, 3600);
				put(doc, AIS::KEY_SETTING_TIMEOUT, arg1);
				if (count == 2)
					doc.Add(AIS::KEY_SETTING_TIMEOUT_NOMSG, true);
				break;
			case 'q':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				put(doc, AIS::KEY_SETTING_SCREEN, "0");
				break;
			case 'n':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				put(doc, AIS::KEY_SETTING_SCREEN, "1");
				break;
			case 'o':
			{
				if (count % 2 == 1)
					put(doc, AIS::KEY_SETTING_SCREEN, arg1);
				Object *settings = pool.addObject();
				parseSettings(args, ptr + (count % 2), settings, pool);
				if (!settings->getMembers().empty())
					doc.Add(AIS::KEY_SETTING_SCREEN, settings);
			}
			break;
			case 'F':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				{
					addEngine(receiver, AIS::MODEL_V1_BASE);
					Object &settings = lastModelSettings(receiver);
					put(settings, AIS::KEY_SETTING_FP_DS, "ON");
					put(settings, AIS::KEY_SETTING_PS_EMA, "ON");
					std::string tags;
					for (char c : state(receiver).tagString())
						if (c != 'D' && c != 'T')
							tags += c;
					put(receiver, AIS::KEY_SETTING_META, tags);
				}
				break;
			case 't':
				Assert(count > 0, param, "requires one parameter [url], or two or three parameters [[protocol]] [host] [port].");
				device(Type::RTLTCP, count == 1 ? Keys{AIS::KEY_SETTING_URL} : (count & 1) == 0 ? Keys{AIS::KEY_SETTING_HOST, AIS::KEY_SETTING_PORT} : Keys{AIS::KEY_SETTING_PROTOCOL, AIS::KEY_SETTING_HOST, AIS::KEY_SETTING_PORT}, args, ptr, count);
				break;
			case 'x':
				Assert(count >= 2 && (count & 1) == 0, param, "requires two parameters [server] [port] (optionally followed by key value pairs).");
				device(Type::UDP, {AIS::KEY_SETTING_SERVER, AIS::KEY_SETTING_PORT}, args, ptr, count);
				break;
			case 'D':
			{
				// bare target = libpq string; a "sqlite:" or "csv:" prefix picks the backend
				std::string type = "postgres";
				std::string target = count % 2 == 1 ? arg1 : std::string();

				if (target.compare(0, 7, "sqlite:") == 0)
				{
					type = "sqlite";
					target = target.substr(7);
				}
				else if (target.compare(0, 4, "csv:") == 0)
				{
					type = "csv";
					target = target.substr(4);
				}

				Object &out = item(list(doc, AIS::KEY_SETTING_DB));
				if (type != "postgres")
					put(out, AIS::KEY_SETTING_MODEL_TYPE, type);
				if (count % 2 == 1)
					put(out, AIS::KEY_SETTING_CONN_STR, target);
				parseSettings(args, ptr + (count % 2), &out, pool);
			}
			break;
			case 'y':
				Assert(count <= 2, param, "requires one or two parameters [url] or [host] [port].");
				device(Type::SPYSERVER, count == 1 ? Keys{AIS::KEY_SETTING_URL} : count == 2 ? Keys{AIS::KEY_SETTING_HOST, AIS::KEY_SETTING_PORT} : Keys(), args, ptr, count);
				break;
			case 'z':
				Assert(count > 0, param, "requires one parameter [endpoint] or two parameters [[format]] [endpoint].");
				device(Type::ZMQ, (count & 1) == 1 ? Keys{AIS::KEY_SETTING_ENDPOINT} : Keys{AIS::KEY_SETTING_FORMAT, AIS::KEY_SETTING_ENDPOINT}, args, ptr, count);
				break;
			case 'b':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				receiver.Add(AIS::KEY_SETTING_TIMING, true);
				break;
			case 'i':
				device(Type::N2K, leading(count & 1, AIS::KEY_SETTING_INTERFACE), args, ptr, count);
				break;
			case 'w':
				device(Type::WAVFILE, leading(count & 1, AIS::KEY_SETTING_FILE), args, ptr, count);
				break;
			case 'r':
				Assert(count > 0, param, "requires one parameter [filename] or two parameters [[format]] [filename].");
				device(Type::RAWFILE, (count & 1) == 1 ? Keys{AIS::KEY_SETTING_FILE} : Keys{AIS::KEY_SETTING_FORMAT, AIS::KEY_SETTING_FILE}, args, ptr, count);
				break;
			case 'e':
				Assert(count >= 2 && (count & 1) == 0, param, "requires two parameters [baudrate] [portname] (optionally followed by key value pairs).");
				device(Type::SERIALPORT, {AIS::KEY_SETTING_BAUDRATE, AIS::KEY_SETTING_PORT}, args, ptr, count);
				break;
			case 'l':
				Assert(count == 0 || count == 2, param, "takes no parameters or [JSON on/off].");
				if (count == 2)
				{
					Assert(arg1 == "JSON", param, "requires JSON on/off");
					actions.list_devices_json = Util::Parse::Switch(arg2);
				}
				actions.list_devices = true;
				break;
			case 'L':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				actions.list_support = true;
				break;
			case 'd':
			{
				Object &rx = newReceiver();

				if (param.length() >= 4 && param[2] == ':')
				{
					Assert(count == 0, param, MSG_NO_PARAMETER);
					int n = (int)Util::Parse::Integer(param.substr(3));
					// an index is of the devices found now: the document names the device by its serial
					DeviceManager found;
					found.selectDeviceByIndex(n);
					if (!found.serial().empty())
						put(rx, AIS::KEY_SETTING_SERIAL, found.serial());
					std::string name = Util::Parse::DeviceTypeString(found.inputType());
					Util::Convert::toLower(name);
					put(rx, AIS::KEY_SETTING_INPUT, name.empty() ? "none" : name);
				}
				else
				{
					Assert(param.length() == 2, param, "syntax error in device setting");
					Assert(count == 1, param, "device setting requires one parameter [serial number]");
					put(rx, AIS::KEY_SETTING_SERIAL, arg1);
					if (arg1.empty())
						put(rx, AIS::KEY_SETTING_INPUT, "none");
				}
			}
			break;
			case 'u':
				Assert(count >= 2 && count % 2 == 0, param, "requires at least two parameters [address] [port].");
				output(AIS::KEY_SETTING_UDP, {AIS::KEY_SETTING_HOST, AIS::KEY_SETTING_PORT}, args, ptr, count);
				break;
			case 'P':
				Assert(count >= 2 && count % 2 == 0, param, "requires at least two parameters [address] [port].");
				output(AIS::KEY_SETTING_TCP, {AIS::KEY_SETTING_HOST, AIS::KEY_SETTING_PORT}, args, ptr, count);
				break;
			case 'Q':
				Assert(count >= 1, param, "invalid number of arguments");
				output(AIS::KEY_SETTING_MQTT, leading(count % 2, AIS::KEY_SETTING_URL), args, ptr, count);
				break;
			case 'X':
				Assert(count <= 1, param, "Only one optional parameter [sharing key] allowed.");
				{
					std::string upper = arg1;
					Util::Convert::toUpper(upper);
					const bool off = count == 1 && upper == "OFF";
					share(true, !off, off || upper == "ON" ? std::string() : arg1);
				}
				break;
			case 'H':
				Assert(count > 0, param);
				output(AIS::KEY_SETTING_HTTP, leading(count % 2, AIS::KEY_SETTING_URL), args, ptr, count);
				break;
			case 'W':
#ifdef HASDELIVERY
				Assert(count >= 1 && count % 2 == 1, param, "requires a url, then key value pairs, e.g. -W https://example.com/hook EVENTS safety SECRET whsec_...");
				{
					const std::string where = "-W " + arg1;
					const auto starts = [&arg1](const char *scheme)
					{ return arg1.compare(0, strlen(scheme), scheme) == 0; };
					const bool hook = starts("http://") || starts("https://");
					if (!hook && !starts("mqtt://") && !starts("mqtts://") && !starts("ws://") && !starts("wss://"))
						throw std::runtime_error(where + ": a url with http:// or https:// for a webhook, or mqtt://, mqtts://, ws:// or wss:// for the MQTT state.");
					Object *entry = pool.addObject();
					put(*entry, AIS::KEY_SETTING_URL, arg1);
					for (int k = 2; k + 1 <= count; k += 2)
						put(*entry, lookup(args[ptr + k], where), args[ptr + k + 1]);

					if (hook)
						list(doc, AIS::KEY_SETTING_WEBHOOKS).push_back(JSON::Value(entry));
					else
						doc.Add(AIS::KEY_SETTING_MQTT_STATE, entry);
				}
#else
				throw std::runtime_error("Option -W needs a build with webhooks and the MQTT state (cmake -DDELIVERY=ON).");
#endif
				break;
			case 'Z':
				Assert(count == 2, param, "Location Setting requires two parameters (lat/lon)");
				put(receiver, AIS::KEY_SETTING_LAT, arg1);
				put(receiver, AIS::KEY_SETTING_LON, arg2);
				break;
			case 'A':
				throw std::runtime_error("Option -A is obsolete. Please use -I instead.");
			case 'E':
				throw std::runtime_error("Option -E (managed mode) must be the only option: AIS-catcher -E [config file] [bind address:port] [TLS_CERT file TLS_KEY file]. To run this command line in managed mode, write it as a configuration with -J config.json, then start AIS-catcher -E config.json.");
			case 'I':
#ifdef HASNMEA2000
				output(AIS::KEY_SETTING_NMEA2000_OUTPUT, leading(count % 2, AIS::KEY_SETTING_DEVICE), args, ptr, count);
#else
				throw std::runtime_error("NMEA2000 support not compiled in.");
#endif
				break;
			case 'J':
				Assert(count <= 1, param, "takes one optional parameter [file].");
				json = true;
				json_file = arg1;
				break;
			case 'K':
				Assert(count == 0, param, "takes no parameters.");
				actions.new_key = true;
				break;
			case 'h':
				Assert(count == 0 || count == 1, param, "takes no parameters or one parameter [JSON/BUILD].");
				if (count == 1)
				{
					Util::Convert::toUpper(arg1);
					Assert(arg1 == "JSON" || arg1 == "BUILD", param, "parameter needs to be JSON or BUILD");

					if (arg1 == "JSON")
						std::cout << "{\"version\":\"" << VERSION << "\",\"version_describe\":\"" << VERSION_DESCRIBE << "\",\"version_code\":" << VERSION_NUMBER << "}\n";
					else
						std::cout << VERSION_DESCRIBE << "\n";

					actions.no_run = true;
				}
				else
					actions.list_options = true;
				break;
			case 'p':
				Assert(count == 1, param, "requires one parameter [frequency offset].");
				put(receiver, AIS::KEY_SETTING_FREQOFFSET, arg1);
				break;
			case 'a':
				Assert(count == 1, param, "requires one parameter [bandwidth].");
				put(receiver, AIS::KEY_SETTING_BANDWIDTH, arg1);
				break;
			case 'g':
				Assert(count % 2 == 0 && param.length() == 3, param);
				if (param[2] == 'o')
				{
					// the last model takes them; with none, the model its input decodes with first
					Receiver &so_far = state(receiver);
					if (so_far.modelCount() == 0)
						addEngine(receiver, so_far.defaultModel());
					parseSettings(args, ptr, &lastModelSettings(receiver), pool);
				}
				else
				{
					const AIS::Keys key = DeviceManager::keyForFlag(param[2]);
					if (key == AIS::KEY_COUNT)
						throw std::runtime_error("invalid -g switch on command line");

					parseSettings(args, ptr, &block(receiver, key), pool);
				}
				break;
			default:
				throw std::runtime_error("unknown option on command line (" + std::string(1, param[1]) + ").");
			}

			ptr += count + 1;
			check(count ? param + " " + arg1 : param);
		}
		settle();
	}
}

// throws what the command line got wrong; whether the engine runs: not after -h, -l, -L or -J
static bool build(const std::vector<std::string> &args, Config &c)
{
	std::string document, file;
	Actions actions;
	bool json = false;
	std::string json_file;
	Config::Origins origins;
	{
		Translation t;
		t.read(args);
		for (const auto &w : t.said)
			Warning() << w;
		document = t.document(&origins);
		actions = t.actions;
		json = t.json;
		json_file = t.json_file;
		if (json)
			file = t.document(nullptr);
	}

	if (actions.list_devices)
		DeviceManager::printAvailableDevices(actions.list_devices_json);
	if (actions.list_support)
		printBuildConfiguration();
	if (actions.list_options)
		printUsage();
	if (actions.new_key)
	{
		// the key is the client's; a server holds its hash
		const std::string key = ControlCore::randomHex(48), hash = Util::SHA256::hex(key);
		std::cout << "key:     " << key << "\nsha256:  " << hash << "\n\n"
				  << "on the command line:     -N 8100 KEYS name:data:" << hash << " API on\n"
				  << "in a configuration file: \"keys\": [{\"name\": \"name\", \"sha256\": \"" << hash << "\", \"scopes\": [\"data\"]}]\n"
				  << "scopes: data, or data+system for the installation's own resources; a client sends the key as Authorization: Bearer\n";
	}

	c.set(document, &origins);
	c.finish();

	if (json)
	{
		if (json_file.empty())
			std::cout << file;
		else
		{
			// the document can hold passwords: a new file is its owner's alone
#ifndef _WIN32
			const int fd = ::open(json_file.c_str(), O_WRONLY | O_CREAT, 0600);
			if (fd >= 0)
				::close(fd);
#endif
			std::ofstream out(json_file);
			out << file;
			if (!out)
				throw std::runtime_error("-J: cannot write " + json_file + ".");
			Info() << "Configuration written to " << json_file;
		}
		return false;
	}
	return !(actions.list_devices || actions.list_support || actions.list_options || actions.no_run || actions.new_key);
}

int run(const std::vector<std::string> &args)
{
	Site site;
	Engine engine;
	Config c(engine);

	try
	{
		engine.scanner().refreshDevices();

		if (!build(args, c))
			return 0;

		engine.open(site, c.stations, c.station_defaults, c.site.tracks());
		site.configure(c.site);
#ifdef HASAPI
		// a server that cannot listen stops the start
		const std::vector<int> ports = site.serverPorts();
		for (const auto &s : c.site.servers)
			if (std::find(ports.begin(), ports.end(), s.port) == ports.end())
				throw std::runtime_error("cannot open port " + std::to_string(s.port) + " on " + s.bind + " for the server.");
#endif
		return engine.run();
	}
	catch (std::exception const &e)
	{
		Error() << e.what();
		return -1;
	}
}

}
