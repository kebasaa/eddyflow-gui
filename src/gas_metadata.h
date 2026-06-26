/***************************************************************************
  gas_metadata.h
  -------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller

  This file is part of EddyFlow.
****************************************************************************/

#ifndef GAS_METADATA_H
#define GAS_METADATA_H

#include <QString>
#include <QStringList>

namespace GasMetadata {

enum class DiffusivityStatus {
    Reviewed = 0,
    ModelBased,
    Calculated,
    Manual
};

struct GasEntry {
    QString displayFormula;
    double molecularWeight;
    double diffusivity;
    DiffusivityStatus status;
};

QString normaliseFormula(const QString& s);
const GasEntry* findGas(const QString& formula);
QStringList selectableGasVariableList();
bool isSelectableGasVariable(const QString& formula);
bool isManualDiffusivityGas(const QString& formula);

} // namespace GasMetadata

#endif // GAS_METADATA_H
