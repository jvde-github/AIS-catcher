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
#include <cstdint>
#include <cmath>
#include <ctime>
#include <vector>

#ifdef CHECK_DB_INTEGRITY
#include "Logger.h"
#endif

// Tiered block store for ship tracks. Points live in blocks and form a
// doubly-linked, time-ordered list per ship, circular through a per-ship
// anchor: a reference packs (block << BLOCK_SHIFT | slot) in a uint32_t, the
// top bit marks a ship anchor, prev == NIL marks a dead slot. A point covers
// [time, time + dur]: a stationary ship extends its newest point's dwell
// instead of adding points. Blocks chain into three tiers (head = newest): RT
// holds full resolution for roughly the last hour, FIVE older history thinned
// to one point per GRANULARITY, FREE recycled blocks. Under memory pressure
// young RT blocks are force-compacted first, then the oldest FIVE history is
// overwritten.

class PathStore
{
	static const uint32_t DUR_UNIT = 2; // seconds per dwell unit

public:
	struct Point
	{
		float lat, lon;
		uint32_t time;
		uint32_t prev, next;
		uint16_t dur;	  // dwell past `time` in DUR_UNIT seconds
		uint8_t cog, sog; // 2 degree / 0.5 knot units, NA when unknown

		uint32_t end() const { return time + (uint32_t)dur * DUR_UNIT; }
	};

	static const uint8_t NA = 0xFF;

private:
	static const int BLOCK_SHIFT = 8;
	static const int BLOCK_SIZE = 1 << BLOCK_SHIFT;
	static const uint32_t SHIP_BIT = 0x80000000u;
	static const uint32_t NIL = 0xFFFFFFFFu;

	static const uint32_t HORIZON = 3600;	 // full-resolution window in seconds
	static const uint32_t GRANULARITY = 300; // point spacing beyond HORIZON
	static const uint32_t DWELL_GAP = 900;	 // silence that ends a dwell: two missed 3-minute reports
	static const int DEADBAND = 40;			 // meters a ship must move to count as significant
	static const int BAND_MOORED = 50;		 // alongside, only fenders and GPS wander
	static const int BAND_ANCHORED = 200;	 // swinging on the chain
	static const int BAND_SLOW = 100;		 // idle but status unknown, Class B carries none
	static const uint8_t IDLE_SOG = 1;		 // 0.5 knot units, at or below this a ship is not making way

	static const int NAV_ANCHORED = 1; // ITU-R M.1371 navigational status
	static const int NAV_MOORED = 5;

	enum Tier
	{
		RT = 0,
		FIVE = 1,
		FREE = 2
	};

	struct Block
	{
		Point pts[BLOCK_SIZE];
		int prev = -1, next = -1;
		uint16_t count = 0, live = 0;
		uint8_t tier = FREE;
	};

	struct List
	{
		int head = -1, tail = -1;
	};

	struct Anchor
	{
		uint32_t head, tail;
	};

	std::vector<Block> blocks;
	std::vector<Anchor> anchors;
	List lists[3];

	static uint32_t ref(int b, int i) { return ((uint32_t)b << BLOCK_SHIFT) | i; }

	Point &deref(uint32_t r) { return blocks[r >> BLOCK_SHIFT].pts[r & (BLOCK_SIZE - 1)]; }

	void setNext(uint32_t r, uint32_t v)
	{
		if (r & SHIP_BIT)
			anchors[r & ~SHIP_BIT].head = v;
		else
			deref(r).next = v;
	}

	void setPrev(uint32_t r, uint32_t v)
	{
		if (r & SHIP_BIT)
			anchors[r & ~SHIP_BIT].tail = v;
		else
			deref(r).prev = v;
	}

	void unlink(uint32_t r)
	{
		Point &p = deref(r);
		setNext(p.prev, p.next);
		setPrev(p.next, p.prev);
		p.prev = NIL;
		blocks[r >> BLOCK_SHIFT].live--;
	}

	void pushHead(int tier, int b)
	{
		Block &blk = blocks[b];
		blk.tier = tier;
		blk.prev = -1;
		blk.next = lists[tier].head;
		if (blk.next != -1)
			blocks[blk.next].prev = b;
		lists[tier].head = b;
		if (lists[tier].tail == -1)
			lists[tier].tail = b;
	}

	void detach(int b)
	{
		Block &blk = blocks[b];
		List &l = lists[blk.tier];
		if (blk.prev != -1)
			blocks[blk.prev].next = blk.next;
		else
			l.head = blk.next;
		if (blk.next != -1)
			blocks[blk.next].prev = blk.prev;
		else
			l.tail = blk.prev;
		blk.prev = blk.next = -1;
	}

	int popFree()
	{
		int b = lists[FREE].head;
		if (b != -1)
			detach(b);
		return b;
	}

	int evictBlock(int b)
	{
		Block &blk = blocks[b];
		for (int i = 0; i < blk.count; i++)
			if (blk.pts[i].prev != NIL)
				unlink(ref(b, i));
		detach(b);
		blk.count = blk.live = 0;
		return b;
	}

