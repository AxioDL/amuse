#ifndef __AMUSE_ENTITY_HPP__
#define __AMUSE_ENTITY_HPP__

#include <stdint.h>
#include <functional>
#include <assert.h>

namespace amuse
{
class Engine;
class AudioGroup;

enum class ObjectType : uint8_t
{
    Invalid = 0xff,
    SoundMacro = 0,
    Table = 1,
    Kaymap = 4,
    Layer = 8
};

/** Common ID structure statically tagging
 *  SoundMacros, Tables, Keymaps, Layers */
struct ObjectId
{
    ObjectType type = ObjectType::Invalid;
    uint8_t id = 0xff;
    bool operator ==(const ObjectId& other) const
    {return *reinterpret_cast<const uint16_t*>(this) == reinterpret_cast<const uint16_t&>(other);}
    bool operator !=(const ObjectId& other) const
    {return *reinterpret_cast<const uint16_t*>(this) != reinterpret_cast<const uint16_t&>(other);}
};

/** Common 'engine child' class */
class Entity
{
    /* Only the Engine will manage Entity lifetimes */
    friend class Engine;
    friend class SoundMacroState;
    bool m_destroyed = false;
protected:
    void _destroy() {m_destroyed = true;}
    Engine& m_engine;
    const AudioGroup& m_audioGroup;
    ObjectId m_objectId; /* if applicable */
public:
    Entity(Engine& engine, const AudioGroup& group, ObjectId oid=ObjectId())
    : m_engine(engine), m_audioGroup(group), m_objectId(oid) {}
    ~Entity()
    {
#ifndef NDEBUG
        /* Ensure proper destruction procedure followed */
        assert(m_destroyed);
#endif
    }

    Engine& getEngine() {return m_engine;}
    const AudioGroup& getAudioGroup() const {return m_audioGroup;}
    ObjectId getObjectId() const {return m_objectId;}
};

/** Curves for mapping velocity to volume and other functional mappings
 *  (defined here for visibility)*/
using Curve = uint8_t[128];

}

namespace std
{
template <> struct hash<amuse::ObjectId>
{
    size_t operator()(const amuse::ObjectId& val) const noexcept
    {return std::hash<uint16_t>()(reinterpret_cast<const uint16_t&>(val));}
};
}

#endif // __AMUSE_ENTITY_HPP__
