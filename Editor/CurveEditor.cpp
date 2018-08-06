#include "CurveEditor.hpp"
#include "MainWindow.hpp"
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPainter>
#include <QScriptValueIterator>
#include <QMouseEvent>

class CurveEditUndoCommand : public EditorUndoCommand
{
    uint8_t m_redoData[128];
    uint8_t m_undoData[128];
    bool m_usedExpr;
    bool m_undid = false;
public:
    CurveEditUndoCommand(const uint8_t* redoData, bool usedExpr,
                         amuse::ObjToken<ProjectModel::CurveNode> node)
    : EditorUndoCommand(node.get(), CurveControls::tr("Edit Curve")), m_usedExpr(usedExpr)
    {
        std::memcpy(m_redoData, redoData, 128);
    }
    void undo()
    {
        m_undid = true;
        amuse::ITable& table = **m_node.cast<ProjectModel::CurveNode>()->m_obj;
        if (table.Isa() == amuse::ITable::Type::Curve)
        {
            amuse::Curve& curve = static_cast<amuse::Curve&>(table);
            curve.data.resize(128);
            std::memcpy(curve.data.data(), m_undoData, 128);
        }
        EditorUndoCommand::undo();
    }
    void redo()
    {
        amuse::ITable& table = **m_node.cast<ProjectModel::CurveNode>()->m_obj;
        if (table.Isa() == amuse::ITable::Type::Curve)
        {
            amuse::Curve& curve = static_cast<amuse::Curve&>(table);
            curve.data.resize(128);
            std::memcpy(m_undoData, curve.data.data(), 128);
            std::memcpy(curve.data.data(), m_redoData, 128);
        }
        if (m_undid)
            EditorUndoCommand::redo();
    }
    bool mergeWith(const QUndoCommand* other)
    {
        if (other->id() == id() && !m_usedExpr && !static_cast<const CurveEditUndoCommand*>(other)->m_usedExpr)
        {
            std::memcpy(m_redoData, static_cast<const CurveEditUndoCommand*>(other)->m_redoData, 128);
            return true;
        }
        return false;
    }
    int id() const { return int(Id::CurveEdit); }
};

CurveEditor* CurveView::getEditor() const
{
    return qobject_cast<CurveEditor*>(parentWidget());
}

void CurveView::loadData(ProjectModel::CurveNode* node)
{
    m_node = node;
}

void CurveView::unloadData()
{
    m_node.reset();
}

ProjectModel::INode* CurveView::currentNode() const
{
    return m_node.get();
}

void CurveView::paintEvent(QPaintEvent* ev)
{
    if (!m_node)
        return;
    amuse::ITable& table = **m_node->m_obj;
    if (table.Isa() != amuse::ITable::Type::Curve)
        return;
    amuse::Curve& curve = static_cast<amuse::Curve&>(table);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    qreal deviceRatio = devicePixelRatioF();
    qreal penWidth = std::max(std::floor(deviceRatio), 1.0) / deviceRatio;

    painter.setPen(QPen(QColor(127, 127, 127), penWidth));
    painter.setFont(m_gridFont);
    qreal yIncrement = (height() - 16.0) / 10.0;
    for (int i = 0; i < 11; ++i)
    {
        qreal thisY = i * yIncrement;
        qreal textY = thisY - (i == 0 ? 2.0 : (i == 10 ? 16.0 : 8.0));
        painter.drawStaticText(QPointF(0.0, textY), m_percentTexts[i]);
        painter.drawLine(QPointF(30.0, thisY), QPointF(width(), thisY));
    }

    qreal xIncrement = (width() - 30.0) / 10.0;
    for (int i = 0; i < 11; ++i)
    {
        qreal thisX = i * xIncrement + 30.0;
        qreal textX = thisX - (i == 10 ? 30.0 : 15.0);
        painter.drawStaticText(QPointF(textX, height() - 16.0), m_percentTextsCenter[i]);
        painter.drawLine(QPointF(thisX, 0.0), QPointF(thisX, height() - 16.0));
    }

    int i;
    xIncrement = (width() - 30.0) / 127.0;
    QPointF lastPt;
    painter.setPen(QPen(Qt::white, penWidth * 3.0));
    for (i = 0; i < curve.data.size(); ++i)
    {
        QPointF thisPt(i * xIncrement + 30.0, (height() - 16.0) - (height() - 16.0) * (curve.data[i] / 127.0));
        if (i)
            painter.drawLine(lastPt, thisPt);
        lastPt = thisPt;
    }
    for (; i < 128; ++i)
    {
        QPointF thisPt(i * xIncrement + 30.0, height() - 16.0);
        if (i)
            painter.drawLine(lastPt, thisPt);
        lastPt = thisPt;
    }
}

