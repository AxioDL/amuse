#ifndef AMUSE_SAMPLE_EDITOR_HPP
#define AMUSE_SAMPLE_EDITOR_HPP

#include "EditorWidget.hpp"
#include "ProjectModel.hpp"

class SampleView : public QWidget
{
    Q_OBJECT
    qreal m_samplesPerPx = 100.0;
    amuse::ObjToken<ProjectModel::SampleNode> m_node;
    amuse::ObjToken<amuse::SampleEntryData> m_sample;
    const unsigned char* m_sampleData = nullptr;
    qreal m_curSamplePos = 0.0;
    int16_t m_prev1 = 0;
    int16_t m_prev2 = 0;
    void seekToSample(qreal sample);
    std::pair<std::pair<qreal, qreal>, std::pair<qreal, qreal>> iterateSampleInterval(qreal interval);
public:
    explicit SampleView(QWidget* parent = Q_NULLPTR);
    void loadData(ProjectModel::SampleNode* node);
    void unloadData();
    ProjectModel::INode* currentNode() const;

    void paintEvent(QPaintEvent* ev);
    void mousePressEvent(QMouseEvent* ev);
    void mouseReleaseEvent(QMouseEvent* ev);
    void mouseMoveEvent(QMouseEvent* ev);
};

class SampleEditor : public EditorWidget
{
    Q_OBJECT
    SampleView* m_sampleView;
public:
    explicit SampleEditor(QWidget* parent = Q_NULLPTR);
    bool loadData(ProjectModel::SampleNode* node);
    void unloadData();
    ProjectModel::INode* currentNode() const;
};


#endif //AMUSE_SAMPLE_EDITOR_HPP
