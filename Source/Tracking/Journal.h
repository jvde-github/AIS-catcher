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
#include <ctime>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "EventRing.h"
#include "PlaceCatalogue.h"
#include "Stream.h"

// Every event the tracker makes, in the order made, each with a number that
// never repeats within the journal's epoch. It reads the ship table's event
// stream and keeps what the ticker shows, but unlike the ticker's ring folds
// nothing together: a reader that follows the numbers sees each event once.
// The installation's own events are appended to it. Bounded: the oldest fall
// off, and a reader whose place has fallen off is told so instead of silently
// missing what went.
class Journal : public StreamIn<Tracking::Event>
{
public:
	struct Entry
	{
		uint64_t seq = 0;
		EventRing::Entry event;
		std::string place; // the place's id, for an arrival or a departure
	};

	explicit Journal(std::size_t capacity = 2000) : capacity(capacity ? capacity : 1) {}

	// from the ship table, under its lock: copied, and the place named by its id
	void Receive(const Tracking::Event *data, int len, TAG &) override
	{
		for (int i = 0; i < len; i++)
		{
			// a broadcast repeated within its wait: once is news, the rest is noise
			if (data[i].repeat)
				continue;
			EventRing::Entry e;
			if (!EventRing::entryOf(data[i], e))
				continue;
			std::string place;
			if (e.kind == Tracking::Kind::ENTER || e.kind == Tracking::Kind::LEAVE)
			{
				std::lock_guard<std::mutex> lock(places_mtx);
				const PlaceIndex::Entry *at = places ? places->find(data[i].crossing.place) : nullptr;
				if (at)
					place = at->metadata->uuid;
			}
			append(e, place);
		}
	}

	// the places the ship table judges against, to name a crossing's place
	void setPlaces(std::shared_ptr<const PlaceIndex> index)
	{
		std::lock_guard<std::mutex> lock(places_mtx);
		places = std::move(index);
	}

	void append(const EventRing::Entry &e, const std::string &place = std::string())
	{
		std::lock_guard<std::mutex> lock(mtx);
		Entry entry;
		entry.seq = ++last;
		entry.event = e;
		entry.event.seq = entry.seq;
		entry.place = place;
		entries.push_back(std::move(entry));
		while (entries.size() > capacity)
			entries.pop_front();
	}

	uint64_t epoch() const { return epoch_; }
	// the number just before the newest `n` kept, to start a reader there
	uint64_t before(std::size_t n) const
	{
		std::lock_guard<std::mutex> lock(mtx);
		if (entries.empty())
			return last;
		const uint64_t oldest = entries.front().seq;
		return last >= oldest + n ? last - n : oldest - 1;
	}
	uint64_t newest() const
	{
		std::lock_guard<std::mutex> lock(mtx);
		return last;
	}

	// The entries after `after`, at most `max`. False when `after` lies before
	// the oldest kept, so the reader knows it missed some.
	bool since(uint64_t after, std::size_t max, std::vector<Entry> &out) const
	{
		std::lock_guard<std::mutex> lock(mtx);
		out.clear();
		if (!entries.empty() && after + 1 < entries.front().seq)
			return false;
		for (const auto &e : entries)
		{
			if (e.seq <= after)
				continue;
			if (out.size() >= max)
				break;
			out.push_back(e);
		}
		return true;
	}

	bool save(std::ofstream &f) const;
	bool load(std::ifstream &f);

private:
	std::mutex places_mtx;
	std::shared_ptr<const PlaceIndex> places;
	mutable std::mutex mtx;
	std::deque<Entry> entries;
	std::size_t capacity;
	uint64_t last = 0;
	uint64_t epoch_ = (uint64_t)std::time(nullptr);
};
