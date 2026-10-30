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

namespace CommandLine
{

// -------------------------------
// The command line as a configuration document
//
// Every option writes what it says into a document in the configuration
// file's format, and the configuration reader applies that document as it
// applies a file: one builder for both. An option's settings are told first
// to an object of the kind that takes them, so a mistake is still reported at
// the option that made it, in the words it always had.

namespace
{
	using Keys = std::vector<AIS::Keys>;

	// A document as the command line writes it: the configuration file's format
	// with its members in the order given and a key given twice kept twice, so
	// the reader applies what was said in the order it was said
	struct Node
	{
		enum Kind
		{
			RAW, // a number, switch or null, as written
			STRING,
			OBJECT,
			ARRAY
		} kind = OBJECT;
		std::string text;
		std::vector<std::pair<std::string, Node>> members;
		std::vector<Node> items;

		static Node of(Kind k, const std::string &t = "")
		{
			Node n;
			n.kind = k;
			n.text = t;
			return n;
		}
		static Node str(const std::string &s) { return of(STRING, s); }
		static Node boolean(bool b) { return of(RAW, b ? "true" : "false"); }
		static Node integer(long long v) { return of(RAW, std::to_string(v)); }
		static Node listOf(Node item)
		{
			Node n = of(ARRAY);
			n.items.push_back(std::move(item));
			return n;
		}

		void add(std::string key, Node v) { members.emplace_back(std::move(key), std::move(v)); }
		Node *last(const std::string &key)
		{
			for (auto it = members.rbegin(); it != members.rend(); ++it)
				if (it->first == key)
					return &it->second;
			return nullptr;
		}
		// the last member named key where it is of that kind, else a new one
		Node &child(const std::string &key, Kind k = OBJECT)
		{
			Node *c = last(key);
			if (!c || c->kind != k)
			{
				add(key, of(k));
				c = &members.back().second;
			}
			return *c;
		}
	};

	std::string keyName(int key) { return AIS::settingName(key); }

	// a value from a file, as the reader would read it from the document again
	Node fromJSON(const JSON::Value &v)
	{
		switch (v.getType())
		{
		case JSON::Value::Type::BOOL:
			return Node::boolean(v.getBool());
		case JSON::Value::Type::INT:
			return Node::integer(v.getInt64());
		case JSON::Value::Type::FLOAT:
		{
			// enough digits for the same number, and a point so it stays one with a fraction
			char b[40];
			std::snprintf(b, sizeof(b), "%.17g", v.getFloat());
			std::string t = b;
			if (t.find_first_of(".eE") == std::string::npos)
				t += ".0";
			return Node::of(Node::RAW, t);
		}
		case JSON::Value::Type::OBJECT:
		{
			Node n;
			for (const auto &m : v.getObject().getMembers())
				n.add(keyName(m.Key()), fromJSON(m.Get()));
			return n;
		}
		case JSON::Value::Type::ARRAY:
		{
			Node n = Node::of(Node::ARRAY);
			for (const auto &item : v.getArray())
				n.items.push_back(fromJSON(item));
			return n;
		}
		case JSON::Value::Type::ARRAY_STRING:
		{
			Node n = Node::of(Node::ARRAY);
			for (const auto &s : v.getStringArray())
				n.items.push_back(Node::str(s));
			return n;
		}
		case JSON::Value::Type::EMPTY:
			return Node::of(Node::RAW, "null");
		default:
			return Node::str(v.to_string());
		}
	}

	void write(const Node &n, std::string &out, int depth = 0)
	{
		const std::string pad(2 * (depth + 1), ' '), end(2 * depth, ' ');
		switch (n.kind)
		{
		case Node::STRING:
			out += JSON::Writer::escape(n.text);
			break;
		case Node::OBJECT:
			if (n.members.empty())
			{
				out += "{}";
				break;
			}
			out += "{\n";
			for (std::size_t i = 0; i < n.members.size(); i++)
			{
				out += pad + JSON::Writer::escape(n.members[i].first) + ": ";
				write(n.members[i].second, out, depth + 1);
				out += i + 1 < n.members.size() ? ",\n" : "\n";
			}
			out += end + "}";
			break;
		case Node::ARRAY:
			if (n.items.empty())
			{
				out += "[]";
				break;
			}
			out += "[\n";
			for (std::size_t i = 0; i < n.items.size(); i++)
			{
				out += pad;
				write(n.items[i], out, depth + 1);
				out += i + 1 < n.items.size() ? ",\n" : "\n";
			}
			out += end + "]";
			break;
		default:
			out += n.text;
		}
	}

	bool isActive(const Node &n)
	{
		for (const auto &m : n.members)
			if (m.first == keyName(AIS::KEY_SETTING_ACTIVE))
				return Util::Parse::Switch(m.second.text);
		return true;
	}

