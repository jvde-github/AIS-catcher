package aiscat

import "fmt"

// Format selects the shape returned by Decoder.Next and Decode.
type Format int

const (
	FormatDictionary Format = iota
	FormatAnnotated
	FormatJSON
	FormatJSONNMEA
	FormatNMEA
	FormatNMEATag
	FormatBinary
)

func (f Format) String() string {
	switch f {
	case FormatDictionary:
		return "dictionary"
	case FormatAnnotated:
		return "annotated"
	case FormatJSON:
		return "json"
	case FormatJSONNMEA:
		return "json_nmea"
	case FormatNMEA:
		return "nmea"
	case FormatNMEATag:
		return "nmea_tag"
	case FormatBinary:
		return "binary"
	default:
		return fmt.Sprintf("Format(%d)", int(f))
	}
}

// ParseFormat accepts the same format names as the Python aiscat package.
func ParseFormat(s string) (Format, error) {
	switch s {
	case "", "dictionary":
		return FormatDictionary, nil
	case "annotated":
		return FormatAnnotated, nil
	case "json":
		return FormatJSON, nil
	case "json_nmea":
		return FormatJSONNMEA, nil
	case "nmea":
		return FormatNMEA, nil
	case "nmea_tag":
		return FormatNMEATag, nil
	case "binary":
		return FormatBinary, nil
	default:
		return FormatDictionary, fmt.Errorf("unknown aiscat format %q", s)
	}
}

func (f Format) mapShaped() bool {
	return f == FormatDictionary || f == FormatAnnotated
}

func (f Format) valid() bool {
	return f >= FormatDictionary && f <= FormatBinary
}

// Options configures a Decoder.
type Options struct {
	Format  Format
	Country bool
}

func normalizeOptions(options Options) (Options, error) {
	if !options.Format.valid() {
		return Options{}, fmt.Errorf("unknown aiscat format %d", int(options.Format))
	}
	return options, nil
}
