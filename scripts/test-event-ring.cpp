// Run from the repository root:
// c++ -std=c++11 -ISource -ISource/Application -ISource/Tracking -ISource/Library -ISource/JSON -ISource/Utilities scripts/test-event-ring.cpp -o /tmp/test-event-ring && /tmp/test-event-ring
#include "EventRing.h"
#include <cassert>
#include <iostream>

int main()
{
    EventRing::Event e;
    e.from = 244123456;
    e.from_name = "EXAMPLE";
    e.kind = EventRing::DESTINATION;
    e.label = "destination";
    e.was = "ROTTERDAM";
    e.text = "ANTWERP";
    assert(EventRing::formatText(e) == "**EXAMPLE** · ::destination ROTTERDAM:: → [[ANTWERP]]");
    e.from_name = "A**B";
    e.text = "[[PORT]] :: \\";
    assert(EventRing::formatText(e) == "**A\\*\\*B** · ::destination ROTTERDAM:: → [[\\[\\[PORT\\]\\] \\:\\: \\\\]]");
    e.from_name.clear();
    assert(EventRing::formatText(e).find("**MMSI 244123456**") == 0);
    assert(EventRing::displayName(972123456, "") == "MOB device");
    assert(EventRing::displayName(974123456, "") == "EPIRB");
    assert(EventRing::displayName(970123456, "") == "AIS-SART");
    assert(EventRing::displayName(2320001, "PORT") == "VTS PORT");

    e.kind = EventRing::SAFETY;
    e.level = EventRing::URGENT;
    e.label.clear();
    e.was.clear();
    e.from_name = "EXAMPLE";
    e.to = 244654321;
    e.to_name = "RESPONDER";
    e.text = "MAYDAY";
    e.time = 1000;
    EventRing ring;
    ring.push(e);
    e.time = 1001;
    e.from_name = "RENAMED"; // Presentation does not change repetition identity.
    ring.push(e);
    assert(ring.sequence() == 1);
    std::string result;
    {
        JSON::Writer w(result);
        w.beginObject();
        ring.writeSince(w, 0, 0, 1002);
        w.endObject();
    }
    assert(result.find("\"format\":\"ticker-v1\"") != std::string::npos);
    assert(result.find("\"mmsi\":244123456") != std::string::npos);
    assert(result.find("\"first\":1000") != std::string::npos);
    assert(result.find("\"count\":2") != std::string::npos);
    assert(result.find("⚠ **EXAMPLE** → **RESPONDER** · [[MAYDAY]] (×2)") != std::string::npos);
    assert(result.find("\"lat\"") == std::string::npos);
    std::cout << result << '\n';
}
