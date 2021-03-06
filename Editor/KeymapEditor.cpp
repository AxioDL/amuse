#include "KeymapEditor.hpp"

#include <array>

#include <QCheckBox>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Common.hpp"
#include "KeyboardWidget.hpp"
#include "MainWindow.hpp"

constexpr std::array<int, 10> HueTable{
    0, 30, 60, 80, 120, 170, 200, 240, 280, 320,
};

constexpr std::array<int, 13> SaturationTable{
    255, 255, 255, 255, 255, 127, 127, 127, 127, 127, 63, 63, 63,
};

constexpr std::array<int, 13> ValueTable{
    240, 200, 160, 120, 80, 240, 200, 160, 120, 80, 240, 200, 160,
};

constexpr std::array<QPoint, 12> PointTable{{
    {21, 180},
    {41, 104},
    {61, 180},
    {86, 104},
    {101, 180},
    {141, 180},
    {156, 104},
    {181, 180},
    {201, 104},
    {221, 180},
    {246, 104},
    {261, 180},
}};

constexpr std::array<int, 12> RadiusTable{
    14, 10, 14, 10, 14, 14, 10, 14, 10, 14, 10, 14,
};

constexpr std::array<Qt::GlobalColor, 12> PenColorTable{
    Qt::black, Qt::white, Qt::black, Qt::white, Qt::black, Qt::black,
    Qt::white, Qt::black, Qt::white, Qt::black, Qt::white, Qt::black,
};

constexpr std::array<Qt::GlobalColor, 12> NeutralColorTable{
    Qt::white, Qt::black, Qt::white, Qt::black, Qt::white, Qt::white,
    Qt::black, Qt::white, Qt::black, Qt::white, Qt::black, Qt::white,
};

PaintButton::PaintButton(QWidget* parent) : QPushButton(parent) {
  setIcon(QIcon(QStringLiteral(":/icons/IconPaintbrush.svg")));
  setIconSize(QSize(26, 26));
  setFixedSize(46, 46);
  setToolTip(tr("Activate brush to apply values to keys"));
}

KeymapEditor* KeymapView::getEditor() const {
  return qobject_cast<KeymapEditor*>(parentWidget()->parentWidget()->parentWidget());
}

void KeymapView::loadData(ProjectModel::KeymapNode* node) { m_node = node; }

void KeymapView::unloadData() {
  m_node.reset();
  m_keyPalettes.fill(-1);
}

ProjectModel::INode* KeymapView::currentNode() const { return m_node.get(); }

void KeymapView::drawKey(QPainter& painter, const QRect& octaveRect, qreal penWidth, const PaintPalette& keyPalette,
                         int o, int k) const {
  const int keyIdx = o * 12 + k;
  const int keyPalIdx = m_keyPalettes[keyIdx];

  painter.setPen(QPen(PenColorTable[k], penWidth));
  painter.setBrush(keyPalIdx < 0 ? NeutralColorTable[k] : keyPalette[keyPalIdx]);
  painter.drawEllipse(PointTable[k] + octaveRect.topLeft(), RadiusTable[k], RadiusTable[k]);
  painter.setTransform(
      QTransform()
          .translate(PointTable[k].x() + octaveRect.left() - 13, PointTable[k].y() + octaveRect.top() - 20)
          .rotate(-90.0));
  painter.drawStaticText(QPointF{}, m_keyTexts[keyIdx]);
  painter.setTransform(QTransform());
}

void KeymapView::paintEvent(QPaintEvent* ev) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  const auto& keyPalette = getEditor()->m_paintPalette;

  const qreal deviceRatio = devicePixelRatioF();
  const qreal penWidth = std::max(std::floor(deviceRatio), 1.0) / deviceRatio;

  painter.setFont(m_keyFont);
  const int kbY = height() / 2 - 100;
  for (int o = 0; o < 10; ++o) {
    const QRect thisRect(o * 280, kbY, 280, 200);
    if (ev->rect().intersects(thisRect)) {
      m_octaveRenderer.render(&painter, thisRect);
      for (int k = 0; k < 12; ++k) {
        drawKey(painter, thisRect, penWidth, keyPalette, o, k);
      }
    }
  }

  const QRect thisRect(2800, kbY, 202, 200);
  if (ev->rect().intersects(thisRect)) {
    m_lastOctaveRenderer.render(&painter, thisRect);
    for (int k = 0; k < 8; ++k) {
      drawKey(painter, thisRect, penWidth, keyPalette, 10, k);
    }
  }
}

