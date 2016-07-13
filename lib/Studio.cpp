#include "amuse/Studio.hpp"
#include "amuse/Engine.hpp"

namespace amuse
{

void Studio::_destroy()
{
    m_destroyed = true;
}

void Studio::_bringOutYourDead()
{
    for (auto it = m_studiosOut.begin() ; it != m_studiosOut.end() ;)
    {
        std::shared_ptr<Studio> studio = it->m_targetStudio.lock();
        if (!studio)
            it = m_studiosOut.erase(it);
        else
            ++it;
    }
}

#ifndef NDEBUG
bool Studio::_cyclicCheck(Studio* leaf)
{
    for (auto it = m_studiosOut.begin() ; it != m_studiosOut.end() ;)
    {
        if (std::shared_ptr<Studio> studio = it->m_targetStudio.lock())
        {
            if (leaf == studio.get() || studio->_cyclicCheck(leaf))
                return true;
            ++it;
        }
        else
            it = m_studiosOut.erase(it);
    }
    return false;
}
#endif

Studio::Studio(Engine& engine, bool mainOut)
: m_engine(engine), m_auxA(engine), m_auxB(engine)
{
    if (mainOut)
        addStudioSend(engine.getDefaultStudio(), 1.f, 1.f, 1.f);
}

void Studio::addStudioSend(std::weak_ptr<Studio> studio, float dry, float auxA, float auxB)
{
#ifndef NDEBUG
    /* Cyclic check */
    assert(!_cyclicCheck(this));
#endif

    m_studiosOut.emplace_back(studio, dry, auxA, auxB);
}

void Studio::resetOutputSampleRate(double sampleRate)
{
    m_auxA.resetOutputSampleRate(sampleRate);
    m_auxB.resetOutputSampleRate(sampleRate);
}

}
