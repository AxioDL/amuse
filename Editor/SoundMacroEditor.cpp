#include "SoundMacroEditor.hpp"
#include "MainWindow.hpp"
#include <QPainter>
#include <QPropertyAnimation>
#include <QGridLayout>
#include <QLineEdit>
#include <QSplitter>
#include <QScrollArea>
#include <QScrollBar>
#include <QApplication>
#include <QCheckBox>

FieldProjectNode::FieldProjectNode(ProjectModel::CollectionNode* collection, QWidget* parent)
: FieldComboBox(parent), m_collection(collection)
{
    ProjectModel* model = g_MainWindow->projectModel();
    setModel(model->getNullProxy());
    setRootModelIndex(model->getNullProxy()->mapFromSource(model->index(collection)));
}

TargetButton::TargetButton(QWidget* parent)
: QPushButton(parent)
{
    QIcon targetIcon(QStringLiteral(":/icons/IconSoundMacroTarget.svg"));
    targetIcon.addFile(QStringLiteral(":/icons/IconSoundMacroTargetDisabled.svg"), QSize(), QIcon::Disabled);
    setIcon(targetIcon);
    setToolTip(tr("Set step with target click"));
    setFixedSize(29, 29);
}

SoundMacroEditor* FieldSoundMacroStep::getEditor() const
{
    return qobject_cast<SoundMacroEditor*>(
            parentWidget()->parentWidget()->parentWidget()->
            parentWidget()->parentWidget()->parentWidget()->parentWidget());
}

SoundMacroListing* FieldSoundMacroStep::getListing() const
{
    return qobject_cast<SoundMacroListing*>(
            parentWidget()->parentWidget()->parentWidget());
}

void FieldSoundMacroStep::targetPressed()
{
    ProjectModel::SoundMacroNode* node = nullptr;
    if (m_macroField)
    {
        int val = m_macroField->currentIndex();
        if (val != 0)
            node = static_cast<ProjectModel::SoundMacroNode*>(m_macroField->collection()->nodeOfIndex(val - 1));
    }

    if (!m_macroField || node == getListing()->currentNode())
        if (SoundMacroEditor* editor = getEditor())
            editor->beginStepTarget(this);
}

void FieldSoundMacroStep::updateMacroField()
{
    if (!m_macroField)
    {
        int numCmds = int(static_cast<ProjectModel::SoundMacroNode*>(
            getListing()->currentNode())->m_obj->m_cmds.size());
        m_spinBox.setMaximum(numCmds - 1);
        m_spinBox.setDisabled(false);
        m_targetButton.setDisabled(false);
        return;
    }

    int val = m_macroField->currentIndex();
    if (val == 0)
    {
        m_spinBox.setValue(0);
        m_spinBox.setDisabled(true);
        m_targetButton.setDisabled(true);
    }
    else
    {
        ProjectModel::SoundMacroNode* node = static_cast<ProjectModel::SoundMacroNode*>(
            m_macroField->collection()->nodeOfIndex(val - 1));
        int numCmds = int(node->m_obj->m_cmds.size());
        m_spinBox.setMaximum(numCmds - 1);
        m_spinBox.setDisabled(false);
        m_targetButton.setEnabled(node == getListing()->currentNode());
    }
}

void FieldSoundMacroStep::setIndex(int index)
{
    m_targetButton.setDown(false);
    m_spinBox.setValue(index);
    if (SoundMacroEditor* editor = getEditor())
        editor->endStepTarget();
}

void FieldSoundMacroStep::cancel()
{
    m_targetButton.setDown(false);
    if (SoundMacroEditor* editor = getEditor())
        editor->endStepTarget();
}

FieldSoundMacroStep::~FieldSoundMacroStep()
{
    if (SoundMacroEditor* editor = getEditor())
        if (editor->m_targetField == this)
            editor->endStepTarget();
}

FieldSoundMacroStep::FieldSoundMacroStep(FieldProjectNode* macroField, QWidget* parent)
: QWidget(parent), m_macroField(macroField)
{
    QHBoxLayout* layout = new QHBoxLayout;
    layout->setContentsMargins(QMargins());
    layout->setSpacing(0);
    layout->addWidget(&m_spinBox);
    layout->addWidget(&m_targetButton);
    m_spinBox.setMinimum(0);
    m_spinBox.setDisabled(true);
    m_targetButton.setDisabled(true);
    connect(&m_spinBox, SIGNAL(valueChanged(int)), this, SIGNAL(valueChanged(int)));
    connect(&m_targetButton, SIGNAL(pressed()), this, SLOT(targetPressed()));
    if (macroField)
        connect(macroField, SIGNAL(currentIndexChanged(int)), this, SLOT(updateMacroField()));
    setLayout(layout);
}

