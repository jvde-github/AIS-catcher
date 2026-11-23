//go:build !cgo

package aiscat

import "errors"

var errCgoDisabled = errors.New("aiscat requires cgo and a C++11 compiler")

// Message is a decoded AIS message in map-shaped formats.
type Message = map[string]any

// Decoder is unavailable when cgo is disabled.
type Decoder struct {
	format Format
}

// NewDecoder returns an error when the package is built without cgo.
func NewDecoder(options Options) (*Decoder, error) {
	options, err := normalizeOptions(options)
	if err != nil {
		return nil, err
	}
	return &Decoder{format: options.Format}, errCgoDisabled
}

func (d *Decoder) Close() {}

func (d *Decoder) Format() Format {
	if d == nil {
		return FormatDictionary
	}
	return d.format
}

func (d *Decoder) Feed([]byte) (int, error) {
	return 0, errCgoDisabled
}

func (d *Decoder) FeedString(string) (int, error) {
	return 0, errCgoDisabled
}

func (d *Decoder) Pending() (int, error) {
	return 0, errCgoDisabled
}

func (d *Decoder) NextBytes() ([]byte, bool, error) {
	return nil, false, errCgoDisabled
}

func (d *Decoder) NextMap() (Message, bool, error) {
	return nil, false, errCgoDisabled
}

func (d *Decoder) Next() (any, bool, error) {
	return nil, false, errCgoDisabled
}
