#include "ADSREditor.hpp"
#include "MainWindow.hpp"
#include <QVBoxLayout>
#include <QPainter>
#include <QMouseEvent>

static QColor Red = QColor(255, 127, 127);
static QColor Green = QColor(127, 255, 127);
static QColor Blue = QColor(150, 150, 255);

ADSREditor* ADSRView::getEditor() const { return qobject_cast<ADSREditor*>(parentWidget()); }

void ADSRView::loadData(ProjectModel::ADSRNode* node) { m_node = node; }

void ADSRView::unloadData() { m_node.reset(); }

ProjectModel::INode* ADSRView::currentNode() const { return m_node.get(); }

void ADSRView::paintEvent(QPaintEvent* ev) {
  if (!m_node)
    return;

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  amuse::ITable& table = **m_node->m_obj;
  qreal adTime = 0.0;
  qreal aTime = 0.0;
  qreal relTime = 0.0;
  qreal sustain = 0.0;
  if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
    amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
    adTime += adsr.getAttack();
    aTime = adsr.getAttack();
    adTime += adsr.getDecay();
    sustain = adsr.getSustain();
    relTime = adsr.getRelease();
  } else if (table.Isa() == amuse::ITable::Type::ADSR) {
    amuse::ADSR& adsr = static_cast<amuse::ADSR&>(table);
    adTime += adsr.getAttack();
    aTime = adsr.getAttack();
    adTime += adsr.getDecay();
    sustain = adsr.getSustain();
    relTime = adsr.getRelease();
  }
  qreal totalTime = adTime + 1.0 + relTime;

  qreal deviceRatio = devicePixelRatioF();
  qreal penWidth = std::max(std::floor(deviceRatio), 1.0) / deviceRatio;

  painter.setPen(QPen(QColor(127, 127, 127), penWidth));
  painter.setFont(m_gridFont);
  qreal yIncrement = (height() - 16.0) / 10.0;
  for (int i = 0; i < 11; ++i) {
    qreal thisY = i * yIncrement;
    qreal textY = thisY - (i == 0 ? 2.0 : (i == 10 ? 16.0 : 8.0));
    painter.drawStaticText(QPointF(0.0, textY), m_percentTexts[i]);
    painter.drawLine(QPointF(30.0, thisY), QPointF(width(), thisY));
  }

  size_t maxTime = size_t(std::max(adTime, relTime)) + 1;
  if (m_timeTexts.size() < maxTime) {
    m_timeTexts.reserve(maxTime);
    for (size_t i = m_timeTexts.size(); i < maxTime; ++i) {
      m_timeTexts.emplace_back();
      QStaticText& text = m_timeTexts.back();
      text.setText(QStringLiteral("%1.0s").arg(i));
      text.setTextWidth(32.0);
      text.setTextOption(QTextOption(Qt::AlignHCenter | Qt::AlignBaseline));
    }
  }
  qreal xBase = 30.0;
  qreal xIncrement = (width() - xBase) / totalTime;
  for (size_t i = 0; i <= size_t(adTime); ++i) {
    qreal thisX = i * xIncrement + xBase;
    qreal textX = thisX - 15.0;
    painter.drawStaticText(QPointF(textX, height() - 16.0), m_timeTexts[i]);
    painter.drawLine(QPointF(thisX, 0.0), QPointF(thisX, height() - 16.0));
  }
  xBase = (width() - 30.0) * ((adTime + 1.0) / totalTime) + 30.0;
  for (size_t i = 0; i <= size_t(relTime); ++i) {
    qreal thisX = i * xIncrement + xBase;
    qreal textX = thisX - 15.0;
    painter.drawStaticText(QPointF(textX, height() - 16.0), m_timeTexts[i]);
    painter.drawLine(QPointF(thisX, 0.0), QPointF(thisX, height() - 16.0));
  }

  qreal sustainY = (height() - 16.0) - (height() - 16.0) * sustain;
  QPointF pt0 = QPointF(30.0, height() - 16.0);
  QPointF pt1 = QPointF((width() - 30.0) * (aTime / totalTime) + 30.0, 0.0);
  QPointF pt2 = QPointF((width() - 30.0) * (adTime / totalTime) + 30.0, sustainY);
  QPointF pt3 = QPointF((width() - 30.0) * ((adTime + 1.0) / totalTime) + 30.0, sustainY);
  QPointF pt4 = QPointF(width(), height() - 16.0);

  painter.setPen(QPen(Red, penWidth * 3.0));
  painter.drawLine(pt0, pt1);
  painter.setPen(QPen(Green, penWidth * 3.0));
  painter.drawLine(pt1, pt2);
  painter.setPen(QPen(Qt::white, penWidth * 3.0));
  painter.drawLine(pt2, pt3);
  painter.setPen(QPen(Blue, penWidth * 3.0));
  painter.drawLine(pt3, pt4);
  painter.setPen(Qt::NoPen);
  painter.setBrush(Green);
  painter.drawEllipse(pt1, 8.0, 8.0);
  painter.setBrush(Qt::white);
  painter.drawEllipse(pt2, 8.0, 8.0);
  painter.setBrush(Blue);
  painter.drawEllipse(pt3, 8.0, 8.0);
}

