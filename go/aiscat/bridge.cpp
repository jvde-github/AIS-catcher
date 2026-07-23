// aiscat-go C ABI bridge for AIS-catcher's C++ decoder.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "bridge.h"

#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <new>
#include <string>

#include "Common.h"
#include "Stream.h"
#include "Marine/NMEA.h"
#include "Marine/Message.h"
#include "JSON/JSONAIS.h"
#include "JSON/JSON.h"
#include "JSON/Keys.h"
#include "JSON/Writer.h"

namespace {

enum class OutFormat {
    DICTIONARY = AISCAT_FORMAT_DICTIONARY,
    ANNOTATED = AISCAT_FORMAT_ANNOTATED,
    JSON = AISCAT_FORMAT_JSON,
    JSON_NMEA = AISCAT_FORMAT_JSON_NMEA,
    NMEA = AISCAT_FORMAT_NMEA,
    NMEA_TAG = AISCAT_FORMAT_NMEA_TAG,
    BINARY = AISCAT_FORMAT_BINARY,
};

static constexpr uint64_t kSkipMask =
    (1ULL << AIS::KEY_CLASS) |
    (1ULL << AIS::KEY_DEVICE) |
    (1ULL << AIS::KEY_DRIVER) |
    (1ULL << AIS::KEY_HARDWARE) |
    (1ULL << AIS::KEY_SCALED) |
    (1ULL << AIS::KEY_VERSION) |
    (1ULL << AIS::KEY_RXTIME) |
    (1ULL << AIS::KEY_NMEA);

bool valid_format(int format) {
    return format >= AISCAT_FORMAT_DICTIONARY && format <= AISCAT_FORMAT_BINARY;
}

void write_filtered_value(const JSON::Value &v, JSON::Writer &w, bool annotated);

void write_annotated_scalar(const JSON::Value &v, int key, JSON::Writer &w) {
    w.beginObject().key("value");
    write_filtered_value(v, w, true);

    if (key >= 0 && key < (int)AIS::KEY_COUNT) {
        const AIS::KeyInfo &info = AIS::KeyInfoMap[key];
        if (info.unit && info.unit[0]) {
            w.kv("unit", info.unit);
        }
        if (info.description && info.description[0]) {
            w.kv("description", info.description);
        }
        if (info.lookup_table && (v.isInt() || v.isFloat())) {
            long n = v.isInt() ? v.getInt() : (long)v.getFloat();
            if (n >= 0 && n < (long)info.lookup_table->size()) {
                w.kv("text", (*info.lookup_table)[(size_t)n]);
            }
        }
    }
    w.endObject();
}

void write_filtered_object(const JSON::JSON &obj, JSON::Writer &w, bool annotated) {
    w.beginObject();
    for (const JSON::Member &member : obj.getMembers()) {
        int key_index = member.Key();
        if ((unsigned)key_index < 64 && (kSkipMask & (1ULL << key_index))) {
            continue;
        }
        if (key_index < 0 || key_index >= (int)AIS::KEY_COUNT) {
            continue;
        }
        const AIS::KeyStr &key = AIS::KeyMap[key_index][JSON_DICT_FULL];
        if (key.empty()) {
            continue;
        }

        w.key(key);
        const JSON::Value &value = member.Get();
        using T = JSON::Value::Type;
        T type = value.getType();
        if (annotated && (type == T::BOOL || type == T::INT || type == T::FLOAT || type == T::STRING)) {
            write_annotated_scalar(value, key_index, w);
        } else {
            write_filtered_value(value, w, annotated);
        }
    }
    w.endObject();
}

void write_filtered_value(const JSON::Value &v, JSON::Writer &w, bool annotated) {
    switch (v.getType()) {
    case JSON::Value::Type::BOOL:
        w.val(v.getBool());
        break;
    case JSON::Value::Type::INT:
        w.val((long long)v.getInt());
        break;
    case JSON::Value::Type::FLOAT:
        w.val(v.getFloat());
        break;
    case JSON::Value::Type::STRING:
        w.val(v.getStringRef());
        break;
    case JSON::Value::Type::OBJECT:
        write_filtered_object(v.getObject(), w, annotated);
        break;
    case JSON::Value::Type::ARRAY_STRING: {
        w.beginArray();
        for (const std::string &s : v.getStringArray()) {
            w.val(s);
        }
        w.endArray();
        break;
    }
    case JSON::Value::Type::ARRAY: {
        w.beginArray();
        for (const JSON::Value &item : v.getArray()) {
            write_filtered_value(item, w, annotated);
        }
        w.endArray();
        break;
    }
    case JSON::Value::Type::EMPTY:
    default:
        w.val_null();
        break;
    }
}

std::string stringify_filtered(const JSON::JSON &obj, bool annotated) {
    std::string out;
    JSON::Writer writer(out);
    write_filtered_object(obj, writer, annotated);
    writer.finish();
    return out;
}

class GoSink : public StreamIn<JSON::JSON> {
public:
    std::deque<std::string> queue;
    OutFormat format = OutFormat::DICTIONARY;
    JSON::Serializer serializer;
    std::string scratch;

