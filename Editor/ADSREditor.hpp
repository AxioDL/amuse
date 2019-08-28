#pragma once

#include <cstdint>
#include <vector>

#include <QFrame>
#include <QStaticText>

#include "EditorWidget.hpp"
#include "ProjectModel.hpp"

#include <amuse/Common.hpp>

class ADSREditor;

class QCheckBox;
class QDoubleSpinBox;
class QLabel;

class ADSRView : public QWidget {
  Q_OBJECT
  friend class ADSRControls;
  amuse::ObjToken<ProjectModel::ADSRNode> m_node;
  QFont m_gridFont;
  QStaticText m_percentTexts[11];
  std::vector<QStaticText> m_timeTexts;
  int m_dragPoint = -1;
  uint64_t m_cycleIdx = 0;
  ADSREditor* getEditor() const;

public:
  explicit ADSRView(QWidget* parent = Q_NULLPTR);
  ~ADSRView() override;

  void loadData(ProjectModel::ADSRNode* node);
  void unloadData();
  ProjectModel::INode* currentNode() const;

  void paintEvent(QPaintEvent* ev) override;
  void mousePressEvent(QMouseEvent* ev) override;
  void mouseReleaseEvent(QMouseEvent* ev) override;
  void mouseMoveEvent(QMouseEvent* ev) override;
};

class ADSRControls : public QFrame {
  Q_OBJECT
  friend class ADSRView;
  QDoubleSpinBox* m_attack;
  QDoubleSpinBox* m_decay;
  QDoubleSpinBox* m_sustain;
  QDoubleSpinBox* m_release;
  QCheckBox* m_dls;
  QLabel* m_velToAttackLab;
  QDoubleSpinBox* m_velToAttack;
  QLabel* m_keyToDecayLab;
  QDoubleSpinBox* m_keyToDecay;
  bool m_enableUpdate = true;
  ADSREditor* getEditor() const;
  void setAttackAndDecay(double attack, double decay, uint64_t cycleCount);
  void setDecayAndSustain(double decay, double sustain, uint64_t cycleCount);
  void setRelease(double release, uint64_t cycleCount);

public:
  explicit ADSRControls(QWidget* parent = Q_NULLPTR);
  ~ADSRControls() override;

  void loadData();
  void unloadData();

public slots:
  void attackChanged(double val);
  void decayChanged(double val);
  void sustainChanged(double val);
  void releaseChanged(double val);
  void dlsStateChanged(int state);
  void velToAttackChanged(double val);
  void keyToDecayChanged(double val);
};

class ADSREditor : public EditorWidget {
  Q_OBJECT
  friend class ADSRView;
  friend class ADSRControls;
  ADSRView* m_adsrView;
  ADSRControls* m_controls;

public:
  explicit ADSREditor(QWidget* parent = Q_NULLPTR);
  ~ADSREditor() override;

  bool loadData(ProjectModel::ADSRNode* node);
  void unloadData() override;
  ProjectModel::INode* currentNode() const override;
};
