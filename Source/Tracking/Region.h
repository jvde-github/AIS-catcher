#pragma once

// Marine region of a position: which sea, gulf, strait or lake a vessel is in.
// The table is generated from data/regions/ by scripts/regions/build_regions.py.
namespace Region
{
	static const int NONE = -1;

	int find(float lat, float lon);
	const char *name(int id);
}
