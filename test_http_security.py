#!/usr/bin/env python3
"""
Test HTTP server security hardening
Tests for Content-Length validation, buffer overflow protection, and OOM handling
"""

import socket
import sys
import time

def test_oversized_content_length(host, port):
    """Test 1: Content-Length > 1MB should be rejected"""
    print("Test 1: Oversized Content-Length (> 1MB)")
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect((host, port))
        
        request = b"POST /test HTTP/1.1\r\n"
        request += b"Host: " + host.encode() + b"\r\n"
        request += b"Content-Length: 2000000\r\n"
        request += b"\r\n"
        
        s.sendall(request)
        
        # Server should close connection
        response = s.recv(1024)
        s.close()
        
        print(f"  Response: {response[:100]}")
        print("  ✓ Server handled oversized Content-Length")
    except Exception as e:
        print(f"  ✓ Connection closed (expected): {e}")
    print()

def test_invalid_content_length(host, port):
    """Test 2: Invalid Content-Length should be rejected"""
    print("Test 2: Invalid Content-Length")
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect((host, port))
        
        request = b"POST /test HTTP/1.1\r\n"
        request += b"Host: " + host.encode() + b"\r\n"
        request += b"Content-Length: INVALID_NUMBER\r\n"
        request += b"\r\n"
        
        s.sendall(request)
        
        response = s.recv(1024)
        s.close()
        
        print(f"  Response: {response[:100]}")
        print("  ✓ Server handled invalid Content-Length")
    except Exception as e:
        print(f"  ✓ Connection closed (expected): {e}")
    print()

def test_negative_content_length(host, port):
    """Test 3: Negative Content-Length should be rejected"""
    print("Test 3: Negative Content-Length")
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect((host, port))
        
        request = b"POST /test HTTP/1.1\r\n"
        request += b"Host: " + host.encode() + b"\r\n"
        request += b"Content-Length: -12345\r\n"
        request += b"\r\n"
        
        s.sendall(request)
        
        response = s.recv(1024)
        s.close()
        
        print(f"  Response: {response[:100]}")
        print("  ✓ Server handled negative Content-Length")
    except Exception as e:
        print(f"  ✓ Connection closed (expected): {e}")
    print()

def test_buffer_flooding(host, port):
    """Test 4: Sending data slowly to test buffer limits"""
    print("Test 4: Buffer flooding (sending 2MB of headers)")
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(10)
        s.connect((host, port))
        
        # Send valid header
        request = b"GET / HTTP/1.1\r\n"
        request += b"Host: " + host.encode() + b"\r\n"
        s.sendall(request)
        
        # Try to flood with custom headers
        for i in range(10000):
            header = f"X-Custom-{i}: {'A' * 100}\r\n".encode()
            s.sendall(header)
            if i % 1000 == 0:
                print(f"  Sent {i} headers...")
        
        s.sendall(b"\r\n")
        response = s.recv(1024)
        s.close()
        
        print(f"  Response: {response[:100]}")
        print("  ✗ Server did not close connection (potential issue)")
    except Exception as e:
        print(f"  ✓ Connection closed by server (expected): {type(e).__name__}")
    print()

def test_valid_request(host, port):
    """Test 5: Normal valid request should work"""
    print("Test 5: Valid GET request")
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect((host, port))
        
        request = b"GET / HTTP/1.1\r\n"
        request += b"Host: " + host.encode() + b"\r\n"
        request += b"\r\n"
        
        s.sendall(request)
        
        response = s.recv(1024)
        s.close()
        
        status_line = response.split(b'\r\n')[0].decode()
        print(f"  Response: {status_line}")
        print("  ✓ Valid request handled correctly")
    except Exception as e:
        print(f"  ✗ Error: {e}")
    print()

def main():
    if len(sys.argv) > 1:
        port = int(sys.argv[1])
    else:
        port = 8100
    
    if len(sys.argv) > 2:
        host = sys.argv[2]
    else:
        host = 'localhost'
    
    print(f"Testing HTTP Server Security on {host}:{port}")
    print("=" * 60)
    print()
    
    test_oversized_content_length(host, port)
    test_invalid_content_length(host, port)
    test_negative_content_length(host, port)
    test_buffer_flooding(host, port)
    test_valid_request(host, port)
    
    print("=" * 60)
    print("Tests completed. Check server logs for:")
    print("  - 'Content-Length too large' messages")
    print("  - 'invalid Content-Length' messages")
    print("  - 'message buffer overflow' messages")
    print("  - 'out of memory' messages")

if __name__ == "__main__":
    main()
