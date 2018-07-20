#include "SoundMacroEditor.hpp"
#include <QLabel>
#include <QPainter>
#include <QPropertyAnimation>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QSpinBox>

CommandWidget::CommandWidget(QWidget* parent, const QString& text)
: QWidget(parent), m_numberText(text), m_titleText("Command")
{
    m_titleFont.setWeight(QFont::Bold);
    m_numberText.setTextOption(QTextOption(Qt::AlignRight));
    m_numberText.setTextWidth(25);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(100);
    m_numberFont.setWeight(QFont::Bold);
    m_numberFont.setStyleHint(QFont::Monospace);
    m_numberFont.setPointSize(16);

#if 0
    const amuse::SoundMacro::CmdIntrospection* introspection = amuse::SoundMacro::GetCmdIntrospection(cmd->Isa());
    if (introspection)
    {
        for (int f = 0; f < 7; ++f)
        {
            introspection->m_fields[f];
        }
    }
#endif


    setContentsMargins(54, 4, 0, 4);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addStretch();

    QGridLayout* layout = new QGridLayout;
    layout->addWidget(new QLabel("One"), 0, 0);
    layout->addWidget(new QSpinBox, 1, 0);
    layout->addWidget(new QLabel("Two"), 0, 1);
    layout->addWidget(new QSpinBox, 1, 1);
    layout->addWidget(new QLabel("Three"), 0, 2);
    layout->addWidget(new QSpinBox, 1, 2);
    mainLayout->addLayout(layout);

    setLayout(mainLayout);
}

void CommandWidget::animateOpen()
{
    QPropertyAnimation* animation = new QPropertyAnimation(this, "minimumHeight");
    animation->setDuration(abs(minimumHeight() - 200) * 3);
    animation->setStartValue(minimumHeight());
    animation->setEndValue(200);
    animation->setEasingCurve(QEasingCurve::InOutExpo);
    connect(animation, SIGNAL(valueChanged(const QVariant&)), parentWidget(), SLOT(update()));
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void CommandWidget::animateClosed()
{
    QPropertyAnimation* animation = new QPropertyAnimation(this, "minimumHeight");
    animation->setDuration(abs(minimumHeight() - 100) * 3);
    animation->setStartValue(minimumHeight());
    animation->setEndValue(100);
    animation->setEasingCurve(QEasingCurve::InOutExpo);
    connect(animation, SIGNAL(valueChanged(const QVariant&)), parentWidget(), SLOT(update()));
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void CommandWidget::paintEvent(QPaintEvent* event)
{
    /* Rounded frame */
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(127, 127, 127), 2.0));
    painter.setBrush(QColor(127, 127, 127));

    QTransform mainXf = QTransform::fromTranslate(0, rect().bottom() - 99);
    painter.setTransform(mainXf);
    QPoint points[] =
    {
        {1, 20},
        {1, 99},
        {width() - 1, 99},
        {width() - 1, 1},
        {20, 1},
        {1, 20}
    };
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

    QWidget::paintEvent(event);
}

void CommandWidget::mousePressEvent(QMouseEvent* event)
{
    animateOpen();
}

void CommandWidget::mouseReleaseEvent(QMouseEvent* event)
{
    animateClosed();
}

SoundMacroEditor::SoundMacroEditor(ProjectModel::SoundMacroNode* node, QWidget* parent)
: EditorWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(new CommandWidget(this, "1"));
    layout->addWidget(new CommandWidget(this, "42"));
    layout->addWidget(new CommandWidget(this, "99"));
    layout->addStretch();
    setLayout(layout);
}
