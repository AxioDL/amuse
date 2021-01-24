#include "SongGroupEditor.hpp"
#include "MainWindow.hpp"
#include "amuse/SongConverter.hpp"

class PageDataChangeUndoCommand : public EditorUndoCommand {
  bool m_drum;
  uint8_t m_prog;
  int m_column;
  int m_undoVal, m_redoVal;
  bool m_undid = false;

public:
  explicit PageDataChangeUndoCommand(ProjectModel::SongGroupNode* node, const QString& text, bool drum, uint8_t prog,
                                     int column, int redoVal)
  : EditorUndoCommand(node, text), m_drum(drum), m_prog(prog), m_column(column), m_redoVal(redoVal) {}
  void undo() override {
    m_undid = true;
    amuse::SongGroupIndex& index = *static_cast<ProjectModel::SongGroupNode*>(m_node.get())->m_index;
    auto& map = m_drum ? index.m_drumPages : index.m_normPages;
    amuse::SongGroupIndex::PageEntry& entry = map[m_prog];

    switch (m_column) {
    case 0: {
      if (m_prog == m_undoVal)
        break;
#if __APPLE__
      auto search = map.find(m_prog);
      std::swap(map[m_undoVal], search->second);
      map.erase(search);
#else
      auto nh = map.extract(m_prog);
      nh.key() = m_undoVal;
      map.insert(std::move(nh));
#endif
      m_prog = m_undoVal;
      break;
    }
    case 1:
      entry.objId.id = m_undoVal;
      break;
    case 2:
      entry.priority = m_undoVal;
      break;
    case 3:
      entry.maxVoices = m_undoVal;
      break;
    default:
      break;
    }

    EditorUndoCommand::undo();
  }
  void redo() override {
    amuse::SongGroupIndex& index = *static_cast<ProjectModel::SongGroupNode*>(m_node.get())->m_index;
    auto& map = m_drum ? index.m_drumPages : index.m_normPages;
    amuse::SongGroupIndex::PageEntry& entry = map[m_prog];

    switch (m_column) {
    case 0: {
      m_undoVal = m_prog;
      if (m_prog == m_redoVal)
        break;
#if __APPLE__
      auto search = map.find(m_prog);
      std::swap(map[m_redoVal], search->second);
      map.erase(search);
#else
      auto nh = map.extract(m_prog);
      nh.key() = m_redoVal;
      map.insert(std::move(nh));
#endif
      m_prog = m_redoVal;
      break;
    }
    case 1:
      m_undoVal = entry.objId.id.id;
      entry.objId.id = m_redoVal;
      break;
    case 2:
      m_undoVal = entry.priority;
      entry.priority = m_redoVal;
      break;
    case 3:
      m_undoVal = entry.maxVoices;
      entry.maxVoices = m_redoVal;
      break;
    default:
      break;
    }

    if (m_undid)
      EditorUndoCommand::redo();
  }
};

class SetupDataChangeUndoCommand : public EditorUndoCommand {
  amuse::SongId m_song;
  int m_row, m_column;
  int m_undoVal, m_redoVal;
  bool m_undid = false;

public:
  explicit SetupDataChangeUndoCommand(ProjectModel::SongGroupNode* node, const QString& text, amuse::SongId song,
                                      int row, int column, int redoVal)
  : EditorUndoCommand(node, text), m_song(song), m_row(row), m_column(column), m_redoVal(redoVal) {}
  void undo() override {
    m_undid = true;
    amuse::SongGroupIndex& index = *static_cast<ProjectModel::SongGroupNode*>(m_node.get())->m_index;
    auto& map = index.m_midiSetups;
    std::array<amuse::SongGroupIndex::MIDISetup, 16>& entry = map[m_song];

    switch (m_column) {
    case 0:
      entry[m_row].programNo = m_undoVal;
      break;
    case 2:
      entry[m_row].volume = m_undoVal;
      break;
    case 3:
      entry[m_row].panning = m_undoVal;
      break;
    case 4:
      entry[m_row].reverb = m_undoVal;
      break;
    case 5:
      entry[m_row].chorus = m_undoVal;
      break;
    default:
      break;
    }

    EditorUndoCommand::undo();
  }
  void redo() override {
    amuse::SongGroupIndex& index = *static_cast<ProjectModel::SongGroupNode*>(m_node.get())->m_index;
    auto& map = index.m_midiSetups;
    std::array<amuse::SongGroupIndex::MIDISetup, 16>& entry = map[m_song];

    switch (m_column) {
    case 0:
      m_undoVal = entry[m_row].programNo;
      entry[m_row].programNo = m_redoVal;
      break;
    case 2:
      m_undoVal = entry[m_row].volume;
      entry[m_row].volume = m_redoVal;
      break;
    case 3:
      m_undoVal = entry[m_row].panning;
      entry[m_row].panning = m_redoVal;
      break;
    case 4:
      m_undoVal = entry[m_row].reverb;
      entry[m_row].reverb = m_redoVal;
      break;
    case 5:
      m_undoVal = entry[m_row].chorus;
      entry[m_row].chorus = m_redoVal;
      break;
    default:
      break;
    }

    if (m_undid)
      EditorUndoCommand::redo();
  }
};

class SongNameChangeUndoCommand : public EditorUndoCommand {
  amuse::SongId m_song;
  std::string m_undoVal, m_redoVal;
  bool m_undid = false;

public:
  explicit SongNameChangeUndoCommand(ProjectModel::SongGroupNode* node, const QString& text, amuse::SongId song,
                                     std::string_view redoVal)
  : EditorUndoCommand(node, text), m_song(song), m_redoVal(redoVal) {}
  void undo() override {
    m_undid = true;
    g_MainWindow->projectModel()->setIdDatabases(m_node.get());
    amuse::SongGroupIndex& index = *static_cast<ProjectModel::SongGroupNode*>(m_node.get())->m_index;
    auto& map = index.m_midiSetups;

    amuse::SongId newId = g_MainWindow->projectModel()->exchangeSongId(m_song, m_undoVal);

    if (m_song != newId) {
#if __APPLE__
      auto search = map.find(m_song);
      std::swap(map[newId], search->second);
      map.erase(search);
#else
      auto nh = map.extract(m_song);
      nh.key() = newId;
      map.insert(std::move(nh));
#endif
      m_song = newId;
    }

    EditorUndoCommand::undo();
  }
  void redo() override {
    g_MainWindow->projectModel()->setIdDatabases(m_node.get());
    amuse::SongGroupIndex& index = *static_cast<ProjectModel::SongGroupNode*>(m_node.get())->m_index;
    auto& map = index.m_midiSetups;

    m_undoVal = amuse::SongId::CurNameDB->resolveNameFromId(m_song);
    amuse::SongId newId = g_MainWindow->projectModel()->exchangeSongId(m_song, m_redoVal);

    if (m_song != newId) {
#if __APPLE__
      auto search = map.find(m_song);
      std::swap(map[newId], search->second);
      map.erase(search);
#else
      auto nh = map.extract(m_song);
      nh.key() = newId;
      map.insert(std::move(nh));
#endif
      m_song = newId;
    }

    if (m_undid)
      EditorUndoCommand::redo();
  }
};

