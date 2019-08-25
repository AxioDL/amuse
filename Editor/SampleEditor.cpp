#include "SampleEditor.hpp"
#include "MainWindow.hpp"
#include "amuse/DSPCodec.hpp"
#include <QPainter>
#include <QPaintEvent>
#include <QSpinBox>
#include <QScrollBar>
#include <QCheckBox>

SampleEditor* SampleView::getEditor() const {
  return qobject_cast<SampleEditor*>(parentWidget()->parentWidget()->parentWidget());
}

void SampleView::seekToSample(qreal sample) {
  sample = std::min(sample, qreal(m_sample->getNumSamples()));

  if (m_sample->isFormatDSP()) {
    if (sample < m_curSamplePos) {
      m_prev1 = m_prev2 = 0;
      m_curSamplePos = 0.0;
    }

    uint32_t startBlock = uint32_t(m_curSamplePos) / 14;
    uint32_t startRem = uint32_t(m_curSamplePos) % 14;
    uint32_t endBlock = uint32_t(sample) / 14;
    uint32_t endRem = uint32_t(sample) % 14;

    if (startRem) {
      uint32_t end = 14;
      if (startBlock == endBlock) {
        end = endRem;
        endRem = 0;
      }
      DSPDecompressFrameRangedStateOnly(m_sampleData + 8 * startBlock, m_sample->m_ADPCMParms.dsp.m_coefs, &m_prev1,
                                        &m_prev2, startRem, end);
      if (end == 14)
        ++startBlock;
    }

    for (uint32_t b = startBlock; b < endBlock; ++b) {
      DSPDecompressFrameStateOnly(m_sampleData + 8 * b, m_sample->m_ADPCMParms.dsp.m_coefs, &m_prev1, &m_prev2, 14);
    }

    if (endRem) {
      DSPDecompressFrameStateOnly(m_sampleData + 8 * endBlock, m_sample->m_ADPCMParms.dsp.m_coefs, &m_prev1, &m_prev2,
                                  endRem);
    }
  }

  m_curSamplePos = sample;
}

std::pair<std::pair<qreal, qreal>, std::pair<qreal, qreal>> SampleView::iterateSampleInterval(qreal interval) {
  std::pair<std::pair<qreal, qreal>, std::pair<qreal, qreal>> ret = {{0.0, 1.0}, {0.0, -1.0}}; // min,max avg,peak
  qreal avg = 0.0;
  qreal div = 0.0;
  qreal endSample = std::min(m_curSamplePos + interval, qreal(m_sample->getNumSamples()));

  auto accumulate = [&ret, &avg, &div](int16_t sample) {
    qreal sampleF = sample / 32768.0;
    avg += sampleF;
    div += 1.0;
    ret.first.second = std::min(ret.first.second, sampleF);
    ret.second.second = std::max(ret.second.second, sampleF);
  };

  if (m_sample->isFormatDSP()) {
    uint32_t startBlock = uint32_t(m_curSamplePos) / 14;
    uint32_t startRem = uint32_t(m_curSamplePos) % 14;
    uint32_t endBlock = uint32_t(endSample) / 14;
    uint32_t endRem = uint32_t(endSample) % 14;

    int16_t sampleBlock[14];

    if (startRem) {
      uint32_t end = 14;
      if (startBlock == endBlock) {
        end = endRem;
        endRem = 0;
      }
      DSPDecompressFrameRanged(sampleBlock, m_sampleData + 8 * startBlock, m_sample->m_ADPCMParms.dsp.m_coefs, &m_prev1,
                               &m_prev2, startRem, end);
      for (uint32_t s = 0; s < end - startRem; ++s)
        accumulate(sampleBlock[s]);
      if (end == 14)
        ++startBlock;
    }

    for (uint32_t b = startBlock; b < endBlock; ++b) {
      DSPDecompressFrame(sampleBlock, m_sampleData + 8 * b, m_sample->m_ADPCMParms.dsp.m_coefs, &m_prev1, &m_prev2, 14);
      for (uint32_t s = 0; s < 14; ++s)
        accumulate(sampleBlock[s]);
    }

    if (endRem) {
      DSPDecompressFrame(sampleBlock, m_sampleData + 8 * endBlock, m_sample->m_ADPCMParms.dsp.m_coefs, &m_prev1,
                         &m_prev2, endRem);
      for (uint32_t s = 0; s < endRem; ++s)
        accumulate(sampleBlock[s]);
    }
  } else if (m_sample->getSampleFormat() == amuse::SampleFormat::PCM_PC) {
    for (uint32_t s = uint32_t(m_curSamplePos); s < uint32_t(endSample); ++s)
      accumulate(reinterpret_cast<const int16_t*>(m_sampleData)[s]);
  }

  m_curSamplePos = endSample;

  if (div == 0.0)
    return {};

  avg /= div;

  if (avg > 0.0) {
    ret.first.first = ret.first.second;
    ret.second.first = avg;
  } else {
    ret.first.first = avg;
    ret.second.first = ret.second.second;
  }

  return ret;
}

