#include "NewSoundMacroDialog.hpp"
#include <QVBoxLayout>
#include <QLabel>
#include "amuse/Common.hpp"

static const uint32_t BasicMacro[] =
{
    0x38000000, 0xC07A1000,
    0x10000000, 0x00000000,
    0x07010001, 0x0000FFFF,
    0x11000000, 0x00000000,
    0x31000000, 0x00000000,
    0x00000000, 0x00000000,
};

static const uint32_t Looped[] =
{
    0x38000000, 0xC07A1000,
    0x10000000, 0x00000000,
    0x07010001, 0x0000FFFF,
    0x3000D08A, 0x00000000,
    0x38000000, 0xE8030000,
    0x0F000000, 0x0001E803,
    0x07000001, 0x0000E803,
    0x11000000, 0x00000000,
    0x00000000, 0x00000000,
};

static const uint32_t LoopRelease[] =
{
    0x38000000, 0xC07A1000,
    0x10000000, 0x00000000,
    0x1C000F01, 0x0001FA00,
    0x07010000, 0x0000FFFF,
    0x3000D08A, 0x00000000,
    0x0F500000, 0x00013200,
    0x07000000, 0x00003200,
    0x38000000, 0xE8030000,
    0x0F000000, 0x0001E803,
    0x07000000, 0x0000E803,
    0x11000000, 0x00000000,
    0x00000000, 0x00000000,
};

static const uint32_t LoopSoftRelease[] =
{
    0x38000000, 0xC07A1000,
    0x10000000, 0x00000000,
    0x1C000F01, 0x0001C800,
    0x07010000, 0x0000FFFF,
    0x3000D08A, 0x00000000,
    0x0F600000, 0x00016400,
    0x07000000, 0x00006400,
    0x38000000, 0xE8030000,
    0x0F000000, 0x0001E803,
    0x07000000, 0x0000E803,
    0x11000000, 0x00000000,
    0x00000000, 0x00000000,
};

static const uint32_t LoopSoftReleaseNoClick[] =
{
    0x38000000, 0xC07A1000,
    0x147F0000, 0x00010100,
    0x10000000, 0x00000000,
    0x1C000F01, 0x0001C800,
    0x07010000, 0x0000FFFF,
    0x3000D08A, 0x00000000,
    0x0F600000, 0x00016400,
    0x07000000, 0x00006400,
    0x38000000, 0xE8030000,
    0x0F000000, 0x0001E803,
    0x07000000, 0x0000E803,
    0x11000000, 0x00000000,
    0x00000000, 0x00000000,
};

static const uint32_t LoopADSR[] =
{
    0x38000000, 0xC07A1000,
    0x0C000000, 0x00000000,
    0x10000000, 0x00000000,
    0x1C000F01, 0x0001C800,
    0x07010001, 0x0000FFFF,
    0x3000D08A, 0x00000000,
    0x12000000, 0x00000000,
    0x07000001, 0x0000E803,
    0x38000000, 0xE8030000,
    0x07000001, 0x0000E803,
    0x00000000, 0x00000000,
};

static const uint32_t LoopADSRSoftRelease[] =
{
    0x38000000, 0xC07A1000,
    0x0C000000, 0x00000000,
    0x10000000, 0x00000000,
    0x1C000F01, 0x0001C800,
    0x07010001, 0x0000FFFF,
    0x3000D08A, 0x00000000,
    0x0F600000, 0x00017800,
    0x07000001, 0x00007800,
    0x12000000, 0x00000000,
    0x38000000, 0xE8030000,
    0x07000001, 0x0000E803,
    0x00000000, 0x00000000,
};