void KeymapView::mousePressEvent(QMouseEvent* ev) { mouseMoveEvent(ev); }

int KeymapView::getKey(const QPoint& localPos) const {
  const QPointF localPoint = m_widgetToSvg.map(localPos);
  for (size_t i = 0; i < m_sharp.size(); ++i) {
    if (m_sharp[i].contains(localPoint)) {
      return SharpKeyNumbers[i];
    }
  }
  for (size_t i = 0; i < m_natural.size(); ++i) {
    if (m_natural[i].contains(localPoint)) {
      return NaturalKeyNumbers[i];
    }
  }
  return -1;
}

void KeymapView::mouseMoveEvent(QMouseEvent* ev) {
  const int octave = ev->position().x() / 280;
  const int key = getKey(ev->pos() - QPoint(octave * 280, height() / 2 - 100));

  if (octave >= 0 && key >= 0) {
    getEditor()->touchKey(octave * 12 + key, ev->modifiers() & Qt::ShiftModifier);
  }
}

void KeymapView::wheelEvent(QWheelEvent* event) {
  if (const QScrollArea* scroll = qobject_cast<QScrollArea*>(parentWidget()->parentWidget())) {
    /* Send wheel event directly to the scroll bar */
    QApplication::sendEvent(scroll->horizontalScrollBar(), event);
  }
}

KeymapView::KeymapView(QWidget* parent)
: QWidget(parent)
, m_octaveRenderer(QStringLiteral(":/bg/keyboard.svg"))
, m_lastOctaveRenderer(QStringLiteral(":/bg/keyboard_last.svg")) {
  setMinimumWidth(3002);

  size_t k = 0;
  for (size_t i = 0; i < 11; ++i) {
    for (size_t j = 0; j < KeyStrings.size() && k < m_keyTexts.size(); ++j) {
      m_keyTexts[k++].setText(QStringLiteral("%1%2").arg(KeyStrings[j]).arg(i - 1));
    }
  }

  m_keyFont.setPointSize(12);
  m_keyPalettes.fill(-1);

  for (size_t i = 0; i < m_natural.size(); ++i) {
    const auto& naturalKeyName = NaturalKeyNames[i];

    if (m_octaveRenderer.elementExists(naturalKeyName)) {
      m_natural[i] =
          m_octaveRenderer.transformForElement(naturalKeyName).mapRect(m_octaveRenderer.boundsOnElement(naturalKeyName));
    }
  }

  for (size_t i = 0; i < m_sharp.size(); ++i) {
    const auto& sharpKeyName = SharpKeyNames[i];

    if (m_octaveRenderer.elementExists(sharpKeyName)) {
      m_sharp[i] =
          m_octaveRenderer.transformForElement(sharpKeyName).mapRect(m_octaveRenderer.boundsOnElement(sharpKeyName));
    }
  }

  m_widgetToSvg = RectToRect(QRect(0, 0, 280, 200), m_octaveRenderer.viewBoxF());
}

KeymapView::~KeymapView() = default;

KeymapEditor* KeymapControls::getEditor() const { return qobject_cast<KeymapEditor*>(parentWidget()); }

void KeymapControls::setPaintIdx(int idx) {
  QPalette palette = m_paintButton->palette();
  if (idx < 0) {
    palette.setColor(QPalette::Window, QWidget::palette().color(QPalette::Window));
    palette.setColor(QPalette::Button, QWidget::palette().color(QPalette::Button));
  } else {
    const auto& keyPalette = getEditor()->m_paintPalette;
    palette.setColor(QPalette::Window, keyPalette[idx]);
    palette.setColor(QPalette::Button, keyPalette[idx].darker(300));
  }
  m_paintButton->setPalette(palette);
}

