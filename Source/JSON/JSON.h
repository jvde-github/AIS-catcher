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

#include <vector>
#include <deque>
#include <iostream>

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
			long int i;
			double f;
			std::string *s;
			std::vector<std::string> *as;
			std::vector<Value> *a;
			JSON *o;
		} data;

	public:
		double getFloat(double d = 0.0f) const { return isFloat() ? data.f : (isInt() ? (double)(data.i) : d); }
		long int getInt(long int d = 0) const { return isInt() ? data.i : d; }
		bool getBool(bool d = false) const { return isBool() ? data.b : d; }
		const std::vector<std::string> &getStringArray() const { return *data.as; }
		const std::vector<Value> &getArray() const { return *data.a; }
		const std::string &getString() const
		{
			static const std::string empty;
			return isString() ? *data.s : empty;
		}
		const std::string &getStringRef() const
		{
			static const std::string empty;
			return isString() ? *data.s : empty;
		}
		const JSON &getObject() const { return *data.o; }

		Type getType() const { return type; }
		const bool isObject() const { return type == Type::OBJECT; }
		const bool isBool() const { return type == Type::BOOL; }
		const bool isArray() const { return type == Type::ARRAY; }
		const bool isArrayString() const { return type == Type::ARRAY_STRING; }
		const bool isString() const { return type == Type::STRING; }
		const bool isFloat() const { return type == Type::FLOAT; }
		const bool isInt() const { return type == Type::INT; }

		void setFloat(double v)
		{
			data.f = v;
			type = Type::FLOAT;
		}
		void setInt(long int v)
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

	class Member
	{

		int key;
		Value value;

	public:
		Member(int p, Value v) : key(p), value(v)
		{
		}
		Member(int p, long int v) : key(p)
		{
			value.setInt(v);
		}
		Member(int p, double v) : key(p)
		{
			value.setFloat(v);
		}
		Member(int p, bool v)
		{
			key = p;
			value.setBool(v);
		}
		Member(int p, std::string *v)
		{
			key = p;
			value.setString(v);
		}
		Member(int p, std::vector<std::string> *v)
		{
			key = p;
			value.setStringArray(v);
		}
		Member(int p, JSON *v)
		{
			key = p;
			value.setObject(v);
		}
		Member(int p)
		{
			key = p;
			value.setNull();
		}

		int Key() const { return key; }
		const Value &Get() const { return value; }
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

		const Value *operator[](int p) const { return getValue(p); }

		void Add(int p, int v)
		{
			members.emplace_back(p, (long int)v);
		}

		void Add(int p, double v)
		{
			members.emplace_back(p, (double)v);
		}

		void Add(int p, bool v)
		{
			members.emplace_back(p, (bool)v);
		}

		void Add(int p, JSON *v)
		{
			members.emplace_back(p, v);
		}

		void Add(int p, const std::string &v, Pool &pool);

		void Add(int p)
		{
			members.emplace_back(p);
		}

		void Add(int p, Value v)
		{
			members.emplace_back(p, (Value)v);
		}

		// for items where memory is managed outside the object
		void Add(int p, const std::string *v)
		{
			members.emplace_back(p, (std::string *)v);
		}

		void Add(int p, const std::vector<std::string> *v)
		{
			members.emplace_back(p, (std::vector<std::string> *)v);
		}
	};

	// Flat pool: owns all strings, arrays, and nested JSON objects
	class Pool
	{
		std::deque<JSON> objects;
		std::deque<std::string> strings;
		std::deque<std::vector<Value>> arrays;

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
		}
	};

	inline void JSON::Add(int p, const std::string &v, Pool &pool)
	{
		members.emplace_back(p, pool.addString(v));
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