void CurveView::mousePressEvent(QMouseEvent* ev)
{
    mouseMoveEvent(ev);
}

void CurveView::mouseMoveEvent(QMouseEvent* ev)
{
    CurveView* view = getEditor()->m_curveView;
    amuse::ITable& table = **view->m_node->m_obj;
    if (table.Isa() != amuse::ITable::Type::Curve)
        return;
    amuse::Curve& curve = static_cast<amuse::Curve&>(table);

    qreal xIncrement = (width() - 30.0) / 127.0;
    int idx = int(std::round((ev->localPos().x() - 30.0) / xIncrement));
    if (idx < 0 || idx > 127)
        return;
    int val = 127 - amuse::clamp(0, int(std::round(ev->localPos().y() / (height() - 16.0) * 127.0)), 127);

    curve.data.resize(128);
    uint8_t newData[128];
    std::memcpy(newData, curve.data.data(), 128);
    newData[idx] = uint8_t(val);

    g_MainWindow->pushUndoCommand(new CurveEditUndoCommand(newData, false, m_node));
    update();
}

CurveView::CurveView(QWidget* parent)
: QWidget(parent)
{
    for (int i = 0; i < 11; ++i)
    {
        m_percentTexts[i].setText(QStringLiteral("%1%").arg(100 - i * 10));
        m_percentTexts[i].setTextOption(QTextOption(Qt::AlignVCenter | Qt::AlignRight));
        m_percentTexts[i].setTextWidth(28.0);
        m_percentTextsCenter[i].setText(QStringLiteral("%1%").arg(i * 10));
        m_percentTextsCenter[i].setTextOption(QTextOption(Qt::AlignBaseline | Qt::AlignCenter));
        m_percentTextsCenter[i].setTextWidth(28.0);
    }
    m_gridFont.setPointSize(8);
}

CurveEditor* CurveControls::getEditor() const
{
    return qobject_cast<CurveEditor*>(parentWidget());
}

void CurveControls::loadData()
{
    m_lineEdit->setDisabled(false);
    m_setExpr->setDisabled(false);
}

void CurveControls::unloadData()
{
    m_lineEdit->setDisabled(true);
    m_setExpr->setDisabled(true);
}

void CurveControls::exprCommit()
{
    CurveView* view = getEditor()->m_curveView;
    amuse::ITable& table = **view->m_node->m_obj;
    if (table.Isa() != amuse::ITable::Type::Curve)
        return;
    amuse::Curve& curve = static_cast<amuse::Curve&>(table);

    QScriptSyntaxCheckResult res = QScriptEngine::checkSyntax(m_lineEdit->text());
    if (res.state() != QScriptSyntaxCheckResult::Valid)
    {
        m_errLabel->setText(res.errorMessage());
        return;
    }
    m_errLabel->setText(QString());

    curve.data.resize(128);
    uint8_t newData[128];
    std::memcpy(newData, curve.data.data(), 128);
    QScriptProgram prog(m_lineEdit->text());
    bool notANumber = false;
    for (int i = 0; i < 128; ++i)
    {
        m_engine.globalObject().setProperty(QStringLiteral("x"), i / 127.0);
        QScriptValue val = m_engine.evaluate(prog);
        if (val.isNumber())
        {
            newData[i] = uint8_t(amuse::clamp(0, int(std::round(val.toNumber() * 127.0)), 127));
        }
        else
        {
            notANumber = true;
            newData[i] = 0;
        }
    }
    if (notANumber)
        m_errLabel->setText(tr("Did not evaluate as a number"));

    g_MainWindow->pushUndoCommand(new CurveEditUndoCommand(newData, true, view->m_node));
    view->update();
}