static qreal PointDistanceSq(const QPointF& p1, const QPointF& p2) {
  QPointF delta = p1 - p2;
  return QPointF::dotProduct(delta, delta);
}

static qreal PointDistance(const QPointF& p1, const QPointF& p2) { return std::sqrt(PointDistanceSq(p1, p2)); }

void ADSRView::mousePressEvent(QMouseEvent* ev) {
  if (!m_node)
    return;

  amuse::ITable& table = **m_node->m_obj;
  qreal adTime = 0.0;
  qreal aTime = 0.0;
  qreal relTime = 0.0;
  qreal sustain = 0.0;
  if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
    amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
    adTime += adsr.getAttack();
    aTime = adsr.getAttack();
    adTime += adsr.getDecay();
    sustain = adsr.getSustain();
    relTime = adsr.getRelease();
  } else if (table.Isa() == amuse::ITable::Type::ADSR) {
    amuse::ADSR& adsr = static_cast<amuse::ADSR&>(table);
    adTime += adsr.getAttack();
    aTime = adsr.getAttack();
    adTime += adsr.getDecay();
    sustain = adsr.getSustain();
    relTime = adsr.getRelease();
  }
  qreal totalTime = adTime + 1.0 + relTime;

  qreal sustainY = (height() - 16.0) - (height() - 16.0) * sustain;
  QPointF points[] = {QPointF((width() - 30.0) * (aTime / totalTime) + 30.0, 0.0),
                      QPointF((width() - 30.0) * (adTime / totalTime) + 30.0, sustainY),
                      QPointF((width() - 30.0) * ((adTime + 1.0) / totalTime) + 30.0, sustainY)};
  qreal dists[] = {PointDistance(ev->localPos(), points[0]), PointDistance(ev->localPos(), points[1]),
                   PointDistance(ev->localPos(), points[2])};

  int minDist = 0;
  if (dists[1] < dists[minDist] + 8.0) /* pt1 overlaps pt0, so include radius in test */
    minDist = 1;
  if (dists[2] < dists[minDist])
    minDist = 2;

  if (dists[minDist] < 10.0) {
    ++m_cycleIdx;
    m_dragPoint = minDist;
    mouseMoveEvent(ev);
  }
}

void ADSRView::mouseReleaseEvent(QMouseEvent* ev) { m_dragPoint = -1; }

void ADSRView::mouseMoveEvent(QMouseEvent* ev) {
  amuse::ITable& table = **m_node->m_obj;
  ADSRControls* ctrls = getEditor()->m_controls;

  qreal adTime = 0.0;
  qreal aTime = 0.0;
  qreal relTime = 0.0;
  if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
    amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
    adTime += adsr.getAttack();
    aTime = adsr.getAttack();
    adTime += adsr.getDecay();
    relTime = adsr.getRelease();
  } else if (table.Isa() == amuse::ITable::Type::ADSR) {
    amuse::ADSR& adsr = static_cast<amuse::ADSR&>(table);
    adTime += adsr.getAttack();
    aTime = adsr.getAttack();
    adTime += adsr.getDecay();
    relTime = adsr.getRelease();
  }
  qreal totalTime = adTime + 1.0 + relTime;

  if (m_dragPoint == 0) {
    qreal newAttack = std::max(0.0, (ev->localPos().x() - 30.0) / (width() - 30.0) * totalTime);
    qreal delta = newAttack - aTime;
    ctrls->setAttackAndDecay(newAttack, std::max(0.0, ctrls->m_decay->value() - delta), m_cycleIdx);
  } else if (m_dragPoint == 1) {
    qreal newDecay = std::max(0.0, (ev->localPos().x() - 30.0) * totalTime / (width() - 30.0) - aTime);
    qreal newSustain = (-ev->localPos().y() + (height() - 16.0)) / (height() - 16.0);
    ctrls->setDecayAndSustain(newDecay, newSustain * 100.0, m_cycleIdx);
  } else if (m_dragPoint == 2) {
    qreal newRelease = std::max(0.0, (width() - 30.0) * (adTime + 1.0) / (ev->localPos().x() - 30.0) - (adTime + 1.0));
    ctrls->setRelease(newRelease, m_cycleIdx);
    ctrls->m_release->setValue(newRelease);
  }
}

ADSRView::ADSRView(QWidget* parent) : QWidget(parent) {
  for (int i = 0; i < 11; ++i) {
    m_percentTexts[i].setText(QStringLiteral("%1%").arg(100 - i * 10));
    m_percentTexts[i].setTextOption(QTextOption(Qt::AlignVCenter | Qt::AlignRight));
    m_percentTexts[i].setTextWidth(28.0);
  }
  m_gridFont.setPointSize(8);
}

ADSREditor* ADSRControls::getEditor() const { return qobject_cast<ADSREditor*>(parentWidget()); }