class SongMIDIPathChangeUndoCommand : public EditorUndoCommand {
  amuse::SongId m_song;
  QString m_undoVal, m_redoVal;
  bool m_undid = false;

public:
  explicit SongMIDIPathChangeUndoCommand(ProjectModel::SongGroupNode* node, const QString& text, amuse::SongId song,
                                         QString redoVal)
  : EditorUndoCommand(node, text), m_song(song), m_redoVal(redoVal) {}
  void undo() override {
    m_undid = true;
    g_MainWindow->projectModel()->setMIDIPathOfSong(m_song, m_undoVal);
    EditorUndoCommand::undo();
  }
  void redo() override {
    m_undoVal = g_MainWindow->projectModel()->getMIDIPathOfSong(m_song);
    g_MainWindow->projectModel()->setMIDIPathOfSong(m_song, m_redoVal);
    if (m_undid)
      EditorUndoCommand::redo();
  }
};

PageObjectDelegate::PageObjectDelegate(QObject* parent) : BaseObjectDelegate(parent) {}

PageObjectDelegate::~PageObjectDelegate() = default;

ProjectModel::INode* PageObjectDelegate::getNode(const QAbstractItemModel* __model, const QModelIndex& index) const {
  const PageModel* model = static_cast<const PageModel*>(__model);
  auto entry = model->m_sorted[index.row()];
  if (entry->second.objId.id.id == 0xffff)
    return nullptr;
  ProjectModel::GroupNode* group = g_MainWindow->projectModel()->getGroupNode(model->m_node.get());
  return group->pageObjectNodeOfId(entry->second.objId.id);
}

QWidget* PageObjectDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                          const QModelIndex& index) const {
  const PageModel* model = static_cast<const PageModel*>(index.model());
  ProjectModel::GroupNode* group = g_MainWindow->projectModel()->getGroupNode(model->m_node.get());
  EditorFieldPageObjectNode* cb = new EditorFieldPageObjectNode(group, parent);
  connect(cb, &EditorFieldPageObjectNode::currentIndexChanged, this, &PageObjectDelegate::objIndexChanged);
  return cb;
}

void PageObjectDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
  int idx = 0;
  if (ProjectModel::BasePoolObjectNode* node =
          static_cast<ProjectModel::BasePoolObjectNode*>(getNode(index.model(), index)))
    idx = g_MainWindow->projectModel()
              ->getPageObjectProxy()
              ->mapFromSource(g_MainWindow->projectModel()->index(node))
              .row();
  static_cast<EditorFieldPageObjectNode*>(editor)->setCurrentIndex(idx);
  if (static_cast<EditorFieldPageObjectNode*>(editor)->shouldPopupOpen())
    QApplication::postEvent(editor, new QEvent(QEvent::User));
}

void PageObjectDelegate::setModelData(QWidget* editor, QAbstractItemModel* m, const QModelIndex& index) const {
  const PageModel* model = static_cast<const PageModel*>(m);
  auto entry = model->m_sorted[index.row()];
  ProjectModel::BasePoolObjectNode* node = static_cast<EditorFieldPageObjectNode*>(editor)->currentNode();
  amuse::ObjectId id;
  if (node)
    id = node->id();
  if (id == entry->second.objId.id) {
    emit m->dataChanged(index, index);
    return;
  }
  g_MainWindow->pushUndoCommand(new PageDataChangeUndoCommand(
      model->m_node.get(), tr("Change %1").arg(m->headerData(1, Qt::Horizontal).toString()), model->m_drum,
      entry->first, 1, id.id));
  emit m->dataChanged(index, index);
}

void PageObjectDelegate::objIndexChanged() { emit commitData(static_cast<QWidget*>(sender())); }

void MIDIFileFieldWidget::buttonPressed() {
  if (m_le.text().isEmpty())
    m_dialog.setDirectory(g_MainWindow->projectModel()->dir());
  else
    m_dialog.setDirectory(QFileInfo(g_MainWindow->projectModel()->dir().absoluteFilePath(m_le.text())).path());
  m_dialog.open(this, SLOT(fileDialogOpened(const QString&)));
}

void MIDIFileFieldWidget::fileDialogOpened(const QString& path) {
  m_le.setText(g_MainWindow->projectModel()->dir().relativeFilePath(path));
  emit pathChanged();
}

MIDIFileFieldWidget::MIDIFileFieldWidget(QWidget* parent)
: QWidget(parent)
, m_button(tr("Browse"))
, m_dialog(this, tr("Open Song File"), {}, QStringLiteral("Songs(*.mid *.son *.sng)")) {
  QHBoxLayout* layout = new QHBoxLayout;
  layout->addWidget(&m_le);
  layout->addWidget(&m_button);
  layout->setContentsMargins(QMargins());
  layout->setSpacing(0);
  setLayout(layout);

  connect(&m_le, &QLineEdit::returnPressed, this, &MIDIFileFieldWidget::pathChanged);
  connect(&m_button, &QPushButton::clicked, this, &MIDIFileFieldWidget::buttonPressed);

  m_dialog.setFileMode(QFileDialog::ExistingFile);
  m_dialog.setAcceptMode(QFileDialog::AcceptOpen);
}

MIDIFileFieldWidget::~MIDIFileFieldWidget() = default;

QWidget* MIDIFileDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                        const QModelIndex& index) const {
  MIDIFileFieldWidget* field = new MIDIFileFieldWidget(parent);
  connect(field, &MIDIFileFieldWidget::pathChanged, this, &MIDIFileDelegate::pathChanged);
  return field;
}

void MIDIFileDelegate::destroyEditor(QWidget* editor, const QModelIndex& index) const {
  QTableView* table = static_cast<QTableView*>(editor->parentWidget()->parentWidget());
  editor->deleteLater();
  emit const_cast<QAbstractItemModel*>(index.model())->dataChanged(index, index);
  table->setCurrentIndex(index);
}

void MIDIFileDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
  MIDIFileFieldWidget* widget = static_cast<MIDIFileFieldWidget*>(editor);
  const SetupListModel* model = static_cast<const SetupListModel*>(index.model());
  auto entry = model->m_sorted[index.row()];
  widget->setPath(g_MainWindow->projectModel()->getMIDIPathOfSong(entry->first));
}

void MIDIFileDelegate::setModelData(QWidget* editor, QAbstractItemModel* m, const QModelIndex& index) const {
  MIDIFileFieldWidget* widget = static_cast<MIDIFileFieldWidget*>(editor);
  const SetupListModel* model = static_cast<const SetupListModel*>(index.model());
  auto entry = model->m_sorted[index.row()];
  if (g_MainWindow->projectModel()->getMIDIPathOfSong(entry->first) == widget->path()) {
    emit m->dataChanged(index, index);
    return;
  }
  g_MainWindow->pushUndoCommand(
      new SongMIDIPathChangeUndoCommand(model->m_node.get(), tr("Change MIDI Path"), entry->first, widget->path()));
  emit m->dataChanged(index, index);
}

bool MIDIFileDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                                   const QModelIndex& index) {
  if (event->type() == QEvent::MouseButtonPress) {
    QMouseEvent* ev = static_cast<QMouseEvent*>(event);
    if (ev->button() == Qt::RightButton) {
      QString path = index.data().toString();
      if (path.isEmpty())
        return false;

      ContextMenu* menu = new ContextMenu;

      QAction* midiAction = new QAction(tr("Save As MIDI"), menu);
      midiAction->setData(path);
      midiAction->setIcon(QIcon::fromTheme(QStringLiteral("file-save")));
      connect(midiAction, &QAction::triggered, this, &MIDIFileDelegate::doExportMIDI);
      menu->addAction(midiAction);

      QAction* sngAction = new QAction(tr("Save As SNG"), menu);
      sngAction->setData(path);
      sngAction->setIcon(QIcon::fromTheme(QStringLiteral("file-save")));
      connect(sngAction, &QAction::triggered, this, &MIDIFileDelegate::doExportSNG);
      menu->addAction(sngAction);

      menu->popup(ev->globalPosition().toPoint());
    }
  }
  return false;
}