	// a tracking file or places directory as the reader resolves it, against base; a server's "tracking" too
	void absolute(Node &n, const std::string &base)
	{
		for (auto &m : n.members)
		{
			AIS::Keys key;
			std::string name = m.first;
			Util::Convert::toUpper(name);
			if (m.second.kind == Node::STRING && AIS::lookupSettingKey(name, key) && SiteReader::isPath(key))
				m.second.text = Util::Helper::resolvePath(m.second.text, base);
			else if (m.first == keyName(AIS::KEY_SETTING_TRACKING) && m.second.kind == Node::OBJECT)
				absolute(m.second, base);
			else if (m.first == keyName(AIS::KEY_SETTING_TLS) && m.second.kind == Node::OBJECT)
				for (auto &file : m.second.members)
					file.second.text = Util::Helper::resolvePath(file.second.text, base);
		}
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
	// a setting's key by its name on the command line, said of the option that gave it: -N's and -W's
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

	// what an object that tries a setting says, the reader says again when it
	// applies the setting: while trying, the log keeps quiet
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

	// the document's files are resolved already, each against the folder of the
	// file it came from: read with no folder, and the folder back after
	struct FromHere
	{
		std::string base;
		FromHere() : base(Config::baseDir()) { Config::setConfigBaseDir(""); }
		~FromHere() { Config::setConfigBaseDir(base.empty() ? "" : base + "/."); }
	};
}

// the key/value pairs after an option's positional arguments, into a node; -G's go to the log itself
static void parseSettings(const std::vector<std::string> &args, int ptr, Node *into, Setting *apply = nullptr)
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
					into->add(keyName(key), Node::str(arg));
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
static void positional(const Keys &keys, const std::vector<std::string> &args, int ptr, int count, Node &into)
{
	int i = 0;
	for (AIS::Keys k : keys)
		into.add(keyName(k), Node::str(args[ptr + 1 + i++]));

	if (count > (int)keys.size())
		parseSettings(args, ptr + (int)keys.size(), &into);
}

// the one positional key an option takes when it is given
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
	// one receiver as the document will have it
	struct Rx
	{
		Node entry;
		// the channels it ends with: a channel key is what only a receiver object
		// takes, and the last word wins, as it did when each was applied in turn
		bool channel_set = false;
		std::string channel, nmea;
		bool passive = false; // a file's entry that is off: carried for the reader to skip
	};

	// what the command line asks of the program besides configuring: the
	// banner, the lists, and whether the engine runs at all
	struct Actions
	{
		bool show_copyright = true, list_devices = false, list_devices_json = false, list_support = false, list_options = false, no_run = false;
	};

	// The command line, read into a document. Options that act rather than
	// configure (-h, -l, -L, -G) are noted as actions, or act on the log as they did.
	class Translation
	{
		bool verbose = false; // -v or -v*: every receiver reports

		Node main; // what is the engine's: the main section
		std::vector<std::unique_ptr<Rx>> receivers;
		int nrec = 0;													  // as the reader counts: the first input fills the engine's first receiver
		Node lists;														  // the outputs, stations and services, an array each in order of first appearance
		std::vector<std::pair<Node, std::string>> servers;				  // with the -N that gave each, empty for a file's
		Node webhooks = Node::of(Node::ARRAY);
		std::vector<Node> mqtt;
		Node tracking; // the files' "tracking", read as one
		bool has_tracking = false;

		// -N: a port starts a server, the keys that follow are its settings; the servers come once every argument is in
		struct PendingServer
		{
			std::string port;
			std::vector<std::pair<int, std::string>> args;
		};
		std::vector<PendingServer> pending;

		bool xshare_defined = false, feed = false;
		std::string uuid;
		std::vector<std::string> sharing_zone;

		// the engine the reader makes of the document so far: read again after every
		// option, so a wrong one is refused where it stands, in the reader's words
		std::unique_ptr<Engine> state_;

		void check(const std::string &where)
		{
			Quiet quiet;
			state_.reset();
			Config::Origins origins;
			const std::string doc = document(&origins, false);
			FromHere here;
			std::unique_ptr<Engine> e(new Engine());
			Config c(*e);
			try
			{
				c.set(doc, &origins);
			}
			catch (const std::exception &x)
			{
				const std::string msg = x.what();
				throw std::runtime_error(where.empty() || msg.compare(0, where.size(), where) == 0 ? msg : where + ": " + msg);
			}
			state_ = std::move(e);
		}

		// a receiver as the reader last made it: the engine's receiver its entry fills
		Receiver &state(const Rx &rx)
		{
			std::size_t n = 0;
			for (const auto &r : receivers)
			{
				if (r.get() == &rx)
					break;
				if (!r->passive)
					n++;
			}
			return *state_->receivers[n];
		}

