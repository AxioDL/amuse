#include "CurveEditor.hpp"

#include <algorithm>
#include <array>

#include "MainWindow.hpp"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QJSValueIterator>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>

class CurveEditUndoCommand : public EditorUndoCommand {
public:
  using RedoData = std::array<uint8_t, 128>;
  using UndoData = std::array<uint8_t, 128>;

  CurveEditUndoCommand(const RedoData& redoData, bool usedExpr, amuse::ObjToken<ProjectModel::CurveNode> node)
  : EditorUndoCommand(node.get(), CurveControls::tr("Edit Curve")), m_redoData{redoData}, m_usedExpr(usedExpr) {}

  void undo() override {
    m_undid = true;
    amuse::ITable& table = **m_node.cast<ProjectModel::CurveNode>()->m_obj;
    if (table.Isa() == amuse::ITable::Type::Curve) {
      auto& curve = static_cast<amuse::Curve&>(table);
      curve.data.assign(m_undoData.cbegin(), m_undoData.cend());
    }
    EditorUndoCommand::undo();
  }

  void redo() override {
    amuse::ITable& table = **m_node.cast<ProjectModel::CurveNode>()->m_obj;
    if (table.Isa() == amuse::ITable::Type::Curve) {
      auto& curve = static_cast<amuse::Curve&>(table);
      curve.data.resize(128);
      std::copy(curve.data.cbegin(), curve.data.cend(), m_undoData.begin());
      curve.data.assign(m_redoData.cbegin(), m_redoData.cend());
    }
    if (m_undid) {
      EditorUndoCommand::redo();
    }
  }

  bool mergeWith(const QUndoCommand* other) override {
    const auto* const command = static_cast<const CurveEditUndoCommand*>(other);

    if (other->id() == id() && !m_usedExpr && !command->m_usedExpr) {
      m_redoData = command->m_redoData;
      return true;
    }

    return false;
  }
  int id() const override { return int(Id::CurveEdit); }

private:
  RedoData m_redoData;
  UndoData m_undoData;
  bool m_usedExpr;
  bool m_undid = false;
};

CurveEditor* CurveView::getEditor() const { return qobject_cast<CurveEditor*>(parentWidget()); }

void CurveView::loadData(ProjectModel::CurveNode* node) { m_node = node; }

void CurveView::unloadData() { m_node.reset(); }

ProjectModel::INode* CurveView::currentNode() const { return m_node.get(); }

void CurveView::paintEvent(QPaintEvent* ev) {
  if (!m_node) {
    return;
  }

  amuse::ITable& table = **m_node->m_obj;
  if (table.Isa() != amuse::ITable::Type::Curve) {
    return;
  }
  auto& curve = static_cast<amuse::Curve&>(table);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  const qreal deviceRatio = devicePixelRatioF();
  const qreal penWidth = std::max(std::floor(deviceRatio), 1.0) / deviceRatio;

  painter.setPen(QPen(QColor(127, 127, 127), penWidth));
  painter.setFont(m_gridFont);
  const qreal yIncrement = (height() - 16.0) / 10.0;
  for (size_t i = 0; i < m_percentTexts.size(); ++i) {
    const qreal thisY = i * yIncrement;
    const qreal textY = thisY - (i == 0 ? 2.0 : (i == 10 ? 16.0 : 8.0));
    painter.drawStaticText(QPointF(0.0, textY), m_percentTexts[i]);
    painter.drawLine(QPointF(30.0, thisY), QPointF(width(), thisY));
  }

  qreal xIncrement = (width() - 30.0) / 10.0;
  for (size_t i = 0; i < m_percentTextsCenter.size(); ++i) {
    const qreal thisX = i * xIncrement + 30.0;
    const qreal textX = thisX - (i == 10 ? 30.0 : 15.0);
    painter.drawStaticText(QPointF(textX, height() - 16.0), m_percentTextsCenter[i]);
    painter.drawLine(QPointF(thisX, 0.0), QPointF(thisX, height() - 16.0));
  }

  size_t i;
  xIncrement = (width() - 30.0) / 127.0;
  QPointF lastPt;
  painter.setPen(QPen(Qt::white, penWidth * 3.0));
  for (i = 0; i < curve.data.size(); ++i) {
    const QPointF thisPt(i * xIncrement + 30.0, (height() - 16.0) - (height() - 16.0) * (curve.data[i] / 127.0));
    if (i != 0) {
      painter.drawLine(lastPt, thisPt);
    }
    lastPt = thisPt;
  }
  for (; i < 128; ++i) {
    const QPointF thisPt(i * xIncrement + 30.0, height() - 16.0);
    if (i != 0) {
      painter.drawLine(lastPt, thisPt);
    }
    lastPt = thisPt;
  }
}