static const uint32_t LoopHold[] =
{
    0x38000000, 0xC07A1000,
    0x10000000, 0x00000000,
    0x1C000F01, 0x0001C800,
    0x07010001, 0x0000FFFF,
    0x3000D08A, 0x00000000,
    0x0F600000, 0x00016400,
    0x07000001, 0x00006400,
    0x38000000, 0xE8030000,
    0x0F000000, 0x0001E803,
    0x07000001, 0x0000E803,
    0x11000000, 0x00000000,
    0x00000000, 0x00000000,
};

static const uint32_t OneShot[] =
{
    0x38000000, 0xC07A1000,
    0x10000000, 0x00000000,
    0x07000001, 0x0000FFFF,
    0x31000000, 0x00000000,
    0x00000000, 0x00000000,
};

static const uint32_t OneShotFixedNote[] =
{
    0x38000000, 0xC07A1000,
    0x193C0000, 0x00010000,
    0x10000000, 0x00000000,
    0x07000001, 0x0000FFFF,
    0x31000000, 0x00000000,
    0x00000000, 0x00000000,
};

static const uint32_t OneShotNoClick[] =
{
    0x38000000, 0xC07A1000,
    0x147F0000, 0x00010100,
    0x10000000, 0x00000000,
    0x07000001, 0x0000FFFF,
    0x31000000, 0x00000000,
    0x00000000, 0x00000000,
};

static const uint32_t OneShotFixedNoClick[] =
{
    0x38000000, 0xC07A1000,
    0x147F0000, 0x00010100,
    0x193C0000, 0x00010000,
    0x10000000, 0x00000000,
    0x07000001, 0x0000FFFF,
    0x31000000, 0x00000000,
    0x00000000, 0x00000000,
};

static const uint32_t Bubbles[] =
{
    0x38000000, 0xC07A1000,
    0x0D600000, 0x00010000,
    0x10000000, 0x00000000,
    0x1D08E803, 0x00010000,
    0x1E0544FD, 0x00010000,
    0x0F000000, 0x0001F401,
    0x07000000, 0x0000F401,
    0x11000000, 0x00000000,
    0x31000000, 0x00000000,
    0x00000000, 0x00000000,
};

static const uint32_t DownTrigger[] =
{
    0x38000000, 0xC07A1000,
    0x10000000, 0x00000000,
    0x07000000, 0x00001200,
    0x11000000, 0x00000000,
    0x07000000, 0x00000100,
    0x18FE0000, 0x00010000,
    0x05000000, 0x01000C00,
    0x0F000000, 0x00016400,
    0x07000001, 0x00006400,
    0x31000000, 0x00000000,
    0x00000000, 0x00000000,
};

static const uint32_t LongFadeInAndStop[] =
{
    0x38000000, 0xC07A1000,
    0x147F0000, 0x0001E803,
    0x10000000, 0x00000000,
    0x07000001, 0x0000E803,
    0x11000000, 0x00000000,
    0x31000000, 0x00000000,
    0x00000000, 0x00000000,
};

static const uint32_t FadeInAndStop[] =
{
    0x38000000, 0xC07A1000,
    0x147F0000, 0x0001C800,
    0x10000000, 0x00000000,
    0x07000001, 0x0000C800,
    0x11000000, 0x00000000,
    0x31000000, 0x00000000,
    0x00000000, 0x00000000,
};

static const uint32_t RandomTrigger[] =
{
    0x38000000, 0xC07A1000,
    0x0F000000, 0x0001F401,
    0x195A0000, 0x00010000,
    0x10000000, 0x00000000,
    0x1750005A, 0x01000000,
    0x07000001, 0x00002300,
    0x05000000, 0x03001400,
    0x11000000, 0x00000000,
    0x31000000, 0x00000000,
    0x00000000, 0x00000000,
};

static const uint32_t SimplePlaySample[] =
{
    0x10000000, 0x00000000,
    0x00000000, 0x00000000,
};

