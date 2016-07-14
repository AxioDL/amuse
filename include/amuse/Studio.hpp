#ifndef __AMUSE_STUDIO_HPP__
#define __AMUSE_STUDIO_HPP__

#include <memory>
#include <list>
#include "Entity.hpp"
#include "Voice.hpp"
#include "Submix.hpp"
#include <type_traits>

namespace amuse
{

class Studio
{
    friend class Engine;
    Engine& m_engine;
    Submix m_master;
    Submix m_auxA;
    Submix m_auxB;
    struct StudioSend
    {
        std::shared_ptr<Studio> m_targetStudio;
        float m_dryLevel;
        float m_auxALevel;
        float m_auxBLevel;
        StudioSend(std::weak_ptr<Studio> studio, float dry, float auxA, float auxB)
        : m_targetStudio(studio), m_dryLevel(dry), m_auxALevel(auxA), m_auxBLevel(auxB)
        {
        }
    };
    std::list<StudioSend> m_studiosOut;
#ifndef NDEBUG
    bool _cyclicCheck(Studio* leaf);
#endif

public:
    Studio(Engine& engine, bool mainOut);

    /** Register a target Studio to send this Studio's mixing busses */
    void addStudioSend(std::weak_ptr<Studio> studio, float dry, float auxA, float auxB);

    /** Advise submixes of changing sample rate */
    void resetOutputSampleRate(double sampleRate);

    Submix& getMaster() { return m_master; }
    Submix& getAuxA() { return m_auxA; }
    Submix& getAuxB() { return m_auxB; }

    Engine& getEngine() { return m_engine; }
};
}

#endif // __AMUSE_STUDIO_HPP__
