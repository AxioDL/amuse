#include "SoundMacroEditor.hpp"
#include <QPainter>
#include <QPropertyAnimation>
#include <QGridLayout>
#include <QLineEdit>
#include <QSplitter>
#include <QScrollArea>
#include <QScrollBar>
#include <QApplication>
#include <QCheckBox>

CommandWidget::CommandWidget(amuse::SoundMacro::ICmd* cmd, amuse::SoundMacro::CmdOp op, QWidget* parent)
: QWidget(parent), m_cmd(cmd), m_introspection(amuse::SoundMacro::GetCmdIntrospection(op))
{
    m_titleFont.setWeight(QFont::Bold);
    m_numberText.setTextOption(QTextOption(Qt::AlignRight));
    m_numberText.setTextWidth(25);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(100);
    m_numberFont.setWeight(QFont::Bold);
    m_numberFont.setStyleHint(QFont::Monospace);
    m_numberFont.setPointSize(16);

    setContentsMargins(54, 4, 0, 4);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addStretch();

    QGridLayout* layout = new QGridLayout;
    if (m_introspection)
    {
        m_titleText.setText(tr(m_introspection->m_name.data()));
        for (int f = 0; f < 7; ++f)
        {
            const amuse::SoundMacro::CmdIntrospection::Field& field = m_introspection->m_fields[f];
            if (!field.m_name.empty())
            {
                layout->addWidget(new QLabel(tr(field.m_name.data())), 0, f);
                int value;
                switch (field.m_tp)
                {
                case amuse::SoundMacro::CmdIntrospection::Field::Type::Bool:
                {
                    QCheckBox* cb = new QCheckBox;
                    cb->setProperty("fieldIndex", f);
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
                    sb->setMinimum(int(field.m_min));
                    sb->setMaximum(int(field.m_max));
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
                case amuse::SoundMacro::CmdIntrospection::Field::Type::Choice:
                {
                    FieldComboBox* cb = new FieldComboBox;
                    cb->setProperty("fieldIndex", f);
                    for (int j = 0; j < 4; ++j)
                    {
                        if (field.m_choices->empty())
                            break;
                        cb->addItem(tr(field.m_choices[j].data()));
                    }
                    cb->setCurrentIndex(int(amuse::AccessField<int8_t>(m_cmd, field)));
                    connect(cb, SIGNAL(currentIndexChanged(int)), this, SLOT(choiceChanged(int)));
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
    setLayout(mainLayout);
}

CommandWidget::CommandWidget(amuse::SoundMacro::ICmd* cmd, QWidget* parent)
: CommandWidget(cmd, cmd->Isa(), parent) {}

CommandWidget::CommandWidget(amuse::SoundMacro::CmdOp op, QWidget* parent)
: CommandWidget(nullptr, op, parent) {}

void CommandWidget::boolChanged(int state)
{
    if (m_introspection)
    {
        QCheckBox* cb = static_cast<QCheckBox*>(sender());
        const amuse::SoundMacro::CmdIntrospection::Field& field =
            m_introspection->m_fields[cb->property("fieldIndex").toInt()];
        amuse::AccessField<bool>(m_cmd, field) = state == Qt::Checked;
    }
}

void CommandWidget::numChanged(int value)
{
    if (m_introspection)
    {
        FieldSpinBox* sb = static_cast<FieldSpinBox*>(sender());
        const amuse::SoundMacro::CmdIntrospection::Field& field =
            m_introspection->m_fields[sb->property("fieldIndex").toInt()];
        switch (field.m_tp)
        {
        case amuse::SoundMacro::CmdIntrospection::Field::Type::Int8:
            amuse::AccessField<int8_t>(m_cmd, field) = int8_t(value);
            break;
        case amuse::SoundMacro::CmdIntrospection::Field::Type::UInt8:
            amuse::AccessField<uint8_t>(m_cmd, field) = uint8_t(value);
            break;
        case amuse::SoundMacro::CmdIntrospection::Field::Type::Int16:
            amuse::AccessField<int16_t>(m_cmd, field) = int16_t(value);
            break;
        case amuse::SoundMacro::CmdIntrospection::Field::Type::UInt16:
            amuse::AccessField<uint16_t>(m_cmd, field) = uint16_t(value);
            break;
        case amuse::SoundMacro::CmdIntrospection::Field::Type::Int32:
            amuse::AccessField<int32_t>(m_cmd, field) = int32_t(value);
            break;
        case amuse::SoundMacro::CmdIntrospection::Field::Type::UInt32:
            amuse::AccessField<uint32_t>(m_cmd, field) = uint32_t(value);
            break;
        default:
            break;
        }
    }
}

void CommandWidget::animateOpen()
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

void CommandWidget::animateClosed()
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

void CommandWidget::snapOpen()
{
    if (m_animation)
        m_animation->stop();
    setMinimumHeight(200 + parentWidget()->layout()->spacing());
}

void CommandWidget::snapClosed()
{
    if (m_animation)
        m_animation->stop();
    setMinimumHeight(100);
}

void CommandWidget::setIndex(int index)
{
    m_numberText.setText(QString::number(index));
    update();
}

void CommandWidget::animationDestroyed()
{
    m_animation = nullptr;
}

void CommandWidget::paintEvent(QPaintEvent* event)
{
    /* Rounded frame */
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QTransform mainXf = QTransform::fromTranslate(0, rect().bottom() - 99);
    painter.setTransform(mainXf);
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

    painter.setPen(palette().color(QPalette::Background));
    painter.setFont(m_titleFont);
    painter.drawStaticText(40, -1, m_titleText);

    QTransform rotate;
    rotate.rotate(-45.0);
    painter.setTransform(rotate * mainXf);
    painter.setFont(m_numberFont);
    painter.drawStaticText(-15, 10, m_numberText);
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
    int origIdx = m_layout->indexOf(widget);
    /* Don't allow dragging last command (END command) */
    if (origIdx < 0 || origIdx >= m_layout->count() - 2)
        return false;
    if (origIdx < m_layout->count() - 2)
    {
        // Animate next item open
        m_dragOpenIdx = origIdx;
        if (CommandWidget* nextItem = qobject_cast<CommandWidget*>(m_layout->itemAt(origIdx + 1)->widget()))
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

void SoundMacroListing::endDrag(CommandWidget* widget)
{
    if (m_dragOpenIdx != -1)
    {
        if (CommandWidget* prevItem = qobject_cast<CommandWidget*>(m_layout->itemAt(m_dragOpenIdx)->widget()))
            prevItem->snapClosed();
        m_layout->insertItem(m_dragOpenIdx, m_dragItem);
        m_dragOpenIdx = -1;
    }
    else
    {
        m_layout->insertItem(m_layout->count() - 2, m_dragItem);
    }
    if (m_prevDragOpen)
    {
        m_prevDragOpen->snapClosed();
        m_prevDragOpen = nullptr;
    }
    m_dragItem = nullptr;
    stopAutoscroll();
    reindex();
}

void SoundMacroListing::cancelDrag()
{
    if (m_dragOpenIdx != -1)
    {
        if (CommandWidget* prevItem = qobject_cast<CommandWidget*>(m_layout->itemAt(m_dragOpenIdx)->widget()))
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
            if (CommandWidget* prevItem = qobject_cast<CommandWidget*>(m_layout->itemAt(m_dragOpenIdx)->widget()))
            {
                m_prevDragOpen = prevItem;
                prevItem->animateClosed();
            }
        if (CommandWidget* nextItem = qobject_cast<CommandWidget*>(m_layout->itemAt(hoverIdx)->widget()))
            nextItem->animateOpen();
        m_dragOpenIdx = hoverIdx;
    }
    update();
}

void SoundMacroListing::moveDrag(CommandWidget* widget, const QPoint& pt, QWidget* source, QMouseEvent* event)
{
    int pitch = 100 + m_layout->spacing();
    _moveDrag((widget->pos().y() - m_layout->contentsMargins().top() + pitch / 2) / pitch, pt, source, event);
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
        if (CommandWidget* prevItem = qobject_cast<CommandWidget*>(m_layout->itemAt(m_dragOpenIdx)->widget()))
        {
            m_prevDragOpen = prevItem;
            prevItem->animateClosed();
        }
        m_dragOpenIdx = -1;
    }
    stopAutoscroll();
}

void SoundMacroListing::insert(amuse::SoundMacro::CmdOp op)
{
    CommandWidget* newCmd = new CommandWidget(amuse::SoundMacro::MakeCmd(op).release(), this);
    if (m_dragOpenIdx != -1)
    {
        if (CommandWidget* prevItem = qobject_cast<CommandWidget*>(m_layout->itemAt(m_dragOpenIdx)->widget()))
            prevItem->snapClosed();
        m_layout->insertWidget(m_dragOpenIdx, newCmd);
        m_dragOpenIdx = -1;
    }
    else
    {
        m_layout->insertWidget(m_layout->count() - 2, newCmd);
    }
    if (m_prevDragOpen)
    {
        m_prevDragOpen->snapClosed();
        m_prevDragOpen = nullptr;
    }
    stopAutoscroll();
    reindex();
}

void SoundMacroListing::reindex()
{
    for (int i = 0; i < m_layout->count() - 1; ++i)
        if (CommandWidget* item = qobject_cast<CommandWidget*>(m_layout->itemAt(i)->widget()))
            item->setIndex(i);
}

SoundMacroListing::SoundMacroListing(ProjectModel::SoundMacroNode* node, QWidget* parent)
: QWidget(parent), m_layout(new QVBoxLayout)
{
    m_layout->addWidget(new CommandWidget(amuse::SoundMacro::CmdOp::End, this));
    m_layout->addStretch();
    setLayout(m_layout);
    reindex();
}

CatalogueItem::CatalogueItem(amuse::SoundMacro::CmdOp op, const QString& name,
                             const QString& doc, QWidget* parent)
: QWidget(parent), m_op(op)
{
    QHBoxLayout* layout = new QHBoxLayout;
    QLabel* iconLab = new QLabel;
    QString iconPath = QStringLiteral(":/commands/%1.svg").arg(name);
    if (QFile(iconPath).exists())
        iconLab->setPixmap(QIcon(iconPath).pixmap(32, 32));
    else
        iconLab->setPixmap(QIcon(QStringLiteral(":/icons/IconOpen.svg")).pixmap(32, 32));
    layout->addWidget(iconLab);
    layout->addWidget(new QLabel(name));
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
    QLabel* iconLab = new QLabel;
    iconLab->setPixmap(*static_cast<QLabel*>(oldLayout->itemAt(0)->widget())->pixmap());
    layout->addWidget(iconLab);
    layout->addWidget(new QLabel(static_cast<QLabel*>(oldLayout->itemAt(1)->widget())->text()));
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
                QPoint fromParent2 = child->mapFrom(m_listing, fromParent1);
                beginCommandDrag(child, event->pos(), fromParent2);
            }
        }
    }
}

void SoundMacroEditor::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_draggedItem)
    {
        amuse::SoundMacro::CmdOp op = m_draggedItem->getCmdOp();
        m_draggedItem->deleteLater();
        m_draggedItem = nullptr;

        if (m_listing->parentWidget()->parentWidget()->geometry().contains(event->pos()))
        {
            if (m_dragInsertIdx != -1)
                m_listing->insert(op);
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
        m_listing->endDrag(m_draggedCmd);
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
        m_draggedCmd->move(m_draggedCmd->x(), listingPt.y() - m_draggedPt.y());
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
    }
}

void SoundMacroEditor::catalogueDoubleClicked(QTreeWidgetItem* item, int column)
{
    if (CatalogueItem* cItem = qobject_cast<CatalogueItem*>(m_catalogue->itemWidget(item, column)))
    {
        amuse::SoundMacro::CmdOp op = cItem->getCmdOp();
        if (op != amuse::SoundMacro::CmdOp::Invalid)
            m_listing->insert(op);
    }
}

SoundMacroEditor::SoundMacroEditor(ProjectModel::SoundMacroNode* node, QWidget* parent)
: EditorWidget(parent), m_splitter(new QSplitter),
  m_listing(new SoundMacroListing(node)), m_catalogue(new SoundMacroCatalogue)
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