void SampleView::paintEvent(QPaintEvent* ev) {
  QPainter painter(this);

  constexpr int rulerHeight = 28;
  int sampleHeight = height() - rulerHeight;

  qreal deviceRatio = devicePixelRatioF();
  qreal rectStart = ev->rect().x();
  qreal deviceWidth = ev->rect().width() * deviceRatio;
  qreal increment = 1.0 / deviceRatio;
  qreal deviceSamplesPerPx = m_samplesPerPx / deviceRatio;
  qreal startSample = rectStart * m_samplesPerPx;
  qreal startSampleRem = std::fmod(startSample, deviceSamplesPerPx);
  if (startSampleRem > DBL_EPSILON) {
    startSample -= startSampleRem;
    deviceWidth += startSampleRem;
    rectStart = startSample / m_samplesPerPx;
  }

  if (m_sample) {
    QPen peakPen(QColor(255, 147, 41), increment);
    QPen avgPen(QColor(254, 177, 68), increment);

    qreal scale = -sampleHeight / 2.0;
    qreal trans = sampleHeight / 2.0;

    std::pair<std::pair<qreal, qreal>, std::pair<qreal, qreal>> lastAvgPeak;
    if (startSample >= deviceSamplesPerPx) {
      seekToSample(startSample - deviceSamplesPerPx);
      lastAvgPeak = iterateSampleInterval(deviceSamplesPerPx);
    } else {
      seekToSample(startSample);
      lastAvgPeak = std::pair<std::pair<qreal, qreal>, std::pair<qreal, qreal>>{};
    }

    std::pair<std::pair<qreal, qreal>, std::pair<qreal, qreal>> avgPeak;
    for (qreal i = 0.0; i < deviceWidth; i += increment) {
      if (m_curSamplePos + deviceSamplesPerPx > m_sample->getNumSamples())
        break;
      if (i == 0.0 || std::floor(m_curSamplePos) != std::floor(m_curSamplePos + deviceSamplesPerPx))
        avgPeak = iterateSampleInterval(deviceSamplesPerPx);
      else
        m_curSamplePos += deviceSamplesPerPx;
      std::pair<std::pair<qreal, qreal>, std::pair<qreal, qreal>> drawAvgPeak = avgPeak;
      if (lastAvgPeak.first.first > avgPeak.second.first)
        drawAvgPeak.second.first = lastAvgPeak.first.first;
      else if (lastAvgPeak.second.first < avgPeak.first.first)
        drawAvgPeak.first.first = lastAvgPeak.second.first;
      painter.setPen(peakPen);
      painter.drawLine(QPointF(rectStart + i, drawAvgPeak.first.second * scale + trans),
                       QPointF(rectStart + i, drawAvgPeak.second.second * scale + trans));
      painter.setPen(avgPen);
      painter.drawLine(QPointF(rectStart + i, drawAvgPeak.first.first * scale + trans),
                       QPointF(rectStart + i, drawAvgPeak.second.first * scale + trans));
      lastAvgPeak = avgPeak;
    }
  }

  painter.setBrush(palette().brush(QPalette::Base));
  painter.setPen(Qt::NoPen);
  painter.drawRect(QRect(ev->rect().x(), sampleHeight, ev->rect().width(), rulerHeight));

  constexpr qreal minNumSpacing = 40.0;

  qreal binaryDiv = 1.0;
  while (m_samplesPerPx * minNumSpacing > binaryDiv)
    binaryDiv *= 2.0;

  qreal numSpacing = binaryDiv / m_samplesPerPx;
  qreal tickSpacing = numSpacing / 5;

  int startX = std::max(0, int(ev->rect().x() - numSpacing));
  int lastNumDiv = int(startX / numSpacing);
  int lastTickDiv = int(startX / tickSpacing);

  qreal samplePos = startX * m_samplesPerPx;
  painter.setFont(m_rulerFont);
  painter.setPen(QPen(QColor(127, 127, 127), increment));
  for (int i = startX; i < ev->rect().x() + ev->rect().width() + numSpacing; ++i) {
    int thisNumDiv = int(i / numSpacing);
    int thisTickDiv = int(i / tickSpacing);

    if (thisNumDiv != lastNumDiv) {
      painter.drawLine(i, sampleHeight + 1, i, sampleHeight + 8);
      lastNumDiv = thisNumDiv;
      lastTickDiv = thisTickDiv;

      painter.drawText(QRectF(i - numSpacing / 2.0, sampleHeight + 11.0, numSpacing, 12.0),
                       QString::number(int(samplePos)), QTextOption(Qt::AlignCenter));
    } else if (thisTickDiv != lastTickDiv) {
      painter.drawLine(i, sampleHeight + 1, i, sampleHeight + 5);
      lastTickDiv = thisTickDiv;
    }

    samplePos += m_samplesPerPx;
  }

  if (m_sample) {
    if (m_sample->isLooped()) {
      int loopStart = m_sample->m_loopStartSample;
      int loopEnd = loopStart + m_sample->m_loopLengthSamples - 1;
      QPointF points[4];

      painter.setPen(QPen(Qt::green, increment));
      painter.setBrush(Qt::green);
      qreal startPos = loopStart / m_samplesPerPx;
      painter.drawLine(QPointF(startPos, 0.0), QPointF(startPos, height()));
      points[0] = QPointF(startPos, height() - 6.0);
      points[1] = QPointF(startPos - 6.0, height());
      points[2] = QPointF(startPos + 6.0, height());
      points[3] = QPointF(startPos, height() - 6.0);
      painter.drawPolygon(points, 4);

      painter.setPen(QPen(Qt::red, increment));
      painter.setBrush(Qt::red);
      qreal endPos = loopEnd / m_samplesPerPx;
      painter.drawLine(QPointF(endPos, 0.0), QPointF(endPos, height()));
      points[0] = QPointF(endPos, height() - 6.0);
      points[1] = QPointF(endPos - 6.0, height());
      points[2] = QPointF(endPos + 6.0, height());
      points[3] = QPointF(endPos, height() - 6.0);
      painter.drawPolygon(points, 4);
    }

    if (m_displaySamplePos >= 0) {
      painter.setPen(QPen(Qt::white, increment));
      qreal pos = m_displaySamplePos / m_samplesPerPx;
      painter.drawLine(QPointF(pos, 0.0), QPointF(pos, height()));
    }
  }
}

