#pragma once

#include <cstdint>
#include <cfloat>

static inline int16_t DSPSampClamp(int32_t val) {
  if (val < -32768)
    val = -32768;
  else if (val > 32767)
    val = 32767;
  return val;
}

unsigned DSPDecompressFrame(int16_t* out, const uint8_t* in, const int16_t coefs[8][2], int16_t* prev1, int16_t* prev2,
                            unsigned lastSample);
unsigned DSPDecompressFrameStereoStride(int16_t* out, const uint8_t* in, const int16_t coefs[8][2], int16_t* prev1,
                                        int16_t* prev2, unsigned lastSample);
unsigned DSPDecompressFrameStereoDupe(int16_t* out, const uint8_t* in, const int16_t coefs[8][2], int16_t* prev1,
                                      int16_t* prev2, unsigned lastSample);

unsigned DSPDecompressFrameRanged(int16_t* out, const uint8_t* in, const int16_t coefs[8][2], int16_t* prev1,
                                  int16_t* prev2, unsigned firstSample, unsigned lastSample);

unsigned DSPDecompressFrameStateOnly(const uint8_t* in, const int16_t coefs[8][2], int16_t* prev1, int16_t* prev2,
                                     unsigned lastSample);

unsigned DSPDecompressFrameRangedStateOnly(const uint8_t* in, const int16_t coefs[8][2], int16_t* prev1, int16_t* prev2,
                                           unsigned firstSample, unsigned lastSample);

void DSPCorrelateCoefs(const short* source, int samples, short coefsOut[8][2]);

void DSPEncodeFrame(short pcmInOut[16], int sampleCount, unsigned char adpcmOut[8], const short coefsIn[8][2]);
