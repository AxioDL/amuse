#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QDialogButtonBox>

struct SoundMacroTemplateEntry {
  const char* m_name;
  size_t m_length;
  const uint32_t* m_data;
};

class NewSoundMacroDialog : public QDialog {
  Q_OBJECT
  QLineEdit m_le;
  QComboBox m_combo;
  QDialogButtonBox m_buttonBox;

public:
  explicit NewSoundMacroDialog(const QString& groupName, QWidget* parent = Q_NULLPTR);
  QString getName() const { return m_le.text(); }
  const SoundMacroTemplateEntry* getSelectedTemplate() const;
};
