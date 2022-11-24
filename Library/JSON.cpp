/*
	Copyright(c) 2021-2022 jvde.github@gmail.com

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

#include <string>
#include <cmath>

#include "JSON.h"

namespace JSON {

	// StringBuilder - Build string from JSON object

	void StringBuilder::jsonify(const std::string& str, std::string& json) {
		json += '\"';
		for (char c : str) {
			switch (c) {
			case '\"':
				json += "\\\"";
				break;
			case '\\':
				json += "\\\\";
				break;
			case '\r':
			case '\n':
				break;
			default:
				json += c;
			}
		}
		json += '\"';
	}

	void StringBuilder::to_string(std::string& json, const Value& v, int& idx) {

		bool first;

		switch (v.type) {
		case Value::Type::STRING:
			jsonify(*(v.d.s), json);
			break;
		case Value::Type::BOOL:
			json += (v.d.b ? "true" : "false");
			break;
		case Value::Type::INT:
			json += std::to_string(v.d.i);
			break;
		case Value::Type::FLOAT:
			json += std::to_string(v.d.f);
			break;
		case Value::Type::EMPTY:
			json += "null";
			break;
		case Value::Type::OBJECT:
			build(*v.d.o, json);
			break;
		case Value::Type::ARRAY_STRING: {
			json += '[';
			if (v.d.as->size()) {
				jsonify((*v.d.as)[0], json);

				for (int i = 1; i < v.d.as->size(); i++) {
					json += ',';
					jsonify((*(v.d.as))[i], json);
				}
			}
			json += ']';
		} break;
		case Value::Type::ARRAY:

			json += '[';
			first = true;
			for (const auto val : *(v.d.a)) {
				if (!first) json += ',';
				first = false;
				to_string(json, val, idx);
			}
			json += ']';
			break;
		default:
			std::cerr << "JSON: not implemented!" << std::endl;
		}
	}

	void StringBuilder::build(const JSON& object, std::string& json) {
		bool first = true;
		json += '{';
		int idx = 0;
		for (const Property& m : object.objects) {
			int k = m.getKey();
			const std::string& key = KeyMap[k][map];

			if (!key.empty()) {

				if (!first) json += ',';
				json += "\"" + key + "\"" + ':';
				first = false;
				to_string(json, m.Get(), idx);
			}
		}
		json += '}';
	}

	// Parser -- Build JSON object from String

	void Parser::error(const std::string& err, int pos) {
		const int char_limit = 40;
		int from = MAX(pos - char_limit, 0);
		int to = MIN(pos + char_limit, json.size());

		for (int i = from; i < to; i++) {
			char c = json[i];
			char d = (c == '\t' || c == '\r' || c == '\n') ? ' ' : c;
			std::cerr << d;
		}
		std::cerr << std::endl
				  << std::string(MIN(char_limit, pos), ' ') << "^" << std::endl
				  << std::string(MIN(char_limit, pos), ' ') << "|" << std::endl;
		std::cerr << "JSON parser: " << err << std::endl;
		throw "JSON parser: terminating";
	}

	// Lex analysis

	void Parser::skip_whitespace(int& ptr) {
		while (ptr < json.size() && std::isspace(json[ptr])) ptr++;
	}

	void Parser::tokenizer() {

		tokens.clear();
		std::string s;
		int ptr = 0;

		while (ptr < json.size()) {

			skip_whitespace(ptr);
			if (ptr == json.size()) break;

			char c = json[ptr];

			// number
			if (std::isdigit(c) || c == '-') {
				bool floating = false;
				int start_idx = ptr;

				s.clear();

				do {
					if (json[ptr] == '.') {
						if (floating || start_idx == ptr || !std::isdigit(json[ptr - 1]))
							error("malformed number", ptr);
						else
							floating = true;
					}
					s += json[ptr++];

				} while (ptr != json.size() && (std::isdigit(json[ptr]) || json[ptr] == '.'));

				tokens.push_back(Token(floating ? TokenType::FloatingPoint : TokenType::Integer, s, ptr));
			}
			// string
			else if (c == '\"') {

				s.clear();
				ptr++;

				while (ptr != json.size() && json[ptr] != '\"' && json[ptr] != '\n' && json[ptr] != '\r') {
					s += json[ptr++];
				};

				if (json.size() == ptr || json[ptr] != '\"') error("line ends in string literal", ptr);

				tokens.push_back(Token(TokenType::String, s, ptr));
				ptr++;
			}
			// keyword
			else if (c == 't' || c == 'f' || c == 'n') {

				s.clear();

				while (ptr != json.size() && islower(json[ptr])) s += json[ptr++];

				if (s == "true")
					tokens.push_back(Token(TokenType::True, "", ptr));
				else if (s == "false")
					tokens.push_back(Token(TokenType::False, "", ptr));
				else if (s == "null")
					tokens.push_back(Token(TokenType::Null, "", ptr));
				else
					error("illegal identifier : \"" + s + "\"", ptr);
			}
			// special characters
			else {
				switch (c) {
				case '{':
					tokens.push_back(Token(TokenType::LeftBrace, "", ptr));
					break;
				case '}':
					tokens.push_back(Token(TokenType::RightBrace, "", ptr));
					break;
				case '[':
					tokens.push_back(Token(TokenType::LeftBracket, "", ptr));
					break;
				case ']':
					tokens.push_back(Token(TokenType::RightBracket, "", ptr));
					break;
				case ':':
					tokens.push_back(Token(TokenType::Colon, "", ptr));
					break;
				case ',':
					tokens.push_back(Token(TokenType::Comma, "", ptr));
					break;
				default:
					error("illegal character '" + std::string(1, c) + "'", ptr);
					break;
				}
				ptr++;
			}
		}
		tokens.push_back(Token(TokenType::End, "", ptr));
	}

	// Parsing functions

	void Parser::error_parser(const std::string& err) {
		error(err, tokens[MIN(tokens.size() - 1, idx)].pos);
	}
	bool Parser::is_match(TokenType t) {
		if (idx >= tokens.size() || tokens[idx].type == TokenType::End) error_parser("unexpected end in input");
		return tokens[idx].type == t;
	}

	void Parser::must_match(TokenType t, const std::string& err) {
		if (!is_match(t)) error_parser(err);
	}

	void Parser::next() {
		idx++;
		if (idx >= tokens.size() || tokens[idx].type == TokenType::End) error_parser("unexpected end in input");
	}

	int Parser::search(const std::string& s) {
		int p = -1;
		for (int i = 0; i < KeyMap.size(); i++)
			if (map < KeyMap[i].size() && KeyMap[i][map] == s) {
				p = i;
				break;
			}
		return p;
	}

	Value Parser::parse_value(JSON* o) {
		Value v = Value();
		v.type = Value::Type::EMPTY;
		switch (tokens[idx].type) {
		case TokenType::Integer:
			v.d.i = Util::Parse::Integer(tokens[idx].text);
			v.type = Value::Type::INT;
			break;
		case TokenType::FloatingPoint:
			v.d.f = Util::Parse::Float(tokens[idx].text);
			v.type = Value::Type::FLOAT;
			break;
		case TokenType::True:
			v.d.b = true;
			v.type = Value::Type::BOOL;
			break;
		case TokenType::LeftBrace:
			v.d.o = parse_core();
			v.type = Value::Type::OBJECT;
			break;
		case TokenType::False:
			v.d.b = false;
			v.type = Value::Type::BOOL;
			break;
		case TokenType::Null:
			v.type = Value::Type::EMPTY;
			break;
		case TokenType::String:
			o->strings.push_back(new std::string(tokens[idx].text));
			v.d.s = o->strings.back();
			v.type = Value::Type::STRING;
			break;
		case TokenType::LeftBracket:
			v.type = Value::Type::ARRAY;
			v.d.a = new std::vector<Value>();
			o->arrays.push_back(v.d.a);

			next();

			while (!is_match(TokenType::RightBracket)) {
				v.d.a->push_back(parse_value(o));
				next();
				if (!is_match(TokenType::Comma)) break;
				next();
			}
			must_match(TokenType::RightBracket, "expected ']'");

			break;
		default:
			error_parser("value parse not implemented");

			break;
		}
		return v;
	}

	JSON* Parser::parse_core() {
		JSON* o = new JSON();

		must_match(TokenType::LeftBrace, "expected '{'");
		next();

		while (is_match(TokenType::String)) {
			int p = search(tokens[idx].text);
			if (p < 0)
				error_parser(tokens[idx].text + " is not an allowed \"key\"");
			next();

			must_match(TokenType::Colon, "expected \':\'");
			next();

			o->Submit(p, parse_value(o));
			next();

			if (!is_match(TokenType::Comma)) break;
			next();
		}

		must_match(TokenType::RightBrace, "expected '}'");

		return o;
	}
	JSON* Parser::parse(const std::string& j) {
		json = j;
		idx = 0;
		tokenizer();
		return parse_core();
	}


	const std::vector<std::vector<std::string>> KeyMap = {
		{ "class", "class", "class", "" },
		{ "device", "device", "device", "" },
		{ "scaled", "", "scaled", "" },
		{ "channel", "channel", "channel", "" },
		{ "signalpower", "signalpower", "signalpower", "" },
		{ "ppm", "ppm", "ppm", "" },
		{ "rxtime", "rxtime", "rxtime", "rxtime" },
		{ "nmea", "nmea", "nmea", "" },
		{ "eta", "", "eta", "" },
		{ "shiptype_text", "", "shiptype_text", "" },
		{ "aid_type_text", "", "aid_type_text", "" },
		// setting
		{ "", "", "", "", "bandwidth" },
		{ "", "", "", "", "bias_tee" },
		{ "", "", "", "", "buffer_count" },
		{ "", "", "", "", "callsign" },
		{ "", "", "", "", "channel" },
		{ "", "", "", "", "device" },
		{ "", "", "", "", "filter" },
		{ "", "", "", "", "freq_offset" },
		{ "", "", "", "", "gzip" },
		{ "", "", "", "", "host" },
		{ "", "", "", "", "http" },
		{ "", "", "", "", "interval" },
		{ "", "", "", "", "msg_output" },
		{ "", "", "", "", "output" },
		{ "", "", "", "", "port" },
		{ "", "", "", "", "protocol" },
		{ "", "", "", "", "rtlagc" },
		{ "", "", "", "", "rtlsdr" },
		{ "", "", "", "", "sample_rate" },
		{ "", "", "", "", "serial" },
		{ "", "", "", "", "timeout" },
		{ "", "", "", "", "tuner" },
		{ "", "", "", "", "udp" },
		{ "", "", "", "", "url" },
		{ "", "", "", "", "userpwd" },
		{ "", "", "", "", "verbose" },
		{ "", "", "", "", "verbose_time" },
		// from ODS
		{ "accuracy", "", "accuracy", "" },
		{ "addressed", "", "", "" },
		{ "aid_type", "", "", "" },
		{ "airtemp", "", "", "" },
		{ "ais_version", "", "", "" },
		{ "alt", "", "", "" },
		{ "assigned", "", "", "" },
		{ "band", "", "", "" },
		{ "band_a", "", "", "" },
		{ "band_b", "", "", "" },
		{ "beam", "", "", "" },
		{ "callsign", "", "callsign", "callsign" },
		{ "cdepth2", "", "", "" },
		{ "cdepth3", "", "", "" },
		{ "cdir", "", "", "" },
		{ "cdir2", "", "", "" },
		{ "cdir3", "", "", "" },
		{ "channel_a", "", "", "" },
		{ "channel_b", "", "", "" },
		{ "country", "", "", "" },
		{ "country_code", "", "", "" },
		{ "course", "", "", "course" },
		{ "course_q", "", "", "" },
		{ "cs", "", "", "" },
		{ "cspeed", "", "", "" },
		{ "cspeed2", "", "", "" },
		{ "cspeed3", "", "", "" },
		{ "dac", "", "", "" },
		{ "data", "", "", "" },
		{ "day", "", "", "" },
		{ "dest_mmsi", "", "", "" },
		{ "dest1", "", "", "" },
		{ "dest2", "", "", "" },
		{ "destination", "", "destination", "destination" },
		{ "dewpoint", "", "", "" },
		{ "display", "", "", "" },
		{ "draught", "", "", "draught" },
		{ "dsc", "", "", "" },
		{ "dte", "", "", "" },
		{ "epfd", "", "epfd", "" },
		{ "epfd_text", "", "epfd_text", "" },
		{ "fid", "", "", "" },
		{ "gnss", "", "", "" },
		{ "hazard", "", "", "" },
		{ "heading", "", "heading", "heading" },
		{ "heading_q", "", "", "" },
		{ "hour", "", "", "" },
		{ "humidity", "", "", "" },
		{ "ice", "", "", "" },
		{ "imo", "", "", "" },
		{ "increment1", "", "", "" },
		{ "increment2", "", "", "" },
		{ "increment3", "", "", "" },
		{ "increment4", "", "", "" },
		{ "interval", "", "", "" },
		{ "lat", "", "lat", "lat" },
		{ "length", "", "length", "length" },
		{ "leveltrend", "", "", "" },
		{ "loaded", "", "", "" },
		{ "lon", "", "lon", "lon" },
		{ "maneuver", "", "", "" },
		{ "minute", "", "", "" },
		{ "mmsi", "mmsi", "mmsi", "mmsi" },
		{ "mmsi1", "", "", "" },
		{ "mmsi2", "", "", "" },
		{ "mmsi3", "", "", "" },
		{ "mmsi4", "", "", "" },
		{ "mmsiseq1", "", "", "" },
		{ "mmsiseq2", "", "", "" },
		{ "mmsiseq3", "", "", "" },
		{ "mmsiseq4", "", "", "" },
		{ "mmsi_text", "", "", "" },
		{ "model", "", "", "" },
		{ "month", "", "", "" },
		{ "mothership_mmsi", "", "", "" },
		{ "msg22", "", "", "" },
		{ "name", "", "", "" },
		{ "ne_lat", "", "", "" },
		{ "ne_lon", "", "", "" },
		{ "number1", "", "", "" },
		{ "number2", "", "", "" },
		{ "number3", "", "", "" },
		{ "number4", "", "", "" },
		{ "off_position", "", "", "" },
		{ "offset1", "", "", "" },
		{ "offset1_1", "", "", "" },
		{ "offset1_2", "", "", "" },
		{ "offset2", "", "", "" },
		{ "offset2_1", "", "", "" },
		{ "offset3", "", "", "" },
		{ "offset4", "", "", "" },
		{ "partno", "", "", "partno" },
		{ "power", "", "", "" },
		{ "preciptype", "", "", "" },
		{ "pressure", "", "", "" },
		{ "pressuretend", "", "", "" },
		{ "quiet", "", "", "" },
		{ "radio", "", "", "" },
		{ "raim", "", "", "" },
		{ "regional", "", "", "" },
		{ "repeat", "", "", "" },
		{ "reserved", "", "", "" },
		{ "retransmit", "", "", "" },
		{ "salinity", "", "", "" },
		{ "seastate", "", "", "" },
		{ "second", "", "", "" },
		{ "seqno", "", "", "" },
		{ "serial", "", "", "" },
		{ "ship_type", "", "ship_type", "" },
		{ "shipname", "", "shipname", "shipname" },
		{ "shiptype", "", "shiptype", "shiptype" },
		{ "Spare", "", "", "" },
		{ "speed", "", "speed", "speed" },
		{ "speed_q", "", "", "" },
		{ "station_type", "", "", "" },
		{ "status", "", "status", "status" },
		{ "status_text", "", "status_text", "" },
		{ "sw_lat", "", "", "" },
		{ "sw_lon", "", "", "" },
		{ "swelldir", "", "", "" },
		{ "swellheight", "", "", "" },
		{ "swellperiod", "", "", "" },
		{ "text", "", "", "" },
		{ "timeout1", "", "", "" },
		{ "timeout2", "", "", "" },
		{ "timeout3", "", "", "" },
		{ "timeout4", "", "", "" },
		{ "timestamp", "", "", "" },
		{ "to_bow", "", "to_bow", "ref_front" },
		{ "to_port", "", "to_port", "ref_left" },
		{ "to_starboard", "", "to_starboard", "" },
		{ "to_stern", "", "to_stern", "" },
		{ "turn", "", "turn", "" },
		{ "txrx", "", "", "" },
		{ "type", "", "", "msgtype" },
		{ "type1_1", "", "", "" },
		{ "type1_2", "", "", "" },
		{ "type2_1", "", "", "" },
		{ "vendorid", "", "", "vendorid" },
		{ "vin", "", "", "" },
		{ "virtual_aid", "", "", "" },
		{ "visgreater", "", "", "" },
		{ "visibility", "", "", "" },
		{ "waterlevel", "", "", "" },
		{ "watertemp", "", "", "" },
		{ "wavedir", "", "", "" },
		{ "waveheight", "", "", "" },
		{ "waveperiod", "", "", "" },
		{ "wdir", "", "", "" },
		{ "wgust", "", "", "" },
		{ "wgustdir", "", "", "" },
		{ "wspeed", "", "", "" },
		{ "year", "", "", "" },
		{ "zonesize", "", "", "" }
	};
}