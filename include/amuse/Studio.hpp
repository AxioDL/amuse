#pragma once

#include <list>

#include "amuse/Common.hpp"
#include "amuse/Entity.hpp"
#include "amuse/Submix.hpp"

namespace amuse {
struct StudioSend;

class Studio {
  friend class Engine;
  Engine& m_engine;
  Submix m_master;
  Submix m_auxA;
  Submix m_auxB;

  std::list<StudioSend> m_studiosOut;
#ifndef NDEBUG
  bool _cyclicCheck(Studio* leaf);
#endif

public:
  Studio(Engine& engine, bool mainOut);

  /** Register a target Studio to send this Studio's mixing busses */
  void addStudioSend(ObjToken<Studio> studio, float dry, float auxA, float auxB);

  /** Advise submixes of changing sample rate */
  void resetOutputSampleRate(double sampleRate);

  Submix& getMaster() { return m_master; }
  Submix& getAuxA() { return m_auxA; }
  Submix& getAuxB() { return m_auxB; }

  Engine& getEngine() { return m_engine; }
};

struct StudioSend {
  ObjToken<Studio> m_targetStudio;
  float m_dryLevel;
  float m_auxALevel;
  float m_auxBLevel;
  StudioSend(ObjToken<Studio> studio, float dry, float auxA, float auxB)
  : m_targetStudio(studio), m_dryLevel(dry), m_auxALevel(auxA), m_auxBLevel(auxB) {}
};

} // namespace amuse
