#pragma once

#include <cstdint>

static inline int16_t N64MusyXSampClamp(int32_t val) {
  if (val < -32768)
    val = -32768;
  else if (val > 32767)
    val = 32767;
  return val;
}

unsigned N64MusyXDecompressFrame(int16_t* out, const uint8_t* in, const int16_t coefs[8][2][8], unsigned lastSample);

unsigned N64MusyXDecompressFrameRanged(int16_t* out, const uint8_t* in, const int16_t coefs[8][2][8],
                                       unsigned firstSample, unsigned lastSample);