void SampleView::calculateSamplesPerPx() {
  m_samplesPerPx = (1.0 - m_zoomFactor) * m_baseSamplesPerPx + m_zoomFactor * 1.0;
}

void SampleView::resetZoom() {
  if (QScrollArea* scroll = qobject_cast<QScrollArea*>(parentWidget()->parentWidget())) {
    m_baseSamplesPerPx = m_sample->getNumSamples() / qreal(scroll->width() - 4);
    calculateSamplesPerPx();
    setMinimumWidth(int(m_sample->getNumSamples() / m_samplesPerPx) + 1);
    update();
  }
}

void SampleView::setZoom(int zVal) {
  m_zoomFactor = zVal / 99.0;
  calculateSamplesPerPx();
  setMinimumWidth(int(m_sample->getNumSamples() / m_samplesPerPx) + 1);
  update();
}

void SampleView::showEvent(QShowEvent* ev) { setZoom(0); }

void SampleView::mousePressEvent(QMouseEvent* ev) {
  if (m_sample && m_sample->isLooped()) {
    int loopStart = m_sample->m_loopStartSample;
    int startPos = int(loopStart / m_samplesPerPx);
    int startDelta = std::abs(startPos - ev->pos().x());
    bool startPass = startDelta < 20;

    int loopEnd = loopStart + m_sample->m_loopLengthSamples - 1;
    int endPos = int(loopEnd / m_samplesPerPx);
    int endDelta = std::abs(endPos - ev->pos().x());
    bool endPass = endDelta < 20;

    if (startPass && endPass) {
      if (startDelta == endDelta) {
        if (ev->pos().x() < startPos)
          m_dragState = DragState::Start;
        else
          m_dragState = DragState::End;
      } else if (startDelta < endDelta)
        m_dragState = DragState::Start;
      else
        m_dragState = DragState::End;
    } else if (startPass)
      m_dragState = DragState::Start;
    else if (endPass)
      m_dragState = DragState::End;

    getEditor()->m_controls->setFileWrite(m_dragState == DragState::None);

    mouseMoveEvent(ev);
  }
}