void ADSRControls::loadData() {
  m_enableUpdate = false;

  amuse::ITable& table = **getEditor()->m_adsrView->m_node->m_obj;
  m_attack->setDisabled(false);
  m_decay->setDisabled(false);
  m_sustain->setDisabled(false);
  m_release->setDisabled(false);
  m_dls->setDisabled(false);
  if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
    amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
    m_attack->setValue(adsr.getAttack());
    m_decay->setValue(adsr.getDecay());
    m_sustain->setValue(adsr.getSustain() * 100.0);
    m_release->setValue(adsr.getRelease());
    m_dls->setChecked(true);
    m_velToAttackLab->setVisible(true);
    m_velToAttack->setVisible(true);
    m_velToAttack->setValue(adsr._getVelToAttack());
    m_keyToDecayLab->setVisible(true);
    m_keyToDecay->setVisible(true);
    m_keyToDecay->setValue(adsr._getKeyToDecay());
  } else if (table.Isa() == amuse::ITable::Type::ADSR) {
    amuse::ADSR& adsr = static_cast<amuse::ADSR&>(table);
    m_attack->setValue(adsr.getAttack());
    m_decay->setValue(adsr.getDecay());
    m_sustain->setValue(adsr.getSustain() * 100.0);
    m_release->setValue(adsr.getRelease());
    m_dls->setChecked(false);
    m_velToAttackLab->setVisible(false);
    m_velToAttack->setVisible(false);
    m_keyToDecayLab->setVisible(false);
    m_keyToDecay->setVisible(false);
  } else {
    unloadData();
  }

  m_enableUpdate = true;
}

void ADSRControls::unloadData() {
  m_attack->setDisabled(true);
  m_decay->setDisabled(true);
  m_sustain->setDisabled(true);
  m_release->setDisabled(true);
  m_dls->setDisabled(true);
  m_velToAttackLab->setVisible(false);
  m_velToAttack->setVisible(false);
  m_keyToDecayLab->setVisible(false);
  m_keyToDecay->setVisible(false);
}

class ADSRAttackUndoCommand : public EditorUndoCommand {
  double m_redoVal, m_undoVal;
  bool m_undid = false;

public:
  ADSRAttackUndoCommand(double redoVal, amuse::ObjToken<ProjectModel::ADSRNode> node)
  : EditorUndoCommand(node.get(), ADSRControls::tr("Change Attack")), m_redoVal(redoVal) {}
  void undo() {
    m_undid = true;
    amuse::ITable& table = **m_node.cast<ProjectModel::ADSRNode>()->m_obj;
    if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
      amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
      adsr.setAttack(m_undoVal);
    } else if (table.Isa() == amuse::ITable::Type::ADSR) {
      amuse::ADSR& adsr = static_cast<amuse::ADSR&>(table);
      adsr.setAttack(m_undoVal);
    }
    EditorUndoCommand::undo();
  }
  void redo() {
    amuse::ITable& table = **m_node.cast<ProjectModel::ADSRNode>()->m_obj;
    if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
      amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
      m_undoVal = adsr.getAttack();
      adsr.setAttack(m_redoVal);
    } else if (table.Isa() == amuse::ITable::Type::ADSR) {
      amuse::ADSR& adsr = static_cast<amuse::ADSR&>(table);
      m_undoVal = adsr.getAttack();
      adsr.setAttack(m_redoVal);
    }
    if (m_undid)
      EditorUndoCommand::redo();
  }
  bool mergeWith(const QUndoCommand* other) {
    if (other->id() == id()) {
      m_redoVal = static_cast<const ADSRAttackUndoCommand*>(other)->m_redoVal;
      return true;
    }
    return false;
  }
  int id() const { return int(Id::ADSRAttack); }
};

void ADSRControls::attackChanged(double val) {
  if (m_enableUpdate) {
    ADSRView* view = getEditor()->m_adsrView;
    g_MainWindow->pushUndoCommand(new ADSRAttackUndoCommand(val, view->m_node));
    view->update();
  }
}

class ADSRDecayUndoCommand : public EditorUndoCommand {
  double m_redoVal, m_undoVal;
  bool m_undid = false;

public:
  ADSRDecayUndoCommand(double redoVal, amuse::ObjToken<ProjectModel::ADSRNode> node)
  : EditorUndoCommand(node.get(), ADSRControls::tr("Change Decay")), m_redoVal(redoVal) {}
  void undo() {
    m_undid = true;
    amuse::ITable& table = **m_node.cast<ProjectModel::ADSRNode>()->m_obj;
    if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
      amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
      adsr.setDecay(m_undoVal);
    } else if (table.Isa() == amuse::ITable::Type::ADSR) {
      amuse::ADSR& adsr = static_cast<amuse::ADSR&>(table);
      adsr.setDecay(m_undoVal);
    }
    EditorUndoCommand::undo();
  }
  void redo() {
    amuse::ITable& table = **m_node.cast<ProjectModel::ADSRNode>()->m_obj;
    if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
      amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
      m_undoVal = adsr.getDecay();
      adsr.setDecay(m_redoVal);
    } else if (table.Isa() == amuse::ITable::Type::ADSR) {
      amuse::ADSR& adsr = static_cast<amuse::ADSR&>(table);
      m_undoVal = adsr.getDecay();
      adsr.setDecay(m_redoVal);
    }
    if (m_undid)
      EditorUndoCommand::redo();
  }
  bool mergeWith(const QUndoCommand* other) {
    if (other->id() == id()) {
      m_redoVal = static_cast<const ADSRDecayUndoCommand*>(other)->m_redoVal;
      return true;
    }
    return false;
  }
  int id() const { return int(Id::ADSRDecay); }
};

