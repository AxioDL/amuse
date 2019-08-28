#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <QFileDialog>
#include <QLineEdit>
#include <QProxyStyle>
#include <QPushButton>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTableView>
#include <QToolButton>

#include "EditorWidget.hpp"
#include "ProjectModel.hpp"

#include <amuse/AudioGroupProject.hpp>
#include <amuse/Common.hpp>
#include <amuse/Sequencer.hpp>

class SetupTableView;

class PageObjectDelegate : public BaseObjectDelegate {
  Q_OBJECT
protected:
  ProjectModel::INode* getNode(const QAbstractItemModel* model, const QModelIndex& index) const override;

public:
  explicit PageObjectDelegate(QObject* parent = Q_NULLPTR);
  ~PageObjectDelegate() override;

  QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;

private slots:
  void objIndexChanged();
};

class MIDIFileFieldWidget : public QWidget {
  Q_OBJECT
  QLineEdit m_le;
  QPushButton m_button;
  QFileDialog m_dialog;

public:
  explicit MIDIFileFieldWidget(QWidget* parent = Q_NULLPTR);
  ~MIDIFileFieldWidget() override;

  QString path() const { return m_le.text(); }
  void setPath(const QString& path) { m_le.setText(path); }

public slots:
  void buttonPressed();
  void fileDialogOpened(const QString& path);

signals:
  void pathChanged();
};

class MIDIFileDelegate : public QStyledItemDelegate {
  Q_OBJECT
  QFileDialog m_fileDialogMid, m_fileDialogSng;
  std::vector<uint8_t> m_exportData;

public:
  explicit MIDIFileDelegate(SetupTableView* parent = Q_NULLPTR);
  ~MIDIFileDelegate() override;

  QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  void destroyEditor(QWidget* editor, const QModelIndex& index) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
  bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

private slots:
  void doExportMIDI();
  void _doExportMIDI(const QString& path);
  void doExportSNG();
  void _doExportSNG(const QString& path);

public slots:
  void pathChanged();
};

class PageModel : public QAbstractTableModel {
  Q_OBJECT
  friend class SongGroupEditor;
  friend class PageObjectDelegate;
  friend class PageTableView;
  amuse::ObjToken<ProjectModel::SongGroupNode> m_node;
  struct Iterator {
    using ItTp = std::unordered_map<uint8_t, amuse::SongGroupIndex::PageEntry>::iterator;
    ItTp m_it;
    Iterator(ItTp it) : m_it(it) {}
    ItTp::pointer operator->() { return m_it.operator->(); }
    bool operator<(const Iterator& other) const { return m_it->first < other.m_it->first; }
    bool operator<(uint8_t other) const { return m_it->first < other; }
  };
  std::vector<Iterator> m_sorted;
  bool m_drum;
  std::unordered_map<uint8_t, amuse::SongGroupIndex::PageEntry>& _getMap() const;
  void _buildSortedList();
  QModelIndex _indexOfProgram(uint8_t prog) const;
  int _hypotheticalIndexOfProgram(uint8_t prog) const;

public:
  explicit PageModel(bool drum, QObject* parent = Q_NULLPTR);
  ~PageModel() override;

  void loadData(ProjectModel::SongGroupNode* node);
  void unloadData();

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;

  int _insertRow(const std::pair<uint8_t, amuse::SongGroupIndex::PageEntry>& data);
  std::pair<uint8_t, amuse::SongGroupIndex::PageEntry> _removeRow(uint8_t prog);
};

class SetupListModel : public QAbstractTableModel {
  Q_OBJECT
  friend class SongGroupEditor;
  friend class MIDIFileDelegate;
  friend class SetupTableView;
  friend class SetupModel;
  amuse::ObjToken<ProjectModel::SongGroupNode> m_node;
  struct Iterator {
    using ItTp = std::unordered_map<amuse::SongId, std::array<amuse::SongGroupIndex::MIDISetup, 16>>::iterator;
    ItTp m_it;
    Iterator(ItTp it) : m_it(it) {}
    ItTp::pointer operator->() { return m_it.operator->(); }
    bool operator<(const Iterator& other) const {
      return amuse::SongId::CurNameDB->resolveNameFromId(m_it->first) <
             amuse::SongId::CurNameDB->resolveNameFromId(other.m_it->first);
    }
    bool operator<(amuse::SongId other) const {
      return amuse::SongId::CurNameDB->resolveNameFromId(m_it->first) <
             amuse::SongId::CurNameDB->resolveNameFromId(other);
    }
    bool operator<(const std::string& name) const {
      return amuse::SongId::CurNameDB->resolveNameFromId(m_it->first) < name;
    }
  };
  std::vector<Iterator> m_sorted;
  std::unordered_map<amuse::SongId, std::array<amuse::SongGroupIndex::MIDISetup, 16>>& _getMap() const;
  void _buildSortedList();
  QModelIndex _indexOfSong(amuse::SongId id) const;
  int _hypotheticalIndexOfSong(const std::string& songName) const;

public:
  explicit SetupListModel(QObject* parent = Q_NULLPTR);
  ~SetupListModel() override;

  void loadData(ProjectModel::SongGroupNode* node);
  void unloadData();

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;