CommandWidget::CommandWidget(amuse::SoundMacro::ICmd* cmd, amuse::SoundMacro::CmdOp op, SoundMacroListing* listing)
: QWidget(nullptr), m_cmd(cmd), m_introspection(amuse::SoundMacro::GetCmdIntrospection(op))
{
    QFont titleFont = m_titleLabel.font();
    titleFont.setWeight(QFont::Bold);
    m_titleLabel.setFont(titleFont);
    m_titleLabel.setForegroundRole(QPalette::Background);
    //m_titleLabel.setAutoFillBackground(true);
    //m_titleLabel.setBackgroundRole(QPalette::Text);
    m_titleLabel.setContentsMargins(46, 0, 0, 0);
    m_titleLabel.setFixedHeight(20);
    m_numberText.setTextOption(QTextOption(Qt::AlignRight));
    m_numberText.setTextWidth(25);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_numberFont.setWeight(QFont::Bold);
    m_numberFont.setStyleHint(QFont::Monospace);
    m_numberFont.setPointSize(16);

    setContentsMargins(QMargins());
    setFixedHeight(100);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins(QMargins());
    mainLayout->setSpacing(0);

    QHBoxLayout* headLayout = new QHBoxLayout;
    headLayout->setContentsMargins(QMargins());
    headLayout->setSpacing(0);
    headLayout->addWidget(&m_titleLabel);
    if (op != amuse::SoundMacro::CmdOp::End)
    {
        m_deleteButton.setFixedSize(21, 21);
        m_deleteButton.setIcon(QIcon(QStringLiteral(":/icons/IconSoundMacroDelete.svg")));
        m_deleteButton.setFlat(true);
        m_deleteButton.setToolTip(tr("Delete this SoundMacro"));
        connect(&m_deleteButton, SIGNAL(clicked(bool)), this, SLOT(deleteClicked()));
        headLayout->addWidget(&m_deleteButton);
    }

    mainLayout->addLayout(headLayout);
    mainLayout->addSpacing(8);

    QGridLayout* layout = new QGridLayout;
    layout->setSpacing(6);
    layout->setContentsMargins(64, 0, 12, 12);
    if (m_introspection)
    {
        m_titleLabel.setText(tr(m_introspection->m_name.data()));
        m_titleLabel.setToolTip(tr(m_introspection->m_description.data()));
        FieldProjectNode* nf = nullptr;
        for (int f = 0; f < 7; ++f)
        {
            const amuse::SoundMacro::CmdIntrospection::Field& field = m_introspection->m_fields[f];
            if (!field.m_name.empty())
            {
                QString fieldName = tr(field.m_name.data());
                layout->addWidget(new QLabel(fieldName), 0, f);
                switch (field.m_tp)
                {
                case amuse::SoundMacro::CmdIntrospection::Field::Type::Bool:
                {
                    QCheckBox* cb = new QCheckBox;
                    cb->setProperty("fieldIndex", f);
                    cb->setProperty("fieldName", fieldName);
                    cb->setCheckState(amuse::AccessField<bool>(m_cmd, field) ? Qt::Checked : Qt::Unchecked);
                    connect(cb, SIGNAL(stateChanged(int)), this, SLOT(boolChanged(int)));
                    layout->addWidget(cb, 1, f);
                    break;
                }
                case amuse::SoundMacro::CmdIntrospection::Field::Type::Int8:
                case amuse::SoundMacro::CmdIntrospection::Field::Type::UInt8:
                case amuse::SoundMacro::CmdIntrospection::Field::Type::Int16:
                case amuse::SoundMacro::CmdIntrospection::Field::Type::UInt16:
                case amuse::SoundMacro::CmdIntrospection::Field::Type::Int32:
                case amuse::SoundMacro::CmdIntrospection::Field::Type::UInt32:
                {
                    FieldSpinBox* sb = new FieldSpinBox;
                    sb->setProperty("fieldIndex", f);
                    sb->setProperty("fieldName", fieldName);
                    sb->setMinimum(int(field.m_min));
                    sb->setMaximum(int(field.m_max));
                    sb->setToolTip(QStringLiteral("[%1,%2]").arg(int(field.m_min)).arg(int(field.m_max)));
                    switch (field.m_tp)
                    {
                    case amuse::SoundMacro::CmdIntrospection::Field::Type::Int8:
                        sb->setValue(amuse::AccessField<int8_t>(m_cmd, field));
                        break;
                    case amuse::SoundMacro::CmdIntrospection::Field::Type::UInt8:
                        sb->setValue(amuse::AccessField<uint8_t>(m_cmd, field));
                        break;
                    case amuse::SoundMacro::CmdIntrospection::Field::Type::Int16:
                        sb->setValue(amuse::AccessField<int16_t>(m_cmd, field));
                        break;
                    case amuse::SoundMacro::CmdIntrospection::Field::Type::UInt16:
                        sb->setValue(amuse::AccessField<uint16_t>(m_cmd, field));
                        break;
                    case amuse::SoundMacro::CmdIntrospection::Field::Type::Int32:
                        sb->setValue(amuse::AccessField<int32_t>(m_cmd, field));
                        break;
                    case amuse::SoundMacro::CmdIntrospection::Field::Type::UInt32:
                        sb->setValue(amuse::AccessField<uint32_t>(m_cmd, field));
                        break;
                    default:
                        break;
                    }
                    connect(sb, SIGNAL(valueChanged(int)), this, SLOT(numChanged(int)));
                    layout->addWidget(sb, 1, f);
                    break;
                }
                case amuse::SoundMacro::CmdIntrospection::Field::Type::SoundMacroId:
                case amuse::SoundMacro::CmdIntrospection::Field::Type::TableId:
                {
                    ProjectModel::INode::Type collectionType;
                    if (field.m_tp == amuse::SoundMacro::CmdIntrospection::Field::Type::SoundMacroId)
                    {
                        collectionType = ProjectModel::INode::Type::SoundMacro;
                    }
                    else if (field.m_tp == amuse::SoundMacro::CmdIntrospection::Field::Type::TableId)
                    {
                        if (!field.m_name.compare("ADSR"))
                            collectionType = ProjectModel::INode::Type::ADSR;
                        else
                            collectionType = ProjectModel::INode::Type::Curve;
                    }
                    auto* collection = g_MainWindow->projectModel()->getGroupNode(listing->currentNode())->
                        getCollectionOfType(collectionType);
                    nf = new FieldProjectNode(collection);
                    nf->setProperty("fieldIndex", f);
                    nf->setProperty("fieldName", fieldName);
                    int index = collection->indexOfId(
                        amuse::AccessField<amuse::SoundMacroIdDNA<athena::Little>>(m_cmd, field).id);
                    nf->setCurrentIndex(index < 0 ? 0 : index + 1);
                    connect(nf, SIGNAL(currentIndexChanged(int)), this, SLOT(nodeChanged(int)));
                    layout->addWidget(nf, 1, f);
                    break;
                }
                case amuse::SoundMacro::CmdIntrospection::Field::Type::SoundMacroStep:
                {
                    FieldSoundMacroStep* sb = new FieldSoundMacroStep(nf);
                    sb->setProperty("fieldIndex", f);
                    sb->setProperty("fieldName", fieldName);
                    sb->m_spinBox.setValue(amuse::AccessField<amuse::SoundMacroStepDNA<athena::Little>>(m_cmd, field).step);
                    connect(sb, SIGNAL(valueChanged(int)), this, SLOT(numChanged(int)));
                    layout->addWidget(sb, 1, f);
                    m_stepField = sb;
                    break;
                }
                case amuse::SoundMacro::CmdIntrospection::Field::Type::Choice:
                {
                    FieldComboBox* cb = new FieldComboBox;
                    cb->setProperty("fieldIndex", f);
                    cb->setProperty("fieldName", fieldName);
                    for (int j = 0; j < 4; ++j)
                    {
                        if (field.m_choices[j].empty())
                            break;
                        cb->addItem(tr(field.m_choices[j].data()));
                    }
                    cb->setCurrentIndex(int(amuse::AccessField<int8_t>(m_cmd, field)));
                    connect(cb, SIGNAL(currentIndexChanged(int)), this, SLOT(numChanged(int)));
                    layout->addWidget(cb, 1, f);
                    break;
                }
                default:
                    break;
                }
            }
        }
    }
    mainLayout->addLayout(layout);
    layout->setRowMinimumHeight(0, 22);
    layout->setRowMinimumHeight(1, 37);
    setLayout(mainLayout);
}

