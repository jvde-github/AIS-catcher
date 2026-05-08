// aiscat — Python binding for AIS-catcher's NMEA-to-JSON decoder.
// SPDX-License-Identifier: GPL-3.0-or-later

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <cstring>
#include <deque>
#include <string>

#include "Common.h"
#include "Stream.h"
#include "Marine/NMEA.h"
#include "Marine/Message.h"
#include "JSON/JSONAIS.h"
#include "JSON/JSON.h"
#include "JSON/Keys.h"
#include "JSON/StringBuilder.h"

enum class OutFormat {
    DICTIONARY = 0,  // Python dict, full decoded fields (default)
    ANNOTATED  = 1,  // Python dict, scalars wrapped {value, unit, description, text}
    JSON       = 2,  // bytes — JSON_FULL serialized (-o 5)
    JSON_NMEA  = 3,  // bytes — JSON envelope wrapping the NMEA (-o 3)
    NMEA       = 4,  // bytes — bare NMEA AIVDM/AIVDO lines (-n / -o 1)
    NMEA_TAG   = 5,  // bytes — NMEA + IEC 61162-450 tag block prefix
    BINARY     = 6,  // bytes — AIS-catcher 0xac binary packet
};

static bool parse_format(const char *s, OutFormat &out) {
    if (!s) { out = OutFormat::DICTIONARY; return true; }
    if (!std::strcmp(s, "dictionary")) { out = OutFormat::DICTIONARY; return true; }
    if (!std::strcmp(s, "annotated"))  { out = OutFormat::ANNOTATED;  return true; }
    if (!std::strcmp(s, "json"))       { out = OutFormat::JSON;       return true; }
    if (!std::strcmp(s, "json_nmea"))  { out = OutFormat::JSON_NMEA;  return true; }
    if (!std::strcmp(s, "nmea"))       { out = OutFormat::NMEA;       return true; }
    if (!std::strcmp(s, "nmea_tag"))   { out = OutFormat::NMEA_TAG;   return true; }
    if (!std::strcmp(s, "binary"))     { out = OutFormat::BINARY;     return true; }
    return false;
}

// Cache of interned PyUnicode keys, indexed by AIS::Keys enum.
static PyObject **g_keys = nullptr;

// Bitmask of meta/envelope fields to suppress (AIS-catcher infra, not AIS protocol).
// Every key here lives in the first 64 enum slots; checked by static_assert below.
// signalpower/ppm are kept when present — they're useful reception-quality info from
// the SDR or upstream JSON; JSONAIS already omits them when undefined.
static_assert(AIS::KEY_VERSION < 64,
    "kSkipMask assumes all suppressed keys fit in the first 64 AIS::Keys");
static constexpr uint64_t kSkipMask =
    (1ULL << AIS::KEY_CLASS)        |
    (1ULL << AIS::KEY_DEVICE)       |
    (1ULL << AIS::KEY_DRIVER)       |
    (1ULL << AIS::KEY_HARDWARE)     |
    (1ULL << AIS::KEY_SCALED)       |
    (1ULL << AIS::KEY_VERSION)      |
    (1ULL << AIS::KEY_RXTIME)       |
    (1ULL << AIS::KEY_NMEA);

// Interned keys for annotated-mode wrapper dicts.
static PyObject *g_key_value, *g_key_unit, *g_key_description, *g_key_text;

// Per-key cached PyUnicode for unit and description strings (annotated mode).
// Indexed by AIS::Keys; entries are nullptr when the corresponding KeyInfo field is empty.
static PyObject **g_unit_strs = nullptr;
static PyObject **g_desc_strs = nullptr;

static PyObject *convert_value(const JSON::Value &v, bool annotated);
static PyObject *convert_object(const JSON::JSON &obj, bool annotated);

