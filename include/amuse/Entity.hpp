#pragma once

#include <cassert>
#include "amuse/Common.hpp"

namespace amuse {
class AudioGroup;
class Engine;

/** Common 'engine child' class */
class Entity : public IObj {
  /* Only the Engine will manage Entity lifetimes,
   * but shared_ptrs are issued to the client so it can safely track state */
  friend class Engine;
  friend struct SoundMacroState;

protected:
  bool m_destroyed = false;
  void _destroy() {
    assert(!m_destroyed);
    m_destroyed = true;
  }
  Engine& m_engine;
  const AudioGroup& m_audioGroup;
  GroupId m_groupId;
  ObjectId m_objectId; /* if applicable */
public:
  Entity(Engine& engine, const AudioGroup& group, GroupId groupId, ObjectId oid = ObjectId())
  : m_engine(engine), m_audioGroup(group), m_groupId(groupId), m_objectId(oid) {}
  ~Entity() override {
    /* Ensure proper destruction procedure followed */
    assert(m_destroyed);
  }

  Engine& getEngine() { return m_engine; }
  const AudioGroup& getAudioGroup() const { return m_audioGroup; }
  GroupId getGroupId() const { return m_groupId; }
  ObjectId getObjectId() const { return m_objectId; }
  bool isDestroyed() const { return m_destroyed; }
};

} // namespace amuse
