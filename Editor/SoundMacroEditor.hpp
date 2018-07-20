#ifndef AMUSE_SOUND_MACRO_EDITOR_HPP
#define AMUSE_SOUND_MACRO_EDITOR_HPP

#include "EditorWidget.hpp"
#include <QStaticText>

class CommandWidget : public QWidget
{
Q_OBJECT
    QFont m_numberFont;
    QFont m_titleFont;
    QStaticText m_numberText;
    QStaticText m_titleText;
    void animateOpen();
    void animateClosed();
public:
    CommandWidget(QWidget* parent, const QString& text);

    void paintEvent(QPaintEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
};

class SoundMacroEditor : public EditorWidget
{
    Q_OBJECT
public:
    explicit SoundMacroEditor(ProjectModel::SoundMacroNode* node, QWidget* parent = Q_NULLPTR);
};


#endif //AMUSE_SOUND_MACRO_EDITOR_HPP