// In annotated mode each scalar becomes {"value": x, "unit": ..., "description": ..., "text": ...}.
// Non-scalars (objects, arrays) recurse but are not wrapped themselves.
static PyObject *wrap_annotated(PyObject *val, int key_index) {
    if (key_index < 0 || key_index >= (int)AIS::KEY_COUNT) return val;
    const AIS::KeyInfo &info = AIS::KeyInfoMap[key_index];
    PyObject *d = _PyDict_NewPresized(4);
    if (!d) { Py_DECREF(val); return nullptr; }
    if (PyDict_SetItem(d, g_key_value, val) < 0) { Py_DECREF(val); Py_DECREF(d); return nullptr; }
    if (g_unit_strs[key_index]) {
        if (PyDict_SetItem(d, g_key_unit, g_unit_strs[key_index]) < 0) { Py_DECREF(val); Py_DECREF(d); return nullptr; }
    }
    if (g_desc_strs[key_index]) {
        if (PyDict_SetItem(d, g_key_description, g_desc_strs[key_index]) < 0) { Py_DECREF(val); Py_DECREF(d); return nullptr; }
    }
    if (info.lookup_table && (PyLong_Check(val) || PyFloat_Check(val))) {
        long nv = PyLong_Check(val) ? PyLong_AsLong(val) : (long)PyFloat_AsDouble(val);
        if (nv >= 0 && nv < (long)info.lookup_table->size()) {
            const std::string &s = (*info.lookup_table)[(size_t)nv];
            PyObject *t = PyUnicode_FromStringAndSize(s.data(), (Py_ssize_t)s.size());
            if (!t || PyDict_SetItem(d, g_key_text, t) < 0) { Py_XDECREF(t); Py_DECREF(val); Py_DECREF(d); return nullptr; }
            Py_DECREF(t);
        }
    }
    Py_DECREF(val);
    return d;
}

static PyObject *convert_object(const JSON::JSON &obj, bool annotated) {
    const auto &members = obj.getMembers();
    PyObject *d = _PyDict_NewPresized((Py_ssize_t)members.size());
    if (!d) return nullptr;
    for (const auto &m : members) {
        int k = m.Key();
        if ((unsigned)k < 64 && (kSkipMask & (1ULL << k))) continue;
        PyObject *key;
        if (k >= 0 && k < (int)AIS::KEY_COUNT && g_keys[k]) {
            key = g_keys[k];
            Py_INCREF(key);
        } else {
            key = PyUnicode_FromFormat("key_%d", k);
            if (!key) { Py_DECREF(d); return nullptr; }
        }
        PyObject *val = convert_value(m.Get(), annotated);
        if (!val) { Py_DECREF(key); Py_DECREF(d); return nullptr; }
        // Wrap scalars only — leave nested objects/arrays alone (they recurse internally).
        if (annotated) {
            using T = JSON::Value::Type;
            T t = m.Get().getType();
            if (t == T::BOOL || t == T::INT || t == T::FLOAT || t == T::STRING) {
                val = wrap_annotated(val, k);
                if (!val) { Py_DECREF(key); Py_DECREF(d); return nullptr; }
            }
        }
        if (PyDict_SetItem(d, key, val) < 0) {
            Py_DECREF(key); Py_DECREF(val); Py_DECREF(d); return nullptr;
        }
        Py_DECREF(key);
        Py_DECREF(val);
    }
    return d;
}

static PyObject *convert_value(const JSON::Value &v, bool annotated) {
    using T = JSON::Value::Type;
    switch (v.getType()) {
        case T::BOOL:
            return PyBool_FromLong(v.getBool());
        case T::INT:
            return PyLong_FromLong(v.getInt());
        case T::FLOAT:
            return PyFloat_FromDouble(v.getFloat());
        case T::STRING: {
            const std::string &s = v.getString();
            return PyUnicode_FromStringAndSize(s.data(), (Py_ssize_t)s.size());
        }
        case T::OBJECT:
            return convert_object(v.getObject(), annotated);
        case T::ARRAY: {
            const auto &a = v.getArray();
            PyObject *L = PyList_New((Py_ssize_t)a.size());
            if (!L) return nullptr;
            for (Py_ssize_t i = 0; i < (Py_ssize_t)a.size(); ++i) {
                PyObject *item = convert_value(a[(size_t)i], annotated);
                if (!item) { Py_DECREF(L); return nullptr; }
                PyList_SET_ITEM(L, i, item);
            }
            return L;
        }
        case T::ARRAY_STRING: {
            const auto &a = v.getStringArray();
            PyObject *L = PyList_New((Py_ssize_t)a.size());
            if (!L) return nullptr;
            for (Py_ssize_t i = 0; i < (Py_ssize_t)a.size(); ++i) {
                const std::string &s = a[(size_t)i];
                PyObject *u = PyUnicode_FromStringAndSize(s.data(), (Py_ssize_t)s.size());
                if (!u) { Py_DECREF(L); return nullptr; }
                PyList_SET_ITEM(L, i, u);
            }
            return L;
        }
        case T::EMPTY:
        default:
            Py_RETURN_NONE;
    }
}