CommandWidget::CommandWidget(amuse::SoundMacro::ICmd* cmd, SoundMacroListing* listing)
: CommandWidget(cmd, cmd->Isa(), listing) {}

CommandWidget::CommandWidget(amuse::SoundMacro::CmdOp op, SoundMacroListing* listing)
: CommandWidget(nullptr, op, listing) {}

class ValChangedUndoCommand : public EditorUndoCommand
{
    amuse::SoundMacro::ICmd* m_cmd;
    const amuse::SoundMacro::CmdIntrospection::Field& m_field;
    int m_redoVal, m_undoVal;
    bool m_undid = false;
public:
    ValChangedUndoCommand(amuse::SoundMacro::ICmd* cmd, const QString& fieldName,
                          const amuse::SoundMacro::CmdIntrospection::Field& field,
                          int redoVal, std::shared_ptr<ProjectModel::SoundMacroNode> node)
        : EditorUndoCommand(node, QUndoStack::tr("Change %1").arg(fieldName)),
          m_cmd(cmd), m_field(field), m_redoVal(redoVal) {}
    void undo()
    {
        m_undid = true;
        switch (m_field.m_tp)
        {
        case amuse::SoundMacro::CmdIntrospection::Field::Type::Bool:
            amuse::AccessField<bool>(m_cmd, m_field) = bool(m_undoVal);
            break;
        case amuse::SoundMacro::CmdIntrospection::Field::Type::Int8:
        case amuse::SoundMacro::CmdIntrospection::Field::Type::Choice:
            amuse::AccessField<int8_t>(m_cmd, m_field) = int8_t(m_undoVal);
            break;
        case amuse::SoundMacro::CmdIntrospection::Field::Type::UInt8:
            amuse::AccessField<uint8_t>(m_cmd, m_field) = uint8_t(m_undoVal);
            break;
        case amuse::SoundMacro::CmdIntrospection::Field::Type::Int16:
            amuse::AccessField<int16_t>(m_cmd, m_field) = int16_t(m_undoVal);
            break;
        case amuse::SoundMacro::CmdIntrospection::Field::Type::UInt16:
            amuse::AccessField<uint16_t>(m_cmd, m_field) = uint16_t(m_undoVal);
            break;
        case amuse::SoundMacro::CmdIntrospection::Field::Type::Int32:
            amuse::AccessField<int32_t>(m_cmd, m_field) = int32_t(m_undoVal);
            break;
        case amuse::SoundMacro::CmdIntrospection::Field::Type::UInt32:
            amuse::AccessField<uint32_t>(m_cmd, m_field) = uint32_t(m_undoVal);
            break;
        case amuse::SoundMacro::CmdIntrospection::Field::Type::SoundMacroId:
        case amuse::SoundMacro::CmdIntrospection::Field::Type::SoundMacroStep:
        case amuse::SoundMacro::CmdIntrospection::Field::Type::TableId:
        case amuse::SoundMacro::CmdIntrospection::Field::Type::SampleId:
            amuse::AccessField<amuse::SoundMacroIdDNA<athena::Little>>(m_cmd, m_field).id = uint16_t(m_undoVal);
            break;
        default:
            break;
        }
        EditorUndoCommand::undo();
    }
    void redo()
    {
        switch (m_field.m_tp)
        {
        case amuse::SoundMacro::CmdIntrospection::Field::Type::Bool:
            m_undoVal = amuse::AccessField<bool>(m_cmd, m_field);
            amuse::AccessField<bool>(m_cmd, m_field) = bool(m_redoVal);
            break;
        case amuse::SoundMacro::CmdIntrospection::Field::Type::Int8:
        case amuse::SoundMacro::CmdIntrospection::Field::Type::Choice:
            m_undoVal = amuse::AccessField<int8_t>(m_cmd, m_field);
            amuse::AccessField<int8_t>(m_cmd, m_field) = int8_t(m_redoVal);
            break;
        case amuse::SoundMacro::CmdIntrospection::Field::Type::UInt8:
            m_undoVal = amuse::AccessField<uint8_t>(m_cmd, m_field);
            amuse::AccessField<uint8_t>(m_cmd, m_field) = uint8_t(m_redoVal);
            break;
        case amuse::SoundMacro::CmdIntrospection::Field::Type::Int16:
            m_undoVal = amuse::AccessField<int16_t>(m_cmd, m_field);
            amuse::AccessField<int16_t>(m_cmd, m_field) = int16_t(m_redoVal);
            break;
        case amuse::SoundMacro::CmdIntrospection::Field::Type::UInt16:
            m_undoVal = amuse::AccessField<uint16_t>(m_cmd, m_field);
            amuse::AccessField<uint16_t>(m_cmd, m_field) = uint16_t(m_redoVal);
            break;
        case amuse::SoundMacro::CmdIntrospection::Field::Type::Int32:
            m_undoVal = amuse::AccessField<int32_t>(m_cmd, m_field);
            amuse::AccessField<int32_t>(m_cmd, m_field) = int32_t(m_redoVal);
            break;
        case amuse::SoundMacro::CmdIntrospection::Field::Type::UInt32:
            m_undoVal = amuse::AccessField<uint32_t>(m_cmd, m_field);
            amuse::AccessField<uint32_t>(m_cmd, m_field) = uint32_t(m_redoVal);
            break;
        case amuse::SoundMacro::CmdIntrospection::Field::Type::SoundMacroId:
        case amuse::SoundMacro::CmdIntrospection::Field::Type::SoundMacroStep:
        case amuse::SoundMacro::CmdIntrospection::Field::Type::TableId:
        case amuse::SoundMacro::CmdIntrospection::Field::Type::SampleId:
            m_undoVal = amuse::AccessField<amuse::SoundMacroIdDNA<athena::Little>>(m_cmd, m_field).id;
            amuse::AccessField<amuse::SoundMacroIdDNA<athena::Little>>(m_cmd, m_field).id = uint16_t(m_redoVal);
            break;
        default:
            break;
        }
        if (m_undid)
            EditorUndoCommand::redo();
    }
    bool mergeWith(const QUndoCommand* other)
    {
        if (other->id() == id() && m_cmd == static_cast<const ValChangedUndoCommand*>(other)->m_cmd &&
            &m_field == &static_cast<const ValChangedUndoCommand*>(other)->m_field)
        {
            m_redoVal = static_cast<const ValChangedUndoCommand*>(other)->m_redoVal;
            return true;
        }
        return false;
    }
    int id() const { return int(Id::SMChangeVal); }
};

