#pragma once

#include "EditorWidget.hpp"
#include <QFrame>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QStaticText>

class ADSREditor;

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
  bool loadData(ProjectModel::ADSRNode* node);
  void unloadData() override;
  ProjectModel::INode* currentNode() const override;
};
