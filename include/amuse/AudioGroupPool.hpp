#ifndef __AMUSE_AUDIOGROUPPOOL_HPP__
#define __AMUSE_AUDIOGROUPPOOL_HPP__

#include <stdint.h>
#include <vector>
#include <unordered_map>
#include "Entity.hpp"

namespace amuse
{

/** Defines phase-based volume curve for macro volume control */
struct ADSR
{
    uint8_t attackFine; /* 0-255ms */
    uint8_t attackCoarse; /* 0-65280ms */
    uint8_t decayFine; /* 0-255ms */
    uint8_t decayCoarse; /* 0-65280ms */
    uint8_t sustainFine; /* multiply by 0.0244 for percentage */
    uint8_t sustainCoarse; /* multiply by 6.25 for percentage */
    uint8_t releaseFine; /* 0-255ms */
    uint8_t releaseCoarse; /* 0-65280ms */
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
    int8_t pan; /* -128 for surround-channel only */
    int8_t prioOffset;
    int8_t unk; /* usually 0x40 */
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
    const unsigned char* soundMacro(ObjectId id) const;
    const Keymap* keymap(ObjectId id) const;
    const std::vector<const LayerMapping*>* layer(ObjectId id) const;
    const ADSR* tableAsAdsr(ObjectId id) const;
    const Curve* tableAsCurves(ObjectId id) const
    {return reinterpret_cast<const Curve*>(tableAsAdsr(id));}
};

}

#endif // __AMUSE_AUDIOGROUPPOOL_HPP__
