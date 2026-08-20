/***************************************************************************
  cecpairmodel.h
  --------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef CECPAIRMODEL_H
#define CECPAIRMODEL_H

#include <QAbstractTableModel>
#include <QStyledItemDelegate>
#include <QVector>

#include "measurement_record.h"

class EcProject;

/// The Conditional Eddy Covariance pairings, one per row.
///
/// A pairing is one carbon channel, one water channel, and any further species
/// partitioned in the octants those two define. Which channels go together
/// used to be implicit - the first record of each species, whatever analyser
/// each happened to sit on - and a site running two analysers had no way to
/// say that each should be partitioned against its own water.
///
/// Cells carry the 1-based gas RECORD index in Qt::UserRole, never the label,
/// for the same reason the moisture column in the Basic Settings variable
/// table does: the label is what the user reads and the index is what the
/// engine keys on, and two channels of one species share a label.
class CecPairModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        Use = 0,
        Carbon,
        Water,
        Partition,
        Extra,
        Warning,
        ColumnCount
    };

    explicit CecPairModel(EcProject* ecProject, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    /// Re-read the project. Called whenever the gas records change, because a
    /// pairing names records and a record that has gone takes its pairing with
    /// it.
    void reload();

    /// Seed the same-instrument default the engine would derive on its own.
    void restoreDefaults();

    void addPair();
    void removePair(int row);

    /// Every gas record of a species, as (record index, label) pairs.
    QVector<QPair<int, QString>> channelChoices(const QString& slug) const;
    /// Everything that is neither the carbon nor the water of this pairing.
    QVector<QPair<int, QString>> extraChoices(int row) const;

    bool crossAnalyser(int row) const;

private:
    void commit();
    QString labelFor(int recordIndex) const;

    EcProject* ecProject_;
    QVector<CecPairRecord> pairs_;
};

/// Combo cells for the channel and partition columns, and a checkable popup
/// for the extra species. Same shape as BasicVariableSelectionDelegate, which
/// is the other table in this program where a cell picks a measurement.
class CecPairDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit CecPairDelegate(CecPairModel* model, QObject* parent = nullptr);

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model,
                     const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;

private:
    CecPairModel* model_;
};

#endif // CECPAIRMODEL_H