	public:
		std::vector<std::string> said; // warnings the reader would have given for what the document now has otherwise
		Actions actions;
		bool json = false;			   // -J: write the document
		std::string json_file;

		Translation()
		{
			receivers.push_back(std::unique_ptr<Rx>(new Rx()));
			check("");
		}

		// the last engine tried goes as quietly as the others
		~Translation()
		{
			Quiet quiet;
			state_.reset();
		}

		Rx &current()
		{
			for (auto it = receivers.rbegin(); it != receivers.rend(); ++it)
				if (!(*it)->passive)
					return **it;
			return *receivers.front();
		}

		Rx &newReceiver()
		{
			if (++nrec > 1)
				receivers.push_back(std::unique_ptr<Rx>(new Rx()));
			return current();
		}

		// the receiver's block for a device type: the last one where it is on, else a new one
		Node &block(Rx &rx, AIS::Keys key)
		{
			const std::string name = keyName(key);
			Node *b = rx.entry.last(name);
			if (!b || b->kind != Node::OBJECT || !isActive(*b))
			{
				rx.entry.add(name, Node());
				b = &rx.entry.members.back().second;
			}
			return *b;
		}

		// a device option: a receiver on that input, its positional arguments and settings in its block
		void device(Type t, const Keys &keys, const std::vector<std::string> &args, int ptr, int count)
		{
			Rx &rx = newReceiver();
			std::string name = Util::Parse::DeviceTypeString(t);
			Util::Convert::toLower(name);
			rx.entry.add(keyName(AIS::KEY_SETTING_INPUT), Node::str(name));
			positional(keys, args, ptr, count, block(rx, DeviceManager::keyForType(t)));
		}

		// an output option: a new entry of its kind
		void output(AIS::Keys kind, const Keys &keys, const std::vector<std::string> &args, int ptr, int count)
		{
			Node &list = lists.child(keyName(kind), Node::ARRAY);
			list.items.push_back(Node());
			positional(keys, args, ptr, count, list.items.back());
		}

		// a receiver's plain value
		void value(Rx &rx, AIS::Keys key, const std::string &arg)
		{
			rx.entry.add(keyName(key), Node::str(arg));
		}

		// a model the command line adds, after those the receiver has
		void addEngine(Rx &rx, int type)
		{
			Node item;
			item.add(keyName(AIS::KEY_SETTING_MODEL_TYPE), Node::str(Config::modelName(type)));
			rx.entry.child(keyName(AIS::KEY_SETTING_ENGINES), Node::ARRAY).items.push_back(std::move(item));
		}

		// where the settings of the receiver's last model go
		Node &lastModelSettings(Rx &rx)
		{
			Node &engines = rx.entry.child(keyName(AIS::KEY_SETTING_ENGINES), Node::ARRAY);
			for (auto item = engines.items.rbegin(); item != engines.items.rend(); ++item)
				if (item->kind == Node::OBJECT && isActive(*item))
					return item->child(keyName(AIS::KEY_SETTING_SETTINGS));
			throw std::runtime_error("internal error: no model for -go");
		}

		// a receiver's keys from a file, copied to its entry with what the document says
		// otherwise: the input first, own_mmsi the engine's, and the old "model" as an engine
		void receiverFromFile(Rx &rx, const std::vector<JSON::Member> &members, bool unspec)
		{
			std::string serial, input;
			Config::receiverInput(members, serial, input);
			if (!serial.empty())
				rx.entry.add(keyName(AIS::KEY_SETTING_SERIAL), Node::str(serial));
			if (!input.empty())
				rx.entry.add(keyName(AIS::KEY_SETTING_INPUT), Node::str(input));

			for (const auto &m : members)
			{
				// the main section's other keys are the site's, and its receivers their own: read by file()
				if (unspec && (!Config::isReceiverKey(m.Key()) || m.Key() == AIS::KEY_SETTING_RECEIVER))
					continue;
				const std::string name = keyName(m.Key());
				switch (m.Key())
				{
				case AIS::KEY_SETTING_SERIAL:
				case AIS::KEY_SETTING_INPUT:
				case AIS::KEY_SETTING_ACTIVE:
					break;
				case AIS::KEY_SETTING_CHANNEL:
					rx.channel_set = true;
					rx.channel = m.Get().to_string();
					rx.nmea.clear();
					break;
				case AIS::KEY_SETTING_NMEA_CHANNEL:
					rx.channel_set = true; // after the channel, in whichever order the file has them: below
					break;
				case AIS::KEY_SETTING_OWN_MMSI:
					main.add(name, fromJSON(m.Get()));
					break;
				case AIS::KEY_SETTING_MODEL:
					if (m.Get().isString() || Config::isActiveObject(m.Get()))
					{
						// the reader would have said it is old; as an engine, -go can give it settings
						said.push_back(Config::MODEL_DEPRECATED);
						Node item;
						if (m.Get().isString())
							item.add(keyName(AIS::KEY_SETTING_MODEL_TYPE), Node::str(m.Get().to_string()));
						else
						{
							Node settings;
							for (const auto &p : m.Get().getObject().getMembers())
								if (p.Key() != AIS::KEY_SETTING_ACTIVE)
									settings.add(keyName(p.Key()), fromJSON(p.Get()));
							item.add(keyName(AIS::KEY_SETTING_SETTINGS), std::move(settings));
						}
						rx.entry.add(keyName(AIS::KEY_SETTING_ENGINES), Node::listOf(std::move(item)));
					}
					else
						rx.entry.add(name, fromJSON(m.Get()));
					break;
				default:
					rx.entry.add(name, fromJSON(m.Get()));
				}
			}
			for (const auto &m : members)
				if (m.Key() == AIS::KEY_SETTING_NMEA_CHANNEL)
					rx.nmea = m.Get().to_string();
		}

