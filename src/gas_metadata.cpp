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
        { QLatin1Char('N') + Defs::SUBTWO + QLatin1Char('O'), 44.01, 0.1436, DiffusivityStatus::Reviewed },
        { QStringLiteral("CO"), 28.0101, 0.1807, DiffusivityStatus::Reviewed },
        { QStringLiteral("SO") + Defs::SUBTWO, 64.066, 0.1089, DiffusivityStatus::Reviewed },
        { QStringLiteral("NH") + Defs::SUBTHREE, 17.0305, 0.1978, DiffusivityStatus::Reviewed },
        // Model-based diffusivity: auto-fill, warn.
        { QLatin1Char('O') + Defs::SUBTHREE, 47.9982, 0.1444, DiffusivityStatus::ModelBased },
        { QStringLiteral("NO") + Defs::SUBTWO, 46.0055, 0.1361, DiffusivityStatus::ModelBased },
        // Calculated diffusivity: auto-fill, warn.
        { QStringLiteral("NO"), 30.0061, 0.1988, DiffusivityStatus::Calculated },
        { QLatin1Char('N') + Defs::SUBTWO, 28.0134, 0.19939, DiffusivityStatus::Calculated },
        { QLatin1Char('O') + Defs::SUBTWO, 31.9988, 0.20255, DiffusivityStatus::Calculated },
        { QStringLiteral("Ar"), 39.948, 0.19064, DiffusivityStatus::Calculated },
        { QStringLiteral("COS"), 60.075, 0.12344, DiffusivityStatus::Calculated },
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
