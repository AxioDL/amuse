#include "amuse/Studio.hpp"

#include <algorithm>

#include "amuse/Engine.hpp"

namespace amuse {

#ifndef NDEBUG
bool Studio::_cyclicCheck(const Studio* leaf) const {
  return std::any_of(m_studiosOut.cbegin(), m_studiosOut.cend(), [leaf](const auto& studio) {
    return leaf == studio.m_targetStudio.get() || studio.m_targetStudio->_cyclicCheck(leaf);
  });
}
#endif

Studio::Studio(Engine& engine, bool mainOut) : m_engine(engine), m_master(engine), m_auxA(engine), m_auxB(engine) {
  if (mainOut && engine.m_defaultStudioReady)
    addStudioSend(engine.getDefaultStudio(), 1.f, 1.f, 1.f);
}

void Studio::addStudioSend(ObjToken<Studio> studio, float dry, float auxA, float auxB) {
  m_studiosOut.emplace_back(studio, dry, auxA, auxB);

  /* Cyclic check */
  assert(!_cyclicCheck(this));
}

void Studio::resetOutputSampleRate(double sampleRate) {
  m_master.resetOutputSampleRate(sampleRate);
  m_auxA.resetOutputSampleRate(sampleRate);
  m_auxB.resetOutputSampleRate(sampleRate);
}
} // namespace amuse