void CommandWidget::boolChanged(int state)
{
    if (m_introspection)
    {
        const amuse::SoundMacro::CmdIntrospection::Field& field =
            m_introspection->m_fields[sender()->property("fieldIndex").toInt()];
        g_MainWindow->pushUndoCommand(new ValChangedUndoCommand(m_cmd, sender()->property("fieldName").toString(),
                                                                field, state == Qt::Checked, getParent()->m_node));
    }
}

void CommandWidget::numChanged(int value)
{
    if (m_introspection)
    {
        const amuse::SoundMacro::CmdIntrospection::Field& field =
            m_introspection->m_fields[sender()->property("fieldIndex").toInt()];
        g_MainWindow->pushUndoCommand(new ValChangedUndoCommand(m_cmd, sender()->property("fieldName").toString(),
                                                                field, value, getParent()->m_node));
    }
}

void CommandWidget::nodeChanged(int value)
{
    if (m_introspection)
    {
        FieldProjectNode* fieldW = static_cast<FieldProjectNode*>(sender());
        int v = value == 0 ? 65535 : fieldW->collection()->idOfIndex(value - 1).id;
        const amuse::SoundMacro::CmdIntrospection::Field& field =
            m_introspection->m_fields[fieldW->property("fieldIndex").toInt()];
        g_MainWindow->pushUndoCommand(new ValChangedUndoCommand(m_cmd, fieldW->property("fieldName").toString(),
                                                                field, v, getParent()->m_node));
    }
}

void CommandWidget::deleteClicked()
{
    if (m_index != -1)
        if (SoundMacroListing* listing = qobject_cast<SoundMacroListing*>(parentWidget()->parentWidget()))
            listing->deleteCommand(m_index);
}

void CommandWidget::setIndex(int index)
{
    m_index = index;
    m_numberText.setText(QString::number(index));
    if (m_stepField)
        m_stepField->updateMacroField();
    update();
}

SoundMacroListing* CommandWidget::getParent() const
{
    return qobject_cast<SoundMacroListing*>(parentWidget()->parentWidget());
}

void CommandWidget::paintEvent(QPaintEvent* event)
{
    /* Rounded frame */
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPoint points[] =
    {
        {1, 20},
        {1, 99},
        {width() - 1, 99},
        {width() - 1, 1},
        {20, 1},
        {1, 20},
    };
    painter.setBrush(palette().brush(QPalette::Background));
    painter.drawPolygon(points, 6);
    painter.setPen(QPen(QColor(127, 127, 127), 2.0));
    painter.drawPolyline(points, 6);

    QPoint headPoints[] =
    {
        {1, 20},
        {1, 55},
        {35, 20},
        {width() - 1, 20},
        {width() - 1, 1},
        {20, 1},
        {1, 20}
    };
    painter.setBrush(QColor(127, 127, 127));
    painter.drawPolygon(headPoints, 7);

    painter.drawRect(17, 51, 32, 32);

    QTransform rotate;
    rotate.rotate(-45.0);
    painter.setTransform(rotate);
    painter.setFont(m_numberFont);
    painter.setPen(palette().color(QPalette::Background));
    painter.drawStaticText(-15, 10, m_numberText);
}

