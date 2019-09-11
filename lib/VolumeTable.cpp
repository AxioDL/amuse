#include "amuse/VolumeTable.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace amuse {

constexpr std::array<float, 129> VolumeTable{
    0.000000f, 0.000031f, 0.000153f, 0.000397f, 0.000702f, 0.001129f, 0.001648f, 0.002228f, 0.002930f, 0.003723f,
    0.004608f, 0.005585f, 0.006653f, 0.007843f, 0.009125f, 0.010498f, 0.011963f, 0.013550f, 0.015198f, 0.016999f,
    0.018860f, 0.020844f, 0.022919f, 0.025117f, 0.027406f, 0.029817f, 0.032319f, 0.034944f, 0.037660f, 0.040468f,
    0.043428f, 0.046480f, 0.049623f, 0.052889f, 0.056276f, 0.059786f, 0.063387f, 0.067110f, 0.070956f, 0.074923f,
    0.078982f, 0.083163f, 0.087466f, 0.091922f, 0.096469f, 0.101138f, 0.105930f, 0.110843f, 0.115879f, 0.121036f,
    0.126347f, 0.131748f, 0.137303f, 0.142979f, 0.148778f, 0.154729f, 0.160772f, 0.166997f, 0.173315f, 0.179785f,
    0.186407f, 0.193121f, 0.200018f, 0.207007f, 0.214179f, 0.221473f, 0.228919f, 0.236488f, 0.244209f, 0.252083f,
    0.260079f, 0.268258f, 0.276559f, 0.285012f, 0.293649f, 0.302408f, 0.311319f, 0.320383f, 0.329600f, 0.339000f,
    0.348521f, 0.358226f, 0.368084f, 0.378094f, 0.388287f, 0.398633f, 0.409131f, 0.419813f, 0.430647f, 0.441664f,
    0.452864f, 0.464217f, 0.475753f, 0.487442f, 0.499313f, 0.511399f, 0.523606f, 0.536027f, 0.548631f, 0.561419f,
    0.574389f, 0.587542f, 0.600879f, 0.614399f, 0.628132f, 0.642018f, 0.656148f, 0.670431f, 0.684927f, 0.699637f,
    0.714530f, 0.729637f, 0.744926f, 0.760430f, 0.776147f, 0.792077f, 0.808191f, 0.824549f, 0.841090f, 0.857845f,
    0.874844f, 0.892056f, 0.909452f, 0.927122f, 0.945006f, 0.963073f, 0.981414f, 1.000000f, 1.000000f,
};

constexpr std::array<float, 129> DLSVolumeTable{
    0.000000f, 0.000062f, 0.000248f, 0.000558f, 0.000992f, 0.001550f, 0.002232f, 0.003038f, 0.003968f, 0.005022f,
    0.006200f, 0.007502f, 0.008928f, 0.010478f, 0.012152f, 0.013950f, 0.015872f, 0.017918f, 0.020088f, 0.022382f,
    0.024800f, 0.027342f, 0.030008f, 0.032798f, 0.035712f, 0.038750f, 0.041912f, 0.045198f, 0.048608f, 0.052142f,
    0.055800f, 0.059582f, 0.063488f, 0.067518f, 0.071672f, 0.075950f, 0.080352f, 0.084878f, 0.089528f, 0.094302f,
    0.099200f, 0.104222f, 0.109368f, 0.114638f, 0.120032f, 0.125550f, 0.131192f, 0.136958f, 0.142848f, 0.148862f,
    0.155000f, 0.161262f, 0.167648f, 0.174158f, 0.180792f, 0.187550f, 0.194432f, 0.201438f, 0.208568f, 0.215822f,
    0.223200f, 0.230702f, 0.238328f, 0.246078f, 0.253953f, 0.261951f, 0.270073f, 0.278319f, 0.286689f, 0.295183f,
    0.303801f, 0.312543f, 0.321409f, 0.330399f, 0.339513f, 0.348751f, 0.358113f, 0.367599f, 0.377209f, 0.386943f,
    0.396801f, 0.406783f, 0.416889f, 0.427119f, 0.437473f, 0.447951f, 0.458553f, 0.469279f, 0.480129f, 0.491103f,
    0.502201f, 0.513423f, 0.524769f, 0.536239f, 0.547833f, 0.559551f, 0.571393f, 0.583359f, 0.595449f, 0.607663f,
    0.620001f, 0.632463f, 0.645049f, 0.657759f, 0.670593f, 0.683551f, 0.696633f, 0.709839f, 0.723169f, 0.736623f,
    0.750202f, 0.763904f, 0.777730f, 0.791680f, 0.805754f, 0.819952f, 0.834274f, 0.848720f, 0.863290f, 0.877984f,
    0.892802f, 0.907744f, 0.922810f, 0.938000f, 0.953314f, 0.968752f, 0.984314f, 1.000000f, 1.000000f,
};

float LookupVolume(float vol) {
  vol = std::clamp(vol * 127.f, 0.f, 127.f);
  const float f = std::floor(vol);
  const float c = std::ceil(vol);

  if (f == c) {
    return VolumeTable[int(f)];
  }

  const float t = vol - f;
  return (1.f - t) * VolumeTable[int(f)] + t * VolumeTable[int(c)];
}

float LookupDLSVolume(float vol) {
  vol = std::clamp(vol * 127.f, 0.f, 127.f);
  const float f = std::floor(vol);
  const float c = std::ceil(vol);

  if (f == c) {
    return DLSVolumeTable[int(f)];
  }

  const float t = vol - f;
  return (1.f - t) * DLSVolumeTable[int(f)] + t * DLSVolumeTable[int(c)];
}

} // namespace amuse
