#ifndef AMUSE_LAYERS_EDITOR_HPP
#define AMUSE_LAYERS_EDITOR_HPP

#include "EditorWidget.hpp"
#include <QAbstractTableModel>
#include <QTableView>
#include <QAction>
#include <QToolButton>
#include <QStyledItemDelegate>

class SoundMacroDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit SoundMacroDelegate(QObject* parent = Q_NULLPTR);
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    void setEditorData(QWidget* editor, const QModelIndex& index) const;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const;
private slots:
    void smIndexChanged();
};

class LayersModel : public QAbstractTableModel
{
    Q_OBJECT
    friend class LayersEditor;
    friend class SoundMacroDelegate;
    friend class LayersTableView;
    amuse::ObjToken<ProjectModel::LayersNode> m_node;
public:
    explicit LayersModel(QObject* parent = Q_NULLPTR);
    void loadData(ProjectModel::LayersNode* node);
    void unloadData();

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex& index) const;
    Qt::DropActions supportedDropActions() const;
    Qt::DropActions supportedDragActions() const;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent);

    bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex());
    bool moveRows(const QModelIndex& sourceParent, int sourceRow, int count,
                  const QModelIndex& destinationParent, int destinationChild);
    bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex());

    void _insertRow(int row, const amuse::LayerMapping& data);
    amuse::LayerMapping _removeRow(int row);
};

class LayersTableView : public QTableView
{
    Q_OBJECT
    SoundMacroDelegate m_smDelegate;
    RangedValueFactory<-128, 127> m_signedFactory;
    RangedValueFactory<0, 127> m_unsignedFactory;
    QStyledItemDelegate m_signedDelegate, m_unsignedDelegate;
public:
    explicit LayersTableView(QWidget* parent = Q_NULLPTR);
    void setModel(QAbstractItemModel* model);
    void deleteSelection();
};

class LayersEditor : public EditorWidget
{
    Q_OBJECT
    friend class LayersModel;
    LayersModel m_model;
    LayersTableView m_tableView;
    AddRemoveButtons m_addRemoveButtons;
public:
    explicit LayersEditor(QWidget* parent = Q_NULLPTR);
    bool loadData(ProjectModel::LayersNode* node);
    void unloadData();
    ProjectModel::INode* currentNode() const;
    void resizeEvent(QResizeEvent* ev);
    bool isItemEditEnabled() const;
public slots:
    void doAdd();
    void doSelectionChanged();

    void itemCutAction();
    void itemCopyAction();
    void itemPasteAction();
    void itemDeleteAction();
};

#endif //AMUSE_LAYERS_EDITOR_HPP