void ADSRControls::decayChanged(double val) {
  if (m_enableUpdate) {
    ADSRView* view = getEditor()->m_adsrView;
    g_MainWindow->pushUndoCommand(new ADSRDecayUndoCommand(val, view->m_node));
    view->update();
  }
}

class ADSRSustainUndoCommand : public EditorUndoCommand {
  double m_redoVal, m_undoVal;
  bool m_undid = false;

public:
  ADSRSustainUndoCommand(double redoVal, amuse::ObjToken<ProjectModel::ADSRNode> node)
  : EditorUndoCommand(node.get(), ADSRControls::tr("Change Sustain")), m_redoVal(redoVal) {}
  void undo() {
    m_undid = true;
    amuse::ITable& table = **m_node.cast<ProjectModel::ADSRNode>()->m_obj;
    if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
      amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
      adsr.setSustain(m_undoVal);
    } else if (table.Isa() == amuse::ITable::Type::ADSR) {
      amuse::ADSR& adsr = static_cast<amuse::ADSR&>(table);
      adsr.setSustain(m_undoVal);
    }
    EditorUndoCommand::undo();
  }
  void redo() {
    amuse::ITable& table = **m_node.cast<ProjectModel::ADSRNode>()->m_obj;
    if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
      amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
      m_undoVal = adsr.getSustain();
      adsr.setSustain(m_redoVal);
    } else if (table.Isa() == amuse::ITable::Type::ADSR) {
      amuse::ADSR& adsr = static_cast<amuse::ADSR&>(table);
      m_undoVal = adsr.getSustain();
      adsr.setSustain(m_redoVal);
    }
    if (m_undid)
      EditorUndoCommand::redo();
  }
  bool mergeWith(const QUndoCommand* other) {
    if (other->id() == id()) {
      m_redoVal = static_cast<const ADSRSustainUndoCommand*>(other)->m_redoVal;
      return true;
    }
    return false;
  }
  int id() const { return int(Id::ADSRSustain); }
};

void ADSRControls::sustainChanged(double val) {
  if (m_enableUpdate) {
    ADSRView* view = getEditor()->m_adsrView;
    g_MainWindow->pushUndoCommand(new ADSRSustainUndoCommand(val / 100.0, view->m_node));
    view->update();
  }
}

class ADSRAttackAndDecayUndoCommand : public EditorUndoCommand {
  double m_redoAttack, m_redoDecay;
  double m_undoAttack, m_undoDecay;
  uint64_t m_cycleCount;
  bool m_undid = false;

public:
  ADSRAttackAndDecayUndoCommand(double redoAttack, double redoDecay, uint64_t cycleCount,
                                amuse::ObjToken<ProjectModel::ADSRNode> node)
  : EditorUndoCommand(node.get(), ADSRControls::tr("Change Attack/Decay"))
  , m_redoAttack(redoAttack)
  , m_redoDecay(redoDecay)
  , m_cycleCount(cycleCount) {}
  void undo() {
    m_undid = true;
    amuse::ITable& table = **m_node.cast<ProjectModel::ADSRNode>()->m_obj;
    if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
      amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
      adsr.setAttack(m_undoAttack);
      adsr.setDecay(m_undoDecay);
    } else if (table.Isa() == amuse::ITable::Type::ADSR) {
      amuse::ADSR& adsr = static_cast<amuse::ADSR&>(table);
      adsr.setAttack(m_undoAttack);
      adsr.setDecay(m_undoDecay);
    }
    EditorUndoCommand::undo();
  }
  void redo() {
    amuse::ITable& table = **m_node.cast<ProjectModel::ADSRNode>()->m_obj;
    if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
      amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
      m_undoAttack = adsr.getAttack();
      m_undoDecay = adsr.getDecay();
      adsr.setAttack(m_redoAttack);
      adsr.setDecay(m_redoDecay);
    } else if (table.Isa() == amuse::ITable::Type::ADSR) {
      amuse::ADSR& adsr = static_cast<amuse::ADSR&>(table);
      m_undoAttack = adsr.getAttack();
      m_undoDecay = adsr.getDecay();
      adsr.setAttack(m_redoAttack);
      adsr.setDecay(m_redoDecay);
    }
    if (m_undid)
      EditorUndoCommand::redo();
  }
  bool mergeWith(const QUndoCommand* other) {
    if (other->id() == id() && m_cycleCount == static_cast<const ADSRAttackAndDecayUndoCommand*>(other)->m_cycleCount) {
      m_redoAttack = static_cast<const ADSRAttackAndDecayUndoCommand*>(other)->m_redoAttack;
      m_redoDecay = static_cast<const ADSRAttackAndDecayUndoCommand*>(other)->m_redoDecay;
      return true;
    }
    return false;
  }
  int id() const { return int(Id::ADSRAttackAndDecay); }
};

void ADSRControls::setAttackAndDecay(double attack, double decay, uint64_t cycleCount) {
  m_enableUpdate = false;
  m_attack->setValue(attack);
  m_decay->setValue(decay);
  m_enableUpdate = true;

  ADSRView* view = getEditor()->m_adsrView;
  g_MainWindow->pushUndoCommand(
      new ADSRAttackAndDecayUndoCommand(m_attack->value(), m_decay->value(), cycleCount, view->m_node));
  view->update();
}

