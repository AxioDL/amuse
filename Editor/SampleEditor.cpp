#include "SampleEditor.hpp"
#include "MainWindow.hpp"
#include "amuse/DSPCodec.hpp"
#include <QPainter>
#include <QPaintEvent>

void SampleView::seekToSample(qreal sample)
{
    sample = std::min(sample, qreal(m_sample->getNumSamples()));

    if (m_sample->isFormatDSP())
    {
        if (sample < m_curSamplePos)
        {
            m_prev1 = m_prev2 = 0;
            m_curSamplePos = 0.0;
        }

        uint32_t startBlock = uint32_t(m_curSamplePos) / 14;
        uint32_t startRem = uint32_t(m_curSamplePos) % 14;
        uint32_t endBlock = uint32_t(sample) / 14;
        uint32_t endRem = uint32_t(sample) % 14;

        if (startRem)
            DSPDecompressFrameRangedStateOnly(m_sampleData + 8 * startBlock, m_sample->m_ADPCMParms.dsp.m_coefs,
                                              &m_prev1, &m_prev2, startRem, startBlock == endBlock ? endRem : 14);

        for (uint32_t b = startBlock; b < endBlock; ++b)
            DSPDecompressFrameStateOnly(m_sampleData + 8 * b, m_sample->m_ADPCMParms.dsp.m_coefs,
                                        &m_prev1, &m_prev2, 14);

        if (endRem)
            DSPDecompressFrameStateOnly(m_sampleData + 8 * endBlock, m_sample->m_ADPCMParms.dsp.m_coefs,
                                        &m_prev1, &m_prev2, endRem);
    }

    m_curSamplePos = sample;
}

std::pair<std::pair<qreal, qreal>, std::pair<qreal, qreal>> SampleView::iterateSampleInterval(qreal interval)
{
    std::pair<std::pair<qreal, qreal>, std::pair<qreal, qreal>> ret = {{0.0, 1.0}, {0.0, -1.0}}; // min,max avg,peak
    qreal avg = 0.0;
    qreal div = 0.0;
    qreal endSample = std::min(m_curSamplePos + interval, qreal(m_sample->getNumSamples()));

    auto accumulate = [&ret, &avg, &div](int16_t sample)
    {
        qreal sampleF = sample / 32768.0;
        avg += sampleF;
        div += 1.0;
        ret.first.second = std::min(ret.first.second, sampleF);
        ret.second.second = std::max(ret.second.second, sampleF);
    };

    if (m_sample->isFormatDSP())
    {
        uint32_t startBlock = uint32_t(m_curSamplePos) / 14;
        uint32_t startRem = uint32_t(m_curSamplePos) % 14;
        uint32_t endBlock = uint32_t(endSample) / 14;
        uint32_t endRem = uint32_t(endSample) % 14;

        int16_t sampleBlock[14];

        if (startRem)
        {
            uint32_t end = startBlock == endBlock ? endRem : 14;
            DSPDecompressFrameRanged(sampleBlock, m_sampleData + 8 * startBlock, m_sample->m_ADPCMParms.dsp.m_coefs,
                                     &m_prev1, &m_prev2, startRem, end);
            for (int s = 0; s < end - startRem; ++s)
                accumulate(sampleBlock[s]);
        }

        for (uint32_t b = startBlock; b < endBlock; ++b)
        {
            DSPDecompressFrame(sampleBlock, m_sampleData + 8 * b, m_sample->m_ADPCMParms.dsp.m_coefs,
                               &m_prev1, &m_prev2, 14);
            for (int s = 0; s < 14; ++s)
                accumulate(sampleBlock[s]);
        }

        if (endRem)
        {
            DSPDecompressFrame(sampleBlock, m_sampleData + 8 * endBlock, m_sample->m_ADPCMParms.dsp.m_coefs,
                               &m_prev1, &m_prev2, endRem);
            for (int s = 0; s < endRem; ++s)
                accumulate(sampleBlock[s]);
        }
    }
    else if (m_sample->getSampleFormat() == amuse::SampleFormat::PCM_PC)
    {
        for (uint32_t s = uint32_t(m_curSamplePos); s < uint32_t(endSample); ++s)
            accumulate(reinterpret_cast<const int16_t*>(m_sampleData)[s]);
    }

    m_curSamplePos = endSample;

    if (div == 0.0)
        return {};

    avg /= div;

    if (avg > 0.0)
    {
        ret.first.first = ret.first.second;
        ret.second.first = avg;
    }
    else
    {
        ret.first.first = avg;
        ret.second.first = ret.second.second;
    }

    return ret;
}

void SampleView::paintEvent(QPaintEvent* ev)
{
    QPainter painter(this);
    if (m_sample)
    {
        m_samplesPerPx = m_sample->getNumSamples() / (width() * devicePixelRatioF());

        qreal rectStart = ev->rect().x();
        qreal startSample = rectStart * m_samplesPerPx;
        qreal deviceWidth = ev->rect().width() * devicePixelRatioF();
        qreal increment = 1.0 / devicePixelRatioF();
        qreal deviceSamplesPerPx = m_samplesPerPx / devicePixelRatioF();

        QPen peakPen(QColor(255, 147, 41), increment);
        QPen avgPen(QColor(254, 177, 68), increment);

        qreal scale = -height() / 2.0;
        qreal trans = height() / 2.0;

        seekToSample(startSample);

        for (qreal i = 0.0; i < deviceWidth; i += increment)
        {
            if (m_curSamplePos + deviceSamplesPerPx > m_sample->getNumSamples())
                break;
            auto avgPeak = iterateSampleInterval(deviceSamplesPerPx);
            painter.setPen(peakPen);
            painter.drawLine(QPointF(rectStart + i, avgPeak.first.second * scale + trans),
                             QPointF(rectStart + i, avgPeak.second.second * scale + trans));
            painter.setPen(avgPen);
            painter.drawLine(QPointF(rectStart + i, avgPeak.first.first * scale + trans),
                             QPointF(rectStart + i, avgPeak.second.first * scale + trans));
        }
    }
}

void SampleView::mousePressEvent(QMouseEvent* ev)
{

}

void SampleView::mouseReleaseEvent(QMouseEvent* ev)
{

}

void SampleView::mouseMoveEvent(QMouseEvent* ev)
{

}

void SampleView::loadData(ProjectModel::SampleNode* node)
{
    m_node = node;
    ProjectModel::GroupNode* group = g_MainWindow->projectModel()->getGroupNode(m_node.get());
    std::tie(m_sample, m_sampleData) = group->getAudioGroup()->getSampleData(m_node->id(), m_node->m_obj.get());
    update();
}

void SampleView::unloadData()
{
    m_sample.reset();
    update();
}

ProjectModel::INode* SampleView::currentNode() const
{
    return m_node.get();
}

SampleView::SampleView(QWidget* parent)
: QWidget(parent)
{}

bool SampleEditor::loadData(ProjectModel::SampleNode* node)
{
    m_sampleView->loadData(node);
    return true;
}

void SampleEditor::unloadData()
{
    m_sampleView->unloadData();
}

ProjectModel::INode* SampleEditor::currentNode() const
{
    return m_sampleView->currentNode();
}

SampleEditor::SampleEditor(QWidget* parent)
: EditorWidget(parent), m_sampleView(new SampleView)
{
    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(m_sampleView);
    setLayout(layout);
}
