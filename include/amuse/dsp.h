#ifndef _DSP_h
#define _DSP_h

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

static inline int16_t DSPSampClamp(int32_t val)
{
    if (val < -32768) val = -32768;
    else if (val > 32767) val = 32767;
    return val;
}

unsigned DSPDecompressFrame(int16_t* out, const uint8_t* in,
                            const int16_t coefs[8][2], int16_t* prev1, int16_t* prev2,
                            unsigned lastSample);
unsigned DSPDecompressFrameStereoStride(int16_t* out, const uint8_t* in,
                                        const int16_t coefs[8][2], int16_t* prev1, int16_t* prev2,
                                        unsigned lastSample);
unsigned DSPDecompressFrameStereoDupe(int16_t* out, const uint8_t* in,
                                      const int16_t coefs[8][2], int16_t* prev1, int16_t* prev2,
                                      unsigned lastSample);

unsigned DSPDecompressFrameRanged(int16_t* out, const uint8_t* in,
                                  const int16_t coefs[8][2], int16_t* prev1, int16_t* prev2,
                                  unsigned firstSample, unsigned lastSample);

unsigned DSPDecompressFrameStateOnly(const uint8_t* in,
                                     const int16_t coefs[8][2], int16_t* prev1, int16_t* prev2,
                                     unsigned lastSample);

#ifdef __cplusplus
}
#endif

#endif // _DSP_h
