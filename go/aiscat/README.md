# aiscat-go

Go bindings for AIS-catcher's NMEA-to-JSON AIS decoder. The package mirrors the
Python `aiscat` binding: a thin native bridge owns the AIS-catcher decoder, while
Go provides ergonomic one-shot, reader, TCP, and UDP helpers.

## Build Requirements

- Go with cgo enabled.
- A C++11 compiler available to cgo.
  - Linux/macOS: `g++` or `clang++`.
  - Windows: MSYS2 MinGW-w64 or another cgo-compatible C/C++ toolchain.
  - This checkout was verified on Windows with Zig:
    `CGO_ENABLED=1 CC="zig cc" CXX="zig c++" go test ./...`.

This package links AIS-catcher C++ code and is GPL-3.0-or-later.

## Quickstart

```go
package main

import (
	"fmt"
	"log"

	"github.com/jvde-github/ais-catcher/go/aiscat"
)

func main() {
	msg, err := aiscat.DecodeMap(
		aiscat.Options{Format: aiscat.FormatDictionary},
		[]byte("!AIVDM,1,1,,A,15MgK45P3@G?fl0E`JbR0OwT0@MS,0*4E"),
	)
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println(msg["type"], msg["mmsi"], msg["lat"], msg["lon"])
}
```

## Streaming

```go
dec, err := aiscat.NewDecoder(aiscat.Options{Format: aiscat.FormatDictionary})
if err != nil {
	log.Fatal(err)
}
defer dec.Close()

dec.Feed([]byte("!AIVDM,...\r\n"))
for {
	msg, ok, err := dec.NextMap()
	if err != nil {
		log.Fatal(err)
	}
	if !ok {
		break
	}
	fmt.Println(msg["mmsi"])
}
```

## Formats

The same format names as Python `aiscat` are available:

- `FormatDictionary`
- `FormatAnnotated`
- `FormatJSON`
- `FormatJSONNMEA`
- `FormatNMEA`
- `FormatNMEATag`
- `FormatBinary`

`Decoder.Next()` returns `map[string]any` for dictionary/annotated formats and
`[]byte` for byte-shaped formats. Use `NextMap()` or `NextBytes()` when you want
an explicit shape.