void MIDIFileDelegate::doExportMIDI() {
  QAction* act = static_cast<QAction*>(sender());
  QString path = act->data().toString();
  if (path.isEmpty())
    return;

  QString inPath = g_MainWindow->projectModel()->dir().absoluteFilePath(path);
  m_exportData.clear();
  {
    QFile fi(inPath);
    if (!fi.open(QFile::ReadOnly)) {
      g_MainWindow->uiMessenger().critical(tr("File Error"),
                                           tr("Unable to open %1 for reading: %1").arg(inPath).arg(fi.errorString()));
      return;
    }
    auto d = fi.readAll();
    m_exportData.resize(d.size());
    memcpy(&m_exportData[0], d.data(), d.size());
  }

  if (!memcmp(m_exportData.data(), "MThd", 4)) {
    // data = amuse::SongConverter::MIDIToSong(data, 1, true);
  } else {
    bool isBig;
    if (amuse::SongState::DetectVersion(m_exportData.data(), isBig) < 0) {
      g_MainWindow->uiMessenger().critical(tr("File Error"), tr("Invalid song data at %1").arg(inPath));
      return;
    }
    int version;
    m_exportData = amuse::SongConverter::SongToMIDI(m_exportData.data(), version, isBig);
  }

  QFileInfo inInfo(inPath);
  m_fileDialogMid.setDirectory(QFileInfo(inInfo.path(), inInfo.completeBaseName() + QStringLiteral(".mid")).filePath());
  m_fileDialogMid.setAcceptMode(QFileDialog::AcceptSave);
  m_fileDialogMid.setFileMode(QFileDialog::AnyFile);
  m_fileDialogMid.setOption(QFileDialog::ShowDirsOnly, false);
  m_fileDialogMid.open(this, SLOT(_doExportMIDI(const QString&)));
}

void MIDIFileDelegate::_doExportMIDI(const QString& path) {
  m_fileDialogMid.close();
  if (path.isEmpty())
    return;
  QFile fo(path);
  if (!fo.open(QFile::WriteOnly)) {
    g_MainWindow->uiMessenger().critical(tr("File Error"),
                                         tr("Unable to open %1 for writing: %1").arg(path).arg(fo.errorString()));
    return;
  }
  fo.write((char*)m_exportData.data(), m_exportData.size());
}

void MIDIFileDelegate::doExportSNG() {
  QAction* act = static_cast<QAction*>(sender());
  QString path = act->data().toString();
  if (path.isEmpty())
    return;

  QString inPath = g_MainWindow->projectModel()->dir().absoluteFilePath(path);
  m_exportData.clear();
  {
    QFile fi(inPath);
    if (!fi.open(QFile::ReadOnly)) {
      g_MainWindow->uiMessenger().critical(tr("File Error"),
                                           tr("Unable to open %1 for reading: %1").arg(inPath).arg(fi.errorString()));
      return;
    }
    auto d = fi.readAll();
    m_exportData.resize(d.size());
    memcpy(&m_exportData[0], d.data(), d.size());
  }

  if (!memcmp(m_exportData.data(), "MThd", 4)) {
    m_exportData = amuse::SongConverter::MIDIToSong(m_exportData, 1, true);
    if (m_exportData.empty()) {
      g_MainWindow->uiMessenger().critical(tr("File Error"), tr("Invalid MIDI data at %1").arg(inPath));
      return;
    }
  } else {
    bool isBig;
    if (amuse::SongState::DetectVersion(m_exportData.data(), isBig) < 0) {
      g_MainWindow->uiMessenger().critical(tr("File Error"), tr("Invalid song data at %1").arg(inPath));
      return;
    }
  }

  QFileInfo inInfo(inPath);
  m_fileDialogSng.setDirectory(QFileInfo(inInfo.path(), inInfo.completeBaseName() + QStringLiteral(".sng")).filePath());
  m_fileDialogSng.setAcceptMode(QFileDialog::AcceptSave);
  m_fileDialogSng.setFileMode(QFileDialog::AnyFile);
  m_fileDialogSng.setOption(QFileDialog::ShowDirsOnly, false);
  m_fileDialogSng.open(this, SLOT(_doExportSNG(const QString&)));
}

void MIDIFileDelegate::_doExportSNG(const QString& path) {
  m_fileDialogSng.close();
  if (path.isEmpty())
    return;
  QFile fo(path);
  if (!fo.open(QFile::WriteOnly)) {
    g_MainWindow->uiMessenger().critical(tr("File Error"),
                                         tr("Unable to open %1 for writing: %1").arg(path).arg(fo.errorString()));
    return;
  }
  fo.write((char*)m_exportData.data(), m_exportData.size());
}

void MIDIFileDelegate::pathChanged() { emit commitData(static_cast<MIDIFileFieldWidget*>(sender())); }

MIDIFileDelegate::MIDIFileDelegate(SetupTableView* parent)
: QStyledItemDelegate(parent)
, m_fileDialogMid(parent, tr("Export MIDI"), {}, tr("MIDI(*.mid)"))
, m_fileDialogSng(parent, tr("Export Song"), {}, tr("Song(*.sng)")) {}

MIDIFileDelegate::~MIDIFileDelegate() = default;

std::unordered_map<uint8_t, amuse::SongGroupIndex::PageEntry>& PageModel::_getMap() const {
  return m_drum ? m_node->m_index->m_drumPages : m_node->m_index->m_normPages;
}

void PageModel::_buildSortedList() {
  m_sorted.clear();
  if (!m_node)
    return;
  auto& map = _getMap();
  m_sorted.reserve(map.size());
  for (auto it = map.begin(); it != map.end(); ++it)
    m_sorted.emplace_back(it);
  std::sort(m_sorted.begin(), m_sorted.end());
}

QModelIndex PageModel::_indexOfProgram(uint8_t prog) const {
  auto search = std::lower_bound(m_sorted.cbegin(), m_sorted.cend(), prog);
  if (search == m_sorted.cend() || search->m_it->first != prog)
    return QModelIndex();
  else
    return createIndex(search - m_sorted.begin(), 0);
}

int PageModel::_hypotheticalIndexOfProgram(uint8_t prog) const {
  auto search = std::lower_bound(m_sorted.cbegin(), m_sorted.cend(), prog);
  return search - m_sorted.begin();
}

void PageModel::loadData(ProjectModel::SongGroupNode* node) {
  beginResetModel();
  m_node = node;
  _buildSortedList();
  endResetModel();
}

void PageModel::unloadData() {
  beginResetModel();
  m_node.reset();
  m_sorted.clear();
  endResetModel();
}

int PageModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid())
    return 0;
  if (!m_node)
    return 0;
  return int(m_sorted.size()) + 1;
}

int PageModel::columnCount(const QModelIndex& parent) const {
  if (parent.isValid())
    return 0;
  return 4;
}

QVariant PageModel::data(const QModelIndex& index, int role) const {
  if (!m_node)
    return QVariant();
  if (index.row() == m_sorted.size())
    return QVariant();

  auto entry = m_sorted[index.row()];

  if (role == Qt::DisplayRole || role == Qt::EditRole) {
    switch (index.column()) {
    case 0:
      return entry->first + 1;
    case 1: {
      ProjectModel::GroupNode* group = g_MainWindow->projectModel()->getGroupNode(m_node.get());
      if (ProjectModel::BasePoolObjectNode* node = group->pageObjectNodeOfId(entry->second.objId.id))
        return node->text();
      return QVariant();
    }
    case 2:
      return entry->second.priority;
    case 3:
      return entry->second.maxVoices;
    default:
      break;
    }
  }

  return QVariant();
}

