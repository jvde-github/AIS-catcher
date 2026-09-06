#pragma once

#include <cmath>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include "Parser.h"
#include "Writer.h"

// Static map landmarks, independent of vessel destination matching.
struct Port
{
	static const int KIND = 9;
	std::string code, name, country;
	double lat, lon;
	int size; // 0 unknown/very small, 1 small, 2 medium, 3 large

	void writeFields(JSON::Writer &w) const { w.kv("code", code).kv("country", country); }
	void writeRow(JSON::Writer &w) const
	{
		w.beginObject().kv("id", "p" + code).kv("kind", KIND).kv("lat", lat).kv("lon", lon).kv("label", name);
		writeFields(w);
		w.endObject();
	}

	static std::vector<Port> load(const std::string &filename)
	{
		std::ifstream f(filename);
		if (!f) throw std::runtime_error("Cannot open ports file: " + filename);
		std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		JSON::Parser parser;
		auto doc = parser.parse("{\"ports\":" + text + "}");
		const auto *rows = doc.root[AIS::KEY_SETTING_PORTS];
		if (!rows || !rows->isArray()) throw std::runtime_error("Ports file must contain a JSON array");
		std::vector<Port> ports;
		std::set<std::string> codes;
		for (const auto &row : rows->getArray())
		{
			if (!row.isObject()) throw std::runtime_error("Port must be a JSON object");
			const auto &o = row.getObject();
			const auto *code = o[AIS::KEY_PORT_CODE], *name = o[AIS::KEY_NAME], *country = o[AIS::KEY_COUNTRY];
			const auto *lat = o[AIS::KEY_LAT], *lon = o[AIS::KEY_LON];
			if (!code || !code->isString() || code->getString().empty() || code->getString().size() > 8 ||
				code->getString().find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789^") != std::string::npos ||
				!name || !name->isString() || name->getString().empty() || !country || !country->isString() ||
				country->getString().size() != 2 || country->getString().find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") != std::string::npos ||
				!lat || !(lat->isInt() || lat->isFloat()) || !lon || !(lon->isInt() || lon->isFloat()) ||
				!std::isfinite(lat->getFloat()) || !std::isfinite(lon->getFloat()) || std::abs(lat->getFloat()) > 90 || std::abs(lon->getFloat()) > 180)
				throw std::runtime_error("Invalid port record in " + filename + " at row " + std::to_string(ports.size() + 1));
			Port p{code->getString(), name->getString(), country->getString(), lat->getFloat(), lon->getFloat(), 0};
			if (const auto *size = o[AIS::KEY_PORT_SIZE])
			{
				if (!size->isInt() || size->getInt() < 0 || size->getInt() > 3)
					throw std::runtime_error("Port size must be an integer from 0 to 3: " + p.code);
				p.size = (int)size->getInt();
			}
			if (!codes.insert(p.code).second) throw std::runtime_error("Duplicate port code: " + p.code);
			ports.push_back(std::move(p));
		}
		return ports;
	}
};
