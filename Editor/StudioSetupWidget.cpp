#include "StudioSetupWidget.hpp"
#include "amuse/EffectChorus.hpp"
#include "amuse/EffectDelay.hpp"
#include "amuse/EffectReverb.hpp"
#include <QPainter>
#include <QScrollBar>
#include <QStylePainter>

using namespace std::literals;

static const EffectIntrospection ReverbStdIntrospective =
{
    amuse::EffectType::ReverbStd,
    "Reverb Std"sv,
    "Standard Reverb"sv,
    {
        {
            EffectIntrospection::Field::Type::Float,
            "Coloration"sv,
            0.f, 1.f, 0.f
        },
        {
            EffectIntrospection::Field::Type::Float,
            "Mix"sv,
            0.f, 1.f, 0.f
        },
        {
            EffectIntrospection::Field::Type::Float,
            "Time"sv,
            0.01f, 10.f, 0.01f
        },
        {
            EffectIntrospection::Field::Type::Float,
            "Damping"sv,
            0.f, 1.f, 0.f
        },
        {
            EffectIntrospection::Field::Type::Float,
            "Pre Delay"sv,
            0.f, 0.1f, 0.f
        }
    }
};

typedef float (amuse::EffectReverbStd::*ReverbStdGetFunc)() const;
typedef void (amuse::EffectReverbStd::*ReverbStdSetFunc)(float);
static const ReverbStdGetFunc ReverbStdGetters[] =
{
    &amuse::EffectReverbStd::getColoration,
    &amuse::EffectReverbStd::getMix,
    &amuse::EffectReverbStd::getTime,
    &amuse::EffectReverbStd::getDamping,
    &amuse::EffectReverbStd::getPreDelay
};
static const ReverbStdSetFunc ReverbStdSetters[] =
{
    &amuse::EffectReverbStd::setColoration,
    &amuse::EffectReverbStd::setMix,
    &amuse::EffectReverbStd::setTime,
    &amuse::EffectReverbStd::setDamping,
    &amuse::EffectReverbStd::setPreDelay
};

static const EffectIntrospection ReverbHiIntrospective =
{
    amuse::EffectType::ReverbHi,
    "Reverb Hi"sv,
    "High Reverb"sv,
    {
        {
            EffectIntrospection::Field::Type::Float,
            "Coloration"sv,
            0.f, 1.f, 0.f
        },
        {
            EffectIntrospection::Field::Type::Float,
            "Mix"sv,
            0.f, 1.f, 0.f
        },
        {
            EffectIntrospection::Field::Type::Float,
            "Time"sv,
            0.01f, 10.f, 0.01f
        },
        {
            EffectIntrospection::Field::Type::Float,
            "Damping"sv,
            0.f, 1.f, 0.f
        },
        {
            EffectIntrospection::Field::Type::Float,
            "Pre Delay"sv,
            0.f, 0.1f, 0.f
        },
        {
            EffectIntrospection::Field::Type::Float,
            "Crosstalk"sv,
            0.f, 1.f, 0.f
        }
    }
};

typedef float (amuse::EffectReverbHi::*ReverbHiGetFunc)() const;
typedef void (amuse::EffectReverbHi::*ReverbHiSetFunc)(float);
static const ReverbHiGetFunc ReverbHiGetters[] =
{
    &amuse::EffectReverbHi::getColoration,
    &amuse::EffectReverbHi::getMix,
    &amuse::EffectReverbHi::getTime,
    &amuse::EffectReverbHi::getDamping,
    &amuse::EffectReverbHi::getPreDelay,
    &amuse::EffectReverbHi::getCrosstalk
};
static const ReverbHiSetFunc ReverbHiSetters[] =
{
    &amuse::EffectReverbHi::setColoration,
    &amuse::EffectReverbHi::setMix,
    &amuse::EffectReverbHi::setTime,
    &amuse::EffectReverbHi::setDamping,
    &amuse::EffectReverbHi::setPreDelay,
    &amuse::EffectReverbHi::setCrosstalk
};

static const EffectIntrospection DelayIntrospective =
{
    amuse::EffectType::Delay,
    "Delay"sv,
    "Delay"sv,
    {
        {
            EffectIntrospection::Field::Type::UInt32x8,
            "Delay"sv,
            10, 5000, 10
        },
        {
            EffectIntrospection::Field::Type::UInt32x8,
            "Feedback"sv,
            0, 100, 0
        },
        {
            EffectIntrospection::Field::Type::UInt32x8,
            "Output"sv,
            0, 100, 0
        },
    }
};