void CommandWidgetContainer::animateOpen()
{
    int newHeight = 200 + parentWidget()->layout()->spacing();
    m_animation = new QPropertyAnimation(this, "minimumHeight");
    m_animation->setDuration(abs(minimumHeight() - newHeight) * 4);
    m_animation->setStartValue(minimumHeight());
    m_animation->setEndValue(newHeight);
    m_animation->setEasingCurve(QEasingCurve::InOutExpo);
    connect(m_animation, SIGNAL(valueChanged(const QVariant&)), parentWidget(), SLOT(update()));
    connect(m_animation, SIGNAL(destroyed(QObject*)), this, SLOT(animationDestroyed()));
    m_animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void CommandWidgetContainer::animateClosed()
{
    m_animation = new QPropertyAnimation(this, "minimumHeight");
    m_animation->setDuration(abs(minimumHeight() - 100) * 4);
    m_animation->setStartValue(minimumHeight());
    m_animation->setEndValue(100);
    m_animation->setEasingCurve(QEasingCurve::InOutExpo);
    connect(m_animation, SIGNAL(valueChanged(const QVariant&)), parentWidget(), SLOT(update()));
    connect(m_animation, SIGNAL(destroyed(QObject*)), this, SLOT(animationDestroyed()));
    m_animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void CommandWidgetContainer::snapOpen()
{
    if (m_animation)
        m_animation->stop();
    setMinimumHeight(200 + parentWidget()->layout()->spacing());
}

void CommandWidgetContainer::snapClosed()
{
    if (m_animation)
        m_animation->stop();
    setMinimumHeight(100);
}

void CommandWidgetContainer::animationDestroyed()
{
    m_animation = nullptr;
}

CommandWidgetContainer::CommandWidgetContainer(CommandWidget* child, QWidget* parent)
: QWidget(parent), m_commandWidget(child)
{
    setMinimumHeight(100);
    setContentsMargins(QMargins());
    QVBoxLayout* outerLayout = new QVBoxLayout;
    outerLayout->setAlignment(Qt::AlignBottom);
    outerLayout->setContentsMargins(QMargins());
    outerLayout->setSpacing(0);
    outerLayout->addWidget(child);
    setLayout(outerLayout);
}

void SoundMacroListing::startAutoscroll(QWidget* source, QMouseEvent* event, int delta)
{
    if (m_autoscrollTimer == -1)
        m_autoscrollTimer = startTimer(50);
    m_autoscrollDelta = delta;
    m_autoscrollSource = source;
    m_autoscrollEvent = *event;
}

void SoundMacroListing::stopAutoscroll()
{
    if (m_autoscrollTimer != -1)
    {
        killTimer(m_autoscrollTimer);
        m_autoscrollTimer = -1;
    }
    m_autoscrollDelta = 0;
    m_autoscrollSource = nullptr;
}

void SoundMacroListing::timerEvent(QTimerEvent* event)
{
    if (QScrollArea* scrollArea = qobject_cast<QScrollArea*>(parentWidget()->parentWidget()))
    {
        QScrollBar* bar = scrollArea->verticalScrollBar();
        int oldValue = bar->value();
        bar->setValue(oldValue + m_autoscrollDelta);
        int valueDelta = bar->value() - oldValue;
        if (valueDelta != 0)
        {
            if (m_autoscrollSource)
                QApplication::sendEvent(m_autoscrollSource, &m_autoscrollEvent);
            update();
        }
    }
}

bool SoundMacroListing::beginDrag(CommandWidget* widget)
{
    int origIdx = m_layout->indexOf(widget->parentWidget());
    /* Don't allow dragging last command (END command) */
    if (origIdx < 0 || origIdx >= m_layout->count() - 2)
        return false;
    if (origIdx < m_layout->count() - 2)
    {
        // Animate next item open
        m_dragOpenIdx = origIdx;
        if (CommandWidgetContainer* nextItem =
            qobject_cast<CommandWidgetContainer*>(m_layout->itemAt(origIdx + 1)->widget()))
            nextItem->snapOpen();
    }
    else
    {
        m_dragOpenIdx = -1;
    }
    m_origIdx = origIdx;
    m_dragItem = m_layout->takeAt(origIdx);
    m_dragItem->widget()->raise();
    return true;
}

class ReorderCommandsUndoCommand : public EditorUndoCommand
{
    int m_a, m_b;
    bool m_undid = false;
public:
    ReorderCommandsUndoCommand(int a, int b, const QString& text, std::shared_ptr<ProjectModel::SoundMacroNode> node)
    : EditorUndoCommand(node, QUndoStack::tr("Reorder %1").arg(text)), m_a(a), m_b(b) {}
    void undo()
    {
        m_undid = true;
        static_cast<ProjectModel::SoundMacroNode*>(m_node.get())->
            m_obj->swapPositions(m_a, m_b);
        EditorUndoCommand::undo();
    }
    void redo()
    {
        static_cast<ProjectModel::SoundMacroNode*>(m_node.get())->
            m_obj->swapPositions(m_a, m_b);
        if (m_undid)
            EditorUndoCommand::redo();
    }
};

void SoundMacroListing::endDrag()
{
    int insertIdx;
    if (m_dragOpenIdx != -1)
    {
        if (CommandWidgetContainer* prevItem =
            qobject_cast<CommandWidgetContainer*>(m_layout->itemAt(m_dragOpenIdx)->widget()))
            prevItem->snapClosed();
        insertIdx = m_dragOpenIdx;
        m_dragOpenIdx = -1;
    }
    else
    {
        insertIdx = m_layout->count() - 2;
    }

    if (m_prevDragOpen)
    {
        m_prevDragOpen->snapClosed();
        m_prevDragOpen = nullptr;
    }

    if (m_origIdx != insertIdx)
    {
        CommandWidget* cmd = static_cast<CommandWidgetContainer*>(m_dragItem->widget())->m_commandWidget;
        g_MainWindow->pushUndoCommand(new ReorderCommandsUndoCommand(m_origIdx, insertIdx, cmd->getText(), m_node));
    }
    m_layout->insertItem(insertIdx, m_dragItem);
    m_dragItem = nullptr;
    stopAutoscroll();
    reindex();
}

void SoundMacroListing::cancelDrag()
{
    if (m_dragOpenIdx != -1)
    {
        if (CommandWidgetContainer* prevItem =
            qobject_cast<CommandWidgetContainer*>(m_layout->itemAt(m_dragOpenIdx)->widget()))
            prevItem->snapClosed();
        m_dragOpenIdx = -1;
    }
    m_layout->insertItem(m_origIdx, m_dragItem);
    if (m_prevDragOpen)
    {
        m_prevDragOpen->snapClosed();
        m_prevDragOpen = nullptr;
    }
    m_dragItem = nullptr;
    stopAutoscroll();
}

void SoundMacroListing::_moveDrag(int hoverIdx, const QPoint& pt, QWidget* source, QMouseEvent* event)
{
    QRect scrollVpRect = parentWidget()->parentWidget()->rect();
    QPoint scrollVpPoint = mapTo(parentWidget()->parentWidget(), pt);
    if (scrollVpRect.bottom() - scrollVpPoint.y() < 50)
        startAutoscroll(source, event, 10); // Scroll Down
    else if (scrollVpRect.top() - scrollVpPoint.y() > -50)
        startAutoscroll(source, event, -10);
    else
        stopAutoscroll();

    /* Don't allow insertion after last command (END command) */
    hoverIdx = std::max(0, std::min(hoverIdx, m_layout->count() - 2));
    if (hoverIdx != m_dragOpenIdx)
    {
        if (m_dragOpenIdx != -1)
            if (CommandWidgetContainer* prevItem =
                qobject_cast<CommandWidgetContainer*>(m_layout->itemAt(m_dragOpenIdx)->widget()))
            {
                m_prevDragOpen = prevItem;
                prevItem->animateClosed();
            }
        if (CommandWidgetContainer* nextItem =
            qobject_cast<CommandWidgetContainer*>(m_layout->itemAt(hoverIdx)->widget()))
            nextItem->animateOpen();
        m_dragOpenIdx = hoverIdx;
    }
    update();
}

void SoundMacroListing::moveDrag(CommandWidget* widget, const QPoint& pt, QWidget* source, QMouseEvent* event)
{
    CommandWidgetContainer* container = static_cast<CommandWidgetContainer*>(widget->parentWidget());
    int pitch = 100 + m_layout->spacing();
    _moveDrag((container->pos().y() - m_layout->contentsMargins().top() + pitch / 2) / pitch, pt, source, event);
}

int SoundMacroListing::moveInsertDrag(const QPoint& pt, QWidget* source, QMouseEvent* event)
{
    int pitch = 100 + m_layout->spacing();
    _moveDrag(pt.y() / pitch, pt, source, event);
    return m_dragOpenIdx;
}

void SoundMacroListing::insertDragout()
{
    if (m_dragOpenIdx != -1)
    {
        if (CommandWidgetContainer* prevItem =
            qobject_cast<CommandWidgetContainer*>(m_layout->itemAt(m_dragOpenIdx)->widget()))
        {
            m_prevDragOpen = prevItem;
            prevItem->animateClosed();
        }
        m_dragOpenIdx = -1;
    }
    stopAutoscroll();
}

class InsertCommandUndoCommand : public EditorUndoCommand
{
    int m_insertIdx;
    std::unique_ptr<amuse::SoundMacro::ICmd> m_cmd;
public:
    InsertCommandUndoCommand(int insertIdx, const QString& text, std::shared_ptr<ProjectModel::SoundMacroNode> node)
    : EditorUndoCommand(node, QUndoStack::tr("Insert %1").arg(text)), m_insertIdx(insertIdx) {}
    void undo()
    {
        m_cmd = static_cast<ProjectModel::SoundMacroNode*>(m_node.get())->
            m_obj->deleteCmd(m_insertIdx);
        EditorUndoCommand::undo();
    }
    void redo()
    {
        if (!m_cmd)
            return;
        static_cast<ProjectModel::SoundMacroNode*>(m_node.get())->
            m_obj->insertCmd(m_insertIdx, std::move(m_cmd));
        m_cmd.reset();
        EditorUndoCommand::redo();
    }
};

void SoundMacroListing::insert(amuse::SoundMacro::CmdOp op, const QString& text)
{
    int insertIdx;
    if (m_dragOpenIdx != -1)
    {
        if (CommandWidgetContainer* prevItem =
            qobject_cast<CommandWidgetContainer*>(m_layout->itemAt(m_dragOpenIdx)->widget()))
            prevItem->snapClosed();
        insertIdx = m_dragOpenIdx;
        m_dragOpenIdx = -1;
    }
    else
    {
        insertIdx = m_layout->count() - 2;
    }

    if (m_prevDragOpen)
    {
        m_prevDragOpen->snapClosed();
        m_prevDragOpen = nullptr;
    }

    g_MainWindow->pushUndoCommand(new InsertCommandUndoCommand(insertIdx, text, m_node));
    m_layout->insertWidget(insertIdx,
        new CommandWidgetContainer(new CommandWidget(m_node->m_obj->insertNewCmd(insertIdx, op), this)));

    stopAutoscroll();
    reindex();
}

class DeleteCommandUndoCommand : public EditorUndoCommand
{
    int m_deleteIdx;
    std::unique_ptr<amuse::SoundMacro::ICmd> m_cmd;
    bool m_undid = false;
public:
    DeleteCommandUndoCommand(int deleteIdx, const QString& text, std::shared_ptr<ProjectModel::SoundMacroNode> node)
    : EditorUndoCommand(node, QUndoStack::tr("Delete %1").arg(text)), m_deleteIdx(deleteIdx) {}
    void undo()
    {
        m_undid = true;
        static_cast<ProjectModel::SoundMacroNode*>(m_node.get())->
            m_obj->insertCmd(m_deleteIdx, std::move(m_cmd));
        m_cmd.reset();
        EditorUndoCommand::undo();
    }
    void redo()
    {
        m_cmd = static_cast<ProjectModel::SoundMacroNode*>(m_node.get())->
            m_obj->deleteCmd(m_deleteIdx);
        if (m_undid)
            EditorUndoCommand::redo();
    }
};

void SoundMacroListing::deleteCommand(int index)
{
    QLayoutItem* item = m_layout->takeAt(index);
    CommandWidget* cmd = static_cast<CommandWidgetContainer*>(item->widget())->m_commandWidget;
    g_MainWindow->pushUndoCommand(new DeleteCommandUndoCommand(index, cmd->getText(), m_node));
    item->widget()->deleteLater();
    delete item;
    reindex();
}

void SoundMacroListing::reindex()
{
    for (int i = 0; i < m_layout->count() - 1; ++i)
        if (CommandWidgetContainer* item =
            qobject_cast<CommandWidgetContainer*>(m_layout->itemAt(i)->widget()))
            item->m_commandWidget->setIndex(i);
}

void SoundMacroListing::clear()
{
    while (m_layout->count() > 2)
    {
        QLayoutItem* item = m_layout->takeAt(0);
        item->widget()->deleteLater();
        delete item;
    }
}

bool SoundMacroListing::loadData(ProjectModel::SoundMacroNode* node)
{
    m_node = node->shared_from_this();
    clear();
    int i = 0;
    for (auto& cmd : node->m_obj->m_cmds)
    {
        if (cmd->Isa() == amuse::SoundMacro::CmdOp::End)
            break;
        m_layout->insertWidget(i++, new CommandWidgetContainer(new CommandWidget(cmd.get(), this)));
    }
    reindex();
    update();
    return true;
}

void SoundMacroListing::unloadData()
{
    m_node.reset();
    clear();
    reindex();
    update();
}

ProjectModel::INode* SoundMacroListing::currentNode() const
{
    return m_node.get();
}

SoundMacroListing::SoundMacroListing(QWidget* parent)
: QWidget(parent), m_layout(new QVBoxLayout)
{
    m_layout->addWidget(new CommandWidgetContainer(new CommandWidget(amuse::SoundMacro::CmdOp::End, this)));
    m_layout->addStretch();
    setLayout(m_layout);
    reindex();
}

CatalogueItem::CatalogueItem(amuse::SoundMacro::CmdOp op, const QString& name,
                             const QString& doc, QWidget* parent)
: QWidget(parent), m_op(op), m_label(name)
{
    QHBoxLayout* layout = new QHBoxLayout;
    QString iconPath = QStringLiteral(":/commands/%1.svg").arg(name);
    if (QFile(iconPath).exists())
        m_iconLab.setPixmap(QIcon(iconPath).pixmap(32, 32));
    else
        m_iconLab.setPixmap(QIcon(QStringLiteral(":/icons/IconOpen.svg")).pixmap(32, 32));
    layout->addWidget(&m_iconLab);
    layout->addWidget(&m_label);
    layout->addStretch();
    layout->setContentsMargins(QMargins());
    setLayout(layout);
    setToolTip(doc);
}

CatalogueItem::CatalogueItem(const CatalogueItem& other, QWidget* parent)
: QWidget(parent), m_op(other.getCmdOp())
{
    QHBoxLayout* layout = new QHBoxLayout;
    QHBoxLayout* oldLayout = static_cast<QHBoxLayout*>(other.layout());
    m_iconLab.setPixmap(*static_cast<QLabel*>(oldLayout->itemAt(0)->widget())->pixmap());
    layout->addWidget(&m_iconLab);
    m_label.setText(static_cast<QLabel*>(oldLayout->itemAt(1)->widget())->text());
    layout->addWidget(&m_label);
    layout->addStretch();
    layout->setContentsMargins(QMargins());
    setLayout(layout);
}

static const char* CategoryStrings[] =
{
    QT_TRANSLATE_NOOP("SoundMacroCatalogue", "Control"),
    QT_TRANSLATE_NOOP("SoundMacroCatalogue", "Pitch"),
    QT_TRANSLATE_NOOP("SoundMacroCatalogue", "Sample"),
    QT_TRANSLATE_NOOP("SoundMacroCatalogue", "Setup"),
    QT_TRANSLATE_NOOP("SoundMacroCatalogue", "Special"),
    QT_TRANSLATE_NOOP("SoundMacroCatalogue", "Structure"),
    QT_TRANSLATE_NOOP("SoundMacroCatalogue", "Volume")
};

static const char* CategoryDocStrings[] =
{
    QT_TRANSLATE_NOOP("SoundMacroCatalogue", "Commands to control the voice"),
    QT_TRANSLATE_NOOP("SoundMacroCatalogue", "Commands to control the voice's pitch"),
    QT_TRANSLATE_NOOP("SoundMacroCatalogue", "Commands to control the voice's sample playback"),
    QT_TRANSLATE_NOOP("SoundMacroCatalogue", "Commands to setup the voice's mixing process"),
    QT_TRANSLATE_NOOP("SoundMacroCatalogue", "Miscellaneous commands"),
    QT_TRANSLATE_NOOP("SoundMacroCatalogue", "Commands to control macro branching"),
    QT_TRANSLATE_NOOP("SoundMacroCatalogue", "Commands to control the voice's volume")
};

SoundMacroCatalogue::SoundMacroCatalogue(QWidget* parent)
: QTreeWidget(parent)
{
    setSelectionMode(QAbstractItemView::NoSelection);
    setColumnCount(1);
    setHeaderHidden(true);

    QTreeWidgetItem* rootItems[int(amuse::SoundMacro::CmdType::CmdTypeMAX)];
    for (int i = 0; i < int(amuse::SoundMacro::CmdType::CmdTypeMAX); ++i)
    {
        rootItems[i] = new QTreeWidgetItem(this);
        setItemWidget(rootItems[i], 0, new CatalogueItem(amuse::SoundMacro::CmdOp::Invalid,
                                                         tr(CategoryStrings[i]), tr(CategoryDocStrings[i])));
    }

    for (int i = 1; i < int(amuse::SoundMacro::CmdOp::CmdOpMAX); ++i)
    {
        const amuse::SoundMacro::CmdIntrospection* cmd =
            amuse::SoundMacro::GetCmdIntrospection(amuse::SoundMacro::CmdOp(i));
        if (cmd)
        {
            QTreeWidgetItem* item = new QTreeWidgetItem(rootItems[int(cmd->m_tp)]);
            setItemWidget(item, 0, new CatalogueItem(amuse::SoundMacro::CmdOp(i), tr(cmd->m_name.data()),
                                                     tr(cmd->m_description.data())));
        }
    }
}

void SoundMacroCatalogue::mousePressEvent(QMouseEvent* event)
{
    QTreeWidget::mousePressEvent(event);
    event->ignore();
}

void SoundMacroCatalogue::mouseReleaseEvent(QMouseEvent* event)
{
    QTreeWidget::mouseReleaseEvent(event);
    event->ignore();
}

void SoundMacroCatalogue::mouseMoveEvent(QMouseEvent* event)
{
    SoundMacroEditor* editor = qobject_cast<SoundMacroEditor*>(parentWidget()->parentWidget());
    if (!editor || !editor->m_draggedItem)
        QTreeWidget::mouseMoveEvent(event);
    event->ignore();
}

void SoundMacroEditor::beginCommandDrag(CommandWidget* widget, const QPoint& eventPt, const QPoint& pt)
{
    if (m_listing->beginDrag(widget))
    {
        m_draggedPt = pt;
        m_draggedCmd = widget;
    }
}

void SoundMacroEditor::beginCatalogueDrag(CatalogueItem* item, const QPoint& eventPt, const QPoint& pt)
{
    m_draggedPt = pt;
    m_draggedItem = new CatalogueItem(*item, this);
    m_draggedItem->setGeometry(item->geometry());
    m_draggedItem->move(eventPt - m_draggedPt);
    m_draggedItem->raise();
    m_draggedItem->show();
}

void SoundMacroEditor::beginStepTarget(FieldSoundMacroStep* stepField)
{
    m_targetField = stepField;
    m_catalogue->setDisabled(true);
    m_listing->setDisabled(true);
    setFocus();
}

void SoundMacroEditor::endStepTarget()
{
    m_targetField = nullptr;
    m_catalogue->setDisabled(false);
    m_listing->setDisabled(false);
}

void SoundMacroEditor::mousePressEvent(QMouseEvent* event)
{
    if (m_catalogue->geometry().contains(event->pos()))
    {
        QPoint fromParent1 = m_catalogue->mapFrom(this, event->pos());
        QWidget* ch = m_catalogue->childAt(fromParent1);
        if (ch)
        {
            CatalogueItem* child = nullptr;
            while (ch && !(child = qobject_cast<CatalogueItem*>(ch)))
                ch = ch->parentWidget();
            if (child && child->getCmdOp() != amuse::SoundMacro::CmdOp::Invalid)
            {
                QPoint fromParent2 = child->mapFrom(m_catalogue, fromParent1);
                beginCatalogueDrag(child, event->pos(), fromParent2);
            }
        }
    }
    else if (m_listing->parentWidget()->parentWidget()->geometry().contains(event->pos()))
    {
        QPoint fromParent1 = m_listing->mapFrom(this, event->pos());
        QWidget* ch = m_listing->childAt(fromParent1);
        if (ch)
        {
            CommandWidget* child = nullptr;
            while (ch && !(child = qobject_cast<CommandWidget*>(ch)))
                ch = ch->parentWidget();
            if (child)
            {
                if (m_targetField)
                {
                    m_targetField->setIndex(m_listing->layout()->indexOf(child->parentWidget()));
                    return;
                }
                QPoint fromParent2 = child->mapFrom(m_listing, fromParent1);
                beginCommandDrag(child, event->pos(), fromParent2);
            }
        }
    }

    if (m_targetField)
        m_targetField->cancel();
}

void SoundMacroEditor::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_draggedItem)
    {
        amuse::SoundMacro::CmdOp op = m_draggedItem->getCmdOp();
        QString text = m_draggedItem->getText();
        m_draggedItem->deleteLater();
        m_draggedItem = nullptr;

        if (m_listing->parentWidget()->parentWidget()->geometry().contains(event->pos()))
        {
            if (m_dragInsertIdx != -1)
                m_listing->insert(op, text);
            else
                m_listing->insertDragout();
        }
        else
        {
            m_listing->insertDragout();
        }
        m_dragInsertIdx = -1;
    }
    else if (m_draggedCmd)
    {
        m_listing->endDrag();
        m_draggedCmd = nullptr;
    }
}