class PySink : public StreamIn<JSON::JSON> {
public:
    std::deque<PyObject *> queue;
    OutFormat format = OutFormat::DICTIONARY;
    JSON::Serializer serializer;
    std::string scratch;  // reusable buffer for bytes-format serialization

    void Receive(const JSON::JSON *data, int len, TAG &tag) override {
        for (int i = 0; i < len; ++i) {
            PyObject *d = nullptr;
            switch (format) {
                case OutFormat::DICTIONARY:
                    d = convert_object(data[i], false);
                    break;
                case OutFormat::ANNOTATED:
                    d = convert_object(data[i], true);
                    break;
                case OutFormat::JSON: {
                    scratch.clear();
                    serializer.stringify(data[i], scratch);
                    d = PyBytes_FromStringAndSize(scratch.data(), (Py_ssize_t)scratch.size());
                    break;
                }
                case OutFormat::JSON_NMEA: {
                    auto *msg = static_cast<const AIS::Message *>(data[i].binary);
                    if (!msg) break;
                    scratch.clear();
                    msg->getNMEAJSON(scratch, tag);
                    d = PyBytes_FromStringAndSize(scratch.data(), (Py_ssize_t)scratch.size());
                    break;
                }
                case OutFormat::NMEA: {
                    auto *msg = static_cast<const AIS::Message *>(data[i].binary);
                    if (!msg) break;
                    scratch.clear();
                    auto sentences = msg->sentences();
                    for (size_t j = 0; j < sentences.size(); ++j) {
                        scratch.append(sentences[j]);
                        scratch.push_back('\n');
                    }
                    d = PyBytes_FromStringAndSize(scratch.data(), (Py_ssize_t)scratch.size());
                    break;
                }
                case OutFormat::NMEA_TAG: {
                    auto *msg = static_cast<const AIS::Message *>(data[i].binary);
                    if (!msg) break;
                    scratch.clear();
                    msg->getNMEATagBlock(scratch);
                    d = PyBytes_FromStringAndSize(scratch.data(), (Py_ssize_t)scratch.size());
                    break;
                }
                case OutFormat::BINARY: {
                    auto *msg = static_cast<const AIS::Message *>(data[i].binary);
                    if (!msg) break;
                    scratch.clear();
                    msg->getBinaryNMEA(scratch, tag);
                    d = PyBytes_FromStringAndSize(scratch.data(), (Py_ssize_t)scratch.size());
                    break;
                }
            }
            if (d) queue.push_back(d);
        }
    }

    ~PySink() override {
        for (PyObject *o : queue) Py_DECREF(o);
    }
};

typedef struct {
    PyObject_HEAD
    AIS::NMEA *nmea;
    AIS::JSONAIS *jsonais;
    PySink *sink;
    TAG *tag;
} DecoderObject;

static int Decoder_init(DecoderObject *self, PyObject *args, PyObject *kwds) {
    static const char *kwlist[] = {"format", "country", nullptr};
    const char *format_str = nullptr;
    int country = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|sp", (char **)kwlist, &format_str, &country))
        return -1;

    OutFormat fmt;
    if (!parse_format(format_str, fmt)) {
        PyErr_Format(PyExc_ValueError,
            "format must be one of 'dictionary', 'annotated', 'json', 'json_nmea', 'nmea', 'nmea_tag', 'binary'; got %s",
            format_str);
        return -1;
    }

    self->nmea = new AIS::NMEA();
    self->jsonais = new AIS::JSONAIS();
    self->sink = new PySink();
    self->sink->format = fmt;
    self->tag = new TAG();
    self->tag->clear();
    if (country) self->tag->mode |= 4;  // enables JSONAIS::COUNTRY (MMSI prefix → country, country_code)
    self->nmea->out.Connect(self->jsonais);
    self->jsonais->out.Connect(self->sink);
    return 0;
}

