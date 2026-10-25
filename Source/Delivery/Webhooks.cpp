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

#include "Webhooks.h"
#include <algorithm>
#include <chrono>
#include "AIS-catcher.h"
#include "Convert.h"
#include "JournalJSON.h"
#include "HTTPClient.h"
#include "Logger.h"
#include "SHA256.h"

namespace
{
	// how long a failing webhook waits before the next try
	std::time_t backoff(int failures)
	{
		static const int steps[] = {5, 10, 30, 60, 120, 300, 600, 1800};
		const int n = (int)(sizeof(steps) / sizeof(steps[0]));
		return failures <= 0 ? 0 : failures > n ? 3600 : steps[failures - 1];
	}

	const std::size_t BATCH_MAX = 100, READ_MAX = 500;

	// a Standard Webhooks secret is whsec_ and the key in base64; any other is the key itself
	std::string keyOf(const std::string &secret)
	{
		std::string key;
		if (secret.compare(0, 6, "whsec_") == 0 && Util::Convert::fromBase64(secret.substr(6), key))
			return key;
		return secret;
	}
}

void Webhooks::configure(const std::vector<WebhookSpec> &specs)
{
	std::unique_lock<std::mutex> lock(mtx);
	std::vector<const Hook *> kept(specs.size(), nullptr);
	std::vector<bool> taken(hooks.size(), false);
	for (int pass = 0; pass < 2; pass++)
		for (std::size_t i = 0; i < specs.size(); i++)
			for (std::size_t j = 0; j < hooks.size() && !kept[i]; j++)
				if (!taken[j] && (pass ? hooks[j].spec.url == specs[i].url : hooks[j].spec == specs[i]))
				{
					taken[j] = true;
					kept[i] = &hooks[j];
				}
	std::vector<Hook> next;
	for (std::size_t i = 0; i < specs.size(); i++)
	{
		Hook h;
		if (kept[i])
			h = *kept[i];
		else
			h.cursor = tracker.journal().newest();
		h.spec = specs[i];
		h.key = keyOf(specs[i].secret);
		next.push_back(h);
	}
	hooks.swap(next);
	if (!hooks.empty() && !running)
	{
		running = true;
		worker = std::thread(&Webhooks::loop, this);
	}
	else if (hooks.empty() && running)
	{
		running = false;
		lock.unlock();
		wake();
		worker.join();
	}
}

void Webhooks::stop()
{
	{
		std::lock_guard<std::mutex> lock(mtx);
		running = false;
	}
	wake();
	if (worker.joinable())
		worker.join();
}

void Webhooks::wake()
{
	{
		std::lock_guard<std::mutex> lock(wake_mtx);
		woken = true;
	}
	woken_cv.notify_all();
}

void Webhooks::sleep(std::time_t due)
{
	std::unique_lock<std::mutex> lock(wake_mtx);
	if (due)
		woken_cv.wait_for(lock, std::chrono::seconds(MAX((std::time_t)0, due - std::time(nullptr))), [this] { return woken; });
	else
		woken_cv.wait(lock, [this] { return woken; });
	woken = false;
}

bool Webhooks::wanted(const Hook &hook, const Journal::Entry &e)
{
	const std::string kind = Tracking::journalName(e.event.kind);
	if (!hook.spec.events.empty() && std::find(hook.spec.events.begin(), hook.spec.events.end(), kind) == hook.spec.events.end())
		return false;
	// a ship's event goes when the ship passes the webhook's filter; one no longer kept does not
	if (e.event.kind == Tracking::Kind::SYSTEM || !hook.spec.view.any())
		return true;
	bool passes = false;
	const std::time_t now = std::time(nullptr);
	tracker.ships().withShip(e.event.from, [&](const Ship &s, int) { passes = hook.spec.view.passes(s, now); });
	return passes;
}

