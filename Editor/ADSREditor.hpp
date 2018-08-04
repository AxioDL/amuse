#ifndef AMUSE_ADSR_EDITOR_HPP
#define AMUSE_ADSR_EDITOR_HPP

#include "EditorWidget.hpp"
#include <QFrame>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QStaticText>

class ADSREditor;

class ADSRView : public QWidget
{
Q_OBJECT
    friend class ADSRControls;
    amuse::ObjToken<ProjectModel::ADSRNode> m_node;
    QFont m_gridFont;
    QStaticText m_percentTexts[11];
    std::vector<QStaticText> m_timeTexts;
    int m_dragPoint = -1;
    ADSREditor* getEditor() const;
public:
    explicit ADSRView(QWidget* parent = Q_NULLPTR);
    void loadData(ProjectModel::ADSRNode* node);
    void unloadData();
    ProjectModel::INode* currentNode() const;

    void paintEvent(QPaintEvent* ev);
    void mousePressEvent(QMouseEvent* ev);
    void mouseReleaseEvent(QMouseEvent* ev);
    void mouseMoveEvent(QMouseEvent* ev);
};

class ADSRControls : public QFrame
{
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
    void setAttackAndDecay(double attack, double decay);
    void setDecayAndSustain(double decay, double sustain);
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

class ADSREditor : public EditorWidget
{
Q_OBJECT
    friend class ADSRView;
    friend class ADSRControls;
    ADSRView* m_adsrView;
    ADSRControls* m_controls;
public:
    explicit ADSREditor(QWidget* parent = Q_NULLPTR);
    bool loadData(ProjectModel::ADSRNode* node);
    void unloadData();
    ProjectModel::INode* currentNode() const;
};


#endif //AMUSE_ADSR_EDITOR_HPP
