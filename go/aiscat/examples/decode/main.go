package main

import (
	"fmt"
	"log"

	"github.com/jvde-github/ais-catcher/go/aiscat"
)

func main() {
	msg, err := aiscat.DecodeMap(
		aiscat.Options{Format: aiscat.FormatDictionary, Country: true},
		[]byte("!AIVDM,1,1,,A,15MgK45P3@G?fl0E`JbR0OwT0@MS,0*4E"),
	)
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println(msg["type"], msg["mmsi"], msg["lat"], msg["lon"])
}