class ADSRDecayAndSustainUndoCommand : public EditorUndoCommand {
  double m_redoDecay, m_redoSustain;
  double m_undoDecay, m_undoSustain;
  uint64_t m_cycleCount;
  bool m_undid = false;

public:
  ADSRDecayAndSustainUndoCommand(double redoDecay, double redoSustain, uint64_t cycleCount,
                                 amuse::ObjToken<ProjectModel::ADSRNode> node)
  : EditorUndoCommand(node.get(), ADSRControls::tr("Change Decay/Sustain"))
  , m_redoDecay(redoDecay)
  , m_redoSustain(redoSustain)
  , m_cycleCount(cycleCount) {}
  void undo() {
    m_undid = true;
    amuse::ITable& table = **m_node.cast<ProjectModel::ADSRNode>()->m_obj;
    if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
      amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
      adsr.setDecay(m_undoDecay);
      adsr.setSustain(m_undoSustain);
    } else if (table.Isa() == amuse::ITable::Type::ADSR) {
      amuse::ADSR& adsr = static_cast<amuse::ADSR&>(table);
      adsr.setDecay(m_undoDecay);
      adsr.setSustain(m_undoSustain);
    }
    EditorUndoCommand::undo();
  }
  void redo() {
    amuse::ITable& table = **m_node.cast<ProjectModel::ADSRNode>()->m_obj;
    if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
      amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
      m_undoDecay = adsr.getDecay();
      m_undoSustain = adsr.getSustain();
      adsr.setDecay(m_redoDecay);
      adsr.setSustain(m_redoSustain);
    } else if (table.Isa() == amuse::ITable::Type::ADSR) {
      amuse::ADSR& adsr = static_cast<amuse::ADSR&>(table);
      m_undoDecay = adsr.getDecay();
      m_undoSustain = adsr.getSustain();
      adsr.setDecay(m_redoDecay);
      adsr.setSustain(m_redoSustain);
    }
    if (m_undid)
      EditorUndoCommand::redo();
  }
  bool mergeWith(const QUndoCommand* other) {
    if (other->id() == id() &&
        m_cycleCount == static_cast<const ADSRDecayAndSustainUndoCommand*>(other)->m_cycleCount) {
      m_redoDecay = static_cast<const ADSRDecayAndSustainUndoCommand*>(other)->m_redoDecay;
      m_redoSustain = static_cast<const ADSRDecayAndSustainUndoCommand*>(other)->m_redoSustain;
      return true;
    }
    return false;
  }
  int id() const { return int(Id::ADSRDecayAndSustain); }
};

void ADSRControls::setDecayAndSustain(double decay, double sustain, uint64_t cycleCount) {
  m_enableUpdate = false;
  m_decay->setValue(decay);
  m_sustain->setValue(sustain);
  m_enableUpdate = true;

  ADSRView* view = getEditor()->m_adsrView;
  g_MainWindow->pushUndoCommand(
      new ADSRDecayAndSustainUndoCommand(m_decay->value(), m_sustain->value() / 100.0, cycleCount, view->m_node));
  view->update();
}

class ADSRReleaseUndoCommand : public EditorUndoCommand {
  double m_redoVal, m_undoVal;
  uint64_t m_cycleCount;
  bool m_undid = false;

public:
  ADSRReleaseUndoCommand(double redoVal, uint64_t cycleCount, amuse::ObjToken<ProjectModel::ADSRNode> node)
  : EditorUndoCommand(node.get(), ADSRControls::tr("Change Release")), m_redoVal(redoVal), m_cycleCount(cycleCount) {}
  void undo() {
    m_undid = true;
    amuse::ITable& table = **m_node.cast<ProjectModel::ADSRNode>()->m_obj;
    if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
      amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
      adsr.setRelease(m_undoVal);
    } else if (table.Isa() == amuse::ITable::Type::ADSR) {
      amuse::ADSR& adsr = static_cast<amuse::ADSR&>(table);
      adsr.setRelease(m_undoVal);
    }
    EditorUndoCommand::undo();
  }
  void redo() {
    amuse::ITable& table = **m_node.cast<ProjectModel::ADSRNode>()->m_obj;
    if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
      amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
      m_undoVal = adsr.getRelease();
      adsr.setRelease(m_redoVal);
    } else if (table.Isa() == amuse::ITable::Type::ADSR) {
      amuse::ADSR& adsr = static_cast<amuse::ADSR&>(table);
      m_undoVal = adsr.getRelease();
      adsr.setRelease(m_redoVal);
    }
    if (m_undid)
      EditorUndoCommand::redo();
  }
  bool mergeWith(const QUndoCommand* other) {
    if (other->id() == id() && m_cycleCount == static_cast<const ADSRReleaseUndoCommand*>(other)->m_cycleCount) {
      m_redoVal = static_cast<const ADSRReleaseUndoCommand*>(other)->m_redoVal;
      return true;
    }
    return false;
  }
  int id() const { return int(Id::ADSRRelease); }
};