void SoundMacroEditor::mouseMoveEvent(QMouseEvent* event)
{
    if (m_draggedItem)
    {
        m_draggedItem->move(event->pos() - m_draggedPt);
        if (m_listing->parentWidget()->parentWidget()->geometry().contains(event->pos()))
        {
            m_dragInsertIdx = m_listing->moveInsertDrag(m_listing->mapFrom(this, event->pos()), this, event);
        }
        else if (m_dragInsertIdx != -1)
        {
            m_listing->insertDragout();
            m_dragInsertIdx = -1;
        }
        m_catalogue->update();
        update();
    }
    else if (m_draggedCmd)
    {
        QPoint listingPt = m_listing->mapFrom(this, event->pos());
        CommandWidgetContainer* container = static_cast<CommandWidgetContainer*>(m_draggedCmd->parentWidget());
        container->move(container->x(), listingPt.y() - m_draggedPt.y());
        if (m_listing->parentWidget()->parentWidget()->geometry().contains(event->pos()))
            m_listing->moveDrag(m_draggedCmd, listingPt, this, event);
        m_listing->update();
        update();
    }
}

void SoundMacroEditor::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
    {
        if (m_draggedItem)
        {
            m_draggedItem->deleteLater();
            m_draggedItem = nullptr;
            m_listing->insertDragout();
        }
        else if (m_draggedCmd)
        {
            m_listing->cancelDrag();
            m_draggedCmd = nullptr;
        }
        else if (m_targetField)
        {
            m_targetField->cancel();
        }
    }
}

