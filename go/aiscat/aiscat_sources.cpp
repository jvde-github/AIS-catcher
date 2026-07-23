// Compile the same AIS-catcher source subset used by the Python aiscat module.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../Source/Marine/AIS.cpp"
#include "../../Source/Marine/Message.cpp"
#include "../../Source/Marine/NMEA.cpp"
#include "../../Source/JSON/JSON.cpp"
#include "../../Source/JSON/JSONAIS.cpp"
#include "../../Source/JSON/Keys.cpp"
#include "../../Source/JSON/Parser.cpp"
#include "../../Source/Library/Logger.cpp"
#include "../../Source/Utilities/Convert.cpp"
#include "../../Source/Utilities/Helper.cpp"
#include "../../Source/Utilities/Parse.cpp"
