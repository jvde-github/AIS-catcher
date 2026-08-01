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

// Tiered block store for ship tracks.
//
// Points live in fixed blocks of 256 and form a doubly-linked, time-ordered
// list per ship. The list is circular through a per-ship anchor held here: the
// oldest point's prev and the newest point's next refer back to the ship. A
// reference packs (block << 8 | slot) in a uint32_t; the top bit marks a ship
// anchor instead. A live point never holds NIL, so prev == NIL marks a dead
// slot awaiting block recycling.
//
// Blocks are chained into three tiers, head = newest:
//   RT    full-resolution points, roughly the last hour
//   FIVE  older history, thinned to one point per GRANULARITY
//   FREE  recycled blocks
//
// A full RT block older than HORIZON is compacted: each point is kept (moved
// to FIVE) if it is at least GRANULARITY after the previous kept point of the
// same ship, else unlinked. Under memory pressure the tiers degrade in order:
// force-compact young RT blocks first, then overwrite the oldest FIVE history,
// and only as a last resort drop recent RT data.

class PathStore
{
public:
	struct Point
	{
		float lat, lon;
		uint32_t time;
		uint32_t prev, next;
		uint16_t cog, sog; // 0.1 degree / 0.1 knot, NA when unknown
	};

	static const uint16_t NA = 0xFFFF;

private:
	static const int BLOCK_SIZE = 256;
	static const uint32_t SHIP_BIT = 0x80000000u;
	static const uint32_t NIL = 0xFFFFFFFFu;

	static const uint32_t HORIZON = 3600;	// full-resolution window in seconds
	static const uint32_t GRANULARITY = 300; // point spacing beyond HORIZON, also the prune interval
	static const int DEADBAND = 40;			 // meters a ship must move to count as significant

	enum Tier
	{
		RT = 0,
		FIVE = 1,
		FREE = 2
	};

	struct Block
	{
		Point pts[BLOCK_SIZE];
		uint32_t t_first = 0, t_last = 0; // start times of first/last point written
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

	Point &deref(uint32_t r) { return blocks[r >> 8].pts[r & (BLOCK_SIZE - 1)]; }

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
		blocks[r >> 8].live--;
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
				unlink((uint32_t)(b << 8) | i);
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

		// copy after the potential eviction above, which may have respliced
		// this point's neighbours
		Point &p = deref(r);
		Block &dst = blocks[d];
		int j = dst.count++;
		dst.live++;

		uint32_t nr = (uint32_t)(d << 8) | j;
		dst.pts[j] = p;
		setNext(p.prev, nr);
		setPrev(p.next, nr);

		dst.t_last = p.time;
		if (j == 0)
			dst.t_first = p.time;
	}

	void compactBlock(int b)
	{
		Block &blk = blocks[b];
		for (int i = 0; i < blk.count; i++)
		{
			Point &p = blk.pts[i];
			if (p.prev == NIL)
				continue;
			// prev is the last kept point of this ship: older points are
			// already thinned and drops below unlink as we go
			if ((p.prev & SHIP_BIT) || p.time - deref(p.prev).time >= GRANULARITY)
				moveToFive((uint32_t)(b << 8) | i);
			else
				unlink((uint32_t)(b << 8) | i);
		}
		detach(b);
		blk.count = blk.live = 0;
		pushHead(FREE, b);
	}

	void compactExpired(uint32_t now)
	{
		// cap the drain so a backlog built up during an idle spell cannot
		// stall a single add; two per fill still outpaces expiry
		for (int k = 0; k < 2; k++)
			if (lists[RT].tail != lists[RT].head && blocks[lists[RT].tail].t_last + HORIZON < now)
				compactBlock(lists[RT].tail);
	}

	int allocRT()
	{
		int b = popFree();
		if (b == -1 && lists[RT].tail != lists[RT].head)
		{
			// shrinking a young block ~12:1 into FIVE beats overwriting a
			// whole block of history
			compactBlock(lists[RT].tail);
			// with FIVE empty the keepers above had nowhere to go; compact a
			// second block so the one just freed can seed the FIVE tier
			if (lists[FIVE].head == -1 && lists[RT].tail != lists[RT].head)
				compactBlock(lists[RT].tail);
			b = popFree();
		}
		if (b == -1 && lists[FIVE].tail != -1)
			b = evictBlock(lists[FIVE].tail);
		if (b == -1)
			b = evictBlock(lists[RT].tail);
		return b;
	}

