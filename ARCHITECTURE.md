# AIS-catcher Architecture

This document describes the architecture of AIS-catcher, particularly focusing on how AIS binary messages are decoded and processed.

## Binary Message Decoding

AIS-catcher decodes AIS (Automatic Identification System) binary messages from Software Defined Radio (SDR) inputs. The decoding process involves several key components:

### Core Decoding Files

#### 1. Source/Marine/AIS.cpp and Source/Marine/AIS.h

These files contain the **`AIS::Decoder`** class, which is responsible for:

- **Raw signal decoding**: Converts NRZI-encoded bit streams from SDR devices into AIS binary messages
- **State machine implementation**: Manages decoding states (TRAINING, STARTFLAG, DATAFCS, FOUNDMESSAGE)
- **Bit-stuffing removal**: Handles the HDLC bit-stuffing used in AIS transmissions (removes stuffed bits after five consecutive 1s)
- **CRC validation**: Verifies message integrity using CRC-16 checksums
- **Flag detection**: Identifies start (0x7E) and stop flags in the bit stream

**Key methods:**
- `Receive()`: Processes incoming float data from demodulator
- `processData()`: Validates and processes decoded message data
- `CRC16()`: Performs CRC-16 validation
- `NextState()`: Manages decoder state transitions

**References:**
- ITU-R M.1371: Technical characteristics for AIS
- AIVDM/AIVDO protocol specification: https://gpsd.gitlab.io/gpsd/AIVDM.html

#### 2. Source/Marine/Message.cpp and Source/Marine/Message.h

These files contain the **`AIS::Message`** class, which represents decoded AIS messages and provides:

- **Binary data storage**: Stores up to MAX_AIS_LENGTH (1024) bits or MAX_AIS_BYTES (128) bytes of AIS message data
- **Bit-level manipulation**: Methods to get/set individual bits, unsigned integers, signed integers, and text fields
- **Message validation**: Validates message type and length according to AIS specifications
- **NMEA sentence generation**: Converts binary messages to NMEA 0183 format (!AIVDM/!AIVDO sentences)
- **Binary NMEA protocol**: Supports binary NMEA protocol with escaping and optional CRC

**Key methods:**
- `getUint()`, `setUint()`: Extract/set unsigned integer fields from bit positions
- `getInt()`, `setInt()`: Extract/set signed integer fields with sign extension
- `getText()`, `setText()`: Handle AIS 6-bit ASCII encoded text fields
- `validate()`: Checks message type (1-27) and minimum length requirements
- `buildNMEA()`: Generates NMEA sentences from binary data
- `getBinaryNMEA()`: Creates binary NMEA protocol packets

**Message format:**
- Bits 0-5: Message type (6 bits)
- Bits 6-7: Repeat indicator (2 bits)
- Bits 8-37: MMSI (30 bits)
- Remaining bits: Type-specific payload

#### 3. Source/Marine/NMEA.cpp and Source/Marine/NMEA.h

These files contain the **`AIS::NMEA`** class, which provides the reverse operation:

- **Parses NMEA sentences**: Converts AIVDM/AIVDO NMEA text into `AIS::Message` objects
- **Handles multi-part messages**: Assembles fragmented messages that span multiple NMEA sentences
- **Supports multiple input formats**: NMEA text, JSON, binary NMEA protocol, and NMEA 4.0 tag blocks
- **Validates checksums**: Verifies NMEA sentence integrity

This is used when AIS-catcher receives messages from external sources (serial ports, network streams, files) rather than decoding from SDR.

**Key methods:**
- `Receive()`: Parses incoming raw text/binary data
- `submitAIS()`: Creates and sends AIS::Message from parsed NMEA data
- `addline()`: Processes individual NMEA sentences

#### 4. Source/JSON/JSONAIS.cpp and Source/JSON/JSONAIS.h

These files contain the **`AIS::JSONAIS`** class, which:

