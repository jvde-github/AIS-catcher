//go:build cgo

package aiscat

/*
#cgo CXXFLAGS: -std=c++11 -I../../Source -I../../Source/Application -I../../Source/Library -I../../Source/Marine -I../../Source/JSON -I../../Source/Utilities
#cgo !windows CXXFLAGS: -Wno-unused-parameter -Wno-sign-compare -Wno-psabi
#cgo windows LDFLAGS: -lws2_32
#include <stdlib.h>
#include "bridge.h"
*/
import "C"

import (
	"encoding/json"
	"errors"
	"fmt"
	"runtime"
	"sync"
	"unsafe"
)

// Message is a decoded AIS message in map-shaped formats.
type Message = map[string]any

// Decoder incrementally decodes NMEA, AIS-catcher JSON envelopes, and
// AIS-catcher binary packets using AIS-catcher's native decoder.
type Decoder struct {
	mu      sync.Mutex
	ptr     *C.AisCatDecoder
	format  Format
	closed  bool
	options Options
}

// NewDecoder creates a streaming decoder. Call Close when the decoder is no
// longer needed; a finalizer is also installed as a backstop.
func NewDecoder(options Options) (*Decoder, error) {
	options, err := normalizeOptions(options)
	if err != nil {
		return nil, err
	}

	country := C.int(0)
	if options.Country {
		country = 1
	}
	ptr := C.aiscat_new(C.int(options.Format), country)
	if ptr == nil {
		return nil, errors.New("failed to create native aiscat decoder")
	}

	decoder := &Decoder{
		ptr:     ptr,
		format:  options.Format,
		options: options,
	}
	runtime.SetFinalizer(decoder, (*Decoder).Close)
	return decoder, nil
}

// Close releases the native decoder.
func (d *Decoder) Close() {
	d.mu.Lock()
	defer d.mu.Unlock()
	d.closeLocked()
}

func (d *Decoder) closeLocked() {
	if d.closed {
		return
	}
	if d.ptr != nil {
		C.aiscat_free(d.ptr)
		d.ptr = nil
	}
	d.closed = true
	runtime.SetFinalizer(d, nil)
}

// Format returns the decoder output format.
func (d *Decoder) Format() Format {
	return d.format
}

// Feed appends a chunk to the decoder and returns the number of pending
// decoded messages.
func (d *Decoder) Feed(data []byte) (int, error) {
	d.mu.Lock()
	defer d.mu.Unlock()
	if err := d.checkOpenLocked(); err != nil {
		return 0, err
	}

	var ptr *C.char
	if len(data) > 0 {
		ptr = (*C.char)(unsafe.Pointer(&data[0]))
	}
	n := C.aiscat_feed(d.ptr, ptr, C.size_t(len(data)))
	if n < 0 {
		return 0, d.lastErrorLocked()
	}
	return int(n), nil
}

// FeedString appends a string chunk to the decoder.
func (d *Decoder) FeedString(data string) (int, error) {
	return d.Feed(unsafe.Slice(unsafe.StringData(data), len(data)))
}

// Pending returns the number of decoded messages waiting in the queue.
func (d *Decoder) Pending() (int, error) {
	d.mu.Lock()
	defer d.mu.Unlock()
	if err := d.checkOpenLocked(); err != nil {
		return 0, err
	}

	n := C.aiscat_pending(d.ptr)
	if n < 0 {
		return 0, d.lastErrorLocked()
	}
	return int(n), nil
}

// NextBytes pops the next decoded message as bytes. Map-shaped formats are
// returned as JSON bytes.
func (d *Decoder) NextBytes() ([]byte, bool, error) {
	d.mu.Lock()
	defer d.mu.Unlock()
	if err := d.checkOpenLocked(); err != nil {
		return nil, false, err
	}

	var out *C.uchar
	var outLen C.size_t
	rc := C.aiscat_next(d.ptr, &out, &outLen)
	if rc < 0 {
		return nil, false, d.lastErrorLocked()
	}
	if rc == 0 {
		return nil, false, nil
	}
	defer C.aiscat_free_bytes(out)
	if outLen == 0 {
		return []byte{}, true, nil
	}
	data := C.GoBytes(unsafe.Pointer(out), C.int(outLen))
	return data, true, nil
}

// NextMap pops the next decoded message and decodes it into a Go map. It is
// valid for dictionary, annotated, json, and json_nmea formats.
func (d *Decoder) NextMap() (Message, bool, error) {
	data, ok, err := d.NextBytes()
	if !ok || err != nil {
		return nil, ok, err
	}

	switch d.format {
	case FormatDictionary, FormatAnnotated, FormatJSON, FormatJSONNMEA:
	default:
		return nil, false, fmt.Errorf("format %s cannot be decoded as a map", d.format)
	}

	var msg Message
	if err := json.Unmarshal(data, &msg); err != nil {
		return nil, false, err
	}
	return msg, true, nil
}

// Next pops the next decoded message. It returns Message for dictionary and
// annotated formats, and []byte for byte-shaped formats.
func (d *Decoder) Next() (any, bool, error) {
	if d.format.mapShaped() {
		return d.NextMap()
	}
	return d.NextBytes()
}

func (d *Decoder) checkOpenLocked() error {
	if d == nil || d.closed || d.ptr == nil {
		return errors.New("aiscat decoder is closed")
	}
	return nil
}

func (d *Decoder) lastErrorLocked() error {
	msg := C.aiscat_last_error(d.ptr)
	if msg == nil {
		return errors.New("native aiscat error")
	}
	return errors.New(C.GoString(msg))
}
