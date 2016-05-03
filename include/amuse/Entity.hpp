#ifndef __AMUSE_ENTITY_HPP__
#define __AMUSE_ENTITY_HPP__

namespace amuse
{
class Engine;

/** Common 'engine child' class */
class Entity
{
protected:
    Engine& m_engine;
    int m_groupId;
public:
    Entity(Engine& engine, int groupId)
    : m_engine(engine), m_groupId(groupId) {}

    Engine& getEngine() {return m_engine;}
    int getGroupId() {return m_groupId;}
};

}

#endif // __AMUSE_ENTITY_HPP__
