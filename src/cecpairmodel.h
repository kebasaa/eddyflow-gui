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

/// The Conditional Eddy Covariance table: one row per flux to partition.
///
/// Each row names the flux, the water whose moist ejections select the
/// parcels, and the octant gas whose sign sorts them - CO2 as Zahn et al.
/// (2022) define the method, or carbonyl sulfide, experimentally. The project
/// file does not store rows. It stores pairings, one per octant gas and water,
/// which is what the engine runs; rows sharing both are one pairing, and the
/// table is rebuilt from the pairings after every edit so that what the user
/// sees is exactly what the file reloads.
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
        Flux = 0,
        Water,
        Octants,
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
    /// row names records and a record that has gone takes its row with it.
    void reload();

    /// Seed the same-instrument default the engine would derive on its own.
    void restoreDefaults();

    /// Add a row for the first channel not yet partitioned, carbonyl sulfide
    /// first, with the water and octant gas on its own analyser.
    void addPair();
    void removePair(int row);

    /// The records a column's combo offers, as (record index, label) pairs.
    QVector<QPair<int, QString>> choices(int column) const;

    /// The octant gas and the water this row's pairing reads sit on two
    /// analysers.
    bool crossAnalyser(int row) const;
    /// The row goes beyond the published method. See isCecExperimental.
    bool experimental(int row) const;
    bool anyExperimental() const;

signals:
    /// An edit was not applied, and why - the engine could not have run it.
    void editRefused(const QString& reason);

private:
    bool apply(const QVector<CecFluxRow>& candidate);
    void commit();
    QString labelFor(int recordIndex) const;
    QString fluxLabel(const CecFluxRow& row) const;
    QString octantLabel(int recordIndex) const;
    QString fluxTooltip(const CecFluxRow& row) const;

    EcProject* ecProject_;
    QVector<CecFluxRow> rows_;
};

/// Combo cells for the flux, water and octant columns. Same shape as
/// BasicVariableSelectionDelegate, which is the other table in this program
/// where a cell picks a measurement.
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

private:
    CecPairModel* model_;
};

#endif // CECPAIRMODEL_H
