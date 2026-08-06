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

#include <string>
#include <vector>

#include "Engine.h"
#include "ControlCore.h"

namespace Managed
{
	bool isInvocation(const std::vector<std::string> &args);
	// Managed mode drives the web viewer and serves its control UI from the baked-in
	// web database, so it is only available in a build that includes the viewer.
	int run(const std::vector<std::string> &args);
}
