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
#include <string>
#include <vector>

// Fixed-capacity record table with LRU recycling and hashed key lookup.
// Records are pure data in a flat array sized once at setup; all list
// structure lives in a parallel slot table. Every slot is always on the LRU
// list, so untouched slots sit at the tail and are consumed before any live
// record is evicted - the LRU doubles as the free list. key == 0 marks a slot
// that has never been claimed. h_prev holds either a slot index or
// BUCKET_BIT | bucket, so unlinking from a hash chain never needs the old key.

template <typename T, typename Key>
class SlotTable
{
public:
	enum
	{
		NIL = -1
	};

	void setup(int nrecords, int nbuckets)
	{
		records.assign(nrecords, T());
		slots.assign(nrecords, Slot());
		buckets.assign(nbuckets, (int)NIL);
		head = tail = NIL;
		count = 0;

		for (int i = 0; i < nrecords; i++)
			lruPushFront(i);
	}

	T &operator[](int h) { return records[h]; }
	const T &operator[](int h) const { return records[h]; }

	int size() const { return count; }
	int capacity() const { return (int)records.size(); }

	int front() const { return head; }
	int next(int h) const { return slots[h].lru_next; }
	int prev(int h) const { return slots[h].lru_prev; }

	Key key(int h) const { return slots[h].key; }

	// Keyed slots form a contiguous prefix of the LRU, newest first (validate
	// checks this), so iteration stops at the first empty slot. `f` returns
	// false to stop early.
	template <typename F>
	void forEach(F f) const
	{
		for (int h = head; h != NIL; h = slots[h].lru_next)
		{
			if (slots[h].key == 0)
				break;
			if (!f(h))
				break;
		}
	}

	int find(Key k) const
	{
		for (int h = buckets[bucket(k)]; h != NIL; h = slots[h].h_next)
			if (slots[h].key == k)
				return h;
		return NIL;
	}

	// Claim a slot for a key that is not currently in the table. Recycles the
	// LRU victim, rehashes it and moves it to the front. The record still holds
	// the evicted entry's data: the caller decides what clearing means.
	int create(Key k)
	{
		int h = tail;

		if (slots[h].key != 0)
			hashUnlink(h);
		else
			count++;

		slots[h].key = k;
		hashPushFront(bucket(k), h);
		touch(h);
		return h;
	}

	void touch(int h)
	{
		if (h == head)
			return;
		
		lruUnlink(h);
		lruPushFront(h);
	}

	// Structural invariants. Returns the number of problems found and appends a
	// description of each to `errors`.
	int validate(std::vector<std::string> &errors) const
	{
		const int n = (int)records.size();
		int e = 0;
		auto fail = [&](const std::string &msg) { errors.push_back(msg); e++; };

		int seen = 0, keyed = 0;
		for (int h = head, p = NIL; h != NIL; p = h, h = slots[h].lru_next)
		{
			if (++seen > n)
			{
				fail("LRU list is cyclic");
				return e;
			}
			if (slots[h].lru_prev != p)
				fail("LRU prev broken at slot " + std::to_string(h));
			if (slots[h].key != 0)
				keyed++;
			if (slots[h].lru_next == NIL && h != tail)
				fail("LRU tail mismatch at slot " + std::to_string(h));
		}

		if (seen != n)
			fail("LRU holds " + std::to_string(seen) + " of " + std::to_string(n) + " slots");
		if (keyed != count)
			fail("count " + std::to_string(count) + " but " + std::to_string(keyed) + " keyed slots");

		// keyed slots must form a contiguous prefix of the LRU; persistence walks on that basis
		bool empty_seen = false;
		for (int h = head; h != NIL; h = slots[h].lru_next)
		{
			if (slots[h].key == 0)
				empty_seen = true;
			else if (empty_seen)
			{
				fail("keyed slot " + std::to_string(h) + " sits behind an empty one");
				empty_seen = false;
			}
		}

		std::vector<int> in_bucket(n, 0);
		for (int b = 0; b < (int)buckets.size(); b++)
		{
			int walked = 0;
			for (int h = buckets[b], p = BUCKET_BIT | b; h != NIL; p = h, h = slots[h].h_next)
			{
				if (++walked > n)
				{
					fail("hash chain " + std::to_string(b) + " is cyclic");
					return e;
				}
				if (slots[h].h_prev != p)
					fail("hash prev broken at slot " + std::to_string(h));
				if (slots[h].key == 0)
					fail("empty slot " + std::to_string(h) + " on a hash chain");
				else if (bucket(slots[h].key) != b)
					fail("slot " + std::to_string(h) + " in the wrong bucket");
				in_bucket[h]++;
			}
		}

		for (int h = 0; h < n; h++)
		{
			if (slots[h].key != 0 && in_bucket[h] != 1)
				fail("keyed slot " + std::to_string(h) + " appears on " + std::to_string(in_bucket[h]) + " chains");
		}

		return e;
	}

private:
	enum
	{
		BUCKET_BIT = 1 << 30
	};

	struct Slot
	{
		int lru_prev, lru_next;
		int h_prev, h_next;
		Key key;

		Slot() : lru_prev(NIL), lru_next(NIL), h_prev(NIL), h_next(NIL), key(0) {}
	};

	std::vector<T> records;
	std::vector<Slot> slots;
	std::vector<int> buckets;
	int head = NIL, tail = NIL, count = 0;

	int bucket(Key k) const { return (int)(k % (Key)buckets.size()); }

	void lruPushFront(int h)
	{
		slots[h].lru_prev = NIL;
		slots[h].lru_next = head;

		if (head != NIL)
			slots[head].lru_prev = h;

		head = h;
		if (tail == NIL)
			tail = h;
	}

	void lruUnlink(int h)
	{
		int p = slots[h].lru_prev, n = slots[h].lru_next;
		if (p != NIL)
			slots[p].lru_next = n;
		else
			head = n;
		if (n != NIL)
			slots[n].lru_prev = p;
		else
			tail = p;
	}

	void hashPushFront(int b, int h)
	{
		int n = buckets[b];
		slots[h].h_prev = BUCKET_BIT | b;
		slots[h].h_next = n;
		if (n != NIL)
			slots[n].h_prev = h;
		buckets[b] = h;
	}

	// Only valid on a claimed slot: h_prev is then never NIL, which matters
	// because NIL & BUCKET_BIT is non-zero and would index nonsense.
	void hashUnlink(int h)
	{
		int p = slots[h].h_prev, n = slots[h].h_next;
		if (p & BUCKET_BIT)
			buckets[p & ~BUCKET_BIT] = n;
		else
			slots[p].h_next = n;
		if (n != NIL)
			slots[n].h_prev = p;
		slots[h].h_prev = slots[h].h_next = NIL;
	}
};
