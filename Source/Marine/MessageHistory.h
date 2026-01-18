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
#include <algorithm>
#include <iostream>
#include "Common.h"

namespace AIS
{
    // Circular buffer for tracking key-timestamp pairs with automatic aging
    template <typename KeyType>
    struct MessageHistory
    {
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
            : capacity(initial_cap), max_capacity(max_cap)
        {
            entries.resize(capacity);
        }

        // Expand buffer capacity by factor of 2, up to max_capacity
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

            entries.resize(new_capacity);

            // If buffer wraps around, move wrapped portion to the end and clear old slots
            if (head <= tail && count > 0)
            {
                // Move elements from [0, head) to [capacity, capacity + head)
                std::copy(entries.begin(), entries.begin() + head, entries.begin() + capacity);
                // Clear the old slots to avoid dirty data
                std::fill(entries.begin(), entries.begin() + head, Entry{});
                head = capacity + head;
            }

            capacity = new_capacity;

            Debug() << "Message History buffer expanded capacity to " << capacity;

            return true;
        }

        uint32_t findAge(KeyType key, uint32_t current_time, uint32_t max_age)
        {
            if (count == 0)
                return 0xFFFFFFFF; // Empty

            // Walk backward from most recent
            size_t idx = (head - 1 + capacity) % capacity;
            size_t items_checked = 0;

            while (items_checked < count)
            {
                const Entry &e = entries[idx];

                // Check if entry is too old
                if (current_time - e.timestamp > max_age)
                {
                    // Age out this and all older entries
                    count = items_checked;
                    tail = (idx + 1) % capacity;
                    break;
                }

                if (e.key == key)
                {
                    return current_time - e.timestamp;
                }

                idx = (idx - 1 + capacity) % capacity;
                items_checked++;
            }

            return 0xFFFFFFFF;
        }

        // Returns true if should be included (age >= threshold or not found)
        bool check(KeyType key, uint32_t timestamp, uint32_t threshold)
        {
            uint32_t age = findAge(key, timestamp, threshold);

            if (age >= threshold)
            {
                add(key, timestamp, threshold);
                return true;
            }

            return false;
        }

        void add(KeyType key, uint32_t timestamp, uint32_t max_age)
        {
            if (count == capacity)
            {
                const Entry &e = entries[tail];
                if (timestamp - e.timestamp < max_age)
                {
                    if (!expandCapacity())
                    {
                        tail = (tail + 1) % capacity;
                        count--;
                    }
                }
                else
                {
                    tail = (tail + 1) % capacity;
                    count--;

#ifdef DEBUG_HISTORY
                    if (!validate())
                    {
                        std::cerr << "ERROR: Buffer validation failed after add()" << std::endl;
                    }
#endif
                }
            }

            entries[head].key = key;
            entries[head].timestamp = timestamp;
            head = (head + 1) % capacity;
            count++;
        }
    };

    // Type aliases for specific use cases
    using PositionHistory = MessageHistory<uint32_t>;  // For MMSI position tracking
    using DuplicateHistory = MessageHistory<uint64_t>; // For message duplicate detection
}