void SampleView::mouseReleaseEvent(QMouseEvent* ev) {
  m_dragState = DragState::None;
  getEditor()->m_controls->setFileWrite(true);
}

void SampleView::mouseMoveEvent(QMouseEvent* ev) {
  if (m_dragState == DragState::Start)
    getEditor()->m_controls->setLoopStartSample(int(ev->pos().x() * m_samplesPerPx));
  else if (m_dragState == DragState::End)
    getEditor()->m_controls->setLoopEndSample(int(ev->pos().x() * m_samplesPerPx));
}

void SampleView::wheelEvent(QWheelEvent* ev) {
  if (QScrollArea* scroll = qobject_cast<QScrollArea*>(parentWidget()->parentWidget())) {
    /* Send wheel event directly to the scroll bar */
    QApplication::sendEvent(scroll->horizontalScrollBar(), ev);
  }
}

bool SampleView::loadData(ProjectModel::SampleNode* node) {
  bool reset = m_node.get() != node;
  m_node = node;

  ProjectModel::GroupNode* group = g_MainWindow->projectModel()->getGroupNode(m_node.get());
  std::tie(m_sample, m_sampleData) = group->getAudioGroup()->getSampleData(m_node->id(), m_node->m_obj.get());
  if (reset)
    resetZoom();

  m_playbackMacro = amuse::MakeObj<amuse::SoundMacro>();
  amuse::SoundMacro::CmdStartSample* startSample = static_cast<amuse::SoundMacro::CmdStartSample*>(
      m_playbackMacro->insertNewCmd(0, amuse::SoundMacro::CmdOp::StartSample));
  startSample->sample.id = m_node->id();
  amuse::SoundMacro::CmdWaitMs* waitMillisec = static_cast<amuse::SoundMacro::CmdWaitMs*>(
      m_playbackMacro->insertNewCmd(1, amuse::SoundMacro::CmdOp::WaitMs));
  waitMillisec->keyOff = true;
  waitMillisec->ms = 65535;
  m_playbackMacro->insertNewCmd(2, amuse::SoundMacro::CmdOp::StopSample);
  m_playbackMacro->insertNewCmd(3, amuse::SoundMacro::CmdOp::End);

  update();

  return reset;
}

void SampleView::unloadData() {
  m_node.reset();
  m_sample.reset();
  m_playbackMacro.reset();
  update();
}

ProjectModel::INode* SampleView::currentNode() const { return m_node.get(); }

amuse::SampleEntryData* SampleView::entryData() const { return m_sample.get(); }

const amuse::SoundMacro* SampleView::soundMacro() const { return m_playbackMacro.get(); }

void SampleView::setSamplePos(int pos) {
  if (pos != m_displaySamplePos) {
    if (pos >= 0) {
      int lastPos = m_displaySamplePos;
      m_displaySamplePos = pos;
      updateSampleRange(lastPos, pos);
    } else {
      m_displaySamplePos = pos;
      update();
    }
  }
}

void SampleView::updateSampleRange(int oldSamp, int newSamp) {
  qreal lastPos = oldSamp / m_samplesPerPx;
  qreal newPos = newSamp / m_samplesPerPx;
  update(int(std::min(lastPos, newPos)) - 10, 0, int(std::fabs(newPos - lastPos)) + 20, height());
}

SampleView::SampleView(QWidget* parent) : QWidget(parent) {
  setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
  m_rulerFont.setPointSize(8);
}

void SampleControls::zoomSliderChanged(int val) {
  SampleEditor* editor = qobject_cast<SampleEditor*>(parentWidget());
  editor->m_sampleView->setZoom(val);
}

