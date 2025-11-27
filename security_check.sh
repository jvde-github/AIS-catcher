#!/bin/bash
echo "========================================="
echo "Security Analysis for AIS-catcher"
echo "========================================="
echo ""

echo "1. STATIC ANALYSIS - cppcheck"
echo "-------------------------------------------"
echo "Running cppcheck on security-critical files..."
cppcheck --enable=warning,performance,portability --std=c++11 --quiet \
  Source/IO/HTTPServer.cpp \
  Source/IO/TCPServer.cpp \
  Source/IO/Protocol.cpp \
  Source/Application/WebViewer.cpp \
  Source/Utilities/Helper.cpp 2>&1

echo ""
echo "2. UNSAFE FUNCTIONS CHECK"
echo "-------------------------------------------"
echo "Checking for unsafe C string functions..."
grep -rn "strcpy\|strcat\|sprintf\|gets" Source/ --include="*.cpp" | wc -l | \
  xargs -I {} echo "Found {} instances (should be 0 for security)"

echo ""
echo "3. COMMAND INJECTION CHECK"
echo "-------------------------------------------"
echo "Checking for system command execution..."
grep -rn "system\|popen\|exec[vl]" Source/ --include="*.cpp" | \
  grep -v "tosystem\|System\|xecute\|navigation" | wc -l | \
  xargs -I {} echo "Found {} instances (should be 0)"

echo ""
echo "4. SQL INJECTION CHECK"  
echo "-------------------------------------------"
echo "Checking SQL query construction..."
echo "PostgreSQL escape function: $(grep -c "escape(" Source/DBMS/PostgreSQL.cpp) uses"
echo "SQLite parameterized queries: $(grep -c "sqlite3_bind" Source/Application/MapTiles.cpp) uses"

echo ""
echo "5. EXCEPTION HANDLING CHECK"
echo "-------------------------------------------"
echo "std::stoi/stoul with try-catch: $(grep -B2 "std::sto" Source/ -r --include="*.cpp" | grep -c "try")"
echo "std::bad_alloc caught: $(grep -c "std::bad_alloc\|std::exception" Source/IO/TCPServer.cpp)"

echo ""
echo "6. BUFFER SIZE LIMITS"
echo "-------------------------------------------"
echo "Content-Length validation: $(grep -c "1024 \* 1024" Source/IO/HTTPServer.cpp)"
echo "File size limits: $(grep -c "MAX_FILE_SIZE" Source/Utilities/Helper.cpp)"
echo "MMSI count limit: $(grep -c "MAX_MMSI_COUNT" Source/Application/WebViewer.cpp)"

echo ""
echo "7. MEMORY ALLOCATION ANALYSIS"
echo "-------------------------------------------"
echo "Checking for raw malloc/new[]..."
grep -rn "new \[\|malloc\|calloc" Source/ --include="*.cpp" | \
  grep -v "delete\|free\|comment" | wc -l | \
  xargs -I {} echo "Found {} raw allocations (using RAII is preferred)"

echo ""
echo "8. COMPILE WITH SECURITY FLAGS"
echo "-------------------------------------------"
cd build
echo "Recompiling with extra warnings..."
g++ -c -std=c++11 -Wall -Wextra -Werror=format-security -Werror=array-bounds \
  -D_FORTIFY_SOURCE=2 -fstack-protector-strong \
  -I../Source/Application -I../Source/Library -I../Source/Utilities \
  -I../Source/JSON -I../Source/Marine -I../Source/IO -I../Source/Device \
  ../Source/IO/HTTPServer.cpp -o /tmp/test_security.o 2>&1 | head -20
rm -f /tmp/test_security.o

echo ""
echo "========================================="
echo "Security Check Complete"
echo "========================================="
