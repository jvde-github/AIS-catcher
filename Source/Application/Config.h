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
#include "MsgOut.h"

// applies a document in the configuration file's format to an engine; the site's sections
// go to the readers in SiteConfig.cpp
class Config
{
public:
	// which "server" entries of a document made from the command line are -N's, so a mistake
	// in one is reported as "-N 8100: …"
	struct Origins
	{
		std::vector<std::string> servers;
	};

private:
	Engine &engine;
	// where the configuration file is: its relative file names are there
	std::string folder;
	// managed mode (-E): the control panel's port and address, and the port beside it
	bool managed = false;
	int control_port = 0, viewer_port = 0;
	std::string bind;

	// "receiver": its first active entry fills the engine's first receiver
	void readReceivers(const JSON::Member &m, bool &filled);
	// applied in the reader's order: input, settings, device blocks, channels, engines
	static void applyReceiver(Receiver &r, const std::vector<JSON::Member> &members);
	// "sharing", its key and zones: the community feed, the first output
	static void readSharing(const std::vector<JSON::Member> &members, bool managed, Outputs &into);
	// one section of outputs; false for a key that is not one
	static bool readOutput(const JSON::Member &m, Outputs &into);

public:
	Config(Engine &engine) : engine(engine) {}

	// the site's sections, the stations listed and the default station
	SiteConfig site;
	StationRegistry stations;
	StationDefaults station_defaults;

	// managed mode (-E): the site is read by its rules, its main server on viewer_port
	void setManaged(int control, int viewer, const std::string &address)
	{
		managed = true;
		control_port = control;
		viewer_port = viewer;
		bind = address;
	}
	void setFolder(const std::string &of_file) { folder = of_file; }
	// from: which of the servers are -N's, for a document made from the command line
	void set(const std::string &str, const Origins *from = nullptr);
	void set(JSON::Document &doc, const Origins *from = nullptr);
	// The document in the current form, its files resolved against `folder`; notes: the older forms found.
	// Not alone: a main-section receiver without an input stays the first entry, and true is returned.
	static bool normalise(JSON::Document &doc, const std::string &folder, std::vector<std::string> &notes, bool alone = true);
	static std::string folderOf(const std::string &file);
	// the files a section names, against the document's folder
	static void resolveFiles(JSON::JSON &section, const std::string &folder, JSON::Pool &pool);
	// once every key is read: the stations the receivers name are defined
	void finish();

	// one name-to-backend map for config file and -D alike; throws on unknown type
	static IO::OutputMessage *newDatabaseOutput(const std::string &type);
	// an object's "active", true without one; throws for what is not an object
	static bool isActiveObject(const JSON::Value &m);
	// the document's version and what its main section may not say, checked before anything is applied
	static void preamble(const std::vector<JSON::Member> &members);
	// a receiver's "serial" and "input", wherever they stand among its keys
	static void receiverInput(const std::vector<JSON::Member> &members, std::string &serial, std::string &input);
	// a key of the main section that is its receiver's: a value, a device block, the input, the models, the array
	static bool isReceiverKey(int key);
	static std::string modelName(int model);
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
	static const char *const MODEL_DEPRECATED;
	static const char *const RECEIVERS_NOT_ARRAY;
};
