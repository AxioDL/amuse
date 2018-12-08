#include "amuse/N64MusyXCodec.hpp"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* Acknowledgements:
 * SubDrag for N64 Sound Tool (http://www.goldeneyevault.com/viewfile.php?id=212)
 * Bobby Smiles for MusyX codec research
 */

static int rdot(unsigned n, const int16_t* x, const int16_t* y) {
  int accu = 0;

  y += n;

  while (n != 0) {
    accu += ((int)*(x++) * (int)*(--y));
    --n;
  }

  return accu;
}

static int16_t adpcm_get_predicted_sample(unsigned char byte, unsigned char mask, unsigned lshift, unsigned rshift) {
  int16_t sample = ((int16_t)byte & (int16_t)mask) << lshift;
  sample >>= rshift; /* signed */
  return sample;
}

static void adpcm_get_predicted_frame(int16_t* dst, const unsigned char* src, const unsigned char* nibbles,
                                      unsigned rshift) {
  *(dst++) = (src[0] << 8) | src[1];
  *(dst++) = (src[2] << 8) | src[3];

  for (unsigned i = 1; i < 16; ++i) {
    unsigned char byteData = nibbles[i];

    *(dst++) = adpcm_get_predicted_sample(byteData, 0xf0, 8, rshift);
    *(dst++) = adpcm_get_predicted_sample(byteData, 0x0f, 12, rshift);
  }
}

static unsigned adpcm_decode_upto_8_samples(int16_t* dst, const int16_t* src, const int16_t* cb_entry,
                                            const int16_t* last_samples, unsigned size) {
  if (size == 0)
    return 0;

  const int16_t* book1 = cb_entry;
  const int16_t* book2 = cb_entry + 8;

  int16_t l1 = last_samples[0];
  int16_t l2 = last_samples[1];

  int accu;

  for (unsigned i = 0; i < size; ++i) {
    accu = (int)src[i] << 11;
    accu += book1[i] * l1 + book2[i] * l2 + rdot(i, book2, src);
    dst[i] = N64MusyXSampClamp(accu >> 11);
  }

  return size;
}

unsigned N64MusyXDecompressFrame(int16_t* out, const uint8_t* in, const int16_t coefs[8][2][8], unsigned lastSample) {
  int16_t frame[32];

  unsigned remSamples = lastSample;
  unsigned samples = 0;
  unsigned procSamples, procRes;

  unsigned char c2 = in[0x8];
  c2 = c2 % 0x80;

  const int16_t* bookPredictors = coefs[(c2 & 0xf0) >> 4][0];
  unsigned int rshift = (c2 & 0x0f);

  adpcm_get_predicted_frame(frame, &in[0x0], &in[0x8], rshift);

  procSamples = (remSamples < 2) ? remSamples : 2;
  memmove(out, frame, 2 * procSamples);
  samples += procSamples;
  remSamples -= procSamples;
  if (samples == lastSample)
    return samples;

#define adpcm_decode_upto_end_samples(inOffset, outOffset, size)                                                       \
  procSamples = (remSamples < size) ? remSamples : size;                                                               \
  procRes =                                                                                                            \
      adpcm_decode_upto_8_samples(out + inOffset, frame + inOffset, bookPredictors, out + outOffset, procSamples);     \
  samples += procRes;                                                                                                  \
  remSamples -= procRes;                                                                                               \
  if (samples == lastSample)                                                                                           \
    return samples;

  adpcm_decode_upto_end_samples(2, 0, 6);
  adpcm_decode_upto_end_samples(8, 6, 8);
  adpcm_decode_upto_end_samples(16, 14, 8);
  adpcm_decode_upto_end_samples(24, 22, 8);

  out += 32;

  c2 = in[0x18];
  c2 = c2 % 0x80;

  bookPredictors = coefs[(c2 & 0xf0) >> 4][0];
  rshift = (c2 & 0x0f);

  adpcm_get_predicted_frame(frame, &in[0x4], &in[0x18], rshift);

  procSamples = (remSamples < 2) ? remSamples : 2;
  memmove(out, frame, 2 * procSamples);
  samples += procSamples;
  remSamples -= procSamples;
  if (samples == lastSample)
    return samples;

  adpcm_decode_upto_end_samples(2, 0, 6);
  adpcm_decode_upto_end_samples(8, 6, 8);
  adpcm_decode_upto_end_samples(16, 14, 8);
  adpcm_decode_upto_end_samples(24, 22, 8);

  return samples;
}

unsigned N64MusyXDecompressFrameRanged(int16_t* out, const uint8_t* in, const int16_t coefs[8][2][8],
                                       unsigned firstSample, unsigned lastSample) {
  int16_t final[64];
  unsigned procSamples = N64MusyXDecompressFrame(final, in, coefs, firstSample + lastSample);
  unsigned samples = procSamples - firstSample;
  memmove(out, final + firstSample, samples * 2);
  return samples;
}