class SampLoopUndoCommand : public EditorUndoCommand {
  uint32_t m_redoStartVal, m_redoEndVal, m_undoStartVal, m_undoEndVal;
  int m_fieldIdx;
  bool m_undid = false;

public:
  SampLoopUndoCommand(uint32_t redoStart, uint32_t redoEnd, const QString& fieldName, int fieldIdx,
                      amuse::ObjToken<ProjectModel::SampleNode> node)
  : EditorUndoCommand(node.get(), SampleControls::tr("Change %1").arg(fieldName))
  , m_redoStartVal(redoStart)
  , m_redoEndVal(redoEnd)
  , m_fieldIdx(fieldIdx) {}
  void undo() override {
    m_undid = true;
    amuse::SampleEntryData* data = m_node.cast<ProjectModel::SampleNode>()->m_obj->m_data.get();
    data->setLoopStartSample(m_undoStartVal);
    data->setLoopEndSample(m_undoEndVal);
    EditorUndoCommand::undo();
    if (SampleEditor* e = static_cast<SampleEditor*>(g_MainWindow->getEditorWidget()))
      e->m_controls->doFileWrite();
  }
  void redo() override {
    amuse::SampleEntryData* data = m_node.cast<ProjectModel::SampleNode>()->m_obj->m_data.get();
    m_undoStartVal = data->getLoopStartSample();
    m_undoEndVal = data->getLoopEndSample();
    data->setLoopStartSample(m_redoStartVal);
    data->setLoopEndSample(m_redoEndVal);
    if (m_undid) {
      EditorUndoCommand::redo();
      if (SampleEditor* e = static_cast<SampleEditor*>(g_MainWindow->getEditorWidget()))
        e->m_controls->doFileWrite();
    }
  }
  bool mergeWith(const QUndoCommand* other) override {
    if (other->id() == id() && static_cast<const SampLoopUndoCommand*>(other)->m_fieldIdx == m_fieldIdx) {
      m_redoStartVal = static_cast<const SampLoopUndoCommand*>(other)->m_redoStartVal;
      m_redoEndVal = static_cast<const SampLoopUndoCommand*>(other)->m_redoEndVal;
      return true;
    }
    return false;
  }
  int id() const override { return int(Id::SampLoop); }
};

void SampleControls::loopStateChanged(int state) {
  if (m_enableUpdate) {
    SampleEditor* editor = qobject_cast<SampleEditor*>(parentWidget());
    amuse::SampleEntryData* data = editor->m_sampleView->entryData();
    if (state == Qt::Checked) {
      g_MainWindow->pushUndoCommand(new SampLoopUndoCommand(
          atUint32(m_loopStart->value()), uint32_t(m_loopEnd->value()), tr("Loop"), 0, editor->m_sampleView->m_node));
      m_loopStart->setEnabled(true);
      m_loopEnd->setEnabled(true);
      if (m_loopStart->value() == 0 && m_loopEnd->value() == 0) {
        m_enableUpdate = false;
        m_loopEnd->setValue(data->getNumSamples() - 1);
        m_loopStart->setMaximum(m_loopEnd->value());
        m_enableUpdate = true;
      }
      data->m_loopStartSample = atUint32(m_loopStart->value());
      data->m_loopLengthSamples = m_loopEnd->value() + 1 - data->m_loopStartSample;
    } else {
      g_MainWindow->pushUndoCommand(new SampLoopUndoCommand(0, 0, tr("Loop"), 0, editor->m_sampleView->m_node));
      m_loopStart->setEnabled(false);
      m_loopEnd->setEnabled(false);
      data->m_loopStartSample = 0;
      data->m_loopLengthSamples = 0;
    }
    editor->m_sampleView->update();
    doFileWrite();
  }
}

void SampleControls::startValueChanged(int val) {
  if (m_enableUpdate) {
    SampleEditor* editor = qobject_cast<SampleEditor*>(parentWidget());
    amuse::SampleEntryData* data = editor->m_sampleView->entryData();

    int oldPos = data->getLoopStartSample();
    g_MainWindow->pushUndoCommand(new SampLoopUndoCommand(atUint32(val), data->getLoopEndSample(), tr("Loop Start"), 1,
                                                          editor->m_sampleView->m_node));

    data->setLoopStartSample(atUint32(val));
    m_loopEnd->setMinimum(val);

    editor->m_sampleView->updateSampleRange(oldPos, val);
    doFileWrite();
  }
}