void KeymapControls::setKeymap(const amuse::Keymap& km) {
  m_enableUpdate = false;
  const int idx = m_macro->collection()->indexOfId(km.macro.id);
  m_macro->setCurrentIndex(idx + 1);
  m_transpose->setValue(km.transpose);
  if (km.pan == -128) {
    m_pan->setDisabled(true);
    m_pan->setValue(true);
    m_surround->setChecked(true);
  } else {
    m_pan->setEnabled(true);
    m_pan->setValue(km.pan);
    m_surround->setChecked(false);
  }
  m_prioOffset->setValue(km.prioOffset);
  m_enableUpdate = true;
}

void KeymapControls::loadData(ProjectModel::KeymapNode* node) {
  m_enableUpdate = false;
  m_macro->setCollection(
      g_MainWindow->projectModel()->getGroupNode(node)->getCollectionOfType(ProjectModel::INode::Type::SoundMacro));
  m_macro->setDisabled(false);
  m_transpose->setDisabled(false);
  m_transpose->setValue(0);
  m_pan->setDisabled(false);
  m_pan->setValue(64);
  m_surround->setDisabled(false);
  m_surround->setChecked(false);
  m_prioOffset->setDisabled(false);
  m_prioOffset->setValue(0);
  m_paintButton->setDisabled(false);
  m_paintButton->setPalette(palette());
  m_enableUpdate = true;
}

void KeymapControls::unloadData() {
  m_enableUpdate = false;
  m_macro->setCollection(nullptr);
  m_macro->setDisabled(true);
  m_transpose->setDisabled(true);
  m_pan->setDisabled(true);
  m_surround->setDisabled(true);
  m_prioOffset->setDisabled(true);
  m_paintButton->setDisabled(true);
  m_paintButton->setPalette(palette());
  m_enableUpdate = true;
}

void KeymapControls::controlChanged() {
  if (m_enableUpdate) {
    amuse::Keymap km;
    km.macro.id = m_macro->currentIndex() == 0 ? amuse::SoundMacroId{}
                                               : m_macro->collection()->idOfIndex(m_macro->currentIndex() - 1);
    km.transpose = int8_t(m_transpose->value());
    km.pan = int8_t(m_pan->value());
    if (m_surround->isChecked()) {
      km.pan = -128;
      m_pan->setDisabled(true);
    } else {
      m_pan->setEnabled(true);
    }
    km.prioOffset = int8_t(m_prioOffset->value());
    getEditor()->touchControl(km);
  }
}

void KeymapControls::paintButtonPressed() {
  KeymapEditor* editor = getEditor();
  if (!editor->m_inPaint) {
    editor->setCursor(QCursor(QPixmap(QStringLiteral(":/icons/IconPaintbrush.svg")), 2, 30));
    editor->m_inPaint = true;
    m_paintButton->setDown(true);
  } else {
    editor->unsetCursor();
    editor->m_inPaint = false;
    m_paintButton->setDown(false);
  }
}

