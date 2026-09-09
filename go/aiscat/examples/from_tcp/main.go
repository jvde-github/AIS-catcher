package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"time"

	"github.com/jvde-github/ais-catcher/go/aiscat"
)

func main() {
	address := flag.String("addr", "inst4:5012", "NMEA-over-TCP address")
	flag.Parse()

	ctx := context.Background()
	results, err := aiscat.FromTCP(ctx, *address, aiscat.Options{Format: aiscat.FormatDictionary})
	if err != nil {
		log.Fatal(err)
	}

	start := time.Now()
	total := 0
	for result := range results {
		if result.Err != nil {
			log.Fatal(result.Err)
		}
		total++
		if total%1000 == 0 {
			msg := result.Message.(aiscat.Message)
			rate := float64(total) / time.Since(start).Seconds()
			fmt.Printf("MMSI: %.0f | Total: %d | Rate: %.2f msg/s\n", msg["mmsi"], total, rate)
		}
	}
}