void SampleControls::endValueChanged(int val) {
  if (m_enableUpdate) {
    SampleEditor* editor = qobject_cast<SampleEditor*>(parentWidget());
    amuse::SampleEntryData* data = editor->m_sampleView->entryData();

    int oldPos = data->getLoopEndSample();
    g_MainWindow->pushUndoCommand(new SampLoopUndoCommand(data->getLoopStartSample(), atUint32(val), tr("Loop End"), 2,
                                                          editor->m_sampleView->m_node));

    data->setLoopEndSample(atUint32(val));
    m_loopStart->setMaximum(val);

    editor->m_sampleView->updateSampleRange(oldPos, val);
    doFileWrite();
  }
}

class SampPitchUndoCommand : public EditorUndoCommand {
  uint8_t m_redoPitchVal, m_undoPitchVal;
  bool m_undid = false;

public:
  SampPitchUndoCommand(atUint8 redoPitch, amuse::ObjToken<ProjectModel::SampleNode> node)
  : EditorUndoCommand(node.get(), SampleControls::tr("Change Base Pitch")), m_redoPitchVal(redoPitch) {}
  void undo() override {
    m_undid = true;
    amuse::SampleEntryData* data = m_node.cast<ProjectModel::SampleNode>()->m_obj->m_data.get();
    data->m_pitch = m_undoPitchVal;
    EditorUndoCommand::undo();
    if (SampleEditor* e = static_cast<SampleEditor*>(g_MainWindow->getEditorWidget()))
      e->m_controls->doFileWrite();
  }
  void redo() override {
    amuse::SampleEntryData* data = m_node.cast<ProjectModel::SampleNode>()->m_obj->m_data.get();
    m_undoPitchVal = data->m_pitch;
    data->m_pitch = m_redoPitchVal;
    if (m_undid) {
      EditorUndoCommand::redo();
      if (SampleEditor* e = static_cast<SampleEditor*>(g_MainWindow->getEditorWidget()))
        e->m_controls->doFileWrite();
    }
  }
  bool mergeWith(const QUndoCommand* other) override {
    if (other->id() == id()) {
      m_redoPitchVal = static_cast<const SampPitchUndoCommand*>(other)->m_redoPitchVal;
      return true;
    }
    return false;
  }
  int id() const override { return int(Id::SampPitch); }
};

void SampleControls::pitchValueChanged(int val) {
  if (m_enableUpdate) {
    SampleEditor* editor = qobject_cast<SampleEditor*>(parentWidget());
    amuse::SampleEntryData* data = editor->m_sampleView->entryData();

    g_MainWindow->pushUndoCommand(new SampPitchUndoCommand(atUint8(val), editor->m_sampleView->m_node));

    data->m_pitch = atUint8(val);
    doFileWrite();
  }
}

void SampleControls::makeWAVVersion() {
  SampleEditor* editor = qobject_cast<SampleEditor*>(parentWidget());
  ProjectModel::SampleNode* node = static_cast<ProjectModel::SampleNode*>(editor->currentNode());
  g_MainWindow->projectModel()->getGroupNode(node)->getAudioGroup()->makeWAVVersion(node->id(), node->m_obj.get());
  updateFileState();
}

void SampleControls::makeCompressedVersion() {
  SampleEditor* editor = qobject_cast<SampleEditor*>(parentWidget());
  ProjectModel::SampleNode* node = static_cast<ProjectModel::SampleNode*>(editor->currentNode());
  g_MainWindow->projectModel()->getGroupNode(node)->getAudioGroup()->makeCompressedVersion(node->id(),
                                                                                           node->m_obj.get());
  updateFileState();
}

void SampleControls::showInBrowser() { ShowInGraphicalShell(this, m_path); }