	static uint16_t encodeCOG(float c) { return (c >= 0 && c < 360) ? (uint16_t)(c * 10 + 0.5f) : NA; }
	static uint16_t encodeSOG(float s) { return (s >= 0 && s < 6553) ? (uint16_t)(s * 10 + 0.5f) : NA; }

	bool significant(const Point &q, float lat, float lon, uint16_t cog, uint16_t sog)
	{
		float dlat = (lat - q.lat) * 111120.0f;
		float dlon = (lon - q.lon) * 111120.0f * cosf(lat * 0.01745329f);
		if (dlat * dlat + dlon * dlon > (float)(DEADBAND * DEADBAND))
			return true;
		if (q.sog != NA && sog != NA && (sog > q.sog ? sog - q.sog : q.sog - sog) > 5)
			return true;
		if (sog != NA && sog > 5 && q.cog != NA && cog != NA)
		{
			int d = cog > q.cog ? cog - q.cog : q.cog - cog;
			if (d > 1800)
				d = 3600 - d;
			if (d > 100)
				return true;
		}
		return false;
	}

public:
	void setup(int nblocks, int nships)
	{
		if (nblocks < 2)
			nblocks = 2;

		blocks.assign(nblocks, Block());
		anchors.resize(nships);
		for (int i = 0; i < nships; i++)
			anchors[i].head = anchors[i].tail = SHIP_BIT | i;

		lists[RT] = lists[FIVE] = lists[FREE] = List();
		for (int b = nblocks - 1; b >= 0; b--)
			pushHead(FREE, b);
	}

	void add(int ship, float lat, float lon, float cog, float sog, std::time_t now)
	{
		uint32_t t = now > 0 ? (uint32_t)now : 0;
		uint16_t c = encodeCOG(cog), s = encodeSOG(sog);

		if (!(anchors[ship].tail & SHIP_BIT))
		{
			Point &q = deref(anchors[ship].tail);
			if (t < q.time)
				t = q.time; // time cannot run backwards within a track
			if (t - q.time < GRANULARITY && !significant(q, lat, lon, c, s))
				return;
		}

		int b = lists[RT].head;
		if (b == -1 || blocks[b].count == BLOCK_SIZE)
		{
			compactExpired(t);
			b = allocRT();
			pushHead(RT, b);
		}

		// fetch the tail only now: compaction or eviction above may have
		// moved or dropped this ship's newest point
		uint32_t tl = anchors[ship].tail;

		Block &blk = blocks[b];
		int i = blk.count++;
		blk.live++;

		Point &p = blk.pts[i];
		p.lat = lat;
		p.lon = lon;
		p.time = t;
		p.cog = c;
		p.sog = s;
		p.prev = tl;
		p.next = SHIP_BIT | ship;

		uint32_t r = (uint32_t)(b << 8) | i;
		setNext(tl, r);
		anchors[ship].tail = r;

		blk.t_last = t;
		if (i == 0)
			blk.t_first = t;
	}

	void wipe(int ship)
	{
		// unlinking the head point advances the anchor, so this walks the
		// whole track and leaves the anchor self-referential
		while (!(anchors[ship].head & SHIP_BIT))
		{
			uint32_t r = anchors[ship].head;
			unlink(r);
			freeIfDead(r >> 8);
		}
	}

	// newest-first traversal: for (r = tail(ship); isPoint(r); r = at(r).prev)
	uint32_t tail(int ship) const { return anchors[ship].tail; }
	static bool isPoint(uint32_t r) { return !(r & SHIP_BIT); }
	const Point &at(uint32_t r) const { return blocks[r >> 8].pts[r & (BLOCK_SIZE - 1)]; }

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
			uint32_t prev_time = 0;
			int steps = 0;

			while (r != self)
			{
				if (!isPoint(r) || (int)(r >> 8) >= nblocks || (int)(r & (BLOCK_SIZE - 1)) >= blocks[r >> 8].count)
				{
					Error() << "PathStore integrity: ship " << ship << " bad ref " << r;
					errors++;
					break;
				}
				Point &p = deref(r);
				if (p.prev != back || p.time < prev_time)
				{
					Error() << "PathStore integrity: ship " << ship << " chain broken at " << r;
					errors++;
					break;
				}
				prev_time = p.time;
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
