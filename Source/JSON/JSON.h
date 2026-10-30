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

#include <algorithm>
#include <vector>
#include <deque>
#include <string>
#include <cstdint>

#include "Common.h"

namespace JSON
{

	class JSON;
	class Member;

	// JSON value item, 8 bytes (32 bits), 16 bytes (64 bits)
	class Value
	{
	public:
		enum class Type
		{
			BOOL,
			INT,
			FLOAT,
			STRING,
			CSTRING, // a C string the caller owns: an event's text while it is in flight
			OBJECT,
			ARRAY_STRING,
			ARRAY,
			EMPTY
		};

	private:
		Type type;

		union Data
		{
			bool b;
			int64_t i;
			double f;
			std::string *s;
			const char *cs;
			std::vector<std::string> *as;
			std::vector<Value> *a;
			JSON *o;
		} data;

	public:
		double getFloat(double d = 0.0f) const { return isFloat() ? data.f : (isInt() ? (double)(data.i) : d); }
		long int getInt(long int d = 0) const { return isInt() ? (long int)data.i : d; }
		// the full 64 bits: a long is 32 bits on Windows, too short for e.g. nanoseconds since the epoch
		int64_t getInt64(int64_t d = 0) const { return isInt() ? data.i : d; }
		bool getBool(bool d = false) const { return isBool() ? data.b : d; }
		const std::vector<std::string> &getStringArray() const { return *data.as; }
		const std::vector<Value> &getArray() const { return *data.a; }
		std::vector<Value> &getArray() { return *data.a; }
		const std::string &getString() const
		{
			static const std::string empty;
			return isString() ? *data.s : empty;
		}
		// the characters of either string type; "" otherwise
		const char *getCString() const
		{
			return isString() ? data.s->c_str() : (isCString() && data.cs ? data.cs : "");
		}
		const JSON &getObject() const { return *data.o; }
		JSON &getObject() { return *data.o; }

		Type getType() const { return type; }
		bool isObject() const { return type == Type::OBJECT; }
		bool isBool() const { return type == Type::BOOL; }
		bool isArray() const { return type == Type::ARRAY; }
		bool isArrayString() const { return type == Type::ARRAY_STRING; }
		bool isString() const { return type == Type::STRING; }
		bool isCString() const { return type == Type::CSTRING; }
		bool isFloat() const { return type == Type::FLOAT; }
		bool isInt() const { return type == Type::INT; }
		bool isNull() const { return type == Type::EMPTY; }

		void setFloat(double v)
		{
			data.f = v;
			type = Type::FLOAT;
		}
		void setInt(int64_t v)
		{
			data.i = v;
			type = Type::INT;
		}
		void setBool(bool v)
		{
			data.b = v;
			type = Type::BOOL;
		}
		void setNull() { type = Type::EMPTY; }

		void setArray(std::vector<Value> *v)
		{
			data.a = v;
			type = Type::ARRAY;
		}
		void setStringArray(std::vector<std::string> *v)
		{
			data.as = v;
			type = Type::ARRAY_STRING;
		}
		void setString(std::string *v)
		{
			data.s = v;
			type = Type::STRING;
		}
		void setCString(const char *v)
		{
			data.cs = v;
			type = Type::CSTRING;
		}
		void setObject(JSON *v)
		{
			data.o = v;
			type = Type::OBJECT;
		}

		void to_string(std::string &) const;
		std::string to_string() const
		{
			std::string str;
			to_string(str);
			return str;
		}
	};

	// a value as a setting takes it: a list as text separated by commas
	std::string text(const Value &v);

	class Member
	{

		int key;
		Value value;

	public:
		Member(int p, Value v) : key(p), value(v) {}
		Member(int p, int64_t v) : key(p) { value.setInt(v); }
		Member(int p, double v) : key(p) { value.setFloat(v); }
		Member(int p, bool v) : key(p) { value.setBool(v); }
		Member(int p, std::string *v) : key(p) { value.setString(v); }
		Member(int p, std::vector<std::string> *v) : key(p) { value.setStringArray(v); }
		Member(int p, JSON *v) : key(p) { value.setObject(v); }
		Member(int p) : key(p) { value.setNull(); }

		int Key() const { return key; }
		const Value &Get() const { return value; }
		Value &Get() { return value; }
	};

	class Pool;

	class JSON
	{
		friend class Parser;
		friend class Pool;

	private:
		std::vector<Member> members;

	public:
		void *binary = NULL;

		void clear()
		{
			members.clear();
		}

		const std::vector<Member> &getMembers() const { return members; }

