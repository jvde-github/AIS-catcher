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

#include "Common.h"

template <typename T>
class StreamIn {
	uint64_t groups_in = GROUPS_ALL;

public:
	virtual ~StreamIn() {}
	virtual void Receive(const T* data, int len, TAG& tag) {}
	virtual void Receive(T* data, int len, TAG& tag) {
		Receive((const T*)data, len, tag);
	}

	uint64_t getGroupsIn() { return groups_in; }
	void setGroupsIn(uint64_t g) { groups_in = g; }
};

template <typename S>
class Connection {
	std::vector<StreamIn<S>*> connections;
	uint64_t groups = GROUP_OUT_UNDEFINED;

public:
	void Send(const S* data, int len, TAG& tag) {
		for (auto c : connections) c->Receive(data, len, tag);
	}

	void Send(S* data, int len, TAG& tag) {
		if (connections.size() == 0) return;

		if (groups != GROUP_OUT_UNDEFINED)
			tag.group = groups;

		int sz1 = (int)connections.size() - 1;

		for (int i = 0; i < sz1; i++)
			connections[i]->Receive((const S*)data, len, tag);

		connections[sz1]->Receive(data, len, tag);
	}

	void Connect(StreamIn<S>* s) {
		connections.push_back(s);
	}

	void setGroupOut(uint32_t g) { groups = g; }
	uint64_t getGroupOut() { return groups; }
	bool canConnect(uint64_t m) { return (groups & m) > 0; }
	bool isConnected() { return connections.size() > 0; }
	void clear() { connections.resize(0); }
};

template <typename S>
class StreamOut {
public:
	Connection<S> out;

	void Send(const S* data, int len, TAG& tag) {
		out.Send(data, len, tag);
	}

	void Send(S* data, int len, TAG& tag) {
		out.Send(data, len, tag);
	}
};

template <typename T, typename S>
class SimpleStreamInOut : public StreamOut<S>, public StreamIn<T> {
public:
	virtual ~SimpleStreamInOut() {}
};

template <typename S>
class SimpleStreamOut : public StreamOut<S> {
protected:
	TAG tag;

public:
	virtual ~SimpleStreamOut() {}
	void setTag(const TAG& t) { tag = t; }
};

template <typename S>
inline StreamIn<S>& operator>>(Connection<S>& a, StreamIn<S>& b) {
	a.Connect(&b);
	return b;
}
template <typename T, typename S>
inline SimpleStreamInOut<S, T>& operator>>(Connection<S>& a, SimpleStreamInOut<S, T>& b) {
	a.Connect(&b);
	return b;
}

template <typename S>
inline StreamIn<S>& operator>>(StreamOut<S>& a, StreamIn<S>& b) {
	a.out.Connect(&b);
	return b;
}
template <typename S, typename U>
inline SimpleStreamInOut<S, U>& operator>>(StreamOut<S>& a, SimpleStreamInOut<S, U>& b) {
	a.out.Connect(&b);
	return b;
}

template <typename T, typename S>
inline StreamIn<S>& operator>>(SimpleStreamInOut<T, S>& a, StreamIn<S>& b) {
	a.out.Connect(&b);
	return b;
}
template <typename T, typename S, typename U>
inline SimpleStreamInOut<S, U>& operator>>(SimpleStreamInOut<T, S>& a, SimpleStreamInOut<S, U>& b) {
	a.out.Connect(&b);
	return b;
}