void SoundMacroEditor::catalogueDoubleClicked(QTreeWidgetItem* item, int column)
{
    if (CatalogueItem* cItem = qobject_cast<CatalogueItem*>(m_catalogue->itemWidget(item, column)))
    {
        amuse::SoundMacro::CmdOp op = cItem->getCmdOp();
        if (op != amuse::SoundMacro::CmdOp::Invalid)
            m_listing->insert(op, cItem->getText());
    }
}

bool SoundMacroEditor::loadData(ProjectModel::SoundMacroNode* node)
{
    endStepTarget();
    return m_listing->loadData(node);
}

void SoundMacroEditor::unloadData()
{
    endStepTarget();
    m_listing->unloadData();
}

ProjectModel::INode* SoundMacroEditor::currentNode() const
{
    return m_listing->currentNode();
}

SoundMacroEditor::SoundMacroEditor(QWidget* parent)
: EditorWidget(parent), m_splitter(new QSplitter),
  m_listing(new SoundMacroListing), m_catalogue(new SoundMacroCatalogue)
{
    {
        QScrollArea* listingScroll = new QScrollArea;
        listingScroll->setWidget(m_listing);
        listingScroll->setWidgetResizable(true);
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(1);
        sizePolicy.setVerticalStretch(0);
        listingScroll->setSizePolicy(sizePolicy);
        listingScroll->setMinimumWidth(350);
        m_splitter->addWidget(listingScroll);
    }

    {
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        m_catalogue->setSizePolicy(sizePolicy);
        m_catalogue->setMinimumWidth(150);
        m_catalogue->setGeometry(0, 0, 215, 0);
        m_catalogue->setMaximumWidth(300);
        m_splitter->addWidget(m_catalogue);
    }

    connect(m_catalogue, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
            this, SLOT(catalogueDoubleClicked(QTreeWidgetItem*, int)));

    m_splitter->setCollapsible(0, false);
    QGridLayout* layout = new QGridLayout;
    layout->setContentsMargins(QMargins());
    layout->addWidget(m_splitter);
    setLayout(layout);
}