typedef uint32_t (amuse::EffectDelay::*DelayGetFunc)(int) const;
typedef void (amuse::EffectDelay::*DelaySetFunc)(int, uint32_t);
static const DelayGetFunc DelayGetters[] =
{
    &amuse::EffectDelay::getChanDelay,
    &amuse::EffectDelay::getChanFeedback,
    &amuse::EffectDelay::getChanOutput
};
static const DelaySetFunc DelaySetters[] =
{
    &amuse::EffectDelay::setChanDelay,
    &amuse::EffectDelay::setChanFeedback,
    &amuse::EffectDelay::setChanOutput
};

static const EffectIntrospection ChorusIntrospective =
{
    amuse::EffectType::Chorus,
    "Chorus"sv,
    "Chorus"sv,
    {
        {
            EffectIntrospection::Field::Type::UInt32,
            "Base Delay"sv,
            5, 15, 5
        },
        {
            EffectIntrospection::Field::Type::UInt32,
            "Variation"sv,
            0, 5, 0
        },
        {
            EffectIntrospection::Field::Type::UInt32,
            "Period"sv,
            500, 10000, 500
        },
    }
};

typedef uint32_t (amuse::EffectChorus::*ChorusGetFunc)() const;
typedef void (amuse::EffectChorus::*ChorusSetFunc)(uint32_t);
static const ChorusGetFunc ChorusGetters[] =
{
    &amuse::EffectChorus::getBaseDelay,
    &amuse::EffectChorus::getVariation,
    &amuse::EffectChorus::getPeriod
};
static const ChorusSetFunc ChorusSetters[] =
{
    &amuse::EffectChorus::setBaseDelay,
    &amuse::EffectChorus::setVariation,
    &amuse::EffectChorus::setPeriod
};

static const EffectIntrospection* GetEffectIntrospection(amuse::EffectType type)
{
    switch (type)
    {
    case amuse::EffectType::ReverbStd:
        return &ReverbStdIntrospective;
    case amuse::EffectType::ReverbHi:
        return &ReverbHiIntrospective;
    case amuse::EffectType::Delay:
        return &DelayIntrospective;
    case amuse::EffectType::Chorus:
        return &ChorusIntrospective;
    default:
        return nullptr;
    }
}

template <typename T>
static T GetEffectParm(const amuse::EffectBaseTypeless* effect, int idx, int chanIdx)
{
    switch (effect->Isa())
    {
    case amuse::EffectType::ReverbStd:
        return (static_cast<const amuse::EffectReverbStdImp<float>*>(effect)->*ReverbStdGetters[idx])();
    case amuse::EffectType::ReverbHi:
        return (static_cast<const amuse::EffectReverbHiImp<float>*>(effect)->*ReverbHiGetters[idx])();
    case amuse::EffectType::Delay:
        return (static_cast<const amuse::EffectDelayImp<float>*>(effect)->*DelayGetters[idx])(chanIdx);
    case amuse::EffectType::Chorus:
        return (static_cast<const amuse::EffectChorusImp<float>*>(effect)->*ChorusGetters[idx])();
    default:
        return 0.f;
    }
}

template <typename T>
static void SetEffectParm(amuse::EffectBaseTypeless* effect, int idx, int chanIdx, T val)
{
    switch (effect->Isa())
    {
    case amuse::EffectType::ReverbStd:
        (static_cast<amuse::EffectReverbStdImp<float>*>(effect)->*ReverbStdSetters[idx])(val); break;
    case amuse::EffectType::ReverbHi:
        (static_cast<amuse::EffectReverbHiImp<float>*>(effect)->*ReverbHiSetters[idx])(val); break;
    case amuse::EffectType::Delay:
        (static_cast<amuse::EffectDelayImp<float>*>(effect)->*DelaySetters[idx])(chanIdx, val); break;
    case amuse::EffectType::Chorus:
        (static_cast<amuse::EffectChorusImp<float>*>(effect)->*ChorusSetters[idx])(val); break;
    default:
        break;
    }
}

static const char* ChanNames[] =
{
    QT_TRANSLATE_NOOP("Uint32X8Popup", "Front Left"),
    QT_TRANSLATE_NOOP("Uint32X8Popup", "Front Right"),
    QT_TRANSLATE_NOOP("Uint32X8Popup", "Rear Left"),
    QT_TRANSLATE_NOOP("Uint32X8Popup", "Rear Right"),
    QT_TRANSLATE_NOOP("Uint32X8Popup", "Front Center"),
    QT_TRANSLATE_NOOP("Uint32X8Popup", "LFE"),
    QT_TRANSLATE_NOOP("Uint32X8Popup", "Side Left"),
    QT_TRANSLATE_NOOP("Uint32X8Popup", "Side Right")
};