void ADSRControls::setRelease(double release, uint64_t cycleCount) {
  m_enableUpdate = false;
  m_release->setValue(release);
  m_enableUpdate = true;

  ADSRView* view = getEditor()->m_adsrView;
  g_MainWindow->pushUndoCommand(new ADSRReleaseUndoCommand(m_release->value(), cycleCount, view->m_node));
  view->update();
}

void ADSRControls::releaseChanged(double val) {
  if (m_enableUpdate) {
    ADSRView* view = getEditor()->m_adsrView;
    g_MainWindow->pushUndoCommand(new ADSRReleaseUndoCommand(val, ~0ull, view->m_node));
    view->update();
  }
}

template <class T>
static std::unique_ptr<T> MakeAlternateVersion(amuse::ITable& table) {
  std::unique_ptr<T> ret = std::make_unique<T>();
  if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
    amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
    ret->setAttack(adsr.getAttack());
    ret->setDecay(adsr.getDecay());
    ret->setSustain(adsr.getSustain());
    ret->setRelease(adsr.getRelease());
  } else if (table.Isa() == amuse::ITable::Type::ADSR) {
    amuse::ADSR& adsr = static_cast<amuse::ADSR&>(table);
    ret->setAttack(adsr.getAttack());
    ret->setDecay(adsr.getDecay());
    ret->setSustain(adsr.getSustain());
    ret->setRelease(adsr.getRelease());
  }
  return ret;
}

class ADSRDLSUndoCommand : public EditorUndoCommand {
  bool m_redoVal;
  double m_redoVelToAttack, m_redoKeyToDecay;
  double m_undoVelToAttack, m_undoKeyToDecay;
  bool m_undid = false;

public:
  ADSRDLSUndoCommand(bool redoVal, double redoVelToAttack, double redoKeyToDecay,
                     amuse::ObjToken<ProjectModel::ADSRNode> node)
  : EditorUndoCommand(node.get(), ADSRControls::tr("Change DLS"))
  , m_redoVal(redoVal)
  , m_redoVelToAttack(redoVelToAttack)
  , m_redoKeyToDecay(redoKeyToDecay) {}
  void undo() {
    m_undid = true;
    std::unique_ptr<amuse::ITable>& table = *m_node.cast<ProjectModel::ADSRNode>()->m_obj;
    if ((table->Isa() == amuse::ITable::Type::ADSRDLS && !m_redoVal) ||
        (table->Isa() == amuse::ITable::Type::ADSR && m_redoVal))
      return;

    std::unique_ptr<amuse::ITable> oldTable = std::move(table);
    if (!m_redoVal) {
      table = MakeAlternateVersion<amuse::ADSRDLS>(*oldTable);
      static_cast<amuse::ADSRDLS&>(*table)._setVelToAttack(m_undoVelToAttack);
      static_cast<amuse::ADSRDLS&>(*table)._setKeyToDecay(m_undoKeyToDecay);
    } else {
      table = MakeAlternateVersion<amuse::ADSR>(*oldTable);
    }

    EditorUndoCommand::undo();
  }
  void redo() {
    std::unique_ptr<amuse::ITable>& table = *m_node.cast<ProjectModel::ADSRNode>()->m_obj;
    if ((table->Isa() == amuse::ITable::Type::ADSRDLS && m_redoVal) ||
        (table->Isa() == amuse::ITable::Type::ADSR && !m_redoVal))
      return;

    std::unique_ptr<amuse::ITable> oldTable = std::move(table);
    if (m_redoVal) {
      table = MakeAlternateVersion<amuse::ADSRDLS>(*oldTable);
      static_cast<amuse::ADSRDLS&>(*table)._setVelToAttack(m_redoVelToAttack);
      static_cast<amuse::ADSRDLS&>(*table)._setKeyToDecay(m_redoKeyToDecay);
    } else {
      if (oldTable->Isa() == amuse::ITable::Type::ADSRDLS) {
        m_undoVelToAttack = static_cast<amuse::ADSRDLS&>(*oldTable)._getVelToAttack();
        m_undoKeyToDecay = static_cast<amuse::ADSRDLS&>(*oldTable)._getKeyToDecay();
      }
      table = MakeAlternateVersion<amuse::ADSR>(*oldTable);
    }

    if (m_undid)
      EditorUndoCommand::redo();
  }
  bool mergeWith(const QUndoCommand* other) {
    if (other->id() == id()) {
      m_redoVal = static_cast<const ADSRDLSUndoCommand*>(other)->m_redoVal;
      m_redoVelToAttack = static_cast<const ADSRDLSUndoCommand*>(other)->m_redoVelToAttack;
      m_redoKeyToDecay = static_cast<const ADSRDLSUndoCommand*>(other)->m_redoKeyToDecay;
      return true;
    }
    return false;
  }
  int id() const { return int(Id::ADSRDLS); }
};

void ADSRControls::dlsStateChanged(int state) {
  if (m_enableUpdate) {
    m_velToAttackLab->setVisible(state == Qt::Checked);
    m_velToAttack->setVisible(state == Qt::Checked);
    m_keyToDecayLab->setVisible(state == Qt::Checked);
    m_keyToDecay->setVisible(state == Qt::Checked);

    ADSRView* view = getEditor()->m_adsrView;
    g_MainWindow->pushUndoCommand(
        new ADSRDLSUndoCommand(state == Qt::Checked, m_velToAttack->value(), m_keyToDecay->value(), view->m_node));
    view->update();
  }
}