		// a receiver's engines that are on, in the order the reader makes a model of each
		std::vector<Node *> activeEngines(Rx &rx)
		{
			std::vector<Node *> items;
			for (auto &m : rx.entry.members)
				if (m.first == keyName(AIS::KEY_SETTING_ENGINES) && m.second.kind == Node::ARRAY)
					for (Node &item : m.second.items)
						if (item.kind == Node::OBJECT && isActive(item))
							items.push_back(&item);
			return items;
		}

		// -C: the file, merged where it stands, as Config::set would have applied it there
		void file(const std::string &path)
		{
			Config::setConfigBaseDir(path);
			JSON::Parser parser(JSON_DICT_SETTING);
			// a file that does not parse is named, as the reader's refusals are
			const std::string text = Util::Helper::readFile(path);
			JSON::Document doc = [&]() -> JSON::Document {
				try
				{
					return parser.parse(text);
				}
				catch (const std::exception &e)
				{
					throw std::runtime_error("-C " + path + ": " + e.what());
				}
			}();
			const auto &members = doc.getMembers();

			Config::preamble(members);

			// the main section's receiver: its own when it names an input, else the current one
			bool active = true;
			for (const auto &m : members)
				if (m.Key() == AIS::KEY_SETTING_ACTIVE)
					active = Util::Parse::Switch(m.Get().to_string());
			// each receiver the file fills, with the models it had before: the engines the
			// file adds are named below as the reader picks them, with the input known here
			std::vector<std::pair<Rx *, int>> filled;
			if (active)
			{
				std::string serial, input;
				Config::receiverInput(members, serial, input);
				const std::size_t had = receivers.size();
				Rx &rx = !serial.empty() || !input.empty() ? newReceiver() : current();
				filled.emplace_back(&rx, receivers.size() > had ? 0 : state(rx).modelCount());
				receiverFromFile(rx, members, true);
			}

			for (const auto &m : members)
			{
				if (m.Key() != AIS::KEY_SETTING_RECEIVER)
					continue;
				if (!m.Get().isArray())
					throw std::runtime_error(Config::RECEIVERS_NOT_ARRAY);
				for (const auto &v : m.Get().getArray())
				{
					if (!Config::isActiveObject(v))
					{
						std::unique_ptr<Rx> rx(new Rx());
						rx->entry = fromJSON(v);
						rx->passive = true;
						receivers.push_back(std::move(rx));
						continue;
					}
					const std::size_t had = receivers.size();
					Rx &rx = newReceiver();
					filled.emplace_back(&rx, receivers.size() > had ? 0 : state(rx).modelCount());
					receiverFromFile(rx, v.getObject().getMembers(), false);
				}
			}

			// sharing, as Config::readSharing: each file can switch the feed on, none switches it off
			{
				bool xchange = false;
				std::string key;
				std::vector<std::string> zones;
				for (const auto &m : members)
					if (m.Key() == AIS::KEY_SETTING_SHARING)
					{
						xchange = Util::Parse::Switch(m.Get().to_string());
						xshare_defined = true;
					}
					else if (m.Key() == AIS::KEY_SETTING_SHARING_KEY)
						key = m.Get().to_string();
					else if (m.Key() == AIS::KEY_SETTING_SHARING_ZONE && m.Get().isArray())
						for (const auto &v : m.Get().getArray())
							zones.push_back(v.to_string());
				feed = feed || xchange || !key.empty();
				if (!key.empty())
					uuid = key;
				if (!zones.empty() && feed)
					sharing_zone = zones;
			}

			// the rest, where the document keeps it; its files resolve against its own folder,
			// as the reader resolved them when it read the file
			const std::string &base = Config::baseDir();
			bool tracked = false;
			for (const auto &m : members)
			{
				if (Config::isReceiverKey(m.Key()))
					continue;
				const std::string name = keyName(m.Key());
				switch (m.Key())
				{
				case AIS::KEY_SETTING_CONFIG:
				case AIS::KEY_SETTING_VERSION:
				case AIS::KEY_SETTING_SHARING:
				case AIS::KEY_SETTING_SHARING_KEY:
				case AIS::KEY_SETTING_SHARING_ZONE:
					break; // the document's own, and the feed's, read above
				case AIS::KEY_SETTING_SERVER:
				{
					// one server or a list of them, none the -N's
					Node value = fromJSON(m.Get());
					std::vector<Node> entries;
					if (value.kind == Node::ARRAY)
						entries = std::move(value.items);
					else
						entries.push_back(std::move(value));
					for (auto &entry : entries)
					{
						absolute(entry, base);
						servers.emplace_back(std::move(entry), std::string());
					}
				}
				break;
				case AIS::KEY_SETTING_WEBHOOKS:
					if (!m.Get().isArray())
						main.add(name, fromJSON(m.Get())); // for the reader to refuse
					else
						for (const auto &item : m.Get().getArray())
							webhooks.items.push_back(fromJSON(item));
					break;
				case AIS::KEY_SETTING_MQTT_STATE:
					mqtt.push_back(fromJSON(m.Get()));
					break;
				case AIS::KEY_SETTING_TRACKING:
					// one "tracking" a file, its first: the files' are read as one
					if (tracked)
						break;
					if (!m.Get().isObject())
					{
						main.add(name, fromJSON(m.Get())); // for the reader to refuse
						break;
					}
					tracked = true;
					for (const auto &t : m.Get().getObject().getMembers())
					{
						Node v = fromJSON(t.Get());
						if (SiteReader::isPath(t.Key()) && v.kind == Node::STRING)
							v.text = Util::Helper::resolvePath(v.text, base);
						tracking.add(keyName(t.Key()), std::move(v));
					}
					has_tracking = true;
					break;
				case AIS::KEY_SETTING_UDP:
				case AIS::KEY_SETTING_TCP:
				case AIS::KEY_SETTING_MQTT:
				case AIS::KEY_SETTING_TCP_LISTENER:
				case AIS::KEY_SETTING_HTTP:
				case AIS::KEY_SETTING_DB:
				case AIS::KEY_SETTING_FILE_OUTPUT:
				case AIS::KEY_SETTING_NMEA2000_OUTPUT:
				case AIS::KEY_SETTING_STATIONS:
				case AIS::KEY_SETTING_SERVICES:
				{
					if (!m.Get().isArray())
					{
						main.add(name, fromJSON(m.Get())); // for the reader to refuse
						break;
					}
					// one list each, in order of first appearance
					Node &list = lists.child(name, Node::ARRAY);
					for (const auto &item : m.Get().getArray())
					{
						Node entry = fromJSON(item);
						absolute(entry, base);
						list.items.push_back(std::move(entry));
					}
				}
				break;
				default:
					main.add(name, fromJSON(m.Get())); // the screen, the timeout, managed mode's sections
				}
			}

			check("-C " + path);
			for (const auto &f : filled)
			{
				int k = 0;
				for (Node *item : activeEngines(*f.first))
				{
					if (k >= f.second)
						item->child(keyName(AIS::KEY_SETTING_MODEL_TYPE), Node::STRING) = Node::str(Config::modelName(state(*f.first).modelId(k)));
					k++;
				}
			}
		}