void SampleControls::updateFileState() {
  m_enableUpdate = false;

  SampleEditor* editor = qobject_cast<SampleEditor*>(parentWidget());
  ProjectModel::SampleNode* node = static_cast<ProjectModel::SampleNode*>(editor->currentNode());

  amuse::SystemString path;
  amuse::SampleFileState state = g_MainWindow->projectModel()->getGroupNode(node)->getAudioGroup()->getSampleFileState(
      node->id(), node->m_obj.get(), &path);
  disconnect(m_makeOtherConn);

  switch (state) {
  case amuse::SampleFileState::MemoryOnlyWAV:
  case amuse::SampleFileState::CompressedNoWAV:
    m_makeOtherVersion->setText(tr("Make WAV Version"));
    m_makeOtherVersion->setDisabled(false);
    m_makeOtherConn = connect(m_makeOtherVersion, &QPushButton::clicked, this, &SampleControls::makeWAVVersion);
    break;
  case amuse::SampleFileState::MemoryOnlyCompressed:
  case amuse::SampleFileState::WAVRecent:
  case amuse::SampleFileState::WAVNoCompressed:
    m_makeOtherVersion->setText(tr("Make Compressed Version"));
    m_makeOtherVersion->setDisabled(false);
    m_makeOtherConn = connect(m_makeOtherVersion, &QPushButton::clicked, this, &SampleControls::makeCompressedVersion);
    break;
  default:
    m_makeOtherVersion->setText(tr("Up To Date"));
    m_makeOtherVersion->setDisabled(true);
    break;
  }

  amuse::SampleEntryData* data = editor->m_sampleView->entryData();
  m_loopCheck->setChecked(data->isLooped());
  int loopStart = 0;
  int loopEnd = 0;
  int loopMax = data->getNumSamples() - 1;
  if (data->isLooped()) {
    loopStart = data->m_loopStartSample;
    loopEnd = loopStart + data->m_loopLengthSamples - 1;
  }
  m_loopStart->setMaximum(loopEnd);
  m_loopEnd->setMinimum(loopStart);
  m_loopEnd->setMaximum(loopMax);
  m_loopStart->setValue(loopStart);
  m_loopEnd->setValue(loopEnd);
  m_basePitch->setValue(data->m_pitch);
  m_loopStart->setEnabled(data->isLooped());
  m_loopEnd->setEnabled(data->isLooped());

  if (!path.empty()) {
    m_path = SysStringToQString(path);
    m_showInBrowser->setDisabled(false);
  } else {
    m_path = QString();
    m_showInBrowser->setDisabled(true);
  }

  m_enableUpdate = true;
}

void SampleControls::doFileWrite() {
  if (m_enableFileWrite) {
    SampleEditor* editor = qobject_cast<SampleEditor*>(parentWidget());
    ProjectModel::SampleNode* node = static_cast<ProjectModel::SampleNode*>(editor->currentNode());
    g_MainWindow->projectModel()->getGroupNode(node)->getAudioGroup()->patchSampleMetadata(node->id(),
                                                                                           node->m_obj.get());
  }
}

void SampleControls::setFileWrite(bool w) {
  m_enableFileWrite = w;
  doFileWrite();
}

void SampleControls::loadData(bool reset) {
  m_zoomSlider->setDisabled(false);
  m_loopCheck->setDisabled(false);
  m_loopStart->setDisabled(false);
  m_loopEnd->setDisabled(false);
  m_basePitch->setDisabled(false);
  if (reset)
    m_zoomSlider->setValue(0);
  updateFileState();
}

void SampleControls::unloadData() {
  m_zoomSlider->setDisabled(true);
  m_loopCheck->setDisabled(true);
  m_loopStart->setDisabled(true);
  m_loopEnd->setDisabled(true);
  m_basePitch->setDisabled(true);
  m_makeOtherVersion->setText(tr("Nothing Loaded"));
  m_makeOtherVersion->setDisabled(true);
  m_showInBrowser->setDisabled(true);
}