class ADSRVelToAttackUndoCommand : public EditorUndoCommand {
  double m_redoVal, m_undoVal;
  bool m_undid = false;

public:
  ADSRVelToAttackUndoCommand(double redoVal, amuse::ObjToken<ProjectModel::ADSRNode> node)
  : EditorUndoCommand(node.get(), ADSRControls::tr("Change Vel To Attack")), m_redoVal(redoVal) {}
  void undo() {
    m_undid = true;
    amuse::ITable& table = **m_node.cast<ProjectModel::ADSRNode>()->m_obj;
    if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
      amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
      adsr._setVelToAttack(m_undoVal);
    }
    EditorUndoCommand::undo();
  }
  void redo() {
    amuse::ITable& table = **m_node.cast<ProjectModel::ADSRNode>()->m_obj;
    if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
      amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
      m_undoVal = adsr._getVelToAttack();
      adsr._setVelToAttack(m_redoVal);
    }
    if (m_undid)
      EditorUndoCommand::redo();
  }
  bool mergeWith(const QUndoCommand* other) {
    if (other->id() == id()) {
      m_redoVal = static_cast<const ADSRVelToAttackUndoCommand*>(other)->m_redoVal;
      return true;
    }
    return false;
  }
  int id() const { return int(Id::ADSRVelToAttack); }
};

void ADSRControls::velToAttackChanged(double val) {
  if (m_enableUpdate) {
    ADSRView* view = getEditor()->m_adsrView;
    g_MainWindow->pushUndoCommand(new ADSRVelToAttackUndoCommand(val, view->m_node));
    view->update();
  }
}

class ADSRKeyToDecayUndoCommand : public EditorUndoCommand {
  double m_redoVal, m_undoVal;
  bool m_undid = false;

public:
  ADSRKeyToDecayUndoCommand(double redoVal, amuse::ObjToken<ProjectModel::ADSRNode> node)
  : EditorUndoCommand(node.get(), ADSRControls::tr("Change Key To Decay")), m_redoVal(redoVal) {}
  void undo() {
    m_undid = true;
    amuse::ITable& table = **m_node.cast<ProjectModel::ADSRNode>()->m_obj;
    if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
      amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
      adsr._setKeyToDecay(m_undoVal);
    }
    EditorUndoCommand::undo();
  }
  void redo() {
    amuse::ITable& table = **m_node.cast<ProjectModel::ADSRNode>()->m_obj;
    if (table.Isa() == amuse::ITable::Type::ADSRDLS) {
      amuse::ADSRDLS& adsr = static_cast<amuse::ADSRDLS&>(table);
      m_undoVal = adsr._getKeyToDecay();
      adsr._setKeyToDecay(m_redoVal);
    }
    if (m_undid)
      EditorUndoCommand::redo();
  }
  bool mergeWith(const QUndoCommand* other) {
    if (other->id() == id()) {
      m_redoVal = static_cast<const ADSRKeyToDecayUndoCommand*>(other)->m_redoVal;
      return true;
    }
    return false;
  }
  int id() const { return int(Id::ADSRKeyToDecay); }
};

void ADSRControls::keyToDecayChanged(double val) {
  if (m_enableUpdate) {
    ADSRView* view = getEditor()->m_adsrView;
    g_MainWindow->pushUndoCommand(new ADSRKeyToDecayUndoCommand(val, view->m_node));
    view->update();
  }
}

