// Standalone regression test, from the repository root:
//   c++ -std=c++11 $(find Source -type d | sed 's/^/-I/') scripts/test-binary-badges.cpp \
//       Source/JSON/Keys.cpp Source/Utilities/Convert.cpp -o /tmp/test-binary-badges && /tmp/test-binary-badges
#include "BinaryStore.h"
#undef NDEBUG
#include <cassert>
#include <iostream>

// Message::setBit addresses bits within bytes LSB-first; AIS fields are MSB-first.
static void field(AIS::Message &msg, int offset, int length, uint32_t value) {
  for (int i = 0; i < length; ++i)
    msg.setBit((offset + i) ^ 7, (value >> (length - i - 1)) & 1);
}

static void receive(BinaryStore &store, int type, uint32_t sender,
                    uint32_t recipient, std::time_t now,
                    const std::string &text = "") {
  AIS::Message msg;
  msg.clear();
  field(msg, 0, 6, type);
  field(msg, 8, 30, sender);
  field(msg, 40, 30, recipient);
  msg.setLength(72);
  msg.setRxTimeUnix(now);
  JSON::JSON data;
  data.binary = &msg;
  if (recipient && type != 7 && type != 13)
    data.Add(AIS::KEY_DEST_MMSI, (int)recipient);
  if (!text.empty()) data.Add(AIS::KEY_TEXT, &text);
  int h = store.process(data, LAT_UNDEFINED, LON_UNDEFINED);
  assert(h >= 0);
  store.settle(h, [](uint32_t id) { return id != 999; });
}

static std::string messages(const BinaryStore &store, uint32_t mmsi,
                            std::time_t now) {
  std::string json;
  {
    JSON::Writer writer(json);
    store.writeJSON(writer, now, 0, 0, mmsi);
  }
  return json;
}

static void receiveAton(BinaryStore &store, uint32_t sender, std::time_t now,
                        bool located = false) {
  AIS::Message msg;
  msg.clear();
  field(msg, 0, 6, 6);
  field(msg, 8, 30, sender);
  msg.setLength(168);
  msg.setRxTimeUnix(now);
  JSON::JSON data;
  data.binary = &msg;
  data.Add(AIS::KEY_DAC, 235);
  data.Add(AIS::KEY_FID, 10);
  data.Add(AIS::KEY_ASM_LIGHT_STATUS, 1);
  if (located) {
    data.Add(AIS::KEY_LAT, 52.0);
    data.Add(AIS::KEY_LON, 4.0);
  }
  int h = store.process(data, LAT_UNDEFINED, LON_UNDEFINED);
  assert(h >= 0);
  store.settle(h, [](uint32_t) { return true; });
}

int main() {
  BinaryStore store;
  store.setup(128);

  // Receipt-only summaries carry a category flag so the frontend can suppress
  // the badge while keeping the existing hover/detail lookup on both vessels.
  receive(store, 7, 111, 222, 100);
  assert(store.badge(111, 100) == (1 | (1 << 9)));
  assert(store.badge(222, 100) == (1 | (1 << 9)));
  assert(messages(store, 111, 100).find("\"type\":7") != std::string::npos);
  assert(messages(store, 222, 100).find("\"type\":7") != std::string::npos);
  receive(store, 7, 333, 999, 100); // unknown recipient: sender owns both chains
  assert(store.badge(333, 100) == (1 | (1 << 9)));

  // Real text (including the literal word TEST) still produces one badge,
  // once when it appears on both chains. Newer receipts cannot change its
  // count, kind or age bucket on either vessel.
  receive(store, 6, 111, 222, 100, "TEST, PLS ACK");
  receive(store, 8, 444, 0, 100, "Report");
  assert(store.badge(111, 100) == 1);
  assert(store.badge(222, 100) == 1);
  assert(store.badge(444, 100) == 1);
  receive(store, 7, 222, 111, 1100);
  assert(store.badge(111, 1100) == (1 | (1 << 7)));
  assert(store.badge(222, 1100) == (1 | (1 << 7)));
  receive(store, 14, 111, 0, 200, "Navigation warning");
  assert(store.badge(111, 1100) == (2 | (BinaryStore::Item::SAFETY << 4)));
  receive(store, 7, 222, 111, 2100);
  assert(store.badge(111, 2100) ==
         (2 | (BinaryStore::Item::SAFETY << 4) | (2 << 7)));

  // Recently acknowledged traffic does not keep expired content badged.
  receive(store, 7, 222, 111, 3000);
  assert(store.badge(111, 3000) == (3 | (1 << 9)));
  assert(store.badge(222, 3000) == (3 | (1 << 9)));
  store.refresh(3000);
  assert(store.badge(111, 3000) == (3 | (1 << 9)));
  assert(messages(store, 111, 3000).find("\"type\":7") != std::string::npos);

  // This policy is specifically for type 7, not safety acknowledgments.
  receive(store, 13, 555, 666, 3000);
  assert(store.badge(555, 3000) == (1 | (BinaryStore::Item::SAFETY << 4)));
  assert(store.badge(666, 3000) == (1 | (BinaryStore::Item::SAFETY << 4)));

  BinaryStore atons;
  atons.setup(32);
  receiveAton(atons, 777, 100);
  assert(atons.badge(777, 100) == (1 | (BinaryStore::Item::ATON << 4)));
  assert(messages(atons, 777, 100).find("\"asm_light_status\":1") != std::string::npos);
  receive(atons, 8, 777, 0, 100, "Report");
  receiveAton(atons, 777, 1100);
  assert(atons.badge(777, 1100) == (1 | (1 << 7)));
  receiveAton(atons, 777, 3000);
  assert(atons.badge(777, 3000) == (1 | (BinaryStore::Item::ATON << 4)));

  // A located AtoN status retains its own map marker and its sender's message
  // details, while its compact ship summary allows frontend badge suppression.
  receiveAton(atons, 888, 3000, true);
  assert(atons.badge(888, 3000) == (1 | (BinaryStore::Item::ATON << 4)));
  assert(messages(atons, 888, 3000).find("\"asm_light_status\":1") != std::string::npos);
  int markers = 0;
  atons.forEachMarker(3000, [&](const BinaryStore::Marker &marker) {
    assert(marker.kind == BinaryStore::Item::ATON);
    ++markers;
    return true;
  });
  assert(markers == 1);
  std::cout << "Binary badge and message-retention checks passed\n";
  return 0;
}
