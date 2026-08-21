/***************************************************************************
  gas_metadata.cpp
  -------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller

  This file is part of EddyFlow.
****************************************************************************/

#include "gas_metadata.h"

#include <QVector>

#include "defs.h"

namespace {

using GasMetadata::DiffusivityStatus;
using GasMetadata::GasEntry;

const QVector<GasEntry>& gasEntries()
{
    static const QVector<GasEntry> registry = {
        // Reviewed/default diffusivity: auto-fill, no warning.
        //> N2O's floor is the one non-zero entry: ambient nitrous oxide does
        //> not fall to zero, so a reading that low is an instrument fault.
        //> 44.0128, not 44.01: carbon dioxide and nitrous oxide weigh nearly
        //> the same, and at two decimals this table stated they weigh exactly
        //> the same. Six figures on the atomic weights the rest of the table
        //> already uses (N 14.0067, O 15.9994) tells them apart. Must match
        //> the engine's DefaultMolecularWeight - test_species_constants_static
        //> compares the two to nine decimals in kg/mol.
        { QLatin1Char('N') + Defs::SUBTWO + QLatin1Char('O'), 44.0128, 0.1436, DiffusivityStatus::Reviewed, 0.032, 1000.0 },
        //> 5 umol/mol, some thirty times the top of the measured 40-150
        //> nmol/mol background. Deliberately well above it: urban carbon
        //> monoxide reaches several umol/mol, and an absolute limit DELETES the
        //> sample rather than flagging it, so a ceiling drawn tight around the
        //> background would discard a roadside site's real data. Stated rather
        //> than left to the generic, which happens to be the same number today
        //> but is a fallback for species nobody has characterised.
        { QStringLiteral("CO"), 28.0101, 0.1807, DiffusivityStatus::Reviewed, 0.0, 5.0 },
        { QStringLiteral("SO") + Defs::SUBTWO, 64.066, 0.1089, DiffusivityStatus::Reviewed },
        { QStringLiteral("NH") + Defs::SUBTHREE, 17.0305, 0.1978, DiffusivityStatus::Reviewed },
        // Model-based diffusivity: auto-fill, warn.
        { QLatin1Char('O') + Defs::SUBTHREE, 47.9982, 0.1444, DiffusivityStatus::ModelBased },
        { QStringLiteral("NO") + Defs::SUBTWO, 46.0055, 0.1361, DiffusivityStatus::ModelBased },
        // Calculated diffusivity: auto-fill, warn.
        { QStringLiteral("NO"), 30.0061, 0.1988, DiffusivityStatus::Calculated },
        //> The three major constituents need explicit ceilings for the same
        //> reason methane does: they sit at 780,900, 209,500 and 9,340 umol/mol,
        //> so the generic 5 would reject every reading were one of them ever
        //> configured as a flux gas. Their abundances barely vary; these are
        //> those abundances with headroom, not invented windows.
        { QLatin1Char('N') + Defs::SUBTWO, 28.0134, 0.19939, DiffusivityStatus::Calculated, 0.0, 1.0e6 },
        { QLatin1Char('O') + Defs::SUBTWO, 31.9988, 0.20255, DiffusivityStatus::Calculated, 0.0, 3.0e5 },
        { QStringLiteral("Ar"), 39.948, 0.19064, DiffusivityStatus::Calculated, 0.0, 12000.0 },
        //> ~20x the ~0.5 nmol/mol background, deliberately loose: carbonyl
        //> sulfide has strong local sources and sinks.
        { QStringLiteral("COS"), 60.075, 0.12344, DiffusivityStatus::Calculated, 0.0, 0.01 },
        // Molecular-weight only: typeable, but not part of the standard dropdown.
        { QStringLiteral("Ne"), 20.1797, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("He"), 4.0026, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("Kr"), 83.798, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("Xe"), 131.293, 0.0, DiffusivityStatus::Manual },
        { QLatin1Char('H') + Defs::SUBTWO, 2.0159, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("NO") + Defs::SUBTHREE, 62.0049, 0.0, DiffusivityStatus::Manual },
        { QLatin1Char('N') + Defs::SUBTWO + QLatin1Char('O') + QChar(0x2085), 108.0104, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("HONO"), 47.0134, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("HNO") + Defs::SUBTHREE, 63.0128, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("PAN"), 121.0489, 0.0, DiffusivityStatus::Manual },
        { QLatin1Char('H') + Defs::SUBTWO + QLatin1Char('S'), 34.0809, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("CS") + Defs::SUBTWO, 76.1407, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("DMS"), 62.134, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("DMSO"), 78.133, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("CH") + Defs::SUBTHREE + QStringLiteral("SH"), 48.107, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("DMDS"), 94.199, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("isoprene"), 68.117, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("alpha-pinene"), 136.238, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("methanol"), 32.042, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("acetone"), 58.080, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("formaldehyde"), 30.026, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("acetaldehyde"), 44.053, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("glyoxal"), 58.036, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("benzene"), 78.114, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("toluene"), 92.141, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("CFC-11"), 137.368, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("CFC-12"), 120.913, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("HCFC-22"), 86.468, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("HFC-134a"), 102.03, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("HFC-23"), 70.014, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("SF") + QChar(0x2086), 146.055, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("NF") + Defs::SUBTHREE, 71.0019, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("CF") + Defs::SUBFOUR, 88.0043, 0.0, DiffusivityStatus::Manual },
        { QLatin1Char('C') + Defs::SUBTWO + QStringLiteral("F") + QChar(0x2086), 138.0118, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("CCl") + Defs::SUBFOUR, 153.823, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("CH") + Defs::SUBTHREE + QStringLiteral("Cl"), 50.487, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("CH") + Defs::SUBTHREE + QStringLiteral("Br"), 94.939, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("CH") + Defs::SUBTHREE + QLatin1Char('I'), 141.939, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("halon-1211"), 165.364, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("halon-1301"), 148.91, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("halon-2402"), 259.823, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("HCl"), 36.458, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("HBr"), 80.912, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("HOCl"), 52.460, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("HOBr"), 96.912, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("ClO"), 51.452, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("BrO"), 95.904, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("IO"), 142.904, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("OH"), 17.007, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("HO") + Defs::SUBTWO, 33.0067, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("Hg0"), 200.592, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("Rn"), 222.0, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("HCN"), 27.0253, 0.0, DiffusivityStatus::Manual },
        { QStringLiteral("CH") + Defs::SUBTHREE + QStringLiteral("CN"), 41.052, 0.0, DiffusivityStatus::Manual },
    };
    return registry;
}

} // namespace