bool PageModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if (!m_node || role != Qt::EditRole)
    return false;

  auto& map = _getMap();
  auto entry = m_sorted[index.row()];

  switch (index.column()) {
  case 0: {
    int prog = value.toInt() - 1;
    if (prog == entry->first)
      return false;
    if (map.find(prog) != map.cend()) {
      g_MainWindow->uiMessenger().critical(tr("Program Conflict"),
                                           tr("Program %1 is already defined in table").arg(value.toInt()));
      return false;
    }
    int newIdx = _hypotheticalIndexOfProgram(prog);
    bool moving = beginMoveRows(index.parent(), index.row(), index.row(), index.parent(), newIdx);
    g_MainWindow->pushUndoCommand(new PageDataChangeUndoCommand(
        m_node.get(), tr("Change %1").arg(headerData(0, Qt::Horizontal).toString()), m_drum, entry->first, 0, prog));
    _buildSortedList();
    if (moving)
      endMoveRows();
    QModelIndex newIndex = _indexOfProgram(prog);
    emit dataChanged(newIndex, newIndex);
    return true;
  }
  case 1:
    if (entry->second.objId.id == value.toInt())
      return false;
    break;
  case 2:
    if (entry->second.priority == value.toInt())
      return false;
    break;
  case 3:
    if (entry->second.maxVoices == value.toInt())
      return false;
    break;
  default:
    return false;
  }

  g_MainWindow->pushUndoCommand(new PageDataChangeUndoCommand(
      m_node.get(), tr("Change %1").arg(headerData(index.column(), Qt::Horizontal).toString()), m_drum, entry->first,
      index.column(), value.toInt()));

  emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
  return true;
}

QVariant PageModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case 0:
      return tr("Program");
    case 1:
      return tr("Object");
    case 2:
      return tr("Priority");
    case 3:
      return tr("Max Voices");
    default:
      break;
    }
  }
  return QVariant();
}

Qt::ItemFlags PageModel::flags(const QModelIndex& index) const {
  if (!index.isValid())
    return Qt::NoItemFlags;
  if (index.row() == m_sorted.size())
    return Qt::NoItemFlags;
  return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

class PageRowUndoCommand : public EditorUndoCommand {
protected:
  PageTableView* m_view;
  std::vector<std::pair<uint8_t, amuse::SongGroupIndex::PageEntry>> m_data;
  bool m_undid = false;
  void add() {
    m_view->selectionModel()->clearSelection();
    for (const auto& p : m_data) {
      int row = static_cast<PageModel*>(m_view->model())->_insertRow(p);
      m_view->setCurrentIndex(m_view->model()->index(row, 0));
    }
  }
  void del() {
    for (auto it = m_data.rbegin(); it != m_data.rend(); ++it) {
      *it = static_cast<PageModel*>(m_view->model())->_removeRow(it->first);
    }
  }
  void undo() override {
    m_undid = true;
    EditorUndoCommand::undo();
  }
  void redo() override {
    if (m_undid)
      EditorUndoCommand::redo();
  }

public:
  explicit PageRowUndoCommand(ProjectModel::SongGroupNode* node, const QString& text, PageTableView* view,
                              std::vector<std::pair<uint8_t, amuse::SongGroupIndex::PageEntry>>&& data)
  : EditorUndoCommand(node, text), m_view(view), m_data(std::move(data)) {}
};

class PageRowAddUndoCommand : public PageRowUndoCommand {
  using base = PageRowUndoCommand;

public:
  explicit PageRowAddUndoCommand(ProjectModel::SongGroupNode* node, const QString& text, PageTableView* view,
                                 std::vector<std::pair<uint8_t, amuse::SongGroupIndex::PageEntry>>&& data)
  : PageRowUndoCommand(node, text, view, std::move(data)) {}
  void undo() override {
    base::undo();
    base::del();
  }
  void redo() override {
    base::redo();
    base::add();
  }
};

class PageRowDelUndoCommand : public PageRowUndoCommand {
  using base = PageRowUndoCommand;

public:
  explicit PageRowDelUndoCommand(ProjectModel::SongGroupNode* node, const QString& text, PageTableView* view,
                                 std::vector<std::pair<uint8_t, amuse::SongGroupIndex::PageEntry>>&& data)
  : PageRowUndoCommand(node, text, view, std::move(data)) {}
  void undo() override {
    base::undo();
    base::add();
  }
  void redo() override {
    base::redo();
    base::del();
  }
};

int PageModel::_insertRow(const std::pair<uint8_t, amuse::SongGroupIndex::PageEntry>& data) {
  if (!m_node)
    return 0;
  auto& map = _getMap();
  int idx = _hypotheticalIndexOfProgram(data.first);
  beginInsertRows(QModelIndex(), idx, idx);
  map.emplace(data);
  _buildSortedList();
  endInsertRows();
  return idx;
}

std::pair<uint8_t, amuse::SongGroupIndex::PageEntry> PageModel::_removeRow(uint8_t prog) {
  std::pair<uint8_t, amuse::SongGroupIndex::PageEntry> ret;
  if (!m_node)
    return ret;
  auto& map = _getMap();
  int idx = _hypotheticalIndexOfProgram(prog);
  beginRemoveRows(QModelIndex(), idx, idx);
  ret.first = prog;
  auto search = map.find(prog);
  if (search != map.cend()) {
    ret.second = search->second;
    map.erase(search);
  }
  _buildSortedList();
  endRemoveRows();
  return ret;
}

PageModel::PageModel(bool drum, QObject* parent) : QAbstractTableModel(parent), m_drum(drum) {}

PageModel::~PageModel() = default;

std::unordered_map<amuse::SongId, std::array<amuse::SongGroupIndex::MIDISetup, 16>>& SetupListModel::_getMap() const {
  return m_node->m_index->m_midiSetups;
}

void SetupListModel::_buildSortedList() {
  m_sorted.clear();
  if (!m_node)
    return;
  auto& map = _getMap();
  m_sorted.reserve(map.size());
  for (auto it = map.begin(); it != map.end(); ++it)
    m_sorted.emplace_back(it);
  std::sort(m_sorted.begin(), m_sorted.end());
}

QModelIndex SetupListModel::_indexOfSong(amuse::SongId id) const {
  auto search = std::lower_bound(m_sorted.cbegin(), m_sorted.cend(), id);
  if (search == m_sorted.cend() || search->m_it->first != id)
    return QModelIndex();
  else
    return createIndex(search - m_sorted.begin(), 0);
}

int SetupListModel::_hypotheticalIndexOfSong(const std::string& songName) const {
  auto search = std::lower_bound(m_sorted.cbegin(), m_sorted.cend(), songName);
  return search - m_sorted.begin();
}

void SetupListModel::loadData(ProjectModel::SongGroupNode* node) {
  beginResetModel();
  m_node = node;
  g_MainWindow->projectModel()->setIdDatabases(m_node.get());
  _buildSortedList();
  endResetModel();
}

void SetupListModel::unloadData() {
  beginResetModel();
  m_node = nullptr;
  m_sorted.clear();
  endResetModel();
}

int SetupListModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid())
    return 0;
  if (!m_node)
    return 0;
  return int(m_sorted.size()) + 1;
}

int SetupListModel::columnCount(const QModelIndex& parent) const {
  if (parent.isValid())
    return 0;
  return 2;
}