Uint32X8Popup::Uint32X8Popup(int min, int max, QWidget* parent)
: QFrame(parent, Qt::Popup)
{
    setAttribute(Qt::WA_WindowPropagation);
    setAttribute(Qt::WA_X11NetWmWindowTypeCombo);
    Uint32X8Button* combo = static_cast<Uint32X8Button*>(parent);
    QStyleOptionComboBox opt = combo->comboStyleOption();
    setFrameStyle(combo->style()->styleHint(QStyle::SH_ComboBox_PopupFrameStyle, &opt, combo));

    QGridLayout* layout = new QGridLayout;
    for (int i = 0; i < 8; ++i)
    {
        layout->addWidget(new QLabel(tr(ChanNames[i])), i, 0);
        FieldSlider* slider = new FieldSlider(this);
        m_sliders[i] = slider;
        slider->setToolTip(QStringLiteral("[%1,%2]").arg(min).arg(max));
        slider->setProperty("chanIdx", i);
        slider->setRange(min, max);
        connect(slider, SIGNAL(valueChanged(int)), this, SLOT(doValueChanged(int)));
        layout->addWidget(slider, i, 1);
    }
    setLayout(layout);
}

void Uint32X8Popup::setValue(int chanIdx, int val)
{
    m_sliders[chanIdx]->setValue(val);
}

void Uint32X8Popup::doValueChanged(int val)
{
    FieldSlider* slider = static_cast<FieldSlider*>(sender());
    int chanIdx = slider->property("chanIdx").toInt();
    emit valueChanged(chanIdx, val);
}

Uint32X8Button::Uint32X8Button(int min, int max, QWidget* parent)
: QPushButton(parent), m_popup(new Uint32X8Popup(min, max, this))
{
    connect(this, SIGNAL(pressed()), this, SLOT(onPressed()));
}

void Uint32X8Button::paintEvent(QPaintEvent*)
{
    QStylePainter painter(this);
    painter.setPen(palette().color(QPalette::Text));

    // draw the combobox frame, focusrect and selected etc.
    QStyleOptionComboBox opt = comboStyleOption();
    painter.drawComplexControl(QStyle::CC_ComboBox, opt);

    // draw the icon and text
    painter.drawControl(QStyle::CE_ComboBoxLabel, opt);
}

QStyleOptionComboBox Uint32X8Button::comboStyleOption() const
{
    QStyleOptionComboBox opt;
    opt.initFrom(this);
    opt.editable = false;
    opt.frame = true;
    opt.currentText = tr("Channels");
    return opt;
}

void Uint32X8Button::onPressed()
{
    QPoint pt = parentWidget()->mapToGlobal(pos());
    m_popup->move(pt.x(), pt.y());
    m_popup->show();
}