static void Decoder_dealloc(DecoderObject *self) {
    delete self->nmea;
    delete self->jsonais;
    delete self->sink;
    delete self->tag;
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Decoder_feed(DecoderObject *self, PyObject *arg) {
    const char *data = nullptr;
    Py_ssize_t size = 0;

    if (PyBytes_Check(arg)) {
        data = PyBytes_AS_STRING(arg);
        size = PyBytes_GET_SIZE(arg);
    } else if (PyByteArray_Check(arg)) {
        data = PyByteArray_AS_STRING(arg);
        size = PyByteArray_GET_SIZE(arg);
    } else if (PyUnicode_Check(arg)) {
        data = PyUnicode_AsUTF8AndSize(arg, &size);
        if (!data) return nullptr;
    } else {
        PyErr_SetString(PyExc_TypeError, "feed() expects bytes, bytearray, or str");
        return nullptr;
    }

    RAW r{Format::TXT, (void *)data, (int)size};
    self->nmea->Receive(&r, 1, *self->tag);
    return PyLong_FromSsize_t((Py_ssize_t)self->sink->queue.size());
}

static PyObject *Decoder_next(DecoderObject *self, PyObject *Py_UNUSED(ignored)) {
    if (self->sink->queue.empty()) Py_RETURN_NONE;
    PyObject *d = self->sink->queue.front();
    self->sink->queue.pop_front();
    return d;
}

static PyObject *Decoder_pending(DecoderObject *self, PyObject *Py_UNUSED(ignored)) {
    return PyLong_FromSsize_t((Py_ssize_t)self->sink->queue.size());
}

static PyMethodDef Decoder_methods[] = {
    {"feed", (PyCFunction)Decoder_feed, METH_O,
     "Feed bytes/bytearray/str to the decoder. Returns number of pending messages."},
    {"next", (PyCFunction)Decoder_next, METH_NOARGS,
     "Pop the next decoded message as a dict, or None if the queue is empty."},
    {"pending", (PyCFunction)Decoder_pending, METH_NOARGS,
     "Number of decoded messages waiting in the queue."},
    {nullptr, nullptr, 0, nullptr}
};

static PyTypeObject DecoderType = {
    PyVarObject_HEAD_INIT(nullptr, 0)
};

static struct PyModuleDef coremodule = {
    PyModuleDef_HEAD_INIT,
    "_core",
    "AIS-catcher NMEA decoder (C++ binding)",
    -1,
    nullptr
};

PyMODINIT_FUNC PyInit__core(void) {
    DecoderType.tp_name = "aiscat._core.Decoder";
    DecoderType.tp_basicsize = sizeof(DecoderObject);
    DecoderType.tp_dealloc = (destructor)Decoder_dealloc;
    DecoderType.tp_flags = Py_TPFLAGS_DEFAULT;
    DecoderType.tp_doc = "AIS NMEA decoder.";
    DecoderType.tp_methods = Decoder_methods;
    DecoderType.tp_init = (initproc)Decoder_init;
    DecoderType.tp_new = PyType_GenericNew;

    if (PyType_Ready(&DecoderType) < 0) return nullptr;

    PyObject *m = PyModule_Create(&coremodule);
    if (!m) return nullptr;

    g_keys = (PyObject **)PyMem_Calloc((size_t)AIS::KEY_COUNT, sizeof(PyObject *));
    if (!g_keys) { Py_DECREF(m); return nullptr; }
    for (int i = 0; i < (int)AIS::KEY_COUNT; ++i) {
        const AIS::KeyStr &name = AIS::KeyMap[i][JSON_DICT_FULL];
        g_keys[i] = PyUnicode_InternFromString(name.data());
    }

    g_key_value       = PyUnicode_InternFromString("value");
    g_key_unit        = PyUnicode_InternFromString("unit");
    g_key_description = PyUnicode_InternFromString("description");
    g_key_text        = PyUnicode_InternFromString("text");
    if (!g_key_value || !g_key_unit || !g_key_description || !g_key_text) {
        Py_DECREF(m); return nullptr;
    }

    g_unit_strs = (PyObject **)PyMem_Calloc((size_t)AIS::KEY_COUNT, sizeof(PyObject *));
    g_desc_strs = (PyObject **)PyMem_Calloc((size_t)AIS::KEY_COUNT, sizeof(PyObject *));
    if (!g_unit_strs || !g_desc_strs) { Py_DECREF(m); return nullptr; }
    for (int i = 0; i < (int)AIS::KEY_COUNT; ++i) {
        const AIS::KeyInfo &info = AIS::KeyInfoMap[i];
        if (info.unit && info.unit[0])
            g_unit_strs[i] = PyUnicode_InternFromString(info.unit);
        if (info.description && info.description[0])
            g_desc_strs[i] = PyUnicode_InternFromString(info.description);
    }

    Py_INCREF(&DecoderType);
    if (PyModule_AddObject(m, "Decoder", (PyObject *)&DecoderType) < 0) {
        Py_DECREF(&DecoderType);
        Py_DECREF(m);
        return nullptr;
    }
    return m;
}
