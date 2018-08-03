#ifndef AMUSE_SAMPLE_EDITOR_HPP
#define AMUSE_SAMPLE_EDITOR_HPP

#include "EditorWidget.hpp"
#include "ProjectModel.hpp"
#include <QScrollArea>
#include <QSlider>
#include <QCheckBox>
#include <QSpinBox>

class SampleEditor;

class SampleView : public QWidget
{
    Q_OBJECT
    friend class SampleControls;
    qreal m_baseSamplesPerPx = 100.0;
    qreal m_samplesPerPx = 100.0;
    qreal m_zoomFactor = 1.0;
    amuse::ObjToken<ProjectModel::SampleNode> m_node;
    amuse::ObjToken<amuse::SampleEntryData> m_sample;
    amuse::ObjToken<amuse::SoundMacro> m_playbackMacro;
    const unsigned char* m_sampleData = nullptr;
    qreal m_curSamplePos = 0.0;
    int16_t m_prev1 = 0;
    int16_t m_prev2 = 0;
    QFont m_rulerFont;
    int m_displaySamplePos = -1;
    enum class DragState
    {
        None,
        Start,
        End
    };
    DragState m_dragState = DragState::None;
    void seekToSample(qreal sample);
    std::pair<std::pair<qreal, qreal>, std::pair<qreal, qreal>> iterateSampleInterval(qreal interval);
    void calculateSamplesPerPx();
    SampleEditor* getEditor() const;
public:
    explicit SampleView(QWidget* parent = Q_NULLPTR);
    bool loadData(ProjectModel::SampleNode* node);
    void unloadData();
    ProjectModel::INode* currentNode() const;
    amuse::SampleEntryData* entryData() const;
    const amuse::SoundMacro* soundMacro() const;
    void setSamplePos(int pos);
    void updateSampleRange(int oldSamp, int newSamp);

    void paintEvent(QPaintEvent* ev);
    void resetZoom();
    void setZoom(int zVal);
    void showEvent(QShowEvent* ev);
    void mousePressEvent(QMouseEvent* ev);
    void mouseReleaseEvent(QMouseEvent* ev);
    void mouseMoveEvent(QMouseEvent* ev);
    void wheelEvent(QWheelEvent* ev);
};

class SampleControls : public QFrame
{
    Q_OBJECT
    QString m_path;
    QSlider* m_zoomSlider;
    QCheckBox* m_loopCheck;
    QSpinBox* m_loopStart;
    QSpinBox* m_loopEnd;
    QSpinBox* m_basePitch;
    QPushButton* m_makeOtherVersion;
    QMetaObject::Connection m_makeOtherConn;
    QPushButton* m_showInBrowser;
    bool m_enableUpdate = true;
    bool m_enableFileWrite = true;
public:
    SampleControls(QWidget* parent = Q_NULLPTR);
    void doFileWrite();
    void setFileWrite(bool w);
public slots:
    void zoomSliderChanged(int val);
    void loopStateChanged(int state);
    void startValueChanged(int val);
    void endValueChanged(int val);
    void pitchValueChanged(int val);
    void makeWAVVersion();
    void makeCompressedVersion();
    void showInBrowser();
    void updateFileState();

    void setLoopStartSample(int sample) { m_loopStart->setValue(sample); }
    void setLoopEndSample(int sample) { m_loopEnd->setValue(sample); }

    void loadData(bool reset);
    void unloadData();
};

class SampleEditor : public EditorWidget
{
    Q_OBJECT
    friend class SampleView;
    friend class SampleControls;
    friend class SampLoopUndoCommand;
    friend class SampPitchUndoCommand;
    QScrollArea* m_scrollArea;
    SampleView* m_sampleView;
    SampleControls* m_controls;
public:
    explicit SampleEditor(QWidget* parent = Q_NULLPTR);
    bool loadData(ProjectModel::SampleNode* node);
    void unloadData();
    ProjectModel::INode* currentNode() const;
    const amuse::SoundMacro* soundMacro() const;
    void setSamplePos(int pos);

    void resizeEvent(QResizeEvent* ev);
};


#endif //AMUSE_SAMPLE_EDITOR_HPP