ADSRControls::ADSRControls(QWidget* parent) : QFrame(parent) {
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  setFixedHeight(100);
  setFrameShape(QFrame::StyledPanel);
  setFrameShadow(QFrame::Sunken);
  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);

  QHBoxLayout* mainLayout = new QHBoxLayout;

  QGridLayout* leftLayout = new QGridLayout;

  QPalette palette = QWidget::palette();
  palette.setColor(QPalette::Base, palette.color(QPalette::Window));

  palette.setColor(QPalette::Text, Red);
  QLabel* lab = new QLabel(tr("Attack"));
  lab->setPalette(palette);
  leftLayout->addWidget(lab, 0, 0);
  m_attack = new QDoubleSpinBox;
  m_attack->setDisabled(true);
  m_attack->setRange(0, 65.535);
  m_attack->setDecimals(3);
  m_attack->setSingleStep(0.1);
  m_attack->setSuffix(tr(" sec"));
  m_attack->setPalette(palette);
  connect(m_attack, SIGNAL(valueChanged(double)), this, SLOT(attackChanged(double)));
  leftLayout->addWidget(m_attack, 1, 0);

  palette.setColor(QPalette::Text, Green);
  lab = new QLabel(tr("Decay"));
  lab->setPalette(palette);
  leftLayout->addWidget(lab, 0, 1);
  m_decay = new QDoubleSpinBox;
  m_decay->setDisabled(true);
  m_decay->setRange(0, 65.535);
  m_decay->setDecimals(3);
  m_decay->setSingleStep(0.1);
  m_decay->setSuffix(tr(" sec"));
  m_decay->setPalette(palette);
  connect(m_decay, SIGNAL(valueChanged(double)), this, SLOT(decayChanged(double)));
  leftLayout->addWidget(m_decay, 1, 1);

  palette.setColor(QPalette::Text, Qt::white);
  leftLayout->addWidget(new QLabel(tr("Sustain")), 0, 2);
  m_sustain = new QDoubleSpinBox;
  m_sustain->setDisabled(true);
  m_sustain->setRange(0.0, 100.0);
  m_sustain->setDecimals(3);
  m_sustain->setSuffix(tr(" %"));
  m_sustain->setPalette(palette);
  connect(m_sustain, SIGNAL(valueChanged(double)), this, SLOT(sustainChanged(double)));
  leftLayout->addWidget(m_sustain, 1, 2);

  palette.setColor(QPalette::Text, Blue);
  lab = new QLabel(tr("Release"));
  lab->setPalette(palette);
  leftLayout->addWidget(lab, 0, 3);
  m_release = new QDoubleSpinBox;
  m_release->setDisabled(true);
  m_release->setRange(0.0, 65.535);
  m_release->setDecimals(3);
  m_release->setSingleStep(0.1);
  m_release->setSuffix(tr(" sec"));
  m_release->setPalette(palette);
  connect(m_release, SIGNAL(valueChanged(double)), this, SLOT(releaseChanged(double)));
  leftLayout->addWidget(m_release, 1, 3);

  palette.setColor(QPalette::Text, Qt::white);
  leftLayout->addWidget(new QLabel(tr("DLS")), 0, 4);
  m_dls = new QCheckBox;
  m_dls->setPalette(palette);
  m_dls->setDisabled(true);
  m_dls->setChecked(false);
  connect(m_dls, SIGNAL(stateChanged(int)), this, SLOT(dlsStateChanged(int)));
  leftLayout->addWidget(m_dls, 1, 4);

  m_velToAttackLab = new QLabel(tr("Vel To Attack"));
  m_velToAttackLab->setVisible(false);
  leftLayout->addWidget(m_velToAttackLab, 0, 5);
  m_velToAttack = new QDoubleSpinBox;
  m_velToAttack->setVisible(false);
  m_velToAttack->setRange(0.0, 9999.999);
  m_velToAttack->setDecimals(3);
  m_velToAttack->setSingleStep(1.0);
  m_velToAttack->setPalette(palette);
  connect(m_velToAttack, SIGNAL(valueChanged(double)), this, SLOT(velToAttackChanged(double)));
  leftLayout->addWidget(m_velToAttack, 1, 5);

  m_keyToDecayLab = new QLabel(tr("Key To Decay"));
  m_keyToDecayLab->setVisible(false);
  leftLayout->addWidget(m_keyToDecayLab, 0, 6);
  m_keyToDecay = new QDoubleSpinBox;
  m_keyToDecay->setVisible(false);
  m_keyToDecay->setRange(0.0, 9999.999);
  m_keyToDecay->setDecimals(3);
  m_keyToDecay->setSingleStep(1.0);
  m_keyToDecay->setPalette(palette);
  connect(m_keyToDecay, SIGNAL(valueChanged(double)), this, SLOT(keyToDecayChanged(double)));
  leftLayout->addWidget(m_keyToDecay, 1, 6);

  leftLayout->setColumnMinimumWidth(0, 75);
  leftLayout->setColumnStretch(0, 1);
  leftLayout->setColumnMinimumWidth(1, 75);
  leftLayout->setColumnStretch(1, 1);
  leftLayout->setColumnMinimumWidth(2, 75);
  leftLayout->setColumnStretch(2, 1);
  leftLayout->setColumnMinimumWidth(3, 75);
  leftLayout->setColumnStretch(3, 1);
  leftLayout->setColumnMinimumWidth(4, 50);
  leftLayout->setColumnStretch(4, 0);
  leftLayout->setColumnMinimumWidth(5, 75);
  leftLayout->setColumnStretch(5, 1);
  leftLayout->setColumnMinimumWidth(6, 75);
  leftLayout->setColumnStretch(6, 1);
  leftLayout->setRowMinimumHeight(0, 22);
  leftLayout->setRowMinimumHeight(1, 37);
  leftLayout->setContentsMargins(10, 6, 0, 14);

  mainLayout->addLayout(leftLayout);
  mainLayout->addStretch();
  setLayout(mainLayout);
}

bool ADSREditor::loadData(ProjectModel::ADSRNode* node) {
  m_adsrView->loadData(node);
  m_controls->loadData();
  return true;
}

void ADSREditor::unloadData() {
  m_adsrView->unloadData();
  m_controls->unloadData();
}

ProjectModel::INode* ADSREditor::currentNode() const { return m_adsrView->currentNode(); }

ADSREditor::ADSREditor(QWidget* parent) : EditorWidget(parent), m_adsrView(new ADSRView), m_controls(new ADSRControls) {
  QVBoxLayout* layout = new QVBoxLayout;
  layout->setContentsMargins(QMargins());
  layout->setSpacing(1);
  layout->addWidget(m_adsrView);
  layout->addWidget(m_controls);
  setLayout(layout);
}
