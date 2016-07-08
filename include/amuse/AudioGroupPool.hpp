#ifndef __AMUSE_AUDIOGROUPPOOL_HPP__
#define __AMUSE_AUDIOGROUPPOOL_HPP__

#include <stdint.h>
#include <vector>
#include <cmath>
#include <unordered_map>
#include "Entity.hpp"
#include "Common.hpp"

namespace amuse
{

/** Converts time-cents representation to seconds */
static inline double TimeCentsToSeconds(int32_t tc)
{
    if (uint32_t(tc) == 0x80000000)
        return 0.0;
    return std::exp2(tc / (1200.0 * 65536.0));
}

/** Defines phase-based volume curve for macro volume control */
struct ADSR
{
    uint8_t attackFine;    /* 0-255ms */
    uint8_t attackCoarse;  /* 0-65280ms */
    uint8_t decayFine;     /* 0-255ms */
    uint8_t decayCoarse;   /* 0-65280ms */
    uint8_t sustainFine;   /* multiply by 0.0244 for percentage */
    uint8_t sustainCoarse; /* multiply by 6.25 for percentage */
    uint8_t releaseFine;   /* 0-255ms */
    uint8_t releaseCoarse; /* 0-65280ms */

    double getAttack() const { return (attackCoarse * 255 + attackFine) / 1000.0; }
    double getDecay() const { return decayCoarse == 128 ? 0.0 : ((decayCoarse * 255 + decayFine) / 1000.0); }
    double getSustain() const { return decayCoarse == 128 ? 1.0 : ((sustainCoarse * 6.25 + sustainFine * 0.0244) / 100.0); }
    double getRelease() const { return (releaseCoarse * 255 + releaseFine) / 1000.0; }
};

/** Defines phase-based volume curve for macro volume control (modified DLS standard) */
struct ADSRDLS
{
    uint32_t attack;      /* 16.16 Time-cents */
    uint32_t decay;       /* 16.16 Time-cents */
    uint16_t sustain;     /* 0x1000 == 100% */
    uint16_t release;     /* milliseconds */
    uint32_t velToAttack; /* 16.16, 1000.0 == 100%; attack = <attack> + (vel/128) * <velToAttack> */
    uint32_t keyToDecay;  /* 16.16, 1000.0 == 100%; decay = <decay> + (note/128) * <keyToDecay> */

    double getAttack() const { return TimeCentsToSeconds(attack); }
    double getDecay() const { return TimeCentsToSeconds(decay); }
    double getSustain() const { return sustain / double(0x1000); }
    double getRelease() const { return release / double(1000); }
    double getVelToAttack(int8_t vel) const
    {
        if (velToAttack == 0x80000000)
            return getAttack();
        return getAttack() + vel * (velToAttack / 65536.0 / 1000.0) / 128.0;
    }
    double getKeyToDecay(int8_t note) const
    {
        if (keyToDecay == 0x80000000)
            return getDecay();
        return getDecay() + note * (keyToDecay / 65536.0 / 1000.0) / 128.0;
    }
};

/** Maps individual MIDI keys to sound-entity as indexed in table
 *  (macro-voice, keymap, layer) */
struct Keymap
{
    int16_t objectId;
    int8_t transpose;
    int8_t pan; /* -128 for surround-channel only */
    int8_t prioOffset;
    int8_t pad[3];
};

/** Maps ranges of MIDI keys to sound-entity (macro-voice, keymap, layer) */
struct LayerMapping
{
    int16_t objectId;
    int8_t keyLo;
    int8_t keyHi;
    int8_t transpose;
    int8_t volume;
    int8_t prioOffset;
    int8_t span;
    int8_t pan;
};

/** Database of functional objects within Audio Group */
class AudioGroupPool
{
    std::unordered_map<ObjectId, const unsigned char*> m_soundMacros;
    std::unordered_map<ObjectId, const unsigned char*> m_tables;
    std::unordered_map<ObjectId, const Keymap*> m_keymaps;
    std::unordered_map<ObjectId, std::vector<const LayerMapping*>> m_layers;

public:
    AudioGroupPool(const unsigned char* data);
    AudioGroupPool(const unsigned char* data, PCDataTag);
    const unsigned char* soundMacro(ObjectId id) const;
    const Keymap* keymap(ObjectId id) const;
    const std::vector<const LayerMapping*>* layer(ObjectId id) const;
    const ADSR* tableAsAdsr(ObjectId id) const;
    const ADSRDLS* tableAsAdsrDLS(ObjectId id) const { return reinterpret_cast<const ADSRDLS*>(tableAsAdsr(id)); }
    const Curve* tableAsCurves(ObjectId id) const { return reinterpret_cast<const Curve*>(tableAsAdsr(id)); }
};
}

#endif // __AMUSE_AUDIOGROUPPOOL_HPP__