		const Value *getValue(int p) const
		{
			for (auto &o : members)
				if (o.Key() == p)
					return &o.Get();

			return nullptr;
		}

		Value *getValue(int p)
		{
			for (auto &o : members)
				if (o.Key() == p)
					return &o.Get();

			return nullptr;
		}

		const Value *operator[](int p) const { return getValue(p); }
		Value *operator[](int p) { return getValue(p); }

		// the object or the array under a key, made when there is none or the
		// key holds something else
		JSON &object(int p, Pool &pool);
		std::vector<Value> &array(int p, Pool &pool);

		void Add(int p) { members.emplace_back(p); }
		void Add(int p, int v) { members.emplace_back(p, (int64_t)v); }
		void Add(int p, int64_t v) { members.emplace_back(p, v); }
		void Add(int p, double v) { members.emplace_back(p, v); }
		void Add(int p, bool v) { members.emplace_back(p, v); }
		void Add(int p, JSON *v) { members.emplace_back(p, v); }
		void Add(int p, Value v) { members.emplace_back(p, v); }

		// for items where memory is managed outside the object
		void Add(int p, const std::string *v) { members.emplace_back(p, (std::string *)v); }
		void Add(int p, const std::vector<std::string> *v) { members.emplace_back(p, (std::vector<std::string> *)v); }

		void Add(int p, const std::string &v, Pool &pool);
		void Set(int p, const std::string &v, Pool &pool);

		void Set(int p, Value v)
		{
			for (auto &o : members)
				if (o.Key() == p)
				{
					o = Member(p, v);
					return;
				}
			members.emplace_back(p, v);
		}

		// every member under a key: a document may write one twice
		void Remove(int p)
		{
			members.erase(std::remove_if(members.begin(), members.end(), [p](const Member &o) { return o.Key() == p; }), members.end());
		}
	};

	// Flat pool: owns all strings, arrays, and nested JSON objects
	class Pool
	{
		std::deque<JSON> objects;
		std::deque<std::string> strings;
		std::deque<std::vector<Value>> arrays;
		std::vector<std::string> extraKeys; // unknown keys, kept by a document reader that asks

		size_t objectCount = 0;
		size_t stringCount = 0;
		size_t arrayCount = 0;

		template <typename T>
		T &acquire(std::deque<T> &pool, size_t &count)
		{
			if (count >= pool.size())
				pool.emplace_back();
			return pool[count++];
		}

	public:
		int extraKey(const std::string &name)
		{
			for (size_t i = 0; i < extraKeys.size(); ++i)
				if (extraKeys[i] == name) return -1 - (int)i;
			extraKeys.push_back(name);
			return -(int)extraKeys.size();
		}
		const std::string &extraName(int key) const { return extraKeys.at(-1 - key); }

		JSON *addObject()
		{
			auto &obj = acquire(objects, objectCount);
			obj.clear();
			return &obj;
		}

		std::string *addString(const std::string &v)
		{
			auto &s = acquire(strings, stringCount);
			s = v;
			return &s;
		}

		std::string *addString(const char *data, size_t len)
		{
			auto &s = acquire(strings, stringCount);
			s.assign(data, len);
			return &s;
		}

		std::vector<Value> *addArray()
		{
			auto &arr = acquire(arrays, arrayCount);
			arr.clear();
			return &arr;
		}

		void clear()
		{
			objectCount = 0;
			stringCount = 0;
			arrayCount = 0;
			extraKeys.clear();
		}
	};

	inline void JSON::Add(int p, const std::string &v, Pool &pool)
	{
		members.emplace_back(p, pool.addString(v));
	}

	inline void JSON::Set(int p, const std::string &v, Pool &pool)
	{
		Value val;
		val.setString(pool.addString(v));
		Set(p, val);
	}

	inline JSON &JSON::object(int p, Pool &pool)
	{
		Value *v = getValue(p);
		if (v && v->isObject())
			return v->getObject();
		Value made;
		made.setObject(pool.addObject());
		Set(p, made);
		return made.getObject();
	}

	inline std::vector<Value> &JSON::array(int p, Pool &pool)
	{
		Value *v = getValue(p);
		if (v && v->isArray())
			return v->getArray();
		Value made;
		made.setArray(pool.addArray());
		Set(p, made);
		return made.getArray();
	}

	// Document: owns a root JSON and its pool
	struct Document
	{
		Pool pool;
		JSON root;

		void clear()
		{
			pool.clear();
			root.clear();
		}

		const std::vector<Member> &getMembers() const { return root.getMembers(); }
	};
}