	void freeIfDead(int b)
	{
		Block &blk = blocks[b];
		if (blk.live > 0 || blk.tier == FREE || lists[blk.tier].head == b)
			return;
		detach(b);
		blk.count = 0;
		pushHead(FREE, b);
	}

	void moveToFive(uint32_t r)
	{
		int d = lists[FIVE].head;
		if (d == -1 || blocks[d].count == BLOCK_SIZE)
		{
			d = popFree();
			if (d == -1 && lists[FIVE].tail != -1)
				d = evictBlock(lists[FIVE].tail);
			if (d == -1)
			{
				unlink(r);
				return;
			}
			pushHead(FIVE, d);
		}

		// the eviction above may have respliced this point's neighbours
		Point &p = deref(r);
		Block &dst = blocks[d];
		int j = dst.count++;
		dst.live++;

		uint32_t nr = ref(d, j);
		dst.pts[j] = p;
		setNext(p.prev, nr);
		setPrev(p.next, nr);
	}

	void compactBlock(int b)
	{
		Block &blk = blocks[b];
		for (int i = 0; i < blk.count; i++)
		{
			Point &p = blk.pts[i];
			if (p.prev == NIL)
				continue;
			// prev is the last kept point of this ship since drops unlink as we go
			if ((p.prev & SHIP_BIT) || p.time - deref(p.prev).time >= GRANULARITY)
				moveToFive(ref(b, i));
			else
				unlink(ref(b, i));
		}
		detach(b);
		blk.count = blk.live = 0;
		pushHead(FREE, b);
	}

	uint32_t newestTime(int b) { return blocks[b].pts[blocks[b].count - 1].time; }

	void compactExpired(uint32_t now)
	{
		// capped so a backlog after an idle spell cannot stall a single add
		for (int k = 0; k < 2; k++)
			if (lists[RT].tail != lists[RT].head && newestTime(lists[RT].tail) + HORIZON < now)
				compactBlock(lists[RT].tail);
	}

	int allocRT()
	{
		int b = popFree();
		if (b == -1 && lists[RT].tail != lists[RT].head)
		{
			// thinning a young block into FIVE beats overwriting history
			compactBlock(lists[RT].tail);
			// a second pass lets the block just freed seed an empty FIVE tier
			if (lists[FIVE].head == -1 && lists[RT].tail != lists[RT].head)
				compactBlock(lists[RT].tail);
			b = popFree();
		}
		if (b == -1 && lists[FIVE].tail != -1)
			b = evictBlock(lists[FIVE].tail);
		if (b == -1 && lists[RT].tail != -1)
			b = evictBlock(lists[RT].tail);
		return b;
	}

	static uint8_t encodeCOG(float c) { return (c >= 0 && c < 360) ? (uint8_t)(c / 2 + 0.5f) : NA; }

	static uint8_t encodeSOG(float s)
	{
		if (s < 0)
			return NA;
		float v = s * 2 + 0.5f;
		return v < 254.0f ? (uint8_t)v : (uint8_t)254;
	}

	// cos(lat) within 0.7% up to 85 degrees, several times cheaper than libm on the add() path
	static float cosLat(float lat)
	{
		float x = lat * 0.01745329f, x2 = x * x;
		return 1.0f + x2 * (-0.5f + x2 * (0.0416667f - x2 * 0.00138889f));
	}

	static int idleBand(int status)
	{
		if (status == NAV_MOORED)
			return BAND_MOORED;
		if (status == NAV_ANCHORED)
			return BAND_ANCHORED;
		return BAND_SLOW;
	}

	bool significant(const Point &q, float lat, float lon, uint8_t cog, uint8_t sog, int status)
	{
		bool idle = q.sog != NA && sog != NA && q.sog <= IDLE_SOG && sog <= IDLE_SOG;
		int band = idle ? idleBand(status) : DEADBAND;

		float dlat = (lat - q.lat) * 111120.0f;
		float dlon = (lon - q.lon) * 111120.0f * cosLat(lat);
		if (dlat * dlat + dlon * dlon > (float)(band * band))
			return true;
		if (q.sog != NA && sog != NA && (sog > q.sog ? sog - q.sog : q.sog - sog) > 1)
			return true;
		if (sog != NA && sog > 1 && q.cog != NA && cog != NA)
		{
			int d = cog > q.cog ? cog - q.cog : q.cog - cog;
			if (d > 90)
				d = 180 - d;
			if (d > 5)
				return true;
		}
		return false;
	}

public:
	void setup(int nblocks, int nships)
	{
		if (nblocks < 2)
			nblocks = 2;
		if (nblocks > (1 << (31 - BLOCK_SHIFT)))
			nblocks = 1 << (31 - BLOCK_SHIFT); // refs must not collide with SHIP_BIT

		blocks.assign(nblocks, Block());
		anchors.resize(nships);
		for (int i = 0; i < nships; i++)
			anchors[i].head = anchors[i].tail = SHIP_BIT | i;

		lists[RT] = lists[FIVE] = lists[FREE] = List();
		for (int b = nblocks - 1; b >= 0; b--)
			pushHead(FREE, b);
	}

