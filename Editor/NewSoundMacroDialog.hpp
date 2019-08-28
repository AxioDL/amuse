#pragma once

#include <cstddef>
#include <cstdint>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>

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
  ~NewSoundMacroDialog() override;

  QString getName() const { return m_le.text(); }
  const SoundMacroTemplateEntry* getSelectedTemplate() const;
};