  int _insertRow(std::tuple<amuse::SongId, std::string, std::array<amuse::SongGroupIndex::MIDISetup, 16>>& data);
  std::tuple<amuse::SongId, std::string, std::array<amuse::SongGroupIndex::MIDISetup, 16>> _removeRow(amuse::SongId id);
};

class SetupModel : public QAbstractTableModel {
  Q_OBJECT
  friend class SongGroupEditor;
  std::pair<const amuse::SongId, std::array<amuse::SongGroupIndex::MIDISetup, 16>>* m_data = nullptr;

public:
  explicit SetupModel(QObject* parent = Q_NULLPTR);
  ~SetupModel() override;

  void loadData(std::pair<const amuse::SongId, std::array<amuse::SongGroupIndex::MIDISetup, 16>>* data);
  void unloadData();

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
};

class PageTableView : public QTableView {
  Q_OBJECT
  PageObjectDelegate m_poDelegate;
  RangedValueFactory<0, 127> m_127Factory;
  RangedValueFactory<1, 128> m_128Factory;
  RangedValueFactory<0, 255> m_255Factory;
  QStyledItemDelegate m_127Delegate, m_128Delegate, m_255Delegate;

public:
  explicit PageTableView(QWidget* parent = Q_NULLPTR);
  ~PageTableView() override;

  void setModel(QAbstractItemModel* model) override;
  void deleteSelection();
};

class SetupTableView : public QSplitter {
  Q_OBJECT
  friend class SongGroupEditor;
  friend class SetupRowUndoCommand;
  QTableView* m_listView;
  QTableView* m_tableView;
  MIDIFileDelegate m_midiDelegate;
  RangedValueFactory<0, 127> m_127Factory;
  RangedValueFactory<1, 128> m_128Factory;
  QStyledItemDelegate m_127Delegate, m_128Delegate;

public:
  explicit SetupTableView(QWidget* parent = Q_NULLPTR);
  ~SetupTableView() override;

  void setModel(QAbstractItemModel* list, QAbstractItemModel* table);
  void deleteSelection();
  void showEvent(QShowEvent* event) override;
};

class ColoredTabBarStyle : public QProxyStyle {
public:
  using QProxyStyle::QProxyStyle;
  void drawControl(QStyle::ControlElement element, const QStyleOption* option, QPainter* painter,
                   const QWidget* widget = nullptr) const override;
};

class ColoredTabBar : public QTabBar {
  Q_OBJECT
public:
  explicit ColoredTabBar(QWidget* parent = Q_NULLPTR);
};

class ColoredTabWidget : public QTabWidget {
  Q_OBJECT
  ColoredTabBar m_tabBar;

public:
  explicit ColoredTabWidget(QWidget* parent = Q_NULLPTR);
};

class MIDIPlayerWidget : public QWidget {
  Q_OBJECT
  QAction m_playAction;
  QToolButton m_button;
  QModelIndex m_index;
  amuse::GroupId m_groupId;
  amuse::SongId m_songId;
  QString m_path;
  std::vector<uint8_t> m_arrData;
  amuse::ObjToken<amuse::Sequencer> m_seq;

public:
  explicit MIDIPlayerWidget(QModelIndex index, amuse::GroupId gid, amuse::SongId id, const QString& path,
                            QWidget* parent = Q_NULLPTR);
  ~MIDIPlayerWidget() override;
  amuse::SongId songId() const { return m_songId; }
  amuse::Sequencer* sequencer() const { return m_seq.get(); }
  void stopped();
  void resizeEvent(QResizeEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override { event->ignore(); }
public slots:
  void clicked();
};

class SongGroupEditor : public EditorWidget {
  Q_OBJECT
  friend class SetupModel;
  PageModel m_normPages;
  PageModel m_drumPages;
  SetupListModel m_setupList;
  SetupModel m_setup;
  PageTableView* m_normTable;
  PageTableView* m_drumTable;
  SetupTableView* m_setupTable;
  ColoredTabWidget m_tabs;
  AddRemoveButtons m_addRemoveButtons;

public:
  explicit SongGroupEditor(QWidget* parent = Q_NULLPTR);
  ~SongGroupEditor() override;

  bool loadData(ProjectModel::SongGroupNode* node);
  void unloadData() override;
  ProjectModel::INode* currentNode() const override;
  void setEditorEnabled(bool en) override {}
  void resizeEvent(QResizeEvent* ev) override;
  QTableView* getSetupListView() const { return m_setupTable->m_listView; }
  AmuseItemEditFlags itemEditFlags() const override;

private slots:
  void doAdd();
  void doSelectionChanged();
  void doSetupSelectionChanged();
  void currentTabChanged(int idx);
  void normRowsInserted(const QModelIndex& parent, int first, int last);
  void normRowsMoved(const QModelIndex& parent, int start, int end, const QModelIndex& destination, int row);
  void drumRowsInserted(const QModelIndex& parent, int first, int last);
  void drumRowsMoved(const QModelIndex& parent, int start, int end, const QModelIndex& destination, int row);
  void setupRowsInserted(const QModelIndex& parent, int first, int last);
  void setupRowsMoved(const QModelIndex& parent, int start, int end, const QModelIndex& destination, int row);
  void setupRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last);
  void setupModelAboutToBeReset();
  void setupDataChanged();
  void itemDeleteAction() override;
};
