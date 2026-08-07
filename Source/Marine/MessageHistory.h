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

#include <vector>
#include "Common.h"
#include "Logger.h"

namespace AIS
{
    // Circular buffer of key-timestamp pairs. Entries older than the caller's
    // interval are aged out lazily during lookup. The buffer doubles on demand
    // up to max_capacity, after which the oldest entry is overwritten.
    template <typename KeyType>
    struct MessageHistory
    {
        static const uint32_t NOT_FOUND = 0xFFFFFFFF;

        struct Entry
        {
            KeyType key = 0;
            uint32_t timestamp = 0;
        };

        std::vector<Entry> entries;
        size_t head = 0;
        size_t tail = 0;
        size_t count = 0;
        size_t capacity;
        size_t max_capacity;
        bool max_capacity_warning_shown = false;

        MessageHistory(size_t initial_cap = 128, size_t max_cap = 32768)
            : entries(initial_cap), capacity(initial_cap), max_capacity(max_cap) {}

        size_t forward(size_t i) const { return i + 1 == capacity ? 0 : i + 1; }
        size_t backward(size_t i) const { return (i == 0 ? capacity : i) - 1; }

        // Relinearises into a fresh buffer, so the ring always restarts at zero
        bool expandCapacity()
        {
            if (capacity >= max_capacity)
            {
                if (!max_capacity_warning_shown)
                {
                    Error() << "Message History buffer reached maximum capacity (" << max_capacity << "), oldest entry will be overwritten";
                    max_capacity_warning_shown = true;
                }
                return false;
            }

            size_t new_capacity = MIN(capacity * 2, max_capacity);
            std::vector<Entry> grown(new_capacity);

            for (size_t i = 0; i < count; i++)
                grown[i] = entries[(tail + i) % capacity];

            entries.swap(grown);
            capacity = new_capacity;
            tail = 0;
            head = count;

            Debug() << "Message History buffer expanded capacity to " << capacity;

            return true;
        }

        // Walks back from the newest entry, dropping the tail once an entry
        // predates max_age: everything beyond it is older still
        uint32_t findAge(KeyType key, uint32_t current_time, uint32_t max_age)
        {
            size_t idx = backward(head);

            for (size_t checked = 0; checked < count; checked++)
            {
                const Entry &e = entries[idx];

                if (current_time - e.timestamp > max_age)
                {
                    count = checked;
                    tail = forward(idx);
                    break;
                }

                if (e.key == key)
                    return current_time - e.timestamp;

                idx = backward(idx);
            }

            return NOT_FOUND;
        }

        // True when the key is unseen or last seen at least `threshold` ago
        bool check(KeyType key, uint32_t timestamp, uint32_t threshold)
        {
            if (findAge(key, timestamp, threshold) < threshold)
                return false;

            add(key, timestamp, threshold);
            return true;
        }

        void add(KeyType key, uint32_t timestamp, uint32_t max_age)
        {
            if (count == capacity && (timestamp - entries[tail].timestamp >= max_age || !expandCapacity()))
            {
                tail = forward(tail);
                count--;
            }

            entries[head].key = key;
            entries[head].timestamp = timestamp;
            head = forward(head);
            count++;
        }
    };

    // Type aliases for specific use cases
    using PositionHistory = MessageHistory<uint32_t>;  // For MMSI position tracking
    using DuplicateHistory = MessageHistory<uint64_t>; // For message duplicate detection
}