void CurveView::mousePressEvent(QMouseEvent* ev) { mouseMoveEvent(ev); }

void CurveView::mouseMoveEvent(QMouseEvent* ev) {
  CurveView* view = getEditor()->m_curveView;
  amuse::ITable& table = **view->m_node->m_obj;
  if (table.Isa() != amuse::ITable::Type::Curve) {
    return;
  }

  const qreal xIncrement = (width() - 30.0) / 127.0;
  const int idx = int(std::round((ev->localPos().x() - 30.0) / xIncrement));
  if (idx < 0 || idx > 127) {
    return;
  }
  const int val = 127 - std::clamp(0, int(std::round(ev->localPos().y() / (height() - 16.0) * 127.0)), 127);

  CurveEditUndoCommand::RedoData newData;
  auto& curve = static_cast<amuse::Curve&>(table);
  curve.data.resize(newData.size());

  std::copy(curve.data.cbegin(), curve.data.cend(), newData.begin());
  newData[idx] = uint8_t(val);

  g_MainWindow->pushUndoCommand(new CurveEditUndoCommand(newData, false, m_node));
  update();
}

CurveView::CurveView(QWidget* parent) : QWidget(parent) {
  for (size_t i = 0; i < m_percentTexts.size(); ++i) {
    m_percentTexts[i].setText(QStringLiteral("%1%").arg(100 - i * 10));
    m_percentTexts[i].setTextOption(QTextOption(Qt::AlignVCenter | Qt::AlignRight));
    m_percentTexts[i].setTextWidth(28.0);
    m_percentTextsCenter[i].setText(QStringLiteral("%1%").arg(i * 10));
    m_percentTextsCenter[i].setTextOption(QTextOption(Qt::AlignBaseline | Qt::AlignCenter));
    m_percentTextsCenter[i].setTextWidth(28.0);
  }
  m_gridFont.setPointSize(8);
}

CurveView::~CurveView() = default;

CurveEditor* CurveControls::getEditor() const { return qobject_cast<CurveEditor*>(parentWidget()); }

void CurveControls::loadData() {
  m_lineEdit->setDisabled(false);
  m_setExpr->setDisabled(false);
}

void CurveControls::unloadData() {
  m_lineEdit->setDisabled(true);
  m_setExpr->setDisabled(true);
}

void CurveControls::exprCommit() {
  CurveView* view = getEditor()->m_curveView;
  amuse::ITable& table = **view->m_node->m_obj;
  if (table.Isa() != amuse::ITable::Type::Curve) {
    return;
  }
  const QString progText = m_lineEdit->text();

  CurveEditUndoCommand::RedoData newData;
  auto& curve = static_cast<amuse::Curve&>(table);
  curve.data.resize(newData.size());
  std::copy(curve.data.cbegin(), curve.data.cend(), newData.begin());

  bool notANumber = false;
  for (size_t i = 0; i < newData.size(); ++i) {
    m_engine.globalObject().setProperty(QStringLiteral("x"), i / 127.0);
    QJSValue val = m_engine.evaluate(progText);
    if (val.isError()) {
      m_errLabel->setText(val.toString());
      return;
    } else if (val.isNumber()) {
      newData[i] = uint8_t(std::clamp(0, int(std::round(val.toNumber() * 127.0)), 127));
    } else {
      notANumber = true;
      newData[i] = 0;
    }
  }
  if (notANumber)
    m_errLabel->setText(tr("Did not evaluate as a number"));

  g_MainWindow->pushUndoCommand(new CurveEditUndoCommand(newData, true, view->m_node));
  view->update();
}