	void add(int ship, float lat, float lon, float cog, float sog, int status, std::time_t now)
	{
		uint32_t t = now > 0 ? (uint32_t)now : 0;
		uint8_t c = encodeCOG(cog), s = encodeSOG(sog);

		if (!(anchors[ship].tail & SHIP_BIT))
		{
			Point &q = deref(anchors[ship].tail);
			if (t < q.end())
				t = q.end(); // time cannot run backwards within a track

			// stationary and continuous: extend the dwell of the newest point
			if (!significant(q, lat, lon, c, s, status) && t - q.end() <= DWELL_GAP && t - q.time < 0xFFFFu * DUR_UNIT)
			{
				q.dur = (uint16_t)((t - q.time) / DUR_UNIT);
				return;
			}
		}

		int b = lists[RT].head;
		if (b == -1 || blocks[b].count == BLOCK_SIZE)
		{
			compactExpired(t);
			b = allocRT();
			if (b == -1)
				return;
			pushHead(RT, b);
		}

		// compaction or eviction above may have moved this ship's newest point
		uint32_t tl = anchors[ship].tail;

		Block &blk = blocks[b];
		int i = blk.count++;
		blk.live++;

		Point &p = blk.pts[i];
		p.lat = lat;
		p.lon = lon;
		p.time = t;
		p.dur = 0;
		p.cog = c;
		p.sog = s;
		p.prev = tl;
		p.next = SHIP_BIT | ship;

		uint32_t r = ref(b, i);
		setNext(tl, r);
		anchors[ship].tail = r;
	}

	void wipe(int ship)
	{
		// unlinking the head advances the anchor until it is self-referential
		while (!(anchors[ship].head & SHIP_BIT))
		{
			uint32_t r = anchors[ship].head;
			unlink(r);
			freeIfDead(r >> BLOCK_SHIFT);
		}
	}

	// newest-first traversal: for (r = tail(ship); isPoint(r); r = at(r).prev)
	uint32_t tail(int ship) const { return anchors[ship].tail; }
	static bool isPoint(uint32_t r) { return !(r & SHIP_BIT); }
	const Point &at(uint32_t r) const { return blocks[r >> BLOCK_SHIFT].pts[r & (BLOCK_SIZE - 1)]; }

#ifdef CHECK_DB_INTEGRITY
	int check()
	{
		int errors = 0;
		int nblocks = (int)blocks.size();
		std::vector<int> seen(nblocks, 0);

		for (int t = 0; t < 3; t++)
		{
			int prev = -1;
			for (int b = lists[t].head; b != -1; b = blocks[b].next)
			{
				if (b < 0 || b >= nblocks || seen[b]++)
				{
					Error() << "PathStore integrity: invalid or repeated block " << b << " in tier " << t;
					errors++;
					break;
				}
				if (blocks[b].tier != t || blocks[b].prev != prev)
				{
					Error() << "PathStore integrity: block " << b << " tier/prev mismatch";
					errors++;
				}
				if (t != FREE && blocks[b].count == 0)
				{
					Error() << "PathStore integrity: block " << b << " empty in tier " << t;
					errors++;
				}
				prev = b;
			}
			if (lists[t].tail != prev)
			{
				Error() << "PathStore integrity: tier " << t << " tail mismatch";
				errors++;
			}
		}

		for (int b = 0; b < nblocks; b++)
		{
			if (seen[b] != 1)
			{
				Error() << "PathStore integrity: block " << b << " in " << seen[b] << " tiers";
				errors++;
			}
			int live = 0;
			for (int i = 0; i < blocks[b].count; i++)
				if (blocks[b].pts[i].prev != NIL)
					live++;
			if (live != blocks[b].live)
			{
				Error() << "PathStore integrity: block " << b << " live " << blocks[b].live << " != " << live;
				errors++;
			}
		}

		for (int ship = 0; ship < (int)anchors.size(); ship++)
		{
			uint32_t self = SHIP_BIT | ship;
			uint32_t back = self, r = anchors[ship].head;
			uint32_t prev_end = 0;
			int steps = 0;

			while (r != self)
			{
				if (!isPoint(r) || (int)(r >> BLOCK_SHIFT) >= nblocks || (int)(r & (BLOCK_SIZE - 1)) >= blocks[r >> BLOCK_SHIFT].count)
				{
					Error() << "PathStore integrity: ship " << ship << " bad ref " << r;
					errors++;
					break;
				}
				Point &p = deref(r);
				if (p.prev != back || p.time < prev_end)
				{
					Error() << "PathStore integrity: ship " << ship << " chain broken at " << r;
					errors++;
					break;
				}
				prev_end = p.end();
				back = r;
				r = p.next;
				if (++steps > nblocks * BLOCK_SIZE)
				{
					Error() << "PathStore integrity: ship " << ship << " chain has cycle";
					errors++;
					break;
				}
			}
			if (r == self && anchors[ship].tail != back)
			{
				Error() << "PathStore integrity: ship " << ship << " tail anchor mismatch";
				errors++;
			}
		}
		return errors;
	}
#endif
};
