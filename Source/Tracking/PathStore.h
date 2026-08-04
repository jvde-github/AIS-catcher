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
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>

#include "Geodesy.h"

// Tiered block store for ship tracks. Points live in blocks and form a
// doubly-linked, time-ordered list per ship, circular through a per-ship
// anchor: a reference packs (block << BLOCK_SHIFT | slot) in a uint32_t, the
// top bit marks a ship anchor, prev == NIL marks a dead slot. A point covers
// [time, time + dur]: a stationary ship extends its newest point's dwell
// instead of adding points. Blocks chain into three tiers (head = newest): RT
// holds full resolution for roughly the last hour, HIST older history thinned
// to one point per GRANULARITY, FREE recycled blocks. Under memory pressure
// young RT blocks are force-compacted first, then the oldest HIST history is
// overwritten.

class PathStore
{

public:
	struct Point
	{
		float lat, lon;
		uint32_t time;
		uint32_t prev, next;
		uint16_t dur;	  // dwell past `time` in seconds
		uint8_t cog, sog; // 2 degree / 0.5 knot units, NA when unknown

		uint32_t end() const { return time + dur; }
	};

	static const uint8_t NA = 0xFF;


private:
	static const int BLOCK_SHIFT = 8;
	static const int BLOCK_SIZE = 1 << BLOCK_SHIFT;
	static const uint32_t SHIP_BIT = 0x80000000u;
	static const uint32_t NIL = 0xFFFFFFFFu;

	static const uint32_t HORIZON = 3600;	 // full-resolution window in seconds
	static const uint32_t GRANULARITY = 300; // point spacing beyond HORIZON
	static const uint32_t DWELL_GAP = 900;	 // silence that ends a dwell
	static const uint32_t DWELL_MAX = 0xFFFFu; // ceiling of the uint16 dur field
	static const int DEADBAND = 40;			 // meters a ship must move to count as significant
	static const uint8_t IDLE_SOG = 1;		 // 0.5 knot units, at or below this a ship is not making way

	enum Tier
	{
		RT = 0,
		HIST = 1,
		FREE = 2
	};

	struct Block
	{
		Point pts[BLOCK_SIZE];
		int prev = -1, next = -1;
		uint16_t count = 0, live = 0;
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
	static int blockOf(uint32_t r) { return (int)(r >> BLOCK_SHIFT); }
	static int slotOf(uint32_t r) { return (int)(r & (BLOCK_SIZE - 1)); }

	Point &deref(uint32_t r) { return const_cast<Point &>(at(r)); }

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
		blocks[blockOf(r)].live--;
	}

	void pushBlockHead(int tier, int b)
	{
		Block &blk = blocks[b];

		blk.prev = -1;
		blk.next = lists[tier].head;

		if (blk.next != -1)
			blocks[blk.next].prev = b;
		
		lists[tier].head = b;
		if (lists[tier].tail == -1)
			lists[tier].tail = b;
	}