EffectWidget::EffectWidget(QWidget* parent, amuse::EffectBaseTypeless* effect, amuse::EffectType type)
: QWidget(parent), m_titleLabel(this), m_deleteButton(this),
  m_effect(effect), m_introspection(GetEffectIntrospection(type))
{
    QFont titleFont = m_titleLabel.font();
    titleFont.setWeight(QFont::Bold);
    m_titleLabel.setFont(titleFont);
    m_titleLabel.setForegroundRole(QPalette::Background);
    m_titleLabel.setContentsMargins(46, 0, 0, 0);
    m_titleLabel.setFixedHeight(20);
    m_numberText.setTextOption(QTextOption(Qt::AlignRight));
    m_numberText.setTextWidth(25);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_numberFont.setWeight(QFont::Bold);
    m_numberFont.setStyleHint(QFont::Monospace);
    m_numberFont.setPointSize(m_numberFont.pointSize() * 2);

    setContentsMargins(QMargins());
    setFixedHeight(100);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins(QMargins());
    mainLayout->setSpacing(0);

    QHBoxLayout* headLayout = new QHBoxLayout;
    headLayout->setContentsMargins(QMargins());
    headLayout->setSpacing(0);
    headLayout->addWidget(&m_titleLabel);

    m_deleteButton.setVisible(true);
    connect(&m_deleteButton, SIGNAL(clicked(bool)), this, SLOT(deleteClicked()));
    headLayout->addWidget(&m_deleteButton);

    mainLayout->addLayout(headLayout);
    mainLayout->addSpacing(8);

    QGridLayout* layout = new QGridLayout;
    layout->setSpacing(6);
    layout->setContentsMargins(64, 0, 12, 12);
    if (m_introspection)
    {
        m_titleLabel.setText(tr(m_introspection->m_name.data()));
        m_titleLabel.setToolTip(tr(m_introspection->m_description.data()));
        for (int f = 0; f < 7; ++f)
        {
            const EffectIntrospection::Field& field = m_introspection->m_fields[f];
            if (!field.m_name.empty())
            {
                QString fieldName = tr(field.m_name.data());
                layout->addWidget(new QLabel(fieldName, this), 0, f);
                switch (field.m_tp)
                {
                case EffectIntrospection::Field::Type::UInt32:
                {
                    FieldSlider* sb = new FieldSlider(this);
                    sb->setProperty("fieldIndex", f);
                    sb->setRange(int(field.m_min), int(field.m_max));
                    sb->setToolTip(QStringLiteral("[%1,%2]").arg(int(field.m_min)).arg(int(field.m_max)));
                    sb->setValue(GetEffectParm<uint32_t>(m_effect, f, 0));
                    connect(sb, SIGNAL(valueChanged(int)), this, SLOT(numChanged(int)));
                    layout->addWidget(sb, 1, f);
                    break;
                }
                case EffectIntrospection::Field::Type::UInt32x8:
                {
                    Uint32X8Button* sb = new Uint32X8Button(int(field.m_min), int(field.m_max), this);
                    sb->popup()->setProperty("fieldIndex", f);
                    for (int i = 0; i < 8; ++i)
                        sb->popup()->setValue(i, GetEffectParm<uint32_t>(m_effect, f, i));
                    connect(sb->popup(), SIGNAL(valueChanged(int, int)), this, SLOT(chanNumChanged(int, int)));
                    layout->addWidget(sb, 1, f);
                    break;
                }
                case EffectIntrospection::Field::Type::Float:
                {
                    FieldDoubleSlider* sb = new FieldDoubleSlider(this);
                    sb->setProperty("fieldIndex", f);
                    sb->setRange(field.m_min, field.m_max);
                    sb->setToolTip(QStringLiteral("[%1,%2]").arg(field.m_min).arg(field.m_max));
                    sb->setValue(GetEffectParm<float>(m_effect, f, 0));
                    connect(sb, SIGNAL(valueChanged(double)), this, SLOT(numChanged(double)));
                    layout->addWidget(sb, 1, f);
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
    layout->setRowMinimumHeight(1, 22);
    setLayout(mainLayout);
}

void EffectWidget::paintEvent(QPaintEvent* event)
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
    rotate.rotate(-45.0).translate(-15, 8);
    painter.setTransform(rotate);
    painter.setFont(m_numberFont);
    painter.setPen(palette().color(QPalette::Background));
    painter.drawStaticText(0, 0, m_numberText);
}

EffectWidget::EffectWidget(QWidget* parent, amuse::EffectBaseTypeless* cmd)
: EffectWidget(parent, cmd, cmd->Isa()) {}

EffectWidget::EffectWidget(QWidget* parent, amuse::EffectType type)
: EffectWidget(parent, nullptr, type) {}

void EffectWidget::numChanged(int value)
{
    SetEffectParm<uint32_t>(m_effect, sender()->property("fieldIndex").toInt(), 0, value);
}

void EffectWidget::numChanged(double value)
{
    SetEffectParm<float>(m_effect, sender()->property("fieldIndex").toInt(), 0, value);
}

void EffectWidget::chanNumChanged(int chanIdx, int value)
{
    SetEffectParm<uint32_t>(m_effect, sender()->property("fieldIndex").toInt(), chanIdx, value);
}

void EffectWidget::deleteClicked()
{
    if (m_index != -1)
        if (EffectListing* listing = getParent())
            listing->deleteEffect(m_index);
}

EffectListing* EffectWidget::getParent() const
{
    return qobject_cast<EffectListing*>(parentWidget()->parentWidget());
}

void EffectWidget::setIndex(int index)
{
    m_index = index;
    m_numberText.setText(QString::number(index));
    update();
}

void EffectWidgetContainer::animateOpen()
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

void EffectWidgetContainer::animateClosed()
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

void EffectWidgetContainer::snapOpen()
{
    if (m_animation)
        m_animation->stop();
    setMinimumHeight(200 + parentWidget()->layout()->spacing());
}

void EffectWidgetContainer::snapClosed()
{
    if (m_animation)
        m_animation->stop();
    setMinimumHeight(100);
}

void EffectWidgetContainer::animationDestroyed()
{
    m_animation = nullptr;
}

template <class..._Args>
EffectWidgetContainer::EffectWidgetContainer(QWidget* parent, _Args&&... args)
: QWidget(parent), m_effectWidget(new EffectWidget(this, std::forward<_Args>(args)...))
{
    setMinimumHeight(100);
    setContentsMargins(QMargins());
    QVBoxLayout* outerLayout = new QVBoxLayout;
    outerLayout->setAlignment(Qt::AlignBottom);
    outerLayout->setContentsMargins(QMargins());
    outerLayout->setSpacing(0);
    outerLayout->addWidget(m_effectWidget);
    setLayout(outerLayout);
}

void EffectListing::startAutoscroll(QWidget* source, QMouseEvent* event, int delta)
{
    if (m_autoscrollTimer == -1)
        m_autoscrollTimer = startTimer(50);
    m_autoscrollDelta = delta;
    m_autoscrollSource = source;
    m_autoscrollEvent = *event;
}

void EffectListing::stopAutoscroll()
{
    if (m_autoscrollTimer != -1)
    {
        killTimer(m_autoscrollTimer);
        m_autoscrollTimer = -1;
    }
    m_autoscrollDelta = 0;
    m_autoscrollSource = nullptr;
}

void EffectListing::timerEvent(QTimerEvent* event)
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

bool EffectListing::beginDrag(EffectWidget* widget)
{
    int origIdx = m_layout->indexOf(widget->parentWidget());
    if (origIdx < 0 || origIdx >= m_layout->count() - 1)
        return false;
    if (origIdx < m_layout->count() - 1)
    {
        // Animate next item open
        m_dragOpenIdx = origIdx;
        if (EffectWidgetContainer* nextItem =
                qobject_cast<EffectWidgetContainer*>(m_layout->itemAt(origIdx + 1)->widget()))
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

void EffectListing::endDrag()
{
    int insertIdx;
    if (m_dragOpenIdx != -1)
    {
        if (EffectWidgetContainer* prevItem =
                qobject_cast<EffectWidgetContainer*>(m_layout->itemAt(m_dragOpenIdx)->widget()))
            prevItem->snapClosed();
        insertIdx = m_dragOpenIdx;
        m_dragOpenIdx = -1;
    }
    else
    {
        insertIdx = m_layout->count() - 1;
    }

    if (m_prevDragOpen)
    {
        m_prevDragOpen->snapClosed();
        m_prevDragOpen = nullptr;
    }

    if (m_origIdx != insertIdx)
        std::swap(m_submix->getEffectStack()[m_origIdx], m_submix->getEffectStack()[insertIdx]);
    m_layout->insertItem(insertIdx, m_dragItem);
    m_dragItem = nullptr;
    stopAutoscroll();
    reindex();
}

void EffectListing::cancelDrag()
{
    if (m_dragOpenIdx != -1)
    {
        if (EffectWidgetContainer* prevItem =
                qobject_cast<EffectWidgetContainer*>(m_layout->itemAt(m_dragOpenIdx)->widget()))
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

void EffectListing::_moveDrag(int hoverIdx, const QPoint& pt, QWidget* source, QMouseEvent* event)
{
    QRect scrollVpRect = parentWidget()->parentWidget()->rect();
    QPoint scrollVpPoint = mapTo(parentWidget()->parentWidget(), pt);
    if (scrollVpRect.bottom() - scrollVpPoint.y() < 50)
        startAutoscroll(source, event, 10); // Scroll Down
    else if (scrollVpRect.top() - scrollVpPoint.y() > -50)
        startAutoscroll(source, event, -10);
    else
        stopAutoscroll();

    hoverIdx = std::max(0, std::min(hoverIdx, m_layout->count() - 1));
    if (hoverIdx != m_dragOpenIdx)
    {
        if (m_dragOpenIdx != -1)
            if (EffectWidgetContainer* prevItem =
                    qobject_cast<EffectWidgetContainer*>(m_layout->itemAt(m_dragOpenIdx)->widget()))
            {
                m_prevDragOpen = prevItem;
                prevItem->animateClosed();
            }
        if (EffectWidgetContainer* nextItem =
                qobject_cast<EffectWidgetContainer*>(m_layout->itemAt(hoverIdx)->widget()))
            nextItem->animateOpen();
        m_dragOpenIdx = hoverIdx;
    }
    update();
}

void EffectListing::moveDrag(EffectWidget* widget, const QPoint& pt, QWidget* source, QMouseEvent* event)
{
    EffectWidgetContainer* container = static_cast<EffectWidgetContainer*>(widget->parentWidget());
    int pitch = 100 + m_layout->spacing();
    _moveDrag((container->pos().y() - m_layout->contentsMargins().top() + pitch / 2) / pitch, pt, source, event);
}

int EffectListing::moveInsertDrag(const QPoint& pt, QWidget* source, QMouseEvent* event)
{
    int pitch = 100 + m_layout->spacing();
    _moveDrag(pt.y() / pitch, pt, source, event);
    return m_dragOpenIdx;
}

void EffectListing::insertDragout()
{
    if (m_dragOpenIdx != -1)
    {
        if (EffectWidgetContainer* prevItem =
                qobject_cast<EffectWidgetContainer*>(m_layout->itemAt(m_dragOpenIdx)->widget()))
        {
            m_prevDragOpen = prevItem;
            prevItem->animateClosed();
        }
        m_dragOpenIdx = -1;
    }
    stopAutoscroll();
}

void EffectListing::insert(amuse::EffectType type, const QString& text)
{
    int insertIdx;
    if (m_dragOpenIdx != -1)
    {
        if (EffectWidgetContainer* prevItem =
                qobject_cast<EffectWidgetContainer*>(m_layout->itemAt(m_dragOpenIdx)->widget()))
            prevItem->snapClosed();
        insertIdx = m_dragOpenIdx;
        m_dragOpenIdx = -1;
    }
    else
    {
        insertIdx = m_layout->count() - 1;
    }

    if (m_prevDragOpen)
    {
        m_prevDragOpen->snapClosed();
        m_prevDragOpen = nullptr;
    }

    std::unique_ptr<amuse::EffectBaseTypeless> newEffect;
    switch (type)
    {
    case amuse::EffectType::ReverbStd:
        newEffect = m_submix->_makeEffect<amuse::EffectReverbStd>(amuse::EffectReverbStdInfo{});
        break;
    case amuse::EffectType::ReverbHi:
        newEffect = m_submix->_makeEffect<amuse::EffectReverbHi>(amuse::EffectReverbHiInfo{});
        break;
    case amuse::EffectType::Delay:
        newEffect = m_submix->_makeEffect<amuse::EffectDelay>(amuse::EffectDelayInfo{});
        break;
    case amuse::EffectType::Chorus:
        newEffect = m_submix->_makeEffect<amuse::EffectChorus>(amuse::EffectChorusInfo{});
        break;
    default:
        break;
    }
    auto it = m_submix->getEffectStack().insert(m_submix->getEffectStack().begin() + insertIdx, std::move(newEffect));
    m_layout->insertWidget(insertIdx, new EffectWidgetContainer(this, it->get()));

    stopAutoscroll();
    reindex();
}

void EffectListing::deleteEffect(int index)
{
    QLayoutItem* item = m_layout->takeAt(index);
    m_submix->getEffectStack().erase(m_submix->getEffectStack().begin() + index);
    item->widget()->deleteLater();
    delete item;
    reindex();
}

void EffectListing::reindex()
{
    for (int i = 0; i < m_layout->count() - 1; ++i)
        if (EffectWidgetContainer* item =
                qobject_cast<EffectWidgetContainer*>(m_layout->itemAt(i)->widget()))
            item->m_effectWidget->setIndex(i);
}

void EffectListing::clear()
{
    while (m_layout->count() > 2)
    {
        QLayoutItem* item = m_layout->takeAt(0);
        item->widget()->deleteLater();
        delete item;
    }
}

bool EffectListing::loadData(amuse::Submix* submix)
{
    m_submix = submix;
    clear();
    int i = 0;
    for (auto& effect : submix->getEffectStack())
        m_layout->insertWidget(i++, new EffectWidgetContainer(this, effect.get()));
    reindex();
    update();
    return true;
}

void EffectListing::unloadData()
{
    m_submix = nullptr;
    clear();
    reindex();
    update();
}

EffectListing::EffectListing(QWidget* parent)
: QWidget(parent), m_layout(new QVBoxLayout)
{
    m_layout->addStretch();
    setLayout(m_layout);
    reindex();
}

EffectCatalogueItem::EffectCatalogueItem(amuse::EffectType type, const QString& name,
                                         const QString& doc, QWidget* parent)
: QWidget(parent), m_type(type), m_iconLab(this), m_label(name, this)
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

EffectCatalogueItem::EffectCatalogueItem(const EffectCatalogueItem& other, QWidget* parent)
: QWidget(parent), m_type(other.getType())
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

static const char* EffectStrings[] =
{
    QT_TRANSLATE_NOOP("EffectCatalogue", "Reverb Standard"),
    QT_TRANSLATE_NOOP("EffectCatalogue", "Reverb High"),
    QT_TRANSLATE_NOOP("EffectCatalogue", "Delay"),
    QT_TRANSLATE_NOOP("EffectCatalogue", "Chorus")
};

static const char* EffectDocStrings[] =
{
    QT_TRANSLATE_NOOP("EffectCatalogue", "Reverb Standard"),
    QT_TRANSLATE_NOOP("EffectCatalogue", "Reverb High"),
    QT_TRANSLATE_NOOP("EffectCatalogue", "Delay"),
    QT_TRANSLATE_NOOP("EffectCatalogue", "Chorus")
};

EffectCatalogue::EffectCatalogue(QWidget* parent)
: QTreeWidget(parent)
{
    setSelectionMode(QAbstractItemView::NoSelection);
    setColumnCount(1);
    setHeaderHidden(true);

    for (int i = 1; i < int(amuse::EffectType::EffectTypeMAX); ++i)
    {
        QTreeWidgetItem* item = new QTreeWidgetItem(this);
        setItemWidget(item, 0, new EffectCatalogueItem(amuse::EffectType(i),
                                                       tr(EffectStrings[i-1]), tr(EffectDocStrings[i-1]), this));
    }
}

void EffectCatalogue::mousePressEvent(QMouseEvent* event)
{
    QTreeWidget::mousePressEvent(event);
    event->ignore();
}

void EffectCatalogue::mouseReleaseEvent(QMouseEvent* event)
{
    QTreeWidget::mouseReleaseEvent(event);
    event->ignore();
}

void EffectCatalogue::mouseMoveEvent(QMouseEvent* event)
{
    StudioSetupWidget* editor = qobject_cast<StudioSetupWidget*>(parentWidget()->parentWidget()->parentWidget());
    if (!editor || !editor->m_draggedItem)
        QTreeWidget::mouseMoveEvent(event);
    event->ignore();
}

EffectListing* StudioSetupWidget::getCurrentListing() const
{
    return static_cast<EffectListing*>(static_cast<QScrollArea*>(m_tabs->currentWidget())->widget());
}

void StudioSetupWidget::beginCommandDrag(EffectWidget* widget, const QPoint& eventPt, const QPoint& pt)
{
    if (widget->getParent()->beginDrag(widget))
    {
        m_draggedPt = pt;
        m_draggedCmd = widget;
    }
}

void StudioSetupWidget::beginCatalogueDrag(EffectCatalogueItem* item, const QPoint& eventPt, const QPoint& pt)
{
    m_draggedPt = pt;
    m_draggedItem = new EffectCatalogueItem(*item, this);
    m_draggedItem->setGeometry(item->geometry());
    m_draggedItem->move(eventPt - m_draggedPt);
    m_draggedItem->raise();
    m_draggedItem->show();
}

void StudioSetupWidget::mousePressEvent(QMouseEvent* event)
{
    if (m_catalogue->geometry().contains(event->pos()))
    {
        QPoint fromParent1 = m_catalogue->mapFrom(this, event->pos());
        QWidget* ch = m_catalogue->childAt(fromParent1);
        if (ch)
        {
            EffectCatalogueItem* child = nullptr;
            while (ch && !(child = qobject_cast<EffectCatalogueItem*>(ch)))
                ch = ch->parentWidget();
            if (child)
            {
                QPoint fromParent2 = child->mapFrom(m_catalogue, fromParent1);
                beginCatalogueDrag(child, event->pos(), fromParent2);
            }
        }
    }
    else
    {
        EffectListing* listing = getCurrentListing();
        if (listing->parentWidget()->parentWidget()->parentWidget()->geometry().contains(event->pos()))
        {
            QPoint fromParent1 = listing->mapFrom(this, event->pos());
            QWidget* ch = listing->childAt(fromParent1);
            if (ch)
            {
                EffectWidget* child = nullptr;
                while (ch && !(child = qobject_cast<EffectWidget*>(ch)))
                    ch = ch->parentWidget();
                if (child)
                {
                    QPoint fromParent2 = child->mapFrom(listing, fromParent1);
                    beginCommandDrag(child, event->pos(), fromParent2);
                }
            }
        }
    }
}

void StudioSetupWidget::mouseReleaseEvent(QMouseEvent* event)
{
    EffectListing* listing = getCurrentListing();
    if (m_draggedItem)
    {
        amuse::EffectType type = m_draggedItem->getType();
        QString text = m_draggedItem->getText();
        m_draggedItem->deleteLater();
        m_draggedItem = nullptr;

        if (listing->parentWidget()->parentWidget()->parentWidget()->geometry().contains(event->pos()))
        {
            if (m_dragInsertIdx != -1)
                listing->insert(type, text);
            else
                listing->insertDragout();
        }
        else
        {
            listing->insertDragout();
        }
        m_dragInsertIdx = -1;
    }
    else if (m_draggedCmd)
    {
        listing->endDrag();
        m_draggedCmd = nullptr;
    }
}

void StudioSetupWidget::mouseMoveEvent(QMouseEvent* event)
{
    EffectListing* listing = getCurrentListing();
    if (m_draggedItem)
    {
        m_draggedItem->move(event->pos() - m_draggedPt);
        if (listing->parentWidget()->parentWidget()->parentWidget()->geometry().contains(event->pos()))
        {
            m_dragInsertIdx = listing->moveInsertDrag(listing->mapFrom(this, event->pos()), this, event);
        }
        else if (m_dragInsertIdx != -1)
        {
            listing->insertDragout();
            m_dragInsertIdx = -1;
        }
        m_catalogue->update();
        update();
    }
    else if (m_draggedCmd)
    {
        QPoint listingPt = listing->mapFrom(this, event->pos());
        EffectWidgetContainer* container = static_cast<EffectWidgetContainer*>(m_draggedCmd->parentWidget());
        container->move(container->x(), listingPt.y() - m_draggedPt.y());
        if (listing->parentWidget()->parentWidget()->parentWidget()->geometry().contains(event->pos()))
            listing->moveDrag(m_draggedCmd, listingPt, this, event);
        listing->update();
        update();
    }
}

void StudioSetupWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
    {
        EffectListing* listing = getCurrentListing();
        if (m_draggedItem)
        {
            m_draggedItem->deleteLater();
            m_draggedItem = nullptr;
            listing->insertDragout();
        }
        else if (m_draggedCmd)
        {
            listing->cancelDrag();
            m_draggedCmd = nullptr;
        }
    }
}

void StudioSetupWidget::hideEvent(QHideEvent* event)
{
    emit hidden();
}

void StudioSetupWidget::showEvent(QShowEvent* event)
{
    emit shown();
}

void StudioSetupWidget::updateWindowPosition()
{
    QWidget* parent = parentWidget();
    move(parent->width() / 2 - width() / 2 + parent->x(),
         parent->height() / 2 - height() / 2 + parent->y());
}

void StudioSetupWidget::catalogueDoubleClicked(QTreeWidgetItem* item, int column)
{
    if (EffectCatalogueItem* cItem = qobject_cast<EffectCatalogueItem*>(m_catalogue->itemWidget(item, column)))
    {
        amuse::EffectType type = cItem->getType();
        if (type != amuse::EffectType::Invalid)
        {
            EffectListing* listing = getCurrentListing();
            listing->insert(type, cItem->getText());
        }
    }
}

bool StudioSetupWidget::loadData(amuse::Studio* studio)
{
    m_listing[0]->loadData(&studio->getAuxA());
    m_listing[1]->loadData(&studio->getAuxB());
    return true;
}

void StudioSetupWidget::unloadData()
{
    m_listing[0]->unloadData();
    m_listing[1]->unloadData();
}

StudioSetupWidget::StudioSetupWidget(QWidget* parent)
: QWidget(parent, Qt::Tool), m_splitter(new QSplitter), m_tabs(new QTabWidget), m_catalogue(new EffectCatalogue)
{
    setWindowTitle(tr("Studio Setup"));
    setGeometry(0, 0, 900, 450);

    QScrollArea* scrollAreas[2];
    for (int i = 0; i < 2; ++i)
    {
        m_listing[i] = new EffectListing;
        QScrollArea* listingScroll = new QScrollArea;
        scrollAreas[i] = listingScroll;
        listingScroll->setWidget(m_listing[i]);
        listingScroll->setWidgetResizable(true);
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(1);
        sizePolicy.setVerticalStretch(0);
        listingScroll->setSizePolicy(sizePolicy);
        listingScroll->setMinimumWidth(350);
        m_splitter->addWidget(listingScroll);
    }
    m_tabs->addTab(scrollAreas[0], tr("Aux A"));
    m_tabs->addTab(scrollAreas[1], tr("Aux B"));
    m_splitter->addWidget(m_tabs);

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
