#!/bin/bash
# Test HTTP server security hardening

PORT=${1:-8100}
HOST=${2:-localhost}

echo "Testing HTTP server security on $HOST:$PORT"
echo "============================================"
echo ""

# Test 1: Oversized Content-Length
echo "Test 1: Sending request with Content-Length > 1MB (should be rejected)"
{
    echo -e "POST /test HTTP/1.1\r"
    echo -e "Host: $HOST\r"
    echo -e "Content-Length: 2000000\r"
    echo -e "\r"
} | nc -w 2 $HOST $PORT 2>/dev/null
echo "Test 1 completed"
echo ""

# Test 2: Invalid Content-Length
echo "Test 2: Sending request with invalid Content-Length (should be rejected)"
{
    echo -e "POST /test HTTP/1.1\r"
    echo -e "Host: $HOST\r"
    echo -e "Content-Length: invalid\r"
    echo -e "\r"
} | nc -w 2 $HOST $PORT 2>/dev/null
echo "Test 2 completed"
echo ""

# Test 3: Negative Content-Length
echo "Test 3: Sending request with negative Content-Length (should be rejected)"
{
    echo -e "POST /test HTTP/1.1\r"
    echo -e "Host: $HOST\r"
    echo -e "Content-Length: -100\r"
    echo -e "\r"
} | nc -w 2 $HOST $PORT 2>/dev/null
echo "Test 3 completed"
echo ""

# Test 4: Slow POST attack simulation (partial)
echo "Test 4: Sending headers byte-by-byte (slow POST) - first 100 bytes"
{
    for i in {1..100}; do
        echo -n "X"
        sleep 0.01
    done
} | nc -w 2 $HOST $PORT 2>/dev/null &
PID=$!
sleep 2
kill $PID 2>/dev/null
echo "Test 4 completed"
echo ""

# Test 5: Normal valid request
echo "Test 5: Sending valid request (should work)"
{
    echo -e "GET / HTTP/1.1\r"
    echo -e "Host: $HOST\r"
    echo -e "\r"
} | nc -w 2 $HOST $PORT 2>/dev/null | head -1
echo "Test 5 completed"
echo ""

echo "============================================"
echo "All tests completed. Check server logs for:"
echo "  - 'Content-Length too large' messages"
echo "  - 'invalid Content-Length' messages"
echo "  - 'message buffer overflow' messages"
echo "  - 'out of memory' messages"
