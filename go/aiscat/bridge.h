// aiscat-go C ABI bridge for AIS-catcher's C++ decoder.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AisCatDecoder AisCatDecoder;

enum AisCatFormat {
    AISCAT_FORMAT_DICTIONARY = 0,
    AISCAT_FORMAT_ANNOTATED = 1,
    AISCAT_FORMAT_JSON = 2,
    AISCAT_FORMAT_JSON_NMEA = 3,
    AISCAT_FORMAT_NMEA = 4,
    AISCAT_FORMAT_NMEA_TAG = 5,
    AISCAT_FORMAT_BINARY = 6
};

AisCatDecoder *aiscat_new(int format, int country);
void aiscat_free(AisCatDecoder *decoder);

int aiscat_feed(AisCatDecoder *decoder, const char *data, size_t len);
int aiscat_pending(AisCatDecoder *decoder);

int aiscat_next(AisCatDecoder *decoder, unsigned char **out, size_t *len);
void aiscat_free_bytes(unsigned char *data);

const char *aiscat_last_error(AisCatDecoder *decoder);

#ifdef __cplusplus
}
#endif