SampleControls::SampleControls(QWidget* parent) : QFrame(parent) {
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  setFixedHeight(100);
  setFrameShape(QFrame::StyledPanel);
  setFrameShadow(QFrame::Sunken);
  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);

  QPalette palette = QWidget::palette();
  palette.setColor(QPalette::Base, palette.color(QPalette::Window));

  QHBoxLayout* mainLayout = new QHBoxLayout;

  QGridLayout* leftLayout = new QGridLayout;

  leftLayout->addWidget(new QLabel(tr("Zoom")), 0, 0);
  m_zoomSlider = new QSlider(Qt::Horizontal);
  m_zoomSlider->setDisabled(true);
  m_zoomSlider->setRange(0, 99);
  connect(m_zoomSlider, qOverload<int>(&QSlider::valueChanged), this, &SampleControls::zoomSliderChanged);
  leftLayout->addWidget(m_zoomSlider, 1, 0);

  leftLayout->addWidget(new QLabel(tr("Loop")), 0, 1);
  m_loopCheck = new QCheckBox;
  m_loopCheck->setPalette(palette);
  m_loopCheck->setDisabled(true);
  connect(m_loopCheck, qOverload<int>(&QCheckBox::stateChanged), this, &SampleControls::loopStateChanged);
  leftLayout->addWidget(m_loopCheck, 1, 1);

  leftLayout->addWidget(new QLabel(tr("Start")), 0, 2);
  m_loopStart = new QSpinBox;
  m_loopStart->setPalette(palette);
  m_loopStart->setDisabled(true);
  connect(m_loopStart, qOverload<int>(&QSpinBox::valueChanged), this, &SampleControls::startValueChanged);
  leftLayout->addWidget(m_loopStart, 1, 2);

  leftLayout->addWidget(new QLabel(tr("End")), 0, 3);
  m_loopEnd = new QSpinBox;
  m_loopEnd->setPalette(palette);
  m_loopEnd->setDisabled(true);
  connect(m_loopEnd, qOverload<int>(&QSpinBox::valueChanged), this, &SampleControls::endValueChanged);
  leftLayout->addWidget(m_loopEnd, 1, 3);

  leftLayout->addWidget(new QLabel(tr("Base Pitch")), 0, 4);
  m_basePitch = new QSpinBox;
  m_basePitch->setPalette(palette);
  m_basePitch->setDisabled(true);
  m_basePitch->setMinimum(0);
  m_basePitch->setMaximum(127);
  connect(m_basePitch, qOverload<int>(&QSpinBox::valueChanged), this, &SampleControls::pitchValueChanged);
  leftLayout->addWidget(m_basePitch, 1, 4);

  leftLayout->setColumnMinimumWidth(0, 100);
  leftLayout->setColumnMinimumWidth(1, 50);
  leftLayout->setColumnMinimumWidth(2, 75);
  leftLayout->setColumnMinimumWidth(3, 75);
  leftLayout->setColumnMinimumWidth(4, 75);
  leftLayout->setRowMinimumHeight(0, 22);
  leftLayout->setRowMinimumHeight(1, 37);
  leftLayout->setContentsMargins(10, 6, 0, 14);

  QVBoxLayout* rightLayout = new QVBoxLayout;

  m_makeOtherVersion = new QPushButton(tr("Nothing Loaded"));
  m_makeOtherVersion->setMinimumWidth(250);
  m_makeOtherVersion->setDisabled(true);
  rightLayout->addWidget(m_makeOtherVersion);
  m_showInBrowser = new QPushButton(ShowInGraphicalShellString());
  m_showInBrowser->setMinimumWidth(250);
  m_showInBrowser->setDisabled(true);
  connect(m_showInBrowser, &QPushButton::clicked, this, &SampleControls::showInBrowser);
  rightLayout->addWidget(m_showInBrowser);

  mainLayout->addLayout(leftLayout);
  mainLayout->addStretch(1);
  mainLayout->addLayout(rightLayout);
  setLayout(mainLayout);
}

bool SampleEditor::loadData(ProjectModel::SampleNode* node) {
  m_controls->loadData(m_sampleView->loadData(node));
  return true;
}

void SampleEditor::unloadData() {
  m_sampleView->unloadData();
  m_controls->unloadData();
}

ProjectModel::INode* SampleEditor::currentNode() const { return m_sampleView->currentNode(); }

const amuse::SoundMacro* SampleEditor::soundMacro() const { return m_sampleView->soundMacro(); }

void SampleEditor::setSamplePos(int pos) { m_sampleView->setSamplePos(pos); }

void SampleEditor::resizeEvent(QResizeEvent* ev) { m_sampleView->resetZoom(); }

SampleEditor::SampleEditor(QWidget* parent)
: EditorWidget(parent), m_scrollArea(new QScrollArea), m_sampleView(new SampleView), m_controls(new SampleControls) {
  m_scrollArea->setWidget(m_sampleView);
  m_scrollArea->setWidgetResizable(true);

  QVBoxLayout* layout = new QVBoxLayout;
  layout->setContentsMargins(QMargins());
  layout->setSpacing(1);
  layout->addWidget(m_scrollArea);
  layout->addWidget(m_controls);
  setLayout(layout);
}