// what the address is sent: one event, or a batch of them, with the headers
// Standard Webhooks gives a receiver to check that it came from here, once
bool Webhooks::deliver(Hook &hook, const std::vector<Journal::Entry> &events)
{
	std::string body;
	JSON::Writer w(body);
	const uint64_t epoch = tracker.journal().epoch();
	if (hook.spec.batch)
	{
		w.beginObject().key("events").beginArray();
		for (const auto &e : events)
			Services::writeEvent(w, e, epoch);
		w.endArray().endObject();
	}
	else
		Services::writeEvent(w, events.front(), epoch);
	w.finish();

	const std::string id = Services::eventId(epoch, events.front().seq) +
						   (events.size() > 1 ? "_" + std::to_string((unsigned long long)events.back().seq) : std::string());
	const std::string timestamp = std::to_string((long long)std::time(nullptr));

	IO::HTTPClient http;
	try
	{
		http.setURL(hook.spec.url);
	}
	catch (const std::exception &e)
	{
		Error() << "Webhook " << hook.spec.url << ": " << e.what();
		return false;
	}
	http.setTimeout(10);
	http.extra_headers = "User-Agent: AIS-catcher/" + std::string(VERSION) + "\r\nwebhook-id: " + id + "\r\nwebhook-timestamp: " + timestamp + "\r\n";
	if (!hook.key.empty())
		http.extra_headers += "webhook-signature: v1," + Util::Convert::toBase64(Util::SHA256::hmac(hook.key, id + "." + timestamp + "." + body)) + "\r\n";
	const int status = http.Post(body);
	if (status >= 200 && status < 300)
		return true;
	Warning() << "Webhook " << hook.spec.url << ": " << (status > 0 ? "answered " + std::to_string(status) : IO::httpStatusString(status));
	return false;
}

void Webhooks::loop()
{
	std::unique_lock<std::mutex> lock(mtx);
	bool more = false; // a delivery went through, or a read came back full: more may be waiting, so no pause
	while (running)
	{
		if (!more)
		{
			// the soonest retry; without one, only the journal or a new configuration wakes it
			std::time_t due = 0;
			for (const auto &h : hooks)
				if (h.retry_at && (!due || h.retry_at < due))
					due = h.retry_at;
			lock.unlock();
			sleep(due);
			lock.lock();
		}
		more = false;
		if (!running)
			break;
		for (std::size_t i = 0; i < hooks.size() && running; i++)
		{
			const std::time_t now = std::time(nullptr);
			if (hooks[i].retry_at > now)
				continue;
			std::vector<Journal::Entry> entries;
			if (!tracker.journal().since(hooks[i].cursor, READ_MAX, entries))
			{
				Warning() << "Webhook " << hooks[i].spec.url << ": events were dropped before they could be sent";
				hooks[i].cursor = tracker.journal().newest();
				continue;
			}
			// what this webhook wants, in order; the cursor moves past the rest
			std::vector<Journal::Entry> due;
			uint64_t passed = hooks[i].cursor;
			for (const auto &e : entries)
			{
				if (!wanted(hooks[i], e))
				{
					if (due.empty())
						passed = e.seq;
					continue;
				}
				due.push_back(e);
				if (!hooks[i].spec.batch || due.size() >= BATCH_MAX)
					break;
			}
			hooks[i].cursor = MAX(hooks[i].cursor, passed);
			if (due.empty())
			{
				if (!entries.empty())
					hooks[i].cursor = MAX(hooks[i].cursor, entries.back().seq);
				// a full read of events it does not want: the rest are read at once
				more |= entries.size() >= READ_MAX;
				continue;
			}
			// sent without the lock, so the status pages and a new configuration are not held up
			Hook copy = hooks[i];
			lock.unlock();
			const bool ok = deliver(copy, due);
			lock.lock();
			if (i >= hooks.size() || !(hooks[i].spec == copy.spec))
				continue; // the configuration changed meanwhile
			if (ok)
			{
				hooks[i].cursor = due.back().seq;
				if (hooks[i].failures)
					Info() << "Webhook " << hooks[i].spec.url << ": delivering again";
				hooks[i].failures = 0;
				hooks[i].retry_at = 0;
				more = true;
			}
			else
			{
				hooks[i].failures++;
				hooks[i].retry_at = std::time(nullptr) + backoff(hooks[i].failures);
			}
		}
	}
}