KeymapControls::KeymapControls(QWidget* parent) : QFrame(parent) {
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  setFixedHeight(100);
  setFrameShape(QFrame::StyledPanel);
  setFrameShadow(QFrame::Sunken);
  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);

  QPalette palette = QWidget::palette();
  palette.setColor(QPalette::Base, palette.color(QPalette::Window));

  QHBoxLayout* mainLayout = new QHBoxLayout;

  QGridLayout* leftLayout = new QGridLayout;

  leftLayout->addWidget(new QLabel(tr("SoundMacro")), 0, 0);
  m_macro = new FieldProjectNode;
  m_macro->setDisabled(true);
  connect(m_macro, qOverload<int>(&FieldProjectNode::currentIndexChanged), this, &KeymapControls::controlChanged);
  leftLayout->addWidget(m_macro, 1, 0);

  leftLayout->addWidget(new QLabel(tr("Transpose")), 0, 1);
  m_transpose = new QSpinBox;
  m_transpose->setPalette(palette);
  m_transpose->setDisabled(true);
  m_transpose->setRange(-128, 127);
  m_transpose->setToolTip(tr("Offset resulting MIDI note"));
  connect(m_transpose, qOverload<int>(&QSpinBox::valueChanged), this, &KeymapControls::controlChanged);
  leftLayout->addWidget(m_transpose, 1, 1);

  leftLayout->addWidget(new QLabel(tr("Pan")), 0, 2);
  m_pan = new QSpinBox;
  m_pan->setPalette(palette);
  m_pan->setDisabled(true);
  m_pan->setRange(-127, 127);
  m_pan->setToolTip(tr("Set initial pan"));
  connect(m_pan, qOverload<int>(&QSpinBox::valueChanged), this, &KeymapControls::controlChanged);
  leftLayout->addWidget(m_pan, 1, 2);

  leftLayout->addWidget(new QLabel(tr("Surround")), 0, 3);
  m_surround = new QCheckBox;
  m_surround->setPalette(palette);
  m_surround->setDisabled(true);
  m_surround->setToolTip(tr("Initially play through surround channels"));
  connect(m_surround, qOverload<int>(&QCheckBox::stateChanged), this, &KeymapControls::controlChanged);
  leftLayout->addWidget(m_surround, 1, 3);

  leftLayout->addWidget(new QLabel(tr("Prio Offset")), 0, 4);
  m_prioOffset = new QSpinBox;
  m_prioOffset->setPalette(palette);
  m_prioOffset->setDisabled(true);
  m_prioOffset->setRange(-128, 127);
  m_prioOffset->setToolTip(tr("Offset resulting priority"));
  connect(m_prioOffset, qOverload<int>(&QSpinBox::valueChanged), this, &KeymapControls::controlChanged);
  leftLayout->addWidget(m_prioOffset, 1, 4);

  leftLayout->setColumnMinimumWidth(0, 200);
  leftLayout->setColumnMinimumWidth(1, 75);
  leftLayout->setColumnMinimumWidth(2, 75);
  leftLayout->setColumnMinimumWidth(3, 50);
  leftLayout->setColumnMinimumWidth(4, 75);
  leftLayout->setRowMinimumHeight(0, 22);
  leftLayout->setRowMinimumHeight(1, 37);
  leftLayout->setContentsMargins(10, 6, 0, 14);

  QVBoxLayout* rightLayout = new QVBoxLayout;

  m_paintButton = new PaintButton;
  m_paintButton->setDisabled(true);
  connect(m_paintButton, &PaintButton::pressed, this, &KeymapControls::paintButtonPressed);
  rightLayout->addWidget(m_paintButton);
  rightLayout->setContentsMargins(0, 0, 10, 0);

  mainLayout->addLayout(leftLayout);
  mainLayout->addStretch(1);
  mainLayout->addLayout(rightLayout);
  setLayout(mainLayout);
}

KeymapControls::~KeymapControls() = default;

void KeymapEditor::_touch() {
  if (m_controlKeymap.macro.id == 0xffff)
    m_controls->setPaintIdx(-1);
  else
    m_controls->setPaintIdx(getConfigIdx(m_controlKeymap.configKey()));
}

void KeymapEditor::touchKey(int key, bool bulk) {
  if (m_inPaint) {
    if (bulk) {
      uint64_t refKey = (*m_kmView->m_node->m_obj)[key].configKey();
      for (int i = 0; i < 128; ++i) {
        amuse::Keymap& km = (*m_kmView->m_node->m_obj)[i];
        if (km.configKey() != refKey)
          continue;
        if (km.macro.id != 0xffff)
          deallocateConfigIdx(km.configKey());
        km = m_controlKeymap;
        if (km.macro.id == 0xffff)
          m_kmView->m_keyPalettes[i] = -1;
        else
          m_kmView->m_keyPalettes[i] = allocateConfigIdx(km.configKey());
      }
    } else {
      amuse::Keymap& km = (*m_kmView->m_node->m_obj)[key];
      if (km.macro.id != 0xffff)
        deallocateConfigIdx(km.configKey());
      km = m_controlKeymap;
      if (km.macro.id == 0xffff)
        m_kmView->m_keyPalettes[key] = -1;
      else
        m_kmView->m_keyPalettes[key] = allocateConfigIdx(km.configKey());
    }

    m_kmView->update();
  } else {
    amuse::Keymap& km = (*m_kmView->m_node->m_obj)[key];
    m_controlKeymap = km;
    m_controls->setKeymap(km);
    _touch();
  }
}