    void Receive(const JSON::JSON *data, int len, TAG &tag) override {
        for (int i = 0; i < len; ++i) {
            scratch.clear();
            switch (format) {
            case OutFormat::DICTIONARY:
                queue.push_back(stringify_filtered(data[i], false));
                break;
            case OutFormat::ANNOTATED:
                queue.push_back(stringify_filtered(data[i], true));
                break;
            case OutFormat::JSON:
                serializer.stringify(data[i], scratch);
                queue.push_back(scratch);
                break;
            case OutFormat::JSON_NMEA: {
                const AIS::Message *msg = static_cast<const AIS::Message *>(data[i].binary);
                if (!msg) break;
                msg->getNMEAJSON(scratch, tag);
                queue.push_back(scratch);
                break;
            }
            case OutFormat::NMEA: {
                const AIS::Message *msg = static_cast<const AIS::Message *>(data[i].binary);
                if (!msg) break;
                auto sentences = msg->sentences();
                for (size_t j = 0; j < sentences.size(); ++j) {
                    scratch.append(sentences[j]);
                    scratch.push_back('\n');
                }
                queue.push_back(scratch);
                break;
            }
            case OutFormat::NMEA_TAG: {
                const AIS::Message *msg = static_cast<const AIS::Message *>(data[i].binary);
                if (!msg) break;
                msg->getNMEATagBlock(scratch);
                queue.push_back(scratch);
                break;
            }
            case OutFormat::BINARY: {
                const AIS::Message *msg = static_cast<const AIS::Message *>(data[i].binary);
                if (!msg) break;
                msg->getBinaryNMEA(scratch, tag);
                queue.push_back(scratch);
                break;
            }
            }
        }
    }
};

} // namespace

struct AisCatDecoder {
    AIS::NMEA nmea;
    AIS::JSONAIS jsonais;
    GoSink sink;
    TAG tag;
    std::string last_error;

    AisCatDecoder(OutFormat fmt, bool country) {
        sink.format = fmt;
        tag.clear();
        if (country) {
            tag.mode |= 4;
        }
        nmea.out.Connect(&jsonais);
        jsonais.out.Connect(&sink);
    }

    int set_error(const char *msg) {
        last_error = msg ? msg : "unknown error";
        return -1;
    }

    int set_error(const std::exception &e) {
        last_error = e.what();
        return -1;
    }
};

extern "C" {

AisCatDecoder *aiscat_new(int format, int country) {
    if (!valid_format(format)) {
        return nullptr;
    }

    try {
        return new AisCatDecoder((OutFormat)format, country != 0);
    } catch (...) {
        return nullptr;
    }
}

void aiscat_free(AisCatDecoder *decoder) {
    delete decoder;
}

int aiscat_feed(AisCatDecoder *decoder, const char *data, size_t len) {
    if (!decoder) {
        return -1;
    }
    if (len > 0 && !data) {
        return decoder->set_error("feed data is null");
    }
    if (len > (size_t)INT32_MAX) {
        return decoder->set_error("feed chunk is too large");
    }

    try {
        RAW raw{Format::TXT, (void *)data, (int)len};
        decoder->nmea.Receive(&raw, 1, decoder->tag);
        decoder->last_error.clear();
        return (int)decoder->sink.queue.size();
    } catch (const std::exception &e) {
        return decoder->set_error(e);
    } catch (...) {
        return decoder->set_error("unknown C++ exception");
    }
}

int aiscat_pending(AisCatDecoder *decoder) {
    if (!decoder) {
        return -1;
    }
    return (int)decoder->sink.queue.size();
}

int aiscat_next(AisCatDecoder *decoder, unsigned char **out, size_t *len) {
    if (!decoder || !out || !len) {
        return -1;
    }
    *out = nullptr;
    *len = 0;
    if (decoder->sink.queue.empty()) {
        return 0;
    }

    try {
        std::string item = std::move(decoder->sink.queue.front());
        decoder->sink.queue.pop_front();

        unsigned char *buf = nullptr;
        if (!item.empty()) {
            buf = (unsigned char *)std::malloc(item.size());
            if (!buf) {
                return decoder->set_error("malloc failed");
            }
            std::memcpy(buf, item.data(), item.size());
        }

        *out = buf;
        *len = item.size();
        decoder->last_error.clear();
        return 1;
    } catch (const std::exception &e) {
        return decoder->set_error(e);
    } catch (...) {
        return decoder->set_error("unknown C++ exception");
    }
}

void aiscat_free_bytes(unsigned char *data) {
    std::free(data);
}

const char *aiscat_last_error(AisCatDecoder *decoder) {
    if (!decoder) {
        return "decoder is null";
    }
    return decoder->last_error.c_str();
}

} // extern "C"
