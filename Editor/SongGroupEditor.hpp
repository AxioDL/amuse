#ifndef AMUSE_SONG_GROUP_EDITOR_HPP
#define AMUSE_SONG_GROUP_EDITOR_HPP

#include "EditorWidget.hpp"
#include <QTabWidget>
#include <QAbstractTableModel>
#include <QTableView>
#include <QToolButton>
#include <QAction>
#include <QSplitter>
#include <QListView>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QProxyStyle>
#include "amuse/Sequencer.hpp"

class PageObjectDelegate : public BaseObjectDelegate
{
    Q_OBJECT
protected:
    ProjectModel::INode* getNode(const QAbstractItemModel* model, const QModelIndex& index) const;
public:
    explicit PageObjectDelegate(QObject* parent = Q_NULLPTR);
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    void setEditorData(QWidget* editor, const QModelIndex& index) const;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const;
private slots:
    void objIndexChanged();
};

class MIDIFileFieldWidget : public QWidget
{
    Q_OBJECT
    QLineEdit m_le;
    QPushButton m_button;
    QFileDialog m_dialog;
public:
    explicit MIDIFileFieldWidget(QWidget* parent = Q_NULLPTR);
    QString path() const { return m_le.text(); }
    void setPath(const QString& path) { m_le.setText(path); }
public slots:
    void buttonPressed();
    void fileDialogOpened(const QString& path);
signals:
    void pathChanged();
};

class MIDIFileDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit MIDIFileDelegate(QObject* parent = Q_NULLPTR);
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    void destroyEditor(QWidget *editor, const QModelIndex &index) const;
    void setEditorData(QWidget* editor, const QModelIndex& index) const;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const;
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option, const QModelIndex &index);
private slots:
    void doExportMIDI();
    void doExportSNG();
public slots:
    void pathChanged();
};

class PageModel : public QAbstractTableModel
{
    Q_OBJECT
    friend class SongGroupEditor;
    friend class PageObjectDelegate;
    friend class PageTableView;
    amuse::ObjToken<ProjectModel::SongGroupNode> m_node;
    struct Iterator
    {
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
    void loadData(ProjectModel::SongGroupNode* node);
    void unloadData();

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex& index) const;

    int _insertRow(const std::pair<uint8_t, amuse::SongGroupIndex::PageEntry>& data);
    std::pair<uint8_t, amuse::SongGroupIndex::PageEntry> _removeRow(uint8_t prog);
};

class SetupListModel : public QAbstractTableModel
{
    Q_OBJECT
    friend class SongGroupEditor;
    friend class MIDIFileDelegate;
    friend class SetupTableView;
    friend class SetupModel;
    amuse::ObjToken<ProjectModel::SongGroupNode> m_node;
    struct Iterator
    {
        using ItTp = std::unordered_map<amuse::SongId, std::array<amuse::SongGroupIndex::MIDISetup, 16>>::iterator;
        ItTp m_it;
        Iterator(ItTp it) : m_it(it) {}
        ItTp::pointer operator->() { return m_it.operator->(); }
        bool operator<(const Iterator& other) const
        {
            return amuse::SongId::CurNameDB->resolveNameFromId(m_it->first) <
                   amuse::SongId::CurNameDB->resolveNameFromId(other.m_it->first);
        }
        bool operator<(amuse::SongId other) const
        {
            return amuse::SongId::CurNameDB->resolveNameFromId(m_it->first) <
                   amuse::SongId::CurNameDB->resolveNameFromId(other);
        }
        bool operator<(const std::string& name) const
        {
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
    void loadData(ProjectModel::SongGroupNode* node);
    void unloadData();

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex& index) const;

    int _insertRow(
        std::tuple<amuse::SongId, std::string, std::array<amuse::SongGroupIndex::MIDISetup, 16>>& data);
    std::tuple<amuse::SongId, std::string, std::array<amuse::SongGroupIndex::MIDISetup, 16>>
    _removeRow(amuse::SongId id);
};

class SetupModel : public QAbstractTableModel
{
    Q_OBJECT
    friend class SongGroupEditor;
    std::pair<const amuse::SongId, std::array<amuse::SongGroupIndex::MIDISetup, 16>>* m_data = nullptr;
public:
    explicit SetupModel(QObject* parent = Q_NULLPTR);
    void loadData(std::pair<const amuse::SongId, std::array<amuse::SongGroupIndex::MIDISetup, 16>>* data);
    void unloadData();

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex& index) const;
};

class PageTableView : public QTableView
{
    Q_OBJECT
    PageObjectDelegate m_poDelegate;
    RangedValueFactory<0, 127> m_127Factory;
    RangedValueFactory<1, 128> m_128Factory;
    RangedValueFactory<0, 255> m_255Factory;
    QStyledItemDelegate m_127Delegate, m_128Delegate, m_255Delegate;
public:
    explicit PageTableView(QWidget* parent = Q_NULLPTR);
    void setModel(QAbstractItemModel* model);
    void deleteSelection();
};

class SetupTableView : public QSplitter
{
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
    void setModel(QAbstractItemModel* list, QAbstractItemModel* table);
    void deleteSelection();
    void showEvent(QShowEvent* event);
};

class ColoredTabBarStyle : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;
    void drawControl(QStyle::ControlElement element, const QStyleOption *option,
                     QPainter *painter, const QWidget *widget = nullptr) const;
};

class ColoredTabBar : public QTabBar
{
    Q_OBJECT
public:
    explicit ColoredTabBar(QWidget* parent = Q_NULLPTR);
};

class ColoredTabWidget : public QTabWidget
{
    Q_OBJECT
    ColoredTabBar m_tabBar;
public:
    explicit ColoredTabWidget(QWidget* parent = Q_NULLPTR);
};

class MIDIPlayerWidget : public QWidget
{
    Q_OBJECT
    QAction m_playAction;
    QToolButton m_button;
    QModelIndex m_index;
    amuse::GroupId m_groupId;
    amuse::SongId m_songId;
    std::vector<uint8_t> m_arrData;
    amuse::ObjToken<amuse::Sequencer> m_seq;
public:
    explicit MIDIPlayerWidget(QModelIndex index, amuse::GroupId gid, amuse::SongId id,
                              std::vector<uint8_t>&& arrData, QWidget* parent = Q_NULLPTR);
    ~MIDIPlayerWidget();
    amuse::SongId songId() const { return m_songId; }
    amuse::Sequencer* sequencer() const { return m_seq.get(); }
    void stopped();
    void resizeEvent(QResizeEvent* event);
    void mouseDoubleClickEvent(QMouseEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event) { event->ignore(); }
public slots:
    void clicked();
};

class SongGroupEditor : public EditorWidget
{
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
    bool loadData(ProjectModel::SongGroupNode* node);
    void unloadData();
    ProjectModel::INode* currentNode() const;
    void setEditorEnabled(bool en) {}
    void resizeEvent(QResizeEvent* ev);
    QTableView* getSetupListView() const { return m_setupTable->m_listView; }
    AmuseItemEditFlags itemEditFlags() const;
public slots:
    void doAdd();
    void doSelectionChanged();
    void doSetupSelectionChanged();
    void currentTabChanged(int idx);
    void rowsAboutToBeRemoved(const QModelIndex &parent, int first, int last);
    void modelAboutToBeReset();
    void setupDataChanged();
    void itemDeleteAction();
};

#endif //AMUSE_SONG_GROUP_EDITOR_HPP