void CurveControls::resizeEvent(QResizeEvent* ev) { m_errLabel->setGeometry(22, 78, width() - 44, 14); }

CurveControls::CurveControls(QWidget* parent) : QFrame(parent) {
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  setFixedHeight(100);
  setFrameShape(QFrame::StyledPanel);
  setFrameShadow(QFrame::Sunken);
  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);

  QHBoxLayout* mainLayout = new QHBoxLayout;

  QGridLayout* leftLayout = new QGridLayout;

  leftLayout->addWidget(new QLabel(tr("Expression")), 0, 0);
  m_lineEdit = new QLineEdit;
  m_lineEdit->setDisabled(true);
  QPalette palette = m_lineEdit->palette();
  palette.setColor(QPalette::Base, palette.color(QPalette::Window));
  m_lineEdit->setPalette(palette);
  connect(m_lineEdit, &QLineEdit::returnPressed, this, &CurveControls::exprCommit);
  leftLayout->addWidget(m_lineEdit, 1, 0);

  m_setExpr = new QPushButton(tr("Apply"));
  m_setExpr->setDisabled(true);
  connect(m_setExpr, &QPushButton::clicked, this, &CurveControls::exprCommit);
  leftLayout->addWidget(m_setExpr, 1, 1);

  m_errLabel = new QLabel(this);
  QFont font = m_errLabel->font();
  font.setPointSize(8);
  m_errLabel->setFont(font);
  palette.setColor(QPalette::Text, Qt::red);
  m_errLabel->setPalette(palette);

  leftLayout->setColumnMinimumWidth(0, 500);
  leftLayout->setColumnStretch(0, 1);
  leftLayout->setColumnMinimumWidth(1, 75);
  leftLayout->setColumnStretch(1, 0);
  leftLayout->setRowMinimumHeight(0, 22);
  leftLayout->setRowMinimumHeight(1, 37);
  leftLayout->setContentsMargins(10, 6, 10, 14);

  mainLayout->addLayout(leftLayout);
  setLayout(mainLayout);

  QJSValueIterator it(m_engine.globalObject().property(QStringLiteral("Math")));
  QString docStr =
      tr("Expression interpreter mapping x:[0,1] to y:[0,1] with the following constants and functions available:\n");
  bool needsComma = false;
  while (it.hasNext()) {
    it.next();
    m_engine.globalObject().setProperty(it.name(), it.value());
    if (needsComma)
      docStr += QStringLiteral(", ");
    needsComma = true;
    docStr += it.name();
  }
  m_lineEdit->setToolTip(docStr);
}

CurveControls::~CurveControls() = default;

bool CurveEditor::loadData(ProjectModel::CurveNode* node) {
  m_curveView->loadData(node);
  m_controls->loadData();
  return true;
}

void CurveEditor::unloadData() {
  m_curveView->unloadData();
  m_controls->unloadData();
}

ProjectModel::INode* CurveEditor::currentNode() const { return m_curveView->currentNode(); }

CurveEditor::CurveEditor(QWidget* parent)
: EditorWidget(parent), m_curveView(new CurveView), m_controls(new CurveControls) {
  QVBoxLayout* layout = new QVBoxLayout;
  layout->setContentsMargins(QMargins());
  layout->setSpacing(1);
  layout->addWidget(m_curveView);
  layout->addWidget(m_controls);
  setLayout(layout);
}

CurveEditor::~CurveEditor() = default;
