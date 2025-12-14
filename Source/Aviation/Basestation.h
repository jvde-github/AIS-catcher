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

#include <iomanip>
#include <sstream>
#include <vector>

#include "Message.h"
#include "Stream.h"
#include "Parse.h"
#include "Keys.h"
#include "ADSB.h"

class Basestation : public SimpleStreamInOut<RAW, Plane::ADSB>
{
    std::string line;
    bool dropping = false;
    const int MAX_BASESTATION_LINE_LEN = 8192;

    void processLine();

public:
    virtual ~Basestation() {}
    void Receive(const RAW *data, int len, TAG &tag);
};
