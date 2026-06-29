package aiscat

import (
	"context"
	"errors"
	"io"
	"net"
)

// Result is one item emitted by stream helpers.
type Result struct {
	Message any
	Err     error
}

// FromReader decodes AIS messages from r until EOF, context cancellation, or a
// decode/read error. The channel is closed when the stream ends.
func FromReader(ctx context.Context, r io.Reader, options Options) (<-chan Result, error) {
	decoder, err := NewDecoder(options)
	if err != nil {
		return nil, err
	}

	out := make(chan Result)
	go func() {
		defer close(out)
		defer decoder.Close()

		buf := make([]byte, 64*1024)
		for {
			if err := ctx.Err(); err != nil {
				sendResult(ctx, out, Result{Err: err})
				return
			}

			n, readErr := r.Read(buf)
			if n > 0 {
				if _, err := decoder.Feed(buf[:n]); err != nil {
					sendResult(ctx, out, Result{Err: err})
					return
				}
				if !drainDecoder(ctx, decoder, out) {
					return
				}
			}

			if readErr != nil {
				if !errors.Is(readErr, io.EOF) {
					sendResult(ctx, out, Result{Err: readErr})
				}
				return
			}
		}
	}()
	return out, nil
}

// FromTCP connects to address and decodes messages until EOF, context
// cancellation, or an error. Reconnect policy is intentionally left to callers.
func FromTCP(ctx context.Context, address string, options Options) (<-chan Result, error) {
	var dialer net.Dialer
	conn, err := dialer.DialContext(ctx, "tcp", address)
	if err != nil {
		return nil, err
	}
	go func() {
		<-ctx.Done()
		conn.Close()
	}()

	results, err := FromReader(ctx, conn, options)
	if err != nil {
		conn.Close()
		return nil, err
	}

	out := make(chan Result)
	go func() {
		defer close(out)
		defer conn.Close()
		for result := range results {
			if !sendResult(ctx, out, result) {
				return
			}
		}
	}()
	return out, nil
}

// FromUDP listens on address and decodes inbound AIS datagrams until context
// cancellation or an error.
func FromUDP(ctx context.Context, address string, options Options) (<-chan Result, error) {
	addr, err := net.ResolveUDPAddr("udp", address)
	if err != nil {
		return nil, err
	}
	conn, err := net.ListenUDP("udp", addr)
	if err != nil {
		return nil, err
	}

	decoder, err := NewDecoder(options)
	if err != nil {
		conn.Close()
		return nil, err
	}

	out := make(chan Result)
	go func() {
		defer close(out)
		defer conn.Close()
		defer decoder.Close()

		go func() {
			<-ctx.Done()
			conn.Close()
		}()

		buf := make([]byte, 64*1024)
		for {
			n, _, readErr := conn.ReadFromUDP(buf)
			if n > 0 {
				if _, err := decoder.Feed(buf[:n]); err != nil {
					sendResult(ctx, out, Result{Err: err})
					return
				}
				if !drainDecoder(ctx, decoder, out) {
					return
				}
			}
			if readErr != nil {
				if ctx.Err() != nil {
					sendResult(ctx, out, Result{Err: ctx.Err()})
				} else {
					sendResult(ctx, out, Result{Err: readErr})
				}
				return
			}
		}
	}()
	return out, nil
}

func drainDecoder(ctx context.Context, decoder *Decoder, out chan<- Result) bool {
	for {
		msg, ok, err := decoder.Next()
		if err != nil {
			return sendResult(ctx, out, Result{Err: err})
		}
		if !ok {
			return true
		}
		if !sendResult(ctx, out, Result{Message: msg}) {
			return false
		}
	}
}

func sendResult(ctx context.Context, out chan<- Result, result Result) bool {
	select {
	case <-ctx.Done():
		return false
	case out <- result:
		return true
	}
}
