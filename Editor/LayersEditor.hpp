#pragma once

#include "EditorWidget.hpp"
#include <QAbstractTableModel>
#include <QTableView>
#include <QAction>
#include <QToolButton>
#include <QStyledItemDelegate>

class SoundMacroDelegate : public BaseObjectDelegate {
  Q_OBJECT
protected:
  ProjectModel::INode* getNode(const QAbstractItemModel* model, const QModelIndex& index) const override;

public:
  explicit SoundMacroDelegate(QObject* parent = Q_NULLPTR);
  QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
private slots:
  void smIndexChanged();
};

class LayersModel : public QAbstractTableModel {
  Q_OBJECT
  friend class LayersEditor;
  friend class SoundMacroDelegate;
  friend class LayersTableView;
  amuse::ObjToken<ProjectModel::LayersNode> m_node;

public:
  explicit LayersModel(QObject* parent = Q_NULLPTR);
  void loadData(ProjectModel::LayersNode* node);
  void unloadData();

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  Qt::DropActions supportedDropActions() const override;
  Qt::DropActions supportedDragActions() const override;
  bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                    const QModelIndex& parent) override;

  bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
  bool moveRows(const QModelIndex& sourceParent, int sourceRow, int count, const QModelIndex& destinationParent,
                int destinationChild) override;
  bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

  void _insertRow(int row, const amuse::LayerMapping& data);
  amuse::LayerMapping _removeRow(int row);
};

class LayersTableView : public QTableView {
  Q_OBJECT
  SoundMacroDelegate m_smDelegate;
  RangedValueFactory<-128, 127> m_signedFactory;
  RangedValueFactory<0, 127> m_unsignedFactory;
  QStyledItemDelegate m_signedDelegate, m_unsignedDelegate;

public:
  explicit LayersTableView(QWidget* parent = Q_NULLPTR);
  void setModel(QAbstractItemModel* model) override;
  void deleteSelection();
};

class LayersEditor : public EditorWidget {
  Q_OBJECT
  friend class LayersModel;
  LayersModel m_model;
  LayersTableView m_tableView;
  AddRemoveButtons m_addRemoveButtons;

public:
  explicit LayersEditor(QWidget* parent = Q_NULLPTR);
  bool loadData(ProjectModel::LayersNode* node);
  void unloadData() override;
  ProjectModel::INode* currentNode() const override;
  void resizeEvent(QResizeEvent* ev) override;
  AmuseItemEditFlags itemEditFlags() const override;
private slots:
  void rowsInserted(const QModelIndex& parent, int first, int last);
  void rowsMoved(const QModelIndex& parent, int start, int end, const QModelIndex& destination, int row);
  void doAdd();
  void doSelectionChanged();
  void itemDeleteAction() override;
};