- **Converts decoded AIS messages to JSON format**: Parses message-specific fields based on message type
- **Handles all 27 AIS message types**: Implements field extraction for position reports, static data, safety messages, etc.
- **Provides structured output**: Makes decoded data accessible for web interfaces and APIs

**Key methods:**
- `Receive()`: Entry point for processing messages
- `ProcessMsg()`: Routes messages by type and extracts fields
- Various field extraction helpers: `U()`, `S()`, `T()`, `E()` for different data types

## Decoding Flow

### From SDR (Binary Signal Decoding)

```
SDR Device → Demodulator → AIS::Decoder → AIS::Message → Outputs
              (DSP)          (AIS.cpp)      (Message.cpp)   (NMEA/JSON/etc.)
```

1. **Signal Acquisition**: SDR hardware receives 161.975 MHz (Channel A) and 162.025 MHz (Channel B)
2. **Demodulation**: DSP chain (Source/DSP/Demod.cpp) converts RF to baseband and demodulates GMSK
3. **Bit Decoding**: `AIS::Decoder` extracts bits, removes stuffing, validates CRC
4. **Message Creation**: `AIS::Message` stores validated binary data
5. **Format Conversion**: Messages are converted to NMEA, JSON, or other formats
6. **Output**: Data is sent to TCP/UDP sockets, HTTP endpoints, files, or screen

### From NMEA Text (Reverse Direction)

```
NMEA Input → AIS::NMEA → AIS::Message → Outputs
  (Serial/      (NMEA.cpp)  (Message.cpp)  (JSON/Binary/etc.)
   Network/File)
```

1. **Text Reception**: Receive AIVDM/AIVDO sentences from serial ports, network streams, or files
2. **NMEA Parsing**: `AIS::NMEA` validates checksums, assembles multi-part messages
3. **Binary Conversion**: Converts 6-bit ASCII armored payload to binary message data
4. **Message Storage**: Stores in `AIS::Message` object
5. **Format Conversion**: Can be forwarded in different formats (JSON, binary NMEA, etc.)

## Message Types

AIS defines 27 message types with varying minimum lengths (see `Message::validate()` in Message.cpp):

- **Types 1-3**: Position reports (149 bits minimum) - Class A vessels
- **Type 4**: Base station report (168 bits minimum)
- **Type 5**: Static voyage data (418 bits minimum) - Ship name, destination, dimensions
- **Type 8**: Binary broadcast (56 bits minimum) - Application-specific data
- **Type 18**: Position report (168 bits minimum) - Class B vessels
- **Type 19**: Extended Class B position report (312 bits minimum)
- **Type 21**: Aid-to-navigation report (271 bits minimum)
- **Type 24**: Static data report (160 bits minimum) - Class B vessel name/MMSI

Complete minimum lengths: [149, 149, 149, 168, 418, 88, 72, 56, 168, 70, 168, 72, 40, 40, 88, 92, 80, 168, 312, 70, 271, 145, 154, 160, 72, 60, 96] for types 1-27 respectively.

## Key Data Structures

### AIS::Message
- `data[MAX_AIS_BYTES]`: Binary message data (up to 128 bytes)
- `length`: Message length in bits
- `channel`: 'A' or 'B' for AIS channel
- `rxtime`: Reception timestamp
- `NMEA`: Vector of NMEA sentence strings

### AIS::Decoder
- State machine for bit stream parsing
- Handles NRZI encoding (bit transitions)
- Tracks signal level for received messages
- Manages bit-stuffing removal

## Building and Testing

To build the project:
```bash
mkdir build && cd build
cmake ..
make
```

To test with a sample file:
```bash
./AIS-catcher -r posterholt.raw -v
```

## References

- ITU-R M.1371-5: Technical characteristics for AIS
- IEC 61162-1: NMEA 0183 standard
- AIVDM/AIVDO Protocol: https://gpsd.gitlab.io/gpsd/AIVDM.html
- Guide to System Development: https://fidus.com/wp-content/uploads/2016/03/Guide_to_System_Development_March_2009.pdf