QVariant SetupListModel::data(const QModelIndex& index, int role) const {
  if (!m_node)
    return QVariant();
  if (index.row() == m_sorted.size())
    return QVariant();

  auto entry = m_sorted[index.row()];

  if (role == Qt::DisplayRole || role == Qt::EditRole || role == Qt::ForegroundRole) {
    if (index.column() == 0) {
      if (role == Qt::ForegroundRole)
        return QVariant();
      g_MainWindow->projectModel()->setIdDatabases(m_node.get());
      return QString::fromUtf8(amuse::SongId::CurNameDB->resolveNameFromId(entry->first.id).data());
    } else if (index.column() == 1) {
      QString songPath = g_MainWindow->projectModel()->getMIDIPathOfSong(entry.m_it->first);
      if (songPath.isEmpty()) {
        if (role == Qt::ForegroundRole)
          return g_MainWindow->palette().color(QPalette::Disabled, QPalette::Text);
        else if (role == Qt::EditRole)
          return QVariant();
        return tr("Double-click to select file");
      }
      if (role == Qt::ForegroundRole)
        return QVariant();
      return songPath;
    }
  }

  return QVariant();
}

bool SetupListModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if (!m_node || role != Qt::EditRole || index.column() != 0)
    return false;

  auto entry = m_sorted[index.row()];

  g_MainWindow->projectModel()->setIdDatabases(m_node.get());
  auto utf8key = value.toString().toUtf8();
  std::unordered_map<std::string, amuse::ObjectId>::iterator idIt;
  // TODO: Heterogeneous lookup when C++20 available
  if ((idIt = amuse::SongId::CurNameDB->m_stringToId.find(utf8key.data())) !=
      amuse::SongId::CurNameDB->m_stringToId.cend()) {
    if (idIt->second == entry->first)
      return false;
    g_MainWindow->uiMessenger().critical(tr("Song Conflict"),
                                         tr("Song %1 is already defined in project").arg(value.toString()));
    return false;
  }
  bool moving =
      beginMoveRows(index.parent(), index.row(), index.row(), index.parent(), _hypotheticalIndexOfSong(utf8key.data()));
  g_MainWindow->pushUndoCommand(
      new SongNameChangeUndoCommand(m_node.get(), tr("Change Song Name"), entry.m_it->first, utf8key.data()));
  _buildSortedList();
  if (moving)
    endMoveRows();
  QModelIndex newIndex = _indexOfSong(entry.m_it->first);
  emit dataChanged(newIndex, newIndex, {Qt::DisplayRole, Qt::EditRole});

  return true;
}

QVariant SetupListModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case 0:
      return tr("Song");
    case 1:
      return tr("MIDI File");
    default:
      break;
    }
  }
  return QVariant();
}

Qt::ItemFlags SetupListModel::flags(const QModelIndex& index) const {
  if (!m_node)
    return Qt::NoItemFlags;
  if (index.row() == m_sorted.size())
    return Qt::NoItemFlags;
  return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

class SetupRowUndoCommand : public EditorUndoCommand {
protected:
  SetupTableView* m_view;
  std::vector<std::tuple<amuse::SongId, std::string, std::array<amuse::SongGroupIndex::MIDISetup, 16>>> m_data;
  bool m_undid = false;
  void add() {
    QTableView* listView = m_view->m_listView;
    listView->selectionModel()->clearSelection();
    for (auto& p : m_data) {
      int row = static_cast<SetupListModel*>(m_view->m_listView->model())->_insertRow(p);
      listView->setCurrentIndex(listView->model()->index(row, 0));
    }
  }
  void del() {
    QTableView* listView = m_view->m_listView;
    for (auto it = m_data.rbegin(); it != m_data.rend(); ++it) {
      *it = static_cast<SetupListModel*>(listView->model())->_removeRow(std::get<0>(*it));
    }
  }
  void undo() override {
    m_undid = true;
    EditorUndoCommand::undo();
  }
  void redo() override {
    if (m_undid)
      EditorUndoCommand::redo();
  }

public:
  explicit SetupRowUndoCommand(
      ProjectModel::SongGroupNode* node, const QString& text, SetupTableView* view,
      std::vector<std::tuple<amuse::SongId, std::string, std::array<amuse::SongGroupIndex::MIDISetup, 16>>>&& data)
  : EditorUndoCommand(node, text), m_view(view), m_data(std::move(data)) {}
};

class SetupRowAddUndoCommand : public SetupRowUndoCommand {
  using base = SetupRowUndoCommand;

public:
  explicit SetupRowAddUndoCommand(
      ProjectModel::SongGroupNode* node, const QString& text, SetupTableView* view,
      std::vector<std::tuple<amuse::SongId, std::string, std::array<amuse::SongGroupIndex::MIDISetup, 16>>>&& data)
  : SetupRowUndoCommand(node, text, view, std::move(data)) {}
  void undo() override {
    base::undo();
    base::del();
  }
  void redo() override {
    base::redo();
    base::add();
  }
};

class SetupRowDelUndoCommand : public SetupRowUndoCommand {
  using base = SetupRowUndoCommand;

public:
  explicit SetupRowDelUndoCommand(
      ProjectModel::SongGroupNode* node, const QString& text, SetupTableView* view,
      std::vector<std::tuple<amuse::SongId, std::string, std::array<amuse::SongGroupIndex::MIDISetup, 16>>>&& data)
  : SetupRowUndoCommand(node, text, view, std::move(data)) {}
  void undo() override {
    base::undo();
    base::add();
  }
  void redo() override {
    base::redo();
    base::del();
  }
};

int SetupListModel::_insertRow(
    std::tuple<amuse::SongId, std::string, std::array<amuse::SongGroupIndex::MIDISetup, 16>>& data) {
  if (!m_node)
    return 0;
  auto& map = _getMap();
  g_MainWindow->projectModel()->setIdDatabases(m_node.get());
  if (std::get<0>(data).id == 0xffff)
    std::get<0>(data) = amuse::SongId::CurNameDB->generateId(amuse::NameDB::Type::Song);

  g_MainWindow->projectModel()->allocateSongId(std::get<0>(data), std::get<1>(data));
  int idx = _hypotheticalIndexOfSong(std::get<1>(data));
  beginInsertRows(QModelIndex(), idx, idx);
  map[std::get<0>(data)] = std::get<2>(data);
  _buildSortedList();
  endInsertRows();
  return idx;
}

std::tuple<amuse::SongId, std::string, std::array<amuse::SongGroupIndex::MIDISetup, 16>>
SetupListModel::_removeRow(amuse::SongId id) {
  std::tuple<amuse::SongId, std::string, std::array<amuse::SongGroupIndex::MIDISetup, 16>> ret = {};
  if (!m_node)
    return ret;

  auto& map = _getMap();
  g_MainWindow->projectModel()->setIdDatabases(m_node.get());
  std::get<0>(ret) = id;
  std::get<1>(ret) = amuse::SongId::CurNameDB->resolveNameFromId(id);
  int idx = _indexOfSong(id).row();
  beginRemoveRows(QModelIndex(), idx, idx);
  auto search = map.find(id);
  if (search != map.end()) {
    std::get<2>(ret) = search->second;
    g_MainWindow->projectModel()->deallocateSongId(id);
    map.erase(search);
  }
  _buildSortedList();
  endRemoveRows();
  return ret;
}

SetupListModel::SetupListModel(QObject* parent) : QAbstractTableModel(parent) {}

SetupListModel::~SetupListModel() = default;

void SetupModel::loadData(std::pair<const amuse::SongId, std::array<amuse::SongGroupIndex::MIDISetup, 16>>* data) {
  beginResetModel();
  m_data = data;
  endResetModel();
}

void SetupModel::unloadData() {
  beginResetModel();
  m_data = nullptr;
  endResetModel();
}

int SetupModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid())
    return 0;
  if (!m_data)
    return 0;
  return 16;
}

