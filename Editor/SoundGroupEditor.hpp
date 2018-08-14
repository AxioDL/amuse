#ifndef AMUSE_SOUND_GROUP_EDITOR_HPP
#define AMUSE_SOUND_GROUP_EDITOR_HPP

#include "EditorWidget.hpp"
#include <QStyledItemDelegate>
#include <QTableView>
#include "amuse/Voice.hpp"

class SFXObjectDelegate : public QStyledItemDelegate
{
Q_OBJECT
public:
    explicit SFXObjectDelegate(QObject* parent = Q_NULLPTR);
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    void setEditorData(QWidget* editor, const QModelIndex& index) const;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const;
private slots:
    void objIndexChanged();
};

class SFXModel : public QAbstractTableModel
{
Q_OBJECT
    friend class SoundGroupEditor;
    friend class SFXObjectDelegate;
    friend class SFXTableView;
    amuse::ObjToken<ProjectModel::SoundGroupNode> m_node;
    struct Iterator
    {
        using ItTp = std::unordered_map<amuse::SFXId, amuse::SFXGroupIndex::SFXEntry>::iterator;
        ItTp m_it;
        Iterator(ItTp it) : m_it(it) {}
        ItTp::pointer operator->() const { return m_it.operator->(); }
        bool operator<(const Iterator& other) const
        {
            return amuse::SFXId::CurNameDB->resolveNameFromId(m_it->first) <
                   amuse::SFXId::CurNameDB->resolveNameFromId(other.m_it->first);
        }
        bool operator<(amuse::SongId other) const
        {
            return amuse::SFXId::CurNameDB->resolveNameFromId(m_it->first) <
                   amuse::SFXId::CurNameDB->resolveNameFromId(other);
        }
        bool operator<(const std::string& name) const
        {
            return amuse::SFXId::CurNameDB->resolveNameFromId(m_it->first) < name;
        }
    };
    std::vector<Iterator> m_sorted;
    std::unordered_map<amuse::SFXId, amuse::SFXGroupIndex::SFXEntry>& _getMap() const;
    void _buildSortedList();
    QModelIndex _indexOfSFX(amuse::SFXId sfx) const;
    int _hypotheticalIndexOfSFX(const std::string& sfxName) const;
public:
    explicit SFXModel(QObject* parent = Q_NULLPTR);
    void loadData(ProjectModel::SoundGroupNode* node);
    void unloadData();

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex& index) const;

    int _insertRow(const std::tuple<amuse::SFXId, std::string, amuse::SFXGroupIndex::SFXEntry>& data);
    std::tuple<amuse::SFXId, std::string, amuse::SFXGroupIndex::SFXEntry> _removeRow(amuse::SFXId sfx);
};

class SFXTableView : public QTableView
{
Q_OBJECT
    SFXObjectDelegate m_sfxDelegate;
    RangedValueFactory<0, 127> m_127Factory;
    RangedValueFactory<0, 255> m_255Factory;
    QStyledItemDelegate m_127Delegate, m_255Delegate;
public:
    explicit SFXTableView(QWidget* parent = Q_NULLPTR);
    void setModel(QAbstractItemModel* model);
    void deleteSelection();
};

class SFXPlayerWidget : public QWidget
{
    Q_OBJECT
    QAction m_playAction;
    QToolButton m_button;
    QModelIndex m_index;
    amuse::GroupId m_groupId;
    amuse::SFXId m_sfxId;
    amuse::ObjToken<amuse::Voice> m_vox;
public:
    explicit SFXPlayerWidget(QModelIndex index, amuse::GroupId gid, amuse::SFXId id,
                             QWidget* parent = Q_NULLPTR);
    ~SFXPlayerWidget();
    amuse::SongId sfxId() const { return m_sfxId; }
    amuse::Voice* voice() const { return m_vox.get(); }
    void stopped();
    void resizeEvent(QResizeEvent* event);
    void mouseDoubleClickEvent(QMouseEvent* event);
public slots:
    void clicked();
};

class SoundGroupEditor : public EditorWidget
{
Q_OBJECT
    SFXModel m_sfxs;
    SFXTableView* m_sfxTable;
    AddRemoveButtons m_addRemoveButtons;
public:
    explicit SoundGroupEditor(QWidget* parent = Q_NULLPTR);
    bool loadData(ProjectModel::SoundGroupNode* node);
    void unloadData();
    ProjectModel::INode* currentNode() const;
    void setEditorEnabled(bool en) {}
    void resizeEvent(QResizeEvent* ev);
    QTableView* getSFXListView() const { return m_sfxTable; }
    bool isItemEditEnabled() const;
public slots:
    void doAdd();
    void doSelectionChanged();
    void sfxDataChanged();

    void itemCutAction();
    void itemCopyAction();
    void itemPasteAction();
    void itemDeleteAction();
};

#endif //AMUSE_SOUND_GROUP_EDITOR_HPP
