/*
	Copyright(c) 2021-2025 jvde.github@gmail.com

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

#include <iostream>
#include <vector>

enum class DecoderSignals {
	StopTraining,
	StartTraining,
	Reset
};
enum class SystemSignal {
	Stop
};

template <typename T>
class SignalIn {
public:
	virtual ~SignalIn(){};
	virtual void Signal(const T& in) {}
};

template <typename T>
class SignalHub {

public:
	std::vector<SignalIn<T>*> destinations;
	void Send(const T& m) {
		for (const auto& d : destinations)
			d->Signal(m);
	}
	void Connect(SignalIn<T>& s) {
		destinations.push_back(&s);
	}
};