int SetupModel::columnCount(const QModelIndex& parent) const {
  if (parent.isValid())
    return 0;
  return 5;
}

QVariant SetupModel::data(const QModelIndex& index, int role) const {
  if (!m_data)
    return QVariant();

  auto& entry = m_data->second[index.row()];

  if (role == Qt::DisplayRole || role == Qt::EditRole) {
    switch (index.column()) {
    case 0:
      return entry.programNo + 1;
    case 1:
      return entry.volume;
    case 2:
      return entry.panning;
    case 3:
      return entry.reverb;
    case 4:
      return entry.chorus;
    default:
      break;
    }
  }

  return QVariant();
}

bool SetupModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if (!m_data || role != Qt::EditRole)
    return false;

  auto& entry = m_data->second[index.row()];

  int val = value.toInt();
  switch (index.column()) {
  case 0:
    val -= 1;
    if (entry.programNo == val)
      return false;
    break;
  case 1:
    if (entry.volume == val)
      return false;
    break;
  case 2:
    if (entry.panning == val)
      return false;
    break;
  case 3:
    if (entry.reverb == val)
      return false;
    break;
  case 4:
    if (entry.chorus == val)
      return false;
    break;
  default:
    return false;
  }

  g_MainWindow->pushUndoCommand(
      new SetupDataChangeUndoCommand(static_cast<SongGroupEditor*>(parent())->m_setupList.m_node.get(),
                                     tr("Change %1").arg(headerData(index.column(), Qt::Horizontal).toString()),
                                     m_data->first, index.row(), index.column(), val));

  emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});

  return true;
}

QVariant SetupModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (role == Qt::DisplayRole) {
    if (orientation == Qt::Horizontal) {
      switch (section) {
      case 0:
        return tr("Program");
      case 1:
        return tr("Volume");
      case 2:
        return tr("Panning");
      case 3:
        return tr("Reverb");
      case 4:
        return tr("Chorus");
      default:
        break;
      }
    } else {
      return section + 1;
    }
  } else if (role == Qt::BackgroundRole && orientation == Qt::Vertical) {
    if (section == 9)
      return QColor(64, 0, 0);
    return QColor(0, 64, 0);
  }
  return QVariant();
}

Qt::ItemFlags SetupModel::flags(const QModelIndex& index) const { return Qt::ItemIsEnabled | Qt::ItemIsEditable; }

SetupModel::SetupModel(QObject* parent) : QAbstractTableModel(parent) {}

SetupModel::~SetupModel() = default;

void PageTableView::deleteSelection() {
  const QModelIndexList list = selectionModel()->selectedRows();
  if (list.isEmpty()) {
    return;
  }

  std::vector<std::pair<uint8_t, amuse::SongGroupIndex::PageEntry>> data;
  data.reserve(list.size());
  for (const QModelIndex idx : list) {
    data.emplace_back(model()->data(idx).toInt() - 1, amuse::SongGroupIndex::PageEntry{});
  }

  std::sort(data.begin(), data.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  g_MainWindow->pushUndoCommand(new PageRowDelUndoCommand(
      static_cast<PageModel*>(model())->m_node.get(),
      data.size() > 1 ? tr("Delete Page Entries") : tr("Delete Page Entry"), this, std::move(data)));
}

void PageTableView::setModel(QAbstractItemModel* model) {
  QTableView::setModel(model);
  horizontalHeader()->setMinimumSectionSize(75);
  horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
  horizontalHeader()->resizeSection(0, 75);
  horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
  horizontalHeader()->resizeSection(2, 75);
  horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
  horizontalHeader()->resizeSection(3, 100);
}

PageTableView::PageTableView(QWidget* parent) : QTableView(parent) {
  setSelectionBehavior(QAbstractItemView::SelectRows);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setGridStyle(Qt::NoPen);

  m_127Delegate.setItemEditorFactory(&m_127Factory);
  m_128Delegate.setItemEditorFactory(&m_128Factory);
  m_255Delegate.setItemEditorFactory(&m_255Factory);

  setItemDelegateForColumn(0, &m_128Delegate);
  setItemDelegateForColumn(1, &m_poDelegate);
  setItemDelegateForColumn(2, &m_255Delegate);
  setItemDelegateForColumn(3, &m_255Delegate);
}

PageTableView::~PageTableView() = default;

void SetupTableView::setModel(QAbstractItemModel* list, QAbstractItemModel* table) {
  {
    m_listView->setModel(list);
    auto hheader = m_listView->horizontalHeader();
    hheader->setMinimumSectionSize(200);
    hheader->resizeSection(0, 200);
    hheader->setSectionResizeMode(1, QHeaderView::Stretch);
  }
  {
    m_tableView->setModel(table);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  }
}

void SetupTableView::deleteSelection() {
  QModelIndexList list = m_listView->selectionModel()->selectedRows();
  if (list.isEmpty())
    return;
  std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.row() < b.row(); });
  std::vector<std::tuple<amuse::SongId, std::string, std::array<amuse::SongGroupIndex::MIDISetup, 16>>> data;
  data.reserve(list.size());
  for (QModelIndex idx : list) {
    auto& entry = *static_cast<SetupListModel*>(m_listView->model())->m_sorted[idx.row()].m_it;
    data.push_back({entry.first, {}, {}});
  }
  g_MainWindow->pushUndoCommand(new SetupRowDelUndoCommand(
      static_cast<SetupListModel*>(m_listView->model())->m_node.get(),
      data.size() > 1 ? tr("Delete Setup Entries") : tr("Delete Setup Entry"), this, std::move(data)));
}

void SetupTableView::showEvent(QShowEvent* event) { setSizes({width() - 375, 375}); }

SetupTableView::SetupTableView(QWidget* parent)
: QSplitter(parent), m_listView(new QTableView), m_tableView(new QTableView), m_midiDelegate(this) {
  setChildrenCollapsible(false);
  setStretchFactor(0, 1);
  setStretchFactor(1, 0);

  addWidget(m_listView);
  addWidget(m_tableView);

  m_listView->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_listView->setGridStyle(Qt::NoPen);
  m_listView->setItemDelegateForColumn(1, &m_midiDelegate);

  m_tableView->setSelectionMode(QAbstractItemView::NoSelection);
  m_tableView->setGridStyle(Qt::NoPen);

  m_127Delegate.setItemEditorFactory(&m_127Factory);
  m_128Delegate.setItemEditorFactory(&m_128Factory);

  m_tableView->setItemDelegateForColumn(0, &m_128Delegate);
  m_tableView->setItemDelegateForColumn(1, &m_127Delegate);
  m_tableView->setItemDelegateForColumn(2, &m_127Delegate);
  m_tableView->setItemDelegateForColumn(3, &m_127Delegate);
  m_tableView->setItemDelegateForColumn(4, &m_127Delegate);
}

SetupTableView::~SetupTableView() = default;

