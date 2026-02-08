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
#include <iostream>
#include <memory>

#include "Common.h"

namespace JSON
{

	class JSON;
	class Property;

	// JSON value item, 8 bytes (32 bits), 16 bytes (64 bits)
	class Value
	{

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
		} type;

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
		const std::string getString() const { return isString() ? *data.s : std::string(""); }
		const JSON &getObject() const { return *data.o; }

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

	class Property
	{

		int key;
		Value value;

	public:
		Property(int p, Value v)
		{
			key = p;
			value = v;
		}
		Property(int p, long int v)
		{
			key = p;
			value.setInt(v);
		}
		Property(int p, double v)
		{
			key = p;
			value.setFloat(v);
		}
		Property(int p, bool v)
		{
			key = p;
			value.setBool(v);
		}
		Property(int p, std::string *v)
		{
			key = p;
			value.setString(v);
		}
		Property(int p, std::vector<std::string> *v)
		{
			key = p;
			value.setStringArray(v);
		}
		Property(int p, JSON *v)
		{
			key = p;
			value.setObject(v);
		}
		Property(int p)
		{
			key = p;
			value.setNull();
		}

		int Key() const { return key; }
		const Value &Get() const { return value; }
	};

	class JSON
	{
		friend class Parser;

	private:
		std::vector<Property> properties;

		// memory to pointers containing objects, strings and arrays
		// Property and Value can therefore only contain pointers and basic data types
		std::vector<std::shared_ptr<JSON>> objects;
		std::vector<std::shared_ptr<std::string>> strings;
		std::vector<std::shared_ptr<std::vector<Value>>> arrays;

	public:
		void *binary = NULL;

		void clear()
		{
			properties.clear();

			objects.clear();
			strings.clear();
			arrays.clear();
		}

		const std::vector<Property> &getProperties() const { return properties; }

		const Value *getValue(int p) const
		{
			for (auto &o : properties)
				if (o.Key() == p)
					return &o.Get();

			return nullptr;
		}

		const Value *operator[](int p) { return getValue(p); }

		void Add(int p, int v)
		{
			properties.push_back(Property(p, (long int)v));
		}

		void Add(int p, double v)
		{
			properties.push_back(Property(p, (double)v));
		}

		void Add(int p, bool v)
		{
			properties.push_back(Property(p, (bool)v));
		}

		void Add(int p, std::shared_ptr<JSON> &v)
		{
			objects.push_back(v);
			properties.push_back(Property(p, (JSON *)v.get()));
		}

		void Add(int p, const std::string &v)
		{
			strings.push_back(std::shared_ptr<std::string>(new std::string(v)));
			properties.push_back(Property(p, (std::string *)strings.back().get()));
		}

		void Add(int p)
		{
			properties.push_back(Property(p));
		}

		void Add(int p, Value v)
		{
			properties.push_back(Property(p, (Value)v));
		}

		// for items where memory is managed outside the object
		void Add(int p, const std::string *v)
		{
			properties.push_back(Property(p, (std::string *)v));
		}

		void Add(int p, const std::vector<std::string> *v)
		{
			properties.push_back(Property(p, (std::vector<std::string> *)v));
		}
	};
}
