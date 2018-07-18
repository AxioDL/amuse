#include "SoundMacroEditor.hpp"
#include <QLabel>

SoundMacroEditor::SoundMacroEditor(ProjectModel::SoundMacroNode* node, QWidget* parent)
: EditorWidget(parent)
{
    QLabel* lab = new QLabel;
    lab->setText(node->m_name);
    lab->setParent(this);
}