		void read(const std::vector<std::string> &args);
		// the files' servers, then each -N as a server; while reading, the -N's are
		// not servers yet: any later -N may add to one, and they are judged at the end
		std::vector<std::pair<Node, std::string>> allServers(bool settled) const;
		// the document, and which of its servers are -N's; without origins the copy -J
		// writes, a file read from anywhere: its tracking files and places absolute
		std::string document(Config::Origins *origins, bool settled = true) const;
	};

	void Translation::read(const std::vector<std::string> &args)
	{
		const std::string MSG_NO_PARAMETER = "does not allow additional parameter.";
		const int argc = (int)args.size();
		int ptr = 1;

		while (ptr < argc)
		{
			Rx &receiver = current();

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
				parseSettings(args, ptr, nullptr, &Logger::getInstance());
				break;
			case 's':
				Assert(count == 1, param, "does require one parameter [sample rate].");
				value(receiver, AIS::KEY_SETTING_SAMPLE_RATE, arg1);
				break;
			case 'm':
				Assert(count == 1, param, "requires one parameter [model number].");
				addEngine(receiver, Util::Parse::Integer(arg1, 0, AIS::MODEL_MAX));
				break;
			case 'M':
				Assert(count <= 1, param, "requires zero or one parameter [DT].");
				value(receiver, AIS::KEY_SETTING_META, arg1);
				break;
			case 'c':
				Assert(count <= 2 && count >= 1, param, "requires one or two parameter [AB/CD]].");
				receiver.channel_set = true;
				receiver.channel = arg1;
				receiver.nmea = count == 2 ? arg2 : std::string();
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
					// -N port starts a server; -N key value adds to the last one
					const int port = isNumber(arg1) ? 1 : 0;
					Assert((count - port) % 2 == 0, param, "usage: -N [port] [key value]..., with a value that has spaces in quotes, e.g. -N 8100 station \"my station\".");
					// a port starts a server, unless the last -N has none yet: then that one takes it
					if (port)
					{
						if (pending.empty() || !pending.back().port.empty())
							pending.push_back(PendingServer{arg1, {}});
						else
							pending.back().port = arg1;
					}
					else if (pending.empty())
						pending.push_back(PendingServer{std::string(), {}});
					for (int k = 1 + port; k + 1 <= count; k += 2)
						pending.back().args.emplace_back((int)lookup(args[ptr + k], "-N " + arg1), args[ptr + k + 1]);
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
					receiver.entry.add(keyName(AIS::KEY_SETTING_VERBOSE), Node::str("on"));
				}
				else
				{
					// -v or -v* applies to all receivers (after loop)
					Assert(param.length() == 2 || (param.length() == 3 && param[2] == '*'), param, "invalid verbose option");
					verbose = true;
					if (count == 1)
					{
						Util::Parse::Integer(arg1, 1, 3600);
						main.add(keyName(AIS::KEY_SETTING_VERBOSE_TIME), Node::str(arg1));
					}
				}
				break;
			case 'O':
				Assert(count == 1, param);
				main.add(keyName(AIS::KEY_SETTING_OWN_MMSI), Node::integer(Util::Parse::Integer(arg1, 1, 999999999)));
				break;
			case 'T':
				Assert(count == 1 || (count == 2 && arg2 == "nomsg_only"), param, "timeout requires one parameter with optional \"nomsg_only\".");
				Util::Parse::Integer(arg1, 1, 3600);
				main.add(keyName(AIS::KEY_SETTING_TIMEOUT), Node::str(arg1));
				if (count == 2)
					main.add(keyName(AIS::KEY_SETTING_TIMEOUT_NOMSG), Node::boolean(true));
				break;
			case 'q':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				main.add(keyName(AIS::KEY_SETTING_SCREEN), Node::str("0"));
				break;
			case 'n':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				main.add(keyName(AIS::KEY_SETTING_SCREEN), Node::str("1"));
				break;
			case 'o':
			{
				if (count % 2 == 1)
					main.add(keyName(AIS::KEY_SETTING_SCREEN), Node::str(arg1));
				Node settings;
				parseSettings(args, ptr + (count % 2), &settings);
				if (!settings.members.empty())
					main.add(keyName(AIS::KEY_SETTING_SCREEN), std::move(settings));
			}
			break;
			case 'F':
				Assert(count == 0, param, MSG_NO_PARAMETER);
				{
					addEngine(receiver, AIS::MODEL_V1_BASE);
					Node &settings = lastModelSettings(receiver);
					settings.add(keyName(AIS::KEY_SETTING_FP_DS), Node::str("ON"));
					settings.add(keyName(AIS::KEY_SETTING_PS_EMA), Node::str("ON"));
					// the tags it had, without D and T
					std::string tags;
					for (char c : state(receiver).tagString())
						if (c != 'D' && c != 'T')
							tags += c;
					receiver.entry.add(keyName(AIS::KEY_SETTING_META), Node::str(tags));
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

				Node out;
				if (type != "postgres")
					out.add(keyName(AIS::KEY_SETTING_MODEL_TYPE), Node::str(type));
				if (count % 2 == 1)
					out.add(keyName(AIS::KEY_SETTING_CONN_STR), Node::str(target));
				parseSettings(args, ptr + (count % 2), &out);
				lists.child(keyName(AIS::KEY_SETTING_DB), Node::ARRAY).items.push_back(std::move(out));
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
				receiver.entry.add(keyName(AIS::KEY_SETTING_TIMING), Node::boolean(true));
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
				Rx &rx = newReceiver();

				if (param.length() >= 4 && param[2] == ':')
				{
					Assert(count == 0, param, MSG_NO_PARAMETER);
					int n = (int)Util::Parse::Integer(param.substr(3));
					// the index is of the devices found now: the document names the device it found
					DeviceManager found;
					found.selectDeviceByIndex(n);
					if (!found.serial().empty())
						rx.entry.add(keyName(AIS::KEY_SETTING_SERIAL), Node::str(found.serial()));
					std::string name = Util::Parse::DeviceTypeString(found.inputType());
					Util::Convert::toLower(name);
					rx.entry.add(keyName(AIS::KEY_SETTING_INPUT), Node::str(name.empty() ? "none" : name));
				}
				else
				{
					Assert(param.length() == 2, param, "syntax error in device setting");
					Assert(count == 1, param, "device setting requires one parameter [serial number]");
					rx.entry.add(keyName(AIS::KEY_SETTING_SERIAL), Node::str(arg1));
					if (arg1.empty())
						rx.entry.add(keyName(AIS::KEY_SETTING_INPUT), Node::str("none"));
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
					xshare_defined = true;

					std::string xarg_upper = arg1;
					Util::Convert::toUpper(xarg_upper);

					if (count == 1 && xarg_upper == "OFF")
						break;

					feed = true;

					if (count == 1 && xarg_upper != "ON")
						uuid = arg1;
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
					Node entry;
					entry.add(keyName(AIS::KEY_SETTING_URL), Node::str(arg1));
					for (int k = 2; k + 1 <= count; k += 2)
						entry.add(keyName(lookup(args[ptr + k], where)), Node::str(args[ptr + k + 1]));

					if (hook)
						webhooks.items.push_back(std::move(entry));
					else
						mqtt.push_back(std::move(entry));
				}
#else
				throw std::runtime_error("Option -W needs a build with webhooks and the MQTT state (cmake -DDELIVERY=ON).");
#endif
				break;
			case 'Z':
				Assert(count == 2, param, "Location Setting requires two parameters (lat/lon)");
				value(receiver, AIS::KEY_SETTING_LAT, arg1);
				value(receiver, AIS::KEY_SETTING_LON, arg2);
				break;
			case 'A':
				throw std::runtime_error("Option -A is obsolete. Please use -I instead.");
			case 'E':
				throw std::runtime_error("Option -E (managed mode) must be the only option: AIS-catcher -E [config file] [bind address:port]. To run this command line in managed mode, write it as a configuration with -J config.json, then start AIS-catcher -E config.json.");
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
					actions.show_copyright = false;
				}
				else
					actions.list_options = true;
				break;
			case 'p':
				Assert(count == 1, param, "requires one parameter [frequency offset].");
				value(receiver, AIS::KEY_SETTING_FREQOFFSET, arg1);
				break;
			case 'a':
				Assert(count == 1, param, "requires one parameter [bandwidth].");
				value(receiver, AIS::KEY_SETTING_BANDWIDTH, arg1);
				break;
			case 'g':
				Assert(count % 2 == 0 && param.length() == 3, param);
				if (param[2] == 'o')
				{
					// the last model takes them; with none, the model its input decodes with first
					Receiver &so_far = state(receiver);
					if (so_far.modelCount() == 0)
						addEngine(receiver, so_far.defaultModel());
					parseSettings(args, ptr, &lastModelSettings(receiver));
				}
				else
				{
					const AIS::Keys key = DeviceManager::keyForFlag(param[2]);
					if (key == AIS::KEY_COUNT)
						throw std::runtime_error("invalid -g switch on command line");

					parseSettings(args, ptr, &block(receiver, key));
				}
				break;
			default:
				throw std::runtime_error("unknown option on command line (" + std::string(1, param[1]) + ").");
			}