void ColoredTabBarStyle::drawControl(QStyle::ControlElement element, const QStyleOption* option, QPainter* painter,
                                     const QWidget* widget) const {
  if (qobject_cast<const ColoredTabBar*>(widget) && element == QStyle::CE_TabBarTab) {
    QStyleOptionTab optionTab = *static_cast<const QStyleOptionTab*>(option);
    switch (optionTab.position) {
    case QStyleOptionTab::Beginning:
      optionTab.palette.setColor(QPalette::Button, QColor(0, 64, 0));
      break;
    case QStyleOptionTab::Middle:
      optionTab.palette.setColor(QPalette::Button, QColor(64, 0, 0));
      break;
    default:
      break;
    }
    QProxyStyle::drawControl(element, &optionTab, painter, widget);
  } else
    QProxyStyle::drawControl(element, option, painter, widget);
}

ColoredTabBar::ColoredTabBar(QWidget* parent) : QTabBar(parent) { setDrawBase(false); }

ColoredTabWidget::ColoredTabWidget(QWidget* parent) : QTabWidget(parent) { setTabBar(&m_tabBar); }

static std::vector<uint8_t> LoadSongFile(QString path) {
  QFileInfo fi(path);
  if (!fi.isFile())
    return {};

  std::vector<uint8_t> data;
  {
    QFile f(path);
    if (!f.open(QFile::ReadOnly))
      return {};
    auto d = f.readAll();
    data.resize(d.size());
    memcpy(&data[0], d.data(), d.size());
  }

  if (!memcmp(data.data(), "MThd", 4)) {
    data = amuse::SongConverter::MIDIToSong(data, 1, true);
  } else {
    bool isBig;
    if (amuse::SongState::DetectVersion(data.data(), isBig) < 0)
      return {};
  }

  return data;
}

void MIDIPlayerWidget::clicked() {
  if (!m_seq) {
    m_arrData = LoadSongFile(m_path);
    if (!m_arrData.empty()) {
      m_seq = g_MainWindow->startSong(m_groupId, m_songId, m_arrData.data());
      if (m_seq) {
        m_playAction.setText(tr("Stop"));
        m_playAction.setIcon(QIcon(QStringLiteral(":/icons/IconStop.svg")));
      }
    } else {
      g_MainWindow->uiMessenger().critical(tr("Bad Song Data"), tr("Unable to load song data at %1").arg(m_path));
    }
  } else {
    stopped();
  }
}

void MIDIPlayerWidget::stopped() {
  m_seq->stopSong();
  m_seq.reset();
  m_playAction.setText(tr("Play"));
  m_playAction.setIcon(QIcon(QStringLiteral(":/icons/IconSoundMacro.svg")));
}

void MIDIPlayerWidget::resizeEvent(QResizeEvent* event) {
  m_button.setGeometry(event->size().width() - event->size().height(), 0, event->size().height(),
                       event->size().height());
}

void MIDIPlayerWidget::mouseDoubleClickEvent(QMouseEvent* event) {
  qobject_cast<QTableView*>(parentWidget()->parentWidget())->setIndexWidget(m_index, nullptr);
  event->ignore();
}

void MIDIPlayerWidget::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::RightButton) {
    QTableView* view = qobject_cast<QTableView*>(parentWidget()->parentWidget());
    QAbstractItemDelegate* delegate = view->itemDelegateForColumn(1);
    delegate->editorEvent(event, view->model(), {}, m_index);
  }
  event->ignore();
}

MIDIPlayerWidget::~MIDIPlayerWidget() {
  if (m_seq)
    m_seq->stopSong();
}

MIDIPlayerWidget::MIDIPlayerWidget(QModelIndex index, amuse::GroupId gid, amuse::SongId id, const QString& path,
                                   QWidget* parent)
: QWidget(parent)
, m_playAction(tr("Play"))
, m_button(this)
, m_index(index)
, m_groupId(gid)
, m_songId(id)
, m_path(path) {
  m_playAction.setIcon(QIcon(QStringLiteral(":/icons/IconSoundMacro.svg")));
  m_button.setDefaultAction(&m_playAction);
  connect(&m_playAction, &QAction::triggered, this, &MIDIPlayerWidget::clicked);
}

bool SongGroupEditor::loadData(ProjectModel::SongGroupNode* node) {
  m_normPages.loadData(node);
  m_drumPages.loadData(node);
  m_setupList.loadData(node);
  m_setup.unloadData();
  return true;
}

void SongGroupEditor::unloadData() {
  m_normPages.unloadData();
  m_drumPages.unloadData();
  m_setupList.unloadData();
  m_setup.unloadData();
}

ProjectModel::INode* SongGroupEditor::currentNode() const { return m_normPages.m_node.get(); }

void SongGroupEditor::resizeEvent(QResizeEvent* ev) {
  m_tabs.setGeometry(QRect({}, ev->size()));
  m_addRemoveButtons.move(0, ev->size().height() - 32);
}

void SongGroupEditor::doAdd() {
  if (PageTableView* table = qobject_cast<PageTableView*>(m_tabs.currentWidget())) {
    QModelIndex idx = table->selectionModel()->currentIndex();
    int row;
    if (!idx.isValid())
      row = table->model()->rowCount() - 1;
    else
      row = idx.row();

    PageModel* model = static_cast<PageModel*>(table->model());
    int prog = 0;
    if (!model->m_sorted.empty()) {
      prog = -1;
      if (row < model->m_sorted.size()) {
        prog = model->m_sorted[row].m_it->first;
        while (prog >= 0 && model->_indexOfProgram(prog).isValid())
          --prog;
      }
      if (prog == -1) {
        prog = 0;
        while (prog < 128 && model->_indexOfProgram(prog).isValid())
          ++prog;
      }
      if (prog == 128)
        return;
    }

    std::vector<std::pair<uint8_t, amuse::SongGroupIndex::PageEntry>> data;
    data.emplace_back(prog, amuse::SongGroupIndex::PageEntry{});
    g_MainWindow->pushUndoCommand(
        new PageRowAddUndoCommand(model->m_node.get(), tr("Add Page Entry"), table, std::move(data)));

    m_addRemoveButtons.addAction()->setDisabled(table->model()->rowCount() >= 128);
  } else if (SetupTableView* table = qobject_cast<SetupTableView*>(m_tabs.currentWidget())) {
    SetupListModel* model = static_cast<SetupListModel*>(table->m_listView->model());
    g_MainWindow->projectModel()->setIdDatabases(model->m_node.get());
    std::vector<std::tuple<amuse::SongId, std::string, std::array<amuse::SongGroupIndex::MIDISetup, 16>>> data;
    auto songId = g_MainWindow->projectModel()->bootstrapSongId();
    data.emplace_back(songId.first, songId.second, std::array<amuse::SongGroupIndex::MIDISetup, 16>{});
    g_MainWindow->pushUndoCommand(
        new SetupRowAddUndoCommand(model->m_node.get(), tr("Add Setup Entry"), table, std::move(data)));
  }
}

void SongGroupEditor::doSelectionChanged() {
  if (PageTableView* table = qobject_cast<PageTableView*>(m_tabs.currentWidget()))
    m_addRemoveButtons.removeAction()->setDisabled(table->selectionModel()->selectedRows().isEmpty());
  else if (SetupTableView* table = qobject_cast<SetupTableView*>(m_tabs.currentWidget()))
    m_addRemoveButtons.removeAction()->setDisabled(table->m_listView->selectionModel()->selectedRows().isEmpty());
  g_MainWindow->updateFocus();
}

void SongGroupEditor::doSetupSelectionChanged() {
  doSelectionChanged();
  if (m_setupTable->m_listView->selectionModel()->selectedRows().isEmpty() || m_setupList.m_sorted.empty()) {
    m_setup.unloadData();
  } else {
    auto entry = m_setupList.m_sorted[m_setupTable->m_listView->selectionModel()->selectedRows().last().row()];
    m_setup.loadData(&*entry.m_it);
  }
}