void KeymapEditor::touchControl(const amuse::Keymap& km) {
  m_controlKeymap = km;
  _touch();
}

int KeymapEditor::allocateConfigIdx(uint64_t key) {
  auto search = m_configToIdx.find(key);
  if (search != m_configToIdx.end()) {
    ++search->second.second;
    return search->second.first;
  }
  for (size_t i = 0; i < m_idxBitmap.size(); ++i)
    if (!m_idxBitmap[i]) {
      m_configToIdx.insert_or_assign(key, std::make_pair(int(i), 1));
      m_idxBitmap.set(i);
      return int(i);
    }
  Q_UNREACHABLE();
}

void KeymapEditor::deallocateConfigIdx(uint64_t key) {
  auto search = m_configToIdx.find(key);
  if (search != m_configToIdx.end()) {
    --search->second.second;
    if (search->second.second == 0) {
      m_idxBitmap.reset(search->second.first);
      m_configToIdx.erase(search);
    }
    return;
  }
  Q_UNREACHABLE();
}

int KeymapEditor::getConfigIdx(uint64_t key) const {
  const auto search = m_configToIdx.find(key);
  if (search != m_configToIdx.end()) {
    return search->second.first;
  }
  for (size_t i = 0; i < m_idxBitmap.size(); ++i) {
    if (!m_idxBitmap[i]) {
      return int(i);
    }
  }
  Q_UNREACHABLE();
}

bool KeymapEditor::loadData(ProjectModel::KeymapNode* node) {
  if (m_kmView->m_node.get() != node) {
    m_configToIdx.clear();
    m_idxBitmap.reset();

    for (size_t i = 0; i < m_kmView->m_keyPalettes.size(); ++i) {
      const amuse::Keymap& km = (*node->m_obj)[i];

      if (km.macro.id == 0xffff) {
        m_kmView->m_keyPalettes[i] = -1;
      } else {
        m_kmView->m_keyPalettes[i] = allocateConfigIdx(km.configKey());
      }
    }

    m_controlKeymap = amuse::Keymap();
    m_kmView->loadData(node);
    m_controls->loadData(node);
  }

  m_inPaint = false;
  m_controls->m_paintButton->setDown(false);
  unsetCursor();

  return true;
}

void KeymapEditor::unloadData() {
  m_configToIdx.clear();
  m_idxBitmap.reset();
  m_kmView->unloadData();
  m_controls->unloadData();

  m_inPaint = false;
  m_controls->m_paintButton->setDown(false);
  unsetCursor();
}

ProjectModel::INode* KeymapEditor::currentNode() const { return m_kmView->currentNode(); }

void KeymapEditor::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Escape) {
    if (m_inPaint) {
      m_inPaint = false;
      m_controls->m_paintButton->setDown(false);
      unsetCursor();
    }
  }
}

KeymapEditor::KeymapEditor(QWidget* parent)
: EditorWidget(parent), m_scrollArea(new QScrollArea), m_kmView(new KeymapView), m_controls(new KeymapControls) {
  size_t k = 0;
  for (size_t i = 0; i < ValueTable.size(); ++i) {
    for (size_t j = 0; j < HueTable.size() && k < m_paintPalette.size(); ++j) {
      m_paintPalette[k++].setHsv(HueTable[j], SaturationTable[i], ValueTable[i]);
    }
  }

  m_scrollArea->setWidget(m_kmView);
  m_scrollArea->setWidgetResizable(true);

  QVBoxLayout* layout = new QVBoxLayout;
  layout->setContentsMargins(QMargins());
  layout->setSpacing(1);
  layout->addWidget(m_scrollArea);
  layout->addWidget(m_controls);
  setLayout(layout);
}

KeymapEditor::~KeymapEditor() = default;
