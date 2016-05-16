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
using ObjectId = uint16_t;

/** Common 'engine child' class */
class Entity
{
    /* Only the Engine will manage Entity lifetimes,
     * but shared_ptrs are issued to the client so it can safely track state */
    friend class Engine;
    friend class SoundMacroState;
protected:
    bool m_destroyed = false;
    void _destroy()
    {
#ifndef NDEBUG
        assert(!m_destroyed);
#endif
        m_destroyed = true;
    }
    Engine& m_engine;
    const AudioGroup& m_audioGroup;
    int m_groupId;
    ObjectId m_objectId = 0xffff; /* if applicable */
public:
    Entity(Engine& engine, const AudioGroup& group, int groupId, ObjectId oid=ObjectId())
    : m_engine(engine), m_audioGroup(group), m_groupId(groupId), m_objectId(oid) {}
    ~Entity()
    {
#ifndef NDEBUG
        /* Ensure proper destruction procedure followed */
        assert(m_destroyed);
#endif
    }

    Engine& getEngine() {return m_engine;}
    const AudioGroup& getAudioGroup() const {return m_audioGroup;}
    int getGroupId() const {return m_groupId;}
    ObjectId getObjectId() const {return m_objectId;}
};

/** Curves for mapping velocity to volume and other functional mappings
 *  (defined here for visibility)*/
using Curve = uint8_t[128];

}

#endif // __AMUSE_ENTITY_HPP__
