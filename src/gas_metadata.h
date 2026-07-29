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

//> Like findGas, but also answers for CO2, H2O and CH4.
//>
//> Those three are absent from the selectable registry because they are not
//> offered in the open slot's dropdown - they have dedicated rows. They are
//> still gases with a molecular weight and a diffusivity, and any code that
//> works from a species slug rather than a fixed row needs them.
const GasEntry* findSpecies(const QString& formula);

//> Species-specific floor for the absolute-limit test, or 0 for "no floor".
//>
//> This was written into the interface as a literal 0.032 applied whenever
//> the fourth slot happened to hold N2O. It is a property of the species, not
//> of the slot. Only N2O has a published value today; every other species
//> answers 0, which is the same "no species-specific floor" the fourth slot
//> already used for everything that was not N2O.
double defaultAbsoluteLimitMin(const QString& formula);

QStringList selectableGasVariableList();
bool isSelectableGasVariable(const QString& formula);
bool isManualDiffusivityGas(const QString& formula);

} // namespace GasMetadata

#endif // GAS_METADATA_H
