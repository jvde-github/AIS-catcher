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

#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "Engine.h"
#include "JSON/JSON.h"

// The configuration reader: a document in the configuration file's format,
// applied to an engine; the site's sections go to the readers in SiteConfig.cpp
class Config
{
public:
	// which "server" entries of a document made from the command line are its
	// -N's, so a mistake in one is said as -N said it before: "-N 8100: …"
	struct Origins
	{
		std::vector<std::string> servers; // per "server" entry: "-N 8100" as given, or empty for a file's
	};

private:
	Engine &engine;
	// where the configuration file is: its relative file names are there
	static std::string base_dir;
	// managed mode (-E): the control panel's port and address, and the port beside it
	bool managed = false;
	int control_port = 0, viewer_port = 0;
	std::string bind;

	// one output of type T per active object in the array, with its settings applied
	template <typename T>
	void readOutputs(const JSON::Member &m, const std::string &label, const char *default_timeout = nullptr);
	// the "db" array names its backend with a "type" field
	void readDatabases(const JSON::Member &m);
	// a receiver's keys, the main section's (unspecAllowed) or an entry of the "receiver" array, applied
	// to r unless they say it is off; whether they name an input or a serial
	bool readReceiver(const std::vector<JSON::Member> &m, bool unspecAllowed, Receiver &r);
	// the "receiver" array: its first active entry fills the engine's first receiver unless that is
	// filled already, every other is a receiver of its own
	void readReceivers(const JSON::Member &m, bool &filled);
	void readSharing(const std::vector<JSON::Member> &members);

public:
	Config(Engine &engine) : engine(engine) {}

	// managed mode (-E): the site is read by its rules, its main server on viewer_port
	void setManaged(int control, int viewer, const std::string &address)
	{
		managed = true;
		control_port = control;
		viewer_port = viewer;
		bind = address;
	}
	void read(const std::string &file_config);
	// a document; from a command line, with which of its servers are -N's
	void set(const std::string &str, const Origins *from = nullptr);
	// once every key is read: the stations the receivers name are defined
	void finish();

	// one name-to-backend map for config file and -D alike; throws on unknown type
	static IO::OutputMessage *newDatabaseOutput(const std::string &type);
	// an object's "active", true without one; throws for what is not an object
	static bool isActiveObject(const JSON::Value &m);
	// what a document must say of itself, and what its main section may not say, checked before anything is applied
	static void preamble(const std::vector<JSON::Member> &members);
	// a receiver's "serial" and "input", wherever they stand among its keys
	static void receiverInput(const std::vector<JSON::Member> &members, std::string &serial, std::string &input);
	// a receiver's keys, applied to r as a receiver object's are: its input, its settings, its device blocks, its
	// channels and its engines, in the reader's order; unspecAllowed for the main section, whose other keys are the
	// site's; own_mmsi takes the receiver's "own_mmsi", when given a place
	static void applyReceiver(Receiver &r, const std::vector<JSON::Member> &members, bool unspecAllowed, int *own_mmsi);
	// a key of the main section that is its receiver's: a value, a device block, the input, the models, the array
	static bool isReceiverKey(int key);
	// the name a model number is written with as an engine's "type"
	static std::string modelName(int model);
	// f's mistake, said of where: "receiver 2: …"; said as it was with no where
	template <typename F>
	static void prefixed(const std::string &where, F f)
	{
		try
		{
			f();
		}
		catch (const std::exception &e)
		{
			if (where.empty())
				throw;
			throw std::runtime_error(where + ": " + e.what());
		}
	}
	// relative paths in a config file resolve against the file's directory
	static void setConfigBaseDir(const std::string &file_config);
	static const std::string &baseDir() { return base_dir; }

	// said by the reader, and by the command line's translation for what it reads itself
	static const char *const MODEL_DEPRECATED;
	static const char *const RECEIVERS_NOT_ARRAY;
};