	void detachBlock(int tier, int b)
	{
		Block &blk = blocks[b];
		List &l = lists[tier];

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

	int popFreeBlock()
	{
		int b = lists[FREE].head;

		if (b != -1)
			detachBlock(FREE, b);

		return b;
	}

	// only ever called on the HIST tail
	int evictBlock(int b)
	{
		Block &blk = blocks[b];
		for (int i = 0; i < blk.count; i++)
			if (blk.pts[i].prev != NIL)
				unlink(ref(b, i));
		detachBlock(HIST, b);
		blk.count = blk.live = 0;
		return b;
	}

	void moveToHistory(uint32_t r)
	{
		int d = lists[HIST].head;

		if (d == -1 || blocks[d].count == BLOCK_SIZE)
		{
			d = popFreeBlock();
			if (d == -1 && lists[HIST].tail != -1)
				d = evictBlock(lists[HIST].tail);

			if (d == -1)
			{
				unlink(r);
				return;
			}
			pushBlockHead(HIST, d);
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
			// prev is the last kept point of this ship since drops unlink as we go;
			// a ship's newest point always survives, its dwell may still be extending
			if ((p.next & SHIP_BIT) || (p.prev & SHIP_BIT) || p.time - deref(p.prev).time >= GRANULARITY)
				moveToHistory(ref(b, i));
			else
				unlink(ref(b, i));
		}
		detachBlock(RT, b);
		blk.count = blk.live = 0;
		pushBlockHead(FREE, b);
	}

	uint32_t newestTime(int b) { return blocks[b].pts[blocks[b].count - 1].time; }

	void compactExpired(uint32_t now)
	{
		// capped so a backlog after an idle spell cannot stall a single add
		for (int k = 0; k < 2; k++)
			if (lists[RT].tail != lists[RT].head && newestTime(lists[RT].tail) + HORIZON < now)
				compactBlock(lists[RT].tail);
	}

	int allocRTBlock()
	{
		int b = popFreeBlock();

		if (b == -1 && lists[RT].tail != lists[RT].head)
		{
			// thinning a young block into HIST beats overwriting history
			compactBlock(lists[RT].tail);
			// a second pass lets the block just freed seed an empty HIST tier
			if (lists[HIST].head == -1 && lists[RT].tail != lists[RT].head)
				compactBlock(lists[RT].tail);

			b = popFreeBlock();
		}

		if (b == -1 && lists[HIST].tail != -1)
			b = evictBlock(lists[HIST].tail);
		return b;
	}

	static uint8_t encodeCOG(float c) { return (c >= 0 && c < 360) ? (uint8_t)(c / 2 + 0.5f) : NA; }

	static uint8_t encodeSOG(float s)
	{
		if (!(s >= 0))
			return NA;
		float v = s * 2 + 0.5f;
		return v < 254.0f ? (uint8_t)v : (uint8_t)254;
	}

	bool significant(const Point &q, float lat, float lon, uint8_t cog, uint8_t sog, int idle_band)
	{
		bool idle = q.sog != NA && sog != NA && q.sog <= IDLE_SOG && sog <= IDLE_SOG;
		int band = idle ? idle_band : DEADBAND;

		if (Util::Geodesy::distanceSqMeters(q.lat, q.lon, lat, lon) > (float)(band * band))
			return true;

		if (q.sog != NA && sog != NA && std::abs((int)sog - (int)q.sog) > 1)
			return true;

		if (sog != NA && sog > IDLE_SOG && q.cog != NA && cog != NA)
		{
			int d = std::abs((int)cog - (int)q.cog);
			if (d > 90)
				d = 180 - d;
			if (d > 5)
				return true;
		}
		return false;
	}

public:
	// the byte budget covers the per-ship anchors, the block pool gets the
	// rest; returns the resulting block count
	int setup(long budget_bytes, int nships)
	{
		const long left = budget_bytes - (long)nships * (long)sizeof(Anchor);
		int nblocks = left > 0 ? (int)(left / (long)sizeof(Block)) : 0;

		if (nblocks < 2)
			nblocks = 2;
		if (nblocks > (int)(SHIP_BIT >> BLOCK_SHIFT))
			nblocks = (int)(SHIP_BIT >> BLOCK_SHIFT); // refs must not collide with SHIP_BIT

		blocks.assign(nblocks, Block());
		anchors.resize(nships);

		for (int i = 0; i < nships; i++)
			anchors[i].head = anchors[i].tail = SHIP_BIT | i;

		lists[RT] = lists[HIST] = lists[FREE] = List();
		for (int b = nblocks - 1; b >= 0; b--)
			pushBlockHead(FREE, b);

		return nblocks;
	}

	// idle_band: meters a ship not making way may wander before a new point is stored
	void add(int ship, float lat, float lon, float cog, float sog, int idle_band, std::time_t now)
	{
		uint32_t t = now > 0 ? (uint32_t)now : 0;
		uint8_t c = encodeCOG(cog), s = encodeSOG(sog);

		if (isPoint(anchors[ship].tail))
		{
			Point &q = deref(anchors[ship].tail);
			uint32_t qe = q.end();
			if (t < qe)
				t = qe; // time cannot run backwards within a track

			// stationary and continuous: extend the dwell of the newest point
			if (t - qe <= DWELL_GAP && t - q.time < DWELL_MAX && !significant(q, lat, lon, c, s, idle_band))
			{
				q.dur = (uint16_t)(t - q.time);
				return;
			}
		}

		int b = lists[RT].head;
		if (b == -1 || blocks[b].count == BLOCK_SIZE)
		{
			compactExpired(t);
			b = allocRTBlock();
			if (b == -1)
				return;

			pushBlockHead(RT, b);
		}

		// compaction or eviction above may have moved this ship's newest point
		uint32_t tl = anchors[ship].tail;

		Block &blk = blocks[b];
		int i = blk.count++;
		blk.live++;

		blk.pts[i] = {lat, lon, t, tl, SHIP_BIT | (uint32_t)ship, 0, c, s};

		uint32_t r = ref(b, i);
		setNext(tl, r);
		anchors[ship].tail = r;
	}

	void wipe(int ship)
	{
		// unlinking the head advances the anchor until it is self-referential
		while (isPoint(anchors[ship].head))
			unlink(anchors[ship].head);
	}

	// newest-first traversal: for (r = tail(ship); isPoint(r); r = at(r).prev)
	// Blocks fill in slot order and HIST grows at its head, so the oldest point
	// held is the first live slot of the HIST tail, or the RT tail while nothing
	// has aged out. 0 when empty.
	uint32_t oldestTime() const
	{
		const int tiers[2] = {HIST, RT};

		for (int t = 0; t < 2; t++)
		{
			int b = lists[tiers[t]].tail;
			if (b == -1)
				continue;

			const Block &blk = blocks[b];
			for (int i = 0; i < blk.count; i++)
				if (blk.pts[i].prev != NIL)
					return blk.pts[i].time;
		}
		return 0;
	}

	// Mirror of oldestTime: the head block of RT while anything is live, or of
	// HIST on a store that has gone quiet.
	uint32_t newestTime() const
	{
		const int tiers[2] = {RT, HIST};

		for (int t = 0; t < 2; t++)
		{
			int b = lists[tiers[t]].head;
			if (b == -1)
				continue;

			const Block &blk = blocks[b];
			for (int i = blk.count - 1; i >= 0; i--)
				if (blk.pts[i].prev != NIL)
					return blk.pts[i].end();
		}
		return 0;
	}

	uint32_t tail(int ship) const { return anchors[ship].tail; }
	static bool isPoint(uint32_t r) { return !(r & SHIP_BIT); }
	const Point &at(uint32_t r) const { return blocks[blockOf(r)].pts[slotOf(r)]; }

	// only the newest point's dwell can still extend, so an end older than
	// `since` means nothing newer exists
	bool hasSince(int ship, std::time_t since) const
	{
		uint32_t r = tail(ship);
		return isPoint(r) && (std::time_t)at(r).end() >= since;
	}

	// Structural invariants. Returns the number of problems found and appends a
	// description of each to `errors`.
	int check(std::vector<std::string> &errors) const
	{
		int e = 0;
		int nblocks = (int)blocks.size();
		std::vector<int> seen(nblocks, 0);

		auto fail = [&](const std::string &msg) {
			errors.push_back("path store: " + msg);
			e++;
		};

		for (int t = 0; t < 3; t++)
		{
			int prev = -1;
			for (int b = lists[t].head; b != -1; b = blocks[b].next)
			{
				if (b < 0 || b >= nblocks || seen[b]++)
				{
					fail("invalid or repeated block " + std::to_string(b) + " in tier " + std::to_string(t));
					break;
				}
				if (blocks[b].prev != prev)
					fail("block " + std::to_string(b) + " prev mismatch");
				if (t != FREE && blocks[b].count == 0)
					fail("block " + std::to_string(b) + " empty in tier " + std::to_string(t));
				prev = b;
			}
			if (lists[t].tail != prev)
				fail("tier " + std::to_string(t) + " tail mismatch");
		}

		for (int b = 0; b < nblocks; b++)
		{
			if (seen[b] != 1)
				fail("block " + std::to_string(b) + " in " + std::to_string(seen[b]) + " tiers");

			int live = 0;
			for (int i = 0; i < blocks[b].count; i++)
				if (blocks[b].pts[i].prev != NIL)
					live++;
			if (live != blocks[b].live)
				fail("block " + std::to_string(b) + " live " + std::to_string(blocks[b].live) + " != " + std::to_string(live));
		}

		for (int ship = 0; ship < (int)anchors.size(); ship++)
		{
			uint32_t self = SHIP_BIT | ship;
			uint32_t back = self, r = anchors[ship].head;
			uint32_t prev_end = 0;
			long steps = 0;

			while (r != self)
			{
				if (!isPoint(r) || blockOf(r) >= nblocks || slotOf(r) >= blocks[blockOf(r)].count)
				{
					fail("ship " + std::to_string(ship) + " bad ref " + std::to_string(r));
					break;
				}
				const Point &p = at(r);
				if (p.prev != back || p.time < prev_end)
				{
					fail("ship " + std::to_string(ship) + " chain broken at " + std::to_string(r));
					break;
				}
				prev_end = p.end();
				back = r;
				r = p.next;
				if (++steps > (long)nblocks * BLOCK_SIZE)
				{
					fail("ship " + std::to_string(ship) + " chain has cycle");
					break;
				}
			}
			if (r == self && anchors[ship].tail != back)
				fail("ship " + std::to_string(ship) + " tail anchor mismatch");
		}
		return e;
	}
};
