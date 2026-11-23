//go:build cgo

package aiscat

import (
	"bytes"
	"encoding/json"
	"testing"
)

const type1 = "!AIVDM,1,1,,A,15MgK45P3@G?fl0E`JbR0OwT0@MS,0*4E"

func TestDecodeMapType1(t *testing.T) {
	msg, err := DecodeMap(Options{Format: FormatDictionary}, []byte(type1))
	if err != nil {
		t.Fatal(err)
	}
	if got := int(msg["type"].(float64)); got != 1 {
		t.Fatalf("type = %d, want 1", got)
	}
	if got := int(msg["mmsi"].(float64)); got != 366730000 {
		t.Fatalf("mmsi = %d, want 366730000", got)
	}
	if got := msg["lat"].(float64); got < 37.80 || got > 37.81 {
		t.Fatalf("lat = %f, want around 37.803802", got)
	}
	if got := msg["lon"].(float64); got > -122.39 || got < -122.40 {
		t.Fatalf("lon = %f, want around -122.392532", got)
	}
	for _, key := range []string{"class", "device", "version", "scaled", "rxtime", "nmea"} {
		if _, ok := msg[key]; ok {
			t.Fatalf("dictionary output should strip %q", key)
		}
	}
}

func TestDecoderTwoSentences(t *testing.T) {
	dec, err := NewDecoder(Options{Format: FormatDictionary})
	if err != nil {
		t.Fatal(err)
	}
	defer dec.Close()

	if _, err := dec.Feed([]byte(type1 + "\r\n!AIVDM,1,1,,B,177KQJ5000G?tO`K>RA1wUbN0TKH,0*5C\r\n")); err != nil {
		t.Fatal(err)
	}

	var count int
	for {
		_, ok, err := dec.NextMap()
		if err != nil {
			t.Fatal(err)
		}
		if !ok {
			break
		}
		count++
	}
	if count != 2 {
		t.Fatalf("decoded %d messages, want 2", count)
	}
}

func TestAnnotated(t *testing.T) {
	msg, err := DecodeMap(Options{Format: FormatAnnotated}, []byte(type1))
	if err != nil {
		t.Fatal(err)
	}
	lat, ok := msg["lat"].(map[string]any)
	if !ok {
		t.Fatalf("lat = %T, want annotated map", msg["lat"])
	}
	if lat["unit"] != "degrees" {
		t.Fatalf("lat unit = %v, want degrees", lat["unit"])
	}
	if _, ok := lat["value"].(float64); !ok {
		t.Fatalf("lat value = %T, want float64", lat["value"])
	}
}

func TestByteFormats(t *testing.T) {
	jsonBytes, err := DecodeBytes(Options{Format: FormatJSON}, []byte(type1))
	if err != nil {
		t.Fatal(err)
	}
	var obj map[string]any
	if err := json.Unmarshal(jsonBytes, &obj); err != nil {
		t.Fatalf("json output did not unmarshal: %v\n%s", err, jsonBytes)
	}
	if obj["class"] != "AIS" {
		t.Fatalf("json class = %v, want AIS", obj["class"])
	}

	nmeaBytes, err := DecodeBytes(Options{Format: FormatNMEA}, []byte(type1))
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Contains(nmeaBytes, []byte(type1)) {
		t.Fatalf("nmea output %q does not contain input", string(nmeaBytes))
	}
}