namespace GasMetadata {

QString normaliseFormula(const QString& s)
{
    QString r;
    r.reserve(s.size());
    for (const QChar& c : s)
    {
        const ushort u = c.unicode();
        r += (u >= 0x2080 && u <= 0x2089)
             ? QChar(static_cast<ushort>('0' + u - 0x2080))
             : c;
    }
    return r.toLower();
}

const GasEntry* findGas(const QString& formula)
{
    const QString key = normaliseFormula(formula.split(QLatin1Char(' ')).first());
    const auto& registry = gasEntries();
    for (const GasEntry& entry : registry)
    {
        if (normaliseFormula(entry.displayFormula) == key)
        {
            return &entry;
        }
    }
    return nullptr;
}

const GasEntry* findSpecies(const QString& formula)
{
    //> The three historical gases, with the values the variable table used to
    //> carry as literals against a fixed row kind. Kept out of the registry
    //> above so they stay out of the open slot's dropdown, where they would
    //> duplicate the dedicated rows.
    static const QVector<GasEntry> historic = {
        //> Six figures, like the registry above. These three were the last
        //> entries still at the two decimals the old fixed-row table used, and
        //> two decimals is not enough to separate CO2 from N2O. Water's value
        //> is also the engine's MW_H2O, and dry air moved with it - see
        //> m_common_global_var.f90, where mu = Md/MW_H2O and the two roundings
        //> used to cancel.
        //> The ceilings these three carry today, moved here so every species'
        //> window lives in one table. CO2's 900 is 2.1x its 422.8 umol/mol
        //> background; CH4's 1000 is generous but is what the project default
        //> has always been; water's 40 is on the mmol/mol basis, which is the
        //> one place in this table where the unit differs.
        { QStringLiteral("CO") + Defs::SUBTWO, 44.0095, 0.1381, DiffusivityStatus::Reviewed, 0.0, 900.0 },
        { QLatin1Char('H') + Defs::SUBTWO + QLatin1Char('O'), 18.0153, 0.2178, DiffusivityStatus::Reviewed, 0.0, 40.0 },
        { QStringLiteral("CH") + Defs::SUBFOUR, 16.0425, 0.1952, DiffusivityStatus::Reviewed, 0.0, 1000.0 },
    };

    const QString key = normaliseFormula(formula.split(QLatin1Char(' ')).first());
    for (const GasEntry& entry : historic)
    {
        if (normaliseFormula(entry.displayFormula) == key)
        {
            return &entry;
        }
    }
    return findGas(formula);
}

double defaultAbsoluteLimitMin(const QString& formula)
{
    //> From the registry, where the rest of a species' constants live. This
    //> was an `if (key == "n2o")`, so adding a floor for another gas meant
    //> editing a function rather than the row that describes it - and the
    //> function was the only place a species constant did not live beside
    //> its molecular weight.
    const GasEntry* gas = findSpecies(formula);
    return gas ? gas->absoluteLimitMin : 0.0;
}

double defaultAbsoluteLimitMax(const QString& formula)
{
    //> The species' own ceiling when it states one, and the generic otherwise.
    //> Zero cannot be returned: the engine reads max <= min as "limits absent",
    //> declines the absolute-limits test and now says so through Warning(109).
    const GasEntry* gas = findSpecies(formula);
    if (gas && gas->absoluteLimitMax > 0.0) { return gas->absoluteLimitMax; }
    return genericAbsoluteLimitMax;
}

QStringList selectableGasVariableList()
{
    QStringList result;
    const auto& registry = gasEntries();
    for (const GasEntry& entry : registry)
    {
        if (entry.status != DiffusivityStatus::Manual)
        {
            result << entry.displayFormula;
        }
    }
    return result;
}

bool isSelectableGasVariable(const QString& formula)
{
    const GasEntry* entry = findGas(formula);
    return entry && entry->status != DiffusivityStatus::Manual;
}

bool isManualDiffusivityGas(const QString& formula)
{
    const GasEntry* entry = findGas(formula);
    return entry && entry->status == DiffusivityStatus::Manual;
}

} // namespace GasMetadata