void CurveControls::resizeEvent(QResizeEvent* ev)
{
    m_errLabel->setGeometry(22, 78, width() - 44, 14);
}

CurveControls::CurveControls(QWidget* parent)
: QFrame(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setFixedHeight(100);
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Sunken);
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);

    QHBoxLayout* mainLayout = new QHBoxLayout;

    QGridLayout* leftLayout = new QGridLayout;

    leftLayout->addWidget(new QLabel(tr("Expression")), 0, 0);
    m_lineEdit = new QLineEdit;
    m_lineEdit->setDisabled(true);
    QPalette palette = m_lineEdit->palette();
    palette.setColor(QPalette::Base, palette.color(QPalette::Background));
    m_lineEdit->setPalette(palette);
    connect(m_lineEdit, SIGNAL(returnPressed()), this, SLOT(exprCommit()));
    leftLayout->addWidget(m_lineEdit, 1, 0);

    m_setExpr = new QPushButton(tr("Apply"));
    m_setExpr->setDisabled(true);
    connect(m_setExpr, SIGNAL(clicked(bool)), this, SLOT(exprCommit()));
    leftLayout->addWidget(m_setExpr, 1, 1);

    m_errLabel = new QLabel(this);
    QFont font = m_errLabel->font();
    font.setPointSize(8);
    m_errLabel->setFont(font);
    palette.setColor(QPalette::Text, Qt::red);
    m_errLabel->setPalette(palette);

    leftLayout->setColumnMinimumWidth(0, 500);
    leftLayout->setColumnStretch(0, 1);
    leftLayout->setColumnMinimumWidth(1, 75);
    leftLayout->setColumnStretch(1, 0);
    leftLayout->setRowMinimumHeight(0, 22);
    leftLayout->setRowMinimumHeight(1, 37);
    leftLayout->setContentsMargins(10, 6, 10, 14);

    mainLayout->addLayout(leftLayout);
    setLayout(mainLayout);

    QScriptValueIterator it(m_engine.globalObject().property(QStringLiteral("Math")));
    QString docStr = tr("Expression interpreter mapping x:[0,1] to y:[0,1] with the following constants and functions available:\n");
    bool needsComma = false;
    while (it.hasNext())
    {
        it.next();
        m_engine.globalObject().setProperty(it.name(), it.value());
        if (needsComma)
            docStr += QStringLiteral(", ");
        needsComma = true;
        docStr += it.name();
    }
    m_lineEdit->setToolTip(docStr);
}

bool CurveEditor::loadData(ProjectModel::CurveNode* node)
{
    m_curveView->loadData(node);
    m_controls->loadData();
    return true;
}

void CurveEditor::unloadData()
{
    m_curveView->unloadData();
    m_controls->unloadData();
}

ProjectModel::INode* CurveEditor::currentNode() const
{
    return m_curveView->currentNode();
}

CurveEditor::CurveEditor(QWidget* parent)
: EditorWidget(parent), m_curveView(new CurveView), m_controls(new CurveControls)
{
    QVBoxLayout* layout = new QVBoxLayout;
    layout->setContentsMargins(QMargins());
    layout->setSpacing(1);
    layout->addWidget(m_curveView);
    layout->addWidget(m_controls);
    setLayout(layout);
}