void SongGroupEditor::currentTabChanged(int idx) {
  if (PageTableView* table = qobject_cast<PageTableView*>(m_tabs.currentWidget())) {
    m_addRemoveButtons.addAction()->setDisabled(table->model()->rowCount() >= 128);
    doSelectionChanged();
  } else if (SetupTableView* table = qobject_cast<SetupTableView*>(m_tabs.currentWidget())) {
    m_addRemoveButtons.addAction()->setDisabled(false);
    doSelectionChanged();
  }
}

void SongGroupEditor::normRowsInserted(const QModelIndex& parent, int first, int last) {
  m_normTable->scrollTo(m_normTable->model()->index(first, 0));
}

void SongGroupEditor::normRowsMoved(const QModelIndex& parent, int start, int end, const QModelIndex& destination,
                                    int row) {
  m_normTable->scrollTo(m_normTable->model()->index(row, 0));
}

void SongGroupEditor::drumRowsInserted(const QModelIndex& parent, int first, int last) {
  m_drumTable->scrollTo(m_drumTable->model()->index(first, 0));
}

void SongGroupEditor::drumRowsMoved(const QModelIndex& parent, int start, int end, const QModelIndex& destination,
                                    int row) {
  m_drumTable->scrollTo(m_drumTable->model()->index(row, 0));
}

void SongGroupEditor::setupRowsInserted(const QModelIndex& parent, int first, int last) {
  m_setupTable->m_listView->scrollTo(m_setupTable->m_listView->model()->index(first, 0));
  setupDataChanged();
}

void SongGroupEditor::setupRowsMoved(const QModelIndex& parent, int start, int end, const QModelIndex& destination,
                                     int row) {
  m_setupTable->m_listView->scrollTo(m_setupTable->m_listView->model()->index(row, 0));
  setupDataChanged();
}

void SongGroupEditor::setupRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last) {
  for (int i = first; i <= last; ++i) {
    auto entry = m_setupList.m_sorted[i];
    if (&*entry.m_it == m_setup.m_data) {
      m_setup.unloadData();
      return;
    }
  }
}

void SongGroupEditor::setupModelAboutToBeReset() { m_setup.unloadData(); }

void SongGroupEditor::setupDataChanged() {
  int idx = 0;
  for (const auto& p : m_setupList.m_sorted) {
    QString path = g_MainWindow->projectModel()->getMIDIPathOfSong(p.m_it->first);
    QModelIndex index = m_setupList.index(idx, 1);
    if (m_setupTable->m_listView->isPersistentEditorOpen(index)) {
      ++idx;
      continue;
    }
    if (path.isEmpty()) {
      m_setupTable->m_listView->setIndexWidget(index, nullptr);
    } else {
      MIDIPlayerWidget* w = qobject_cast<MIDIPlayerWidget*>(m_setupTable->m_listView->indexWidget(index));
      if (!w || w->songId() != p.m_it->first) {
        QString pathStr = g_MainWindow->projectModel()->dir().absoluteFilePath(path);
        MIDIPlayerWidget* newW = new MIDIPlayerWidget(index, m_setupList.m_node->m_id, p.m_it->first, pathStr,
                                                      m_setupTable->m_listView->viewport());
        m_setupTable->m_listView->setIndexWidget(index, newW);
      }
    }
    ++idx;
  }
}

AmuseItemEditFlags SongGroupEditor::itemEditFlags() const {
  if (PageTableView* table = qobject_cast<PageTableView*>(m_tabs.currentWidget()))
    return (table->hasFocus() && !table->selectionModel()->selectedRows().isEmpty()) ? AmuseItemDelete : AmuseItemNone;
  else if (SetupTableView* table = qobject_cast<SetupTableView*>(m_tabs.currentWidget()))
    return (table->m_listView->hasFocus() && !table->m_listView->selectionModel()->selectedRows().isEmpty())
               ? AmuseItemDelete
               : AmuseItemNone;
  return AmuseItemNone;
}

void SongGroupEditor::itemDeleteAction() {
  if (PageTableView* table = qobject_cast<PageTableView*>(m_tabs.currentWidget()))
    table->deleteSelection();
  else if (SetupTableView* table = qobject_cast<SetupTableView*>(m_tabs.currentWidget()))
    table->deleteSelection();
}

SongGroupEditor::SongGroupEditor(QWidget* parent)
: EditorWidget(parent)
, m_normPages(false, this)
, m_drumPages(true, this)
, m_setup(this)
, m_normTable(new PageTableView)
, m_drumTable(new PageTableView)
, m_setupTable(new SetupTableView)
, m_tabs(this)
, m_addRemoveButtons(this) {
  m_tabs.addTab(m_normTable, tr("Normal Pages"));
  m_tabs.addTab(m_drumTable, tr("Drum Pages"));
  m_tabs.addTab(m_setupTable, tr("MIDI Setups"));

  connect(&m_tabs, &ColoredTabWidget::currentChanged, this, &SongGroupEditor::currentTabChanged);

  m_normTable->setModel(&m_normPages);
  m_drumTable->setModel(&m_drumPages);
  m_setupTable->setModel(&m_setupList, &m_setup);
  connect(m_normTable->selectionModel(), &QItemSelectionModel::selectionChanged, this,
          &SongGroupEditor::doSelectionChanged);
  connect(m_drumTable->selectionModel(), &QItemSelectionModel::selectionChanged, this,
          &SongGroupEditor::doSelectionChanged);
  connect(m_setupTable->m_listView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
          &SongGroupEditor::doSetupSelectionChanged);

  connect(&m_normPages, &PageModel::rowsInserted, this, &SongGroupEditor::normRowsInserted);
  connect(&m_normPages, &PageModel::rowsMoved, this, &SongGroupEditor::normRowsMoved);

  connect(&m_drumPages, &PageModel::rowsInserted, this, &SongGroupEditor::drumRowsInserted);
  connect(&m_drumPages, &PageModel::rowsMoved, this, &SongGroupEditor::drumRowsMoved);

  connect(&m_setupList, &SetupListModel::rowsInserted, this, &SongGroupEditor::setupRowsInserted);
  connect(&m_setupList, &SetupListModel::rowsMoved, this, &SongGroupEditor::setupRowsMoved);
  connect(&m_setupList, &SetupListModel::rowsAboutToBeRemoved, this, &SongGroupEditor::setupRowsAboutToBeRemoved);
  connect(&m_setupList, &SetupListModel::modelAboutToBeReset, this, &SongGroupEditor::setupModelAboutToBeReset);
  connect(&m_setupList, &SetupListModel::rowsRemoved, this, &SongGroupEditor::setupDataChanged);
  connect(&m_setupList, &SetupListModel::dataChanged, this, &SongGroupEditor::setupDataChanged);
  connect(&m_setupList, &SetupListModel::layoutChanged, this, &SongGroupEditor::setupDataChanged);
  connect(&m_setupList, &SetupListModel::modelReset, this, &SongGroupEditor::setupDataChanged);

  m_addRemoveButtons.addAction()->setToolTip(tr("Add new page entry"));
  connect(m_addRemoveButtons.addAction(), &QAction::triggered, this, &SongGroupEditor::doAdd);
  m_addRemoveButtons.removeAction()->setToolTip(tr("Remove selected page entries"));
  connect(m_addRemoveButtons.removeAction(), &QAction::triggered, this, &SongGroupEditor::itemDeleteAction);

  m_tabs.setCurrentIndex(0);
}

SongGroupEditor::~SongGroupEditor() = default;
