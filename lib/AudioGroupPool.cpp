#include "amuse/AudioGroupPool.hpp"
#include "amuse/Common.hpp"
#include "amuse/Entity.hpp"

namespace amuse
{

struct Header
{
    uint32_t soundMacrosOffset;
    uint32_t tablesOffset;
    uint32_t keymapsOffset;
    uint32_t layersOffset;
    void swapBig()
    {
        soundMacrosOffset = SBig(soundMacrosOffset);
        tablesOffset = SBig(tablesOffset);
        keymapsOffset = SBig(keymapsOffset);
        layersOffset = SBig(layersOffset);
    }
};

AudioGroupPool::AudioGroupPool(const unsigned char* data)
{
    Header head = *reinterpret_cast<const Header*>(data);
    head.swapBig();

    if (head.soundMacrosOffset)
    {
        const unsigned char* cur = data + head.soundMacrosOffset;
        while (*reinterpret_cast<const uint32_t*>(cur) != 0xffffffff)
        {
            uint32_t size = SBig(*reinterpret_cast<const uint32_t*>(cur));
            ObjectId id = SBig(*reinterpret_cast<const ObjectId*>(cur + 4));
            m_soundMacros[id] = cur;
            cur += size;
        }
    }

    if (head.tablesOffset)
    {
        const unsigned char* cur = data + head.tablesOffset;
        while (*reinterpret_cast<const uint32_t*>(cur) != 0xffffffff)
        {
            uint32_t size = SBig(*reinterpret_cast<const uint32_t*>(cur));
            ObjectId id = SBig(*reinterpret_cast<const ObjectId*>(cur + 4));
            m_tables[id] = cur + 8;
            cur += size;
        }
    }

    if (head.keymapsOffset)
    {
        const unsigned char* cur = data + head.keymapsOffset;
        while (*reinterpret_cast<const uint32_t*>(cur) != 0xffffffff)
        {
            uint32_t size = SBig(*reinterpret_cast<const uint32_t*>(cur));
            ObjectId id = SBig(*reinterpret_cast<const ObjectId*>(cur + 4));
            m_keymaps[id] = reinterpret_cast<const Keymap*>(cur + 8);
            cur += size;
        }
    }

    if (head.layersOffset)
    {
        const unsigned char* cur = data + head.layersOffset;
        while (*reinterpret_cast<const uint32_t*>(cur) != 0xffffffff)
        {
            uint32_t size = SBig(*reinterpret_cast<const uint32_t*>(cur));
            ObjectId id = SBig(*reinterpret_cast<const ObjectId*>(cur + 4));
            std::vector<const LayerMapping*>& mappingsOut = m_layers[id];

            uint32_t count = SBig(*reinterpret_cast<const uint32_t*>(cur + 8));
            mappingsOut.reserve(count);
            const unsigned char* subcur = cur + 12;
            for (uint32_t i = 0; i < count; ++i)
                mappingsOut.push_back(reinterpret_cast<const LayerMapping*>(subcur + i * 12));

            cur += size;
        }
    }
}

AudioGroupPool::AudioGroupPool(const unsigned char* data, PCDataTag)
{
    const Header* head = reinterpret_cast<const Header*>(data);

    if (head->soundMacrosOffset)
    {
        const unsigned char* cur = data + head->soundMacrosOffset;
        while (*reinterpret_cast<const uint32_t*>(cur) != 0xffffffff)
        {
            uint32_t size = *reinterpret_cast<const uint32_t*>(cur);
            ObjectId id = *reinterpret_cast<const ObjectId*>(cur + 4);
            m_soundMacros[id] = cur;
            cur += size;
        }
    }

    if (head->tablesOffset)
    {
        const unsigned char* cur = data + head->tablesOffset;
        while (*reinterpret_cast<const uint32_t*>(cur) != 0xffffffff)
        {
            uint32_t size = *reinterpret_cast<const uint32_t*>(cur);
            ObjectId id = *reinterpret_cast<const ObjectId*>(cur + 4);
            m_tables[id] = cur + 8;
            cur += size;
        }
    }

    if (head->keymapsOffset)
    {
        const unsigned char* cur = data + head->keymapsOffset;
        while (*reinterpret_cast<const uint32_t*>(cur) != 0xffffffff)
        {
            uint32_t size = *reinterpret_cast<const uint32_t*>(cur);
            ObjectId id = *reinterpret_cast<const ObjectId*>(cur + 4);
            m_keymaps[id] = reinterpret_cast<const Keymap*>(cur + 8);
            cur += size;
        }
    }

    if (head->layersOffset)
    {
        const unsigned char* cur = data + head->layersOffset;
        while (*reinterpret_cast<const uint32_t*>(cur) != 0xffffffff)
        {
            uint32_t size = *reinterpret_cast<const uint32_t*>(cur);
            ObjectId id = *reinterpret_cast<const ObjectId*>(cur + 4);
            std::vector<const LayerMapping*>& mappingsOut = m_layers[id];

            uint32_t count = *reinterpret_cast<const uint32_t*>(cur + 8);
            mappingsOut.reserve(count);
            const unsigned char* subcur = cur + 12;
            for (uint32_t i = 0; i < count; ++i)
                mappingsOut.push_back(reinterpret_cast<const LayerMapping*>(subcur + i * 12));

            cur += size;
        }
    }
}

const unsigned char* AudioGroupPool::soundMacro(ObjectId id) const
{
    auto search = m_soundMacros.find(id);
    if (search == m_soundMacros.cend())
        return nullptr;
    return search->second;
}

const Keymap* AudioGroupPool::keymap(ObjectId id) const
{
    auto search = m_keymaps.find(id);
    if (search == m_keymaps.cend())
        return nullptr;
    return search->second;
}

const std::vector<const LayerMapping*>* AudioGroupPool::layer(ObjectId id) const
{
    auto search = m_layers.find(id);
    if (search == m_layers.cend())
        return nullptr;
    return &search->second;
}

const ADSR* AudioGroupPool::tableAsAdsr(ObjectId id) const
{
    auto search = m_tables.find(id);
    if (search == m_tables.cend())
        return nullptr;
    return reinterpret_cast<const ADSR*>(search->second);
}
}