			ptr += count + 1;
			check(count ? param + " " + arg1 : param);
		}
	}

	std::vector<std::pair<Node, std::string>> Translation::allServers(bool settled) const
	{
		std::vector<std::pair<Node, std::string>> all = servers;
		if (!settled)
			return all;
		for (const auto &p : pending)
		{
			Node server;
			if (!p.port.empty())
				server.add(keyName(AIS::KEY_SETTING_PORT), Node::str(p.port));
			for (const auto &a : p.args)
			{
				if (a.first == AIS::KEY_SETTING_PLACE_GEOMETRY) // a term of the server's view, which -N has no object for
					server.child(keyName(AIS::KEY_SETTING_VIEW)).add(keyName(a.first), Node::str(a.second));
				else if (a.first == AIS::KEY_SETTING_TLS_CERT) // the files of its tls, which -N has no object for either
					server.child(keyName(AIS::KEY_SETTING_TLS)).add(keyName(AIS::KEY_SETTING_CERT), Node::str(a.second));
				else if (a.first == AIS::KEY_SETTING_TLS_KEY)
					server.child(keyName(AIS::KEY_SETTING_TLS)).add(keyName(AIS::KEY_SETTING_KEY), Node::str(a.second));
				else
					server.add(keyName(a.first), Node::str(a.second)); // the reader refuses keys, view and active as -N did
			}
			// its files resolve against the last file's folder, as they did when the servers were made last
			absolute(server, Config::baseDir());
			all.emplace_back(std::move(server), p.port.empty() ? "-N" : "-N " + p.port);
		}
		return all;
	}

	std::string Translation::document(Config::Origins *origins, bool settled) const
	{
		Node doc;
		doc.add(keyName(AIS::KEY_SETTING_CONFIG), Node::str("aiscatcher"));
		doc.add(keyName(AIS::KEY_SETTING_VERSION), Node::integer(1));

		// with no input named, the default receiver is the main section's, unless it
		// has what only a receiver object takes: then it is one, on whatever device is found
		const Rx &first = *receivers.front();
		bool main_receiver = nrec == 0 && !first.channel_set;
		for (const auto &m : first.entry.members)
			main_receiver = main_receiver && m.first != keyName(AIS::KEY_SETTING_ENGINES);
		if (main_receiver)
		{
			for (const auto &m : first.entry.members)
				doc.add(m.first, m.second);
			if (verbose)
				doc.add(keyName(AIS::KEY_SETTING_VERBOSE), Node::str("on"));
		}
		for (const auto &m : main.members)
			doc.add(m.first, m.second);

		if (xshare_defined)
			doc.add(keyName(AIS::KEY_SETTING_SHARING), Node::boolean(feed));
		if (!uuid.empty())
			doc.add(keyName(AIS::KEY_SETTING_SHARING_KEY), Node::str(uuid));
		if (!sharing_zone.empty())
		{
			Node zones = Node::of(Node::ARRAY);
			for (const auto &z : sharing_zone)
				zones.items.push_back(Node::str(z));
			doc.add(keyName(AIS::KEY_SETTING_SHARING_ZONE), std::move(zones));
		}

		Node array = Node::of(Node::ARRAY);
		for (std::size_t i = main_receiver ? 1 : 0; i < receivers.size(); i++)
		{
			const Rx &rx = *receivers[i];
			Node entry = rx.entry;
			if (verbose && !rx.passive) // -v reaches every receiver
				entry.add(keyName(AIS::KEY_SETTING_VERBOSE), Node::str("on"));
			if (rx.channel_set)
			{
				if (!rx.channel.empty())
					entry.add(keyName(AIS::KEY_SETTING_CHANNEL), Node::str(rx.channel));
				if (!rx.nmea.empty())
					entry.add(keyName(AIS::KEY_SETTING_NMEA_CHANNEL), Node::str(rx.nmea));
			}
			if (i == 0 && nrec == 0)
				entry.members.insert(entry.members.begin(), std::make_pair(keyName(AIS::KEY_SETTING_INPUT), Node::str("none")));
			array.items.push_back(std::move(entry));
		}
		if (!array.items.empty())
			doc.add(keyName(AIS::KEY_SETTING_RECEIVER), std::move(array));

		for (const auto &l : lists.members)
			doc.add(l.first, l.second);
		if (has_tracking)
			doc.add(keyName(AIS::KEY_SETTING_TRACKING), tracking);

		const std::vector<std::pair<Node, std::string>> all = allServers(settled);
		if (!all.empty())
		{
			Node list = Node::of(Node::ARRAY);
			for (const auto &s : all)
			{
				list.items.push_back(s.first);
				if (origins)
					origins->servers.push_back(s.second);
			}
			doc.add(keyName(AIS::KEY_SETTING_SERVER), std::move(list));
		}
		if (!webhooks.items.empty())
			doc.add(keyName(AIS::KEY_SETTING_WEBHOOKS), webhooks);
		for (const auto &m : mqtt)
			doc.add(keyName(AIS::KEY_SETTING_MQTT_STATE), m);

		if (!origins)
		{
			// a file read from anywhere: what is still relative is to this folder
			const std::string cwd = currentDirectory();
			for (auto &m : doc.members)
				if (m.second.kind == Node::OBJECT)
					absolute(m.second, cwd);
				else if (m.second.kind == Node::ARRAY)
				{
					for (auto &item : m.second.items)
						if (item.kind == Node::OBJECT)
							absolute(item, cwd);
				}
		}

		std::string out;
		write(doc, out);
		return out + "\n";
	}
}

// The command line, read into a document and applied by the configuration
// reader. Throws what the command line got wrong. Whether the engine runs:
// not after -h, -l, -L or -J.
static bool build(const std::vector<std::string> &args, Engine &engine)
{
	Config c(engine);
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

	if (actions.show_copyright)
		printVersion();

	if (actions.list_devices)
		DeviceManager::printAvailableDevices(actions.list_devices_json);
	if (actions.list_support)
		printBuildConfiguration();
	if (actions.list_options)
		printUsage();

	// the document's files are resolved already, each against the folder of the file it came from
	Config::setConfigBaseDir("");
	c.set(document, &origins);
	c.finish();

	if (json)
	{
		// -J: the document, to a file or the screen, and nothing run
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
	return !(actions.list_devices || actions.list_support || actions.list_options || actions.no_run);
}

int run(const std::vector<std::string> &args)
{
	Site site;
	Engine engine;

	try
	{
		engine.scanner().refreshDevices();

		if (!build(args, engine))
			return 0;

		return engine.run(site);
	}
	catch (std::exception const &e)
	{
		Error() << e.what();
		return -1;
	}
}

}