static const SoundMacroTemplateEntry Entries[] =
{
    {QT_TRANSLATE_NOOP("NewSoundMacroDialog", "Basic Macro"), sizeof(BasicMacro), BasicMacro},
    {QT_TRANSLATE_NOOP("NewSoundMacroDialog", "Looped"), sizeof(Looped), Looped},
    {QT_TRANSLATE_NOOP("NewSoundMacroDialog", "Loop Release"), sizeof(LoopRelease), LoopRelease},
    {QT_TRANSLATE_NOOP("NewSoundMacroDialog", "Loop Soft Release"), sizeof(LoopSoftRelease), LoopSoftRelease},
    {QT_TRANSLATE_NOOP("NewSoundMacroDialog", "Loop Soft Release No Click"), sizeof(LoopSoftReleaseNoClick), LoopSoftReleaseNoClick},
    {QT_TRANSLATE_NOOP("NewSoundMacroDialog", "Loop ADSR"), sizeof(LoopADSR), LoopADSR},
    {QT_TRANSLATE_NOOP("NewSoundMacroDialog", "Loop ADSR Soft Release"), sizeof(LoopADSRSoftRelease), LoopADSRSoftRelease},
    {QT_TRANSLATE_NOOP("NewSoundMacroDialog", "Loop Hold"), sizeof(LoopHold), LoopHold},
    {QT_TRANSLATE_NOOP("NewSoundMacroDialog", "One-Shot"), sizeof(OneShot), OneShot},
    {QT_TRANSLATE_NOOP("NewSoundMacroDialog", "One-Shot Fixed Note"), sizeof(OneShotFixedNote), OneShotFixedNote},
    {QT_TRANSLATE_NOOP("NewSoundMacroDialog", "One-Shot No Click"), sizeof(OneShotNoClick), OneShotNoClick},
    {QT_TRANSLATE_NOOP("NewSoundMacroDialog", "One-Shot Fixed No Click"), sizeof(OneShotFixedNoClick), OneShotFixedNoClick},
    {QT_TRANSLATE_NOOP("NewSoundMacroDialog", "Bubbles"), sizeof(Bubbles), Bubbles},
    {QT_TRANSLATE_NOOP("NewSoundMacroDialog", "Down Trigger"), sizeof(DownTrigger), DownTrigger},
    {QT_TRANSLATE_NOOP("NewSoundMacroDialog", "Long Fade in and Stop"), sizeof(LongFadeInAndStop), LongFadeInAndStop},
    {QT_TRANSLATE_NOOP("NewSoundMacroDialog", "Fade in and Stop"), sizeof(FadeInAndStop), FadeInAndStop},
    {QT_TRANSLATE_NOOP("NewSoundMacroDialog", "Random Trigger"), sizeof(RandomTrigger), RandomTrigger},
    {QT_TRANSLATE_NOOP("NewSoundMacroDialog", "Simple Play Sample"), sizeof(SimplePlaySample), SimplePlaySample}
};

const SoundMacroTemplateEntry* NewSoundMacroDialog::getSelectedTemplate() const
{
    return &Entries[m_combo.currentIndex()];
}

NewSoundMacroDialog::NewSoundMacroDialog(const QString& groupName, QWidget* parent)
: QDialog(parent),
  m_le(QString::fromStdString(amuse::SoundMacroId::CurNameDB->generateDefaultName(amuse::NameDB::Type::SoundMacro))),
  m_buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal)
{
    setWindowTitle(tr("New Sound Macro"));

    int idx = 0;
    for (const auto& ent : Entries)
        m_combo.addItem(tr(ent.m_name), idx++);
    m_combo.setCurrentIndex(0);

    QObject::connect(&m_buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    QObject::connect(&m_buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout* layout = new QVBoxLayout;

    layout->addWidget(new QLabel(tr("What should the new macro in %1 be named?").arg(groupName)));
    layout->addWidget(&m_le);
    layout->addWidget(new QLabel(tr("Sound Macro Template")));
    layout->addWidget(&m_combo);
    layout->addWidget(&m_buttonBox);

    setLayout(layout);
}
