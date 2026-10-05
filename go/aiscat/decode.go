package aiscat

import (
	"bytes"
	"fmt"
)

// Decode decodes one logical AIS message from one or more NMEA fragments. It
// returns Message for dictionary/annotated formats and []byte for byte-shaped
// formats.
func Decode(options Options, parts ...[]byte) (any, error) {
	if len(parts) == 0 {
		return nil, fmt.Errorf("decode requires at least one NMEA fragment")
	}
	decoder, err := NewDecoder(options)
	if err != nil {
		return nil, err
	}
	defer decoder.Close()

	for _, part := range parts {
		if _, err := decoder.Feed(part); err != nil {
			return nil, err
		}
		if _, err := decoder.Feed([]byte{'\n'}); err != nil {
			return nil, err
		}
	}

	msg, ok, err := decoder.Next()
	if err != nil {
		return nil, err
	}
	if !ok {
		return nil, fmt.Errorf("incomplete or invalid NMEA fragment(s)")
	}
	if extra, ok, err := decoder.Next(); err != nil {
		return nil, err
	} else if ok {
		_ = extra
		return nil, fmt.Errorf("decode expects one message; use a streaming decoder for multiple messages")
	}
	return msg, nil
}

// DecodeString decodes one logical AIS message from string fragments.
func DecodeString(options Options, parts ...string) (any, error) {
	byteParts := make([][]byte, len(parts))
	for i, part := range parts {
		byteParts[i] = []byte(part)
	}
	return Decode(options, byteParts...)
}

// DecodeMap decodes one logical AIS message into a Go map. It is valid for
// dictionary, annotated, json, and json_nmea formats.
func DecodeMap(options Options, parts ...[]byte) (Message, error) {
	decoder, err := NewDecoder(options)
	if err != nil {
		return nil, err
	}
	defer decoder.Close()

	for _, part := range parts {
		if _, err := decoder.Feed(part); err != nil {
			return nil, err
		}
		if !bytes.HasSuffix(part, []byte{'\n'}) && !bytes.HasSuffix(part, []byte{'\r'}) {
			if _, err := decoder.Feed([]byte{'\n'}); err != nil {
				return nil, err
			}
		}
	}

	msg, ok, err := decoder.NextMap()
	if err != nil {
		return nil, err
	}
	if !ok {
		return nil, fmt.Errorf("incomplete or invalid NMEA fragment(s)")
	}
	if extra, ok, err := decoder.NextBytes(); err != nil {
		return nil, err
	} else if ok {
		_ = extra
		return nil, fmt.Errorf("decode expects one message; use a streaming decoder for multiple messages")
	}
	return msg, nil
}

// DecodeBytes decodes one logical AIS message into bytes. It works for every
// format; dictionary and annotated are returned as JSON bytes.
func DecodeBytes(options Options, parts ...[]byte) ([]byte, error) {
	decoder, err := NewDecoder(options)
	if err != nil {
		return nil, err
	}
	defer decoder.Close()

	for _, part := range parts {
		if _, err := decoder.Feed(part); err != nil {
			return nil, err
		}
		if _, err := decoder.Feed([]byte{'\n'}); err != nil {
			return nil, err
		}
	}

	msg, ok, err := decoder.NextBytes()
	if err != nil {
		return nil, err
	}
	if !ok {
		return nil, fmt.Errorf("incomplete or invalid NMEA fragment(s)")
	}
	if extra, ok, err := decoder.NextBytes(); err != nil {
		return nil, err
	} else if ok {
		_ = extra
		return nil, fmt.Errorf("decode expects one message; use a streaming decoder for multiple messages")
	}
	return msg, nil
}
