#ifndef __AMUSE_ENTITY_HPP__
#define __AMUSE_ENTITY_HPP__

#include <cstdint>
#include <functional>
#include <cassert>
#include "Common.hpp"

namespace amuse
{
class Engine;
class AudioGroup;

/** Common 'engine child' class */
class Entity : public IObj
{
    /* Only the Engine will manage Entity lifetimes,
     * but shared_ptrs are issued to the client so it can safely track state */
    friend class Engine;
    friend struct SoundMacroState;

protected:
    bool m_destroyed = false;
    void _destroy()
    {
        assert(!m_destroyed);
        m_destroyed = true;
    }
    Engine& m_engine;
    const AudioGroup& m_audioGroup;
    int m_groupId;
    ObjectId m_objectId; /* if applicable */
public:
    Entity(Engine& engine, const AudioGroup& group, int groupId, ObjectId oid = ObjectId())
    : m_engine(engine), m_audioGroup(group), m_groupId(groupId), m_objectId(oid)
    {
    }
    ~Entity()
    {
        /* Ensure proper destruction procedure followed */
        assert(m_destroyed);
    }

    Engine& getEngine() { return m_engine; }
    const AudioGroup& getAudioGroup() const { return m_audioGroup; }
    int getGroupId() const { return m_groupId; }
    ObjectId getObjectId() const { return m_objectId; }
    bool isDestroyed() const { return m_destroyed; }
};

}

#endif // __AMUSE_ENTITY_HPP__
