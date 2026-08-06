/***************************************************************************
  ancillaryfiletest.cpp
  -------------------
  Copyright © 2014-2018, LI-COR Biosciences, Antonio Forgione
  Copyright © 2026,      ETH Zurich, Jonathan Muller

  This file is part of EddyFlow®.

  EddyFlow (TM) is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version. You should have received a copy
  of the GNU General Public License along with EddyFlow (R). If not,
  see <http://www.gnu.org/licenses/>.

  EddyFlow® contains additional Open Source Components. The licenses
  and/or notices these Components can be found in the file LIBRARIES.txt.

  EddyFlow® is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.
****************************************************************************/

#include "ancillaryfiletest.h"

#include <QDateTime>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QTextBrowser>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QVBoxLayout>
#include <QVector>

#include <algorithm>

#include "ecproject.h"
#include "stringutils.h"
#include "container_helpers.h"
#include "widget_utils.h"

const auto helpPage = QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Assessment_Tests.html");

namespace {

/// Whether the project measures the gas that owns \a slot, where slot is a
/// 0-based position in the gas record list.
///
/// The template these tests compare against is positional - rows 33-44 are
/// the third gas's block and 47-58 the fourth's - so the question really is
/// about the slot, not the species. Records answer it directly; the legacy
/// column is the fallback for a project written before records existed.
bool gasSlotConfigured(const EcProject* project, int slot)
{
    if (!project) { return false; }
    const auto& gases = project->gasColumns();
    //> Every slot handed here comes from tfpGasSlots, which builds them from
    //> the record list, so an out-of-range one is a caller bug rather than an
    //> older project. The legacy col_ch4 / col_gas4 fallback that used to sit
    //> here answered by slot number - it read slot two as methane, which was
    //> true only while the record list reserved a position for every species.
    return slot >= 0 && slot < gases.size() && gases.at(slot).rawColumn > 0;
}

/// Whether the project measures \a slug at all.
///
/// For the tests that are about one species rather than one block position -
/// the methane cutoff checks name CH4 in their own labels.
bool gasSpeciesConfigured(const EcProject* project, const QString& slug)
{
    if (!project) { return false; }
    for (const auto& gas : project->gasColumns())
    {
        if (gas.slug == slug && gas.rawColumn > 0) { return true; }
    }
    return false;
}

/// Rows a single gas's transfer-function block occupies: twelve month labels
/// and the two header rows that follow it.
constexpr int kSpectraGasBlockRows = 14;
/// Rows the file carries regardless of how many gases it describes: the
/// header and RH-class table above the blocks, and the exponential-fit and
/// high-pass sections below them.
constexpr int kSpectraFixedRows = 29;

/// Record indices of the gases that get a transfer-function block, in the
/// order the engine writes them.
///
/// Water is not among them: its cutoffs are the nine RH classes tabulated
/// above the blocks, which is why the engine writes "one block per configured
/// gas but water". So the block sequence is not the record sequence, and the
/// third block is the fourth record only on a project laid out CO2, H2O, CH4,
/// other - which is what the three hard-coded positions this replaces assumed.
QVector<int> tfpGasSlots(const EcProject* project)
{
    //> Not named `slots`: Qt #defines that away for the moc's `public slots:`
    //> syntax, so the declaration would expand to `QVector<int> ;`.
    QVector<int> blockSlots;
    if (!project) { return blockSlots; }

    const auto& gases = project->gasColumns();
    for (int i = 0; i < gases.size(); ++i)
    {
        if (gases.at(i).slug == QLatin1String("h2o")) { continue; }
        blockSlots.append(i);
    }
    return blockSlots;
}

/// Display name of the gas in \a slot, for the skip messages. A record knows
/// its species; without one there is nothing to name.
QString gasSlotName(const EcProject* project, int slot)
{
    if (project)
    {
        const auto& gases = project->gasColumns();
        if (slot < gases.size() && !gases.at(slot).slug.isEmpty())
        {
            return gases.at(slot).slug.toUpper();
        }
    }
    return AncillaryFileTest::tr("Other gas");
}

QString joinedLine(const QStringList& line)
{
    return line.join(QLatin1Char(' ')).simplified();
}

QString firstField(const QStringList& line)
{
    return joinedLine(line).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).value(0).trimmed();
}

QStringList whitespaceFields(const QStringList& line)
{
    return joinedLine(line).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
}

QString normalizedTimelagLabel(QString label)
{
    label = label.trimmed().toLower();
    label.replace(QLatin1Char('-'), QLatin1Char('_'));
    label.replace(QStringLiteral("optimisation"), QStringLiteral("optimization"));
    label.replace(QStringLiteral("mimimum"), QStringLiteral("minimum"));
    while (label.endsWith(QLatin1Char(':')))
    {
        label.chop(1);
    }
    return label.simplified();
}

QString normalizedTimelagLine(const QStringList& line)
{
    QString label = joinedLine(line).toLower();
    label.replace(QLatin1Char('-'), QLatin1Char('_'));
    label.replace(QStringLiteral("optimisation"), QStringLiteral("optimization"));
    label.replace(QStringLiteral("mimimum"), QStringLiteral("minimum"));
    while (label.endsWith(QLatin1Char(':')))
    {
        label.chop(1);
    }
    return label.simplified();
}

bool matchesTimelagHeaderRow(const QStringList& line, int row)
{
    const QString label = normalizedTimelagLabel(firstField(line));
    switch (row)
    {
        case 0:
            return label == QLatin1String("time_lag_optimization_results");
        case 1:
            return label == QLatin1String("plausibility_range_[timefolds_standard_deviation]");
        case 2:
            return label == QLatin1String("beginning_of_timelag_optimization_period");
        case 3:
            return label == QLatin1String("end_of_timelag_optimization_period");
        case 4:
            return joinedLine(line).isEmpty();
        default:
            return false;
    }
}

bool parseGasTimelagLabel(const QStringList& line, const QString& expectedPrefix, QString* gas)
{
    const QString label = normalizedTimelagLabel(firstField(line));
    const QString suffix = QStringLiteral("_timelag_[s]");
    if (!label.startsWith(expectedPrefix) || !label.endsWith(suffix))
    {
        return false;
    }

    const int gasStart = expectedPrefix.size();
    const int gasLength = label.size() - gasStart - suffix.size();
    if (gasLength <= 0)
    {
        return false;
    }

    if (gas)
    {
        *gas = label.mid(gasStart, gasLength);
    }
    return true;
}

bool parseNumberOfTimelagsLabel(const QStringList& line, QString* gas)
{
    const QString label = normalizedTimelagLabel(firstField(line));
    const QString prefix = QStringLiteral("number_of_timelags_used_for_");
    if (!label.startsWith(prefix) || label.size() <= prefix.size())
    {
        return false;
    }

    if (gas)
    {
        *gas = label.mid(prefix.size());
    }
    return true;
}

bool matchesGasTimelagBlock(const QList<QStringList>& lines, int start)
{
    QString gas;
    if (!parseNumberOfTimelagsLabel(lines.value(start), &gas))
    {
        return false;
    }

    QString medianGas;
    QString minGas;
    QString maxGas;
    return parseGasTimelagLabel(lines.value(start + 1), QStringLiteral("median_"), &medianGas)
            && parseGasTimelagLabel(lines.value(start + 2), QStringLiteral("minimum_"), &minGas)
            && parseGasTimelagLabel(lines.value(start + 3), QStringLiteral("maximum_"), &maxGas)
            && medianGas == gas
            && minGas == gas
            && maxGas == gas;
}

bool matchesRhTimelagHeader(const QList<QStringList>& lines, int start)
{
    const QString title = normalizedTimelagLabel(firstField(lines.value(start)));
    const QString details = normalizedTimelagLine(lines.value(start + 1));
    const QStringList columns = whitespaceFields(lines.value(start + 2));

    return title == QLatin1String("h2o_timelag_determinations_as_a_function_of_relative_humidity")
            && details.contains(QStringLiteral("classes with numerosity"))
            && details.contains(QStringLiteral("30"))
            && details.contains(QStringLiteral("inferred"))
            && columns.value(0).compare(QLatin1String("class"), Qt::CaseInsensitive) == 0
            && columns.value(1).compare(QLatin1String("RH-range"), Qt::CaseInsensitive) == 0
            && columns.value(2).compare(QLatin1String("med_h2o"), Qt::CaseInsensitive) == 0
            && columns.value(3).compare(QLatin1String("min_h2o"), Qt::CaseInsensitive) == 0
            && columns.value(4).compare(QLatin1String("max_h2o"), Qt::CaseInsensitive) == 0
            && columns.value(5).compare(QLatin1String("class_num"), Qt::CaseInsensitive) == 0;
}

} // namespace

AncillaryFileTest::AncillaryFileTest(FileType type,
                                     EcProject* ecProject,
                                     QWidget *parent) :
    QDialog(parent),
    type_(type),
    ecProject_(ecProject)
{
    setVisible(false);

    setWindowModality(Qt::WindowModal);
    setWindowTitle(tr("Assessment file test results"));
    WidgetUtils::removeContextHelpButton(this);

    testResults_ = new QTextBrowser(this);
    testResults_->setReadOnly(true);
    testResults_->setMinimumWidth(800);

    // neccesary to avoid following the question mark link as a document link
    // the connection with QTextBrowser::anchorClicked will provide the
    // expected behavior
    testResults_->setOpenLinks(false);

    auto cancelButton = new QPushButton(tr("Cancel"));
    cancelButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    cancelButton->setDefault(true);
    cancelButton->setProperty("commonButton", true);

    auto continueButton = new QPushButton(tr("Continue"));
    continueButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    continueButton->setDefault(true);
    continueButton->setProperty("commonButton", true);

    auto saveButton = new QPushButton(tr("Save to file"));
    saveButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    saveButton->setDefault(true);
    saveButton->setProperty("commonButton", true);

    auto buttonBox = new QDialogButtonBox;
    buttonBox->addButton(continueButton, QDialogButtonBox::AcceptRole);
    buttonBox->addButton(cancelButton, QDialogButtonBox::RejectRole);
    buttonBox->addButton(saveButton, QDialogButtonBox::ActionRole);

    auto dialogLayout = new QVBoxLayout(this);
    dialogLayout->addWidget(testResults_);
    dialogLayout->addWidget(buttonBox, 0, Qt::AlignCenter);
    setLayout(dialogLayout);

    connect(cancelButton, &QPushButton::clicked,
            [=](){ this->close(); this->setResult(QDialog::Rejected); });
    connect(continueButton, &QPushButton::clicked,
            [=](){ this->close(); this->setResult(QDialog::Accepted); });
    connect(saveButton, &QPushButton::clicked, [=](){ this->saveResults(); });

    connect(testResults_, &QTextBrowser::anchorClicked,
            [=](const QUrl& link){ WidgetUtils::showHelp(link); });
}

void AncillaryFileTest::refresh(const QString &file)
{
    name_ = file;
    actualLines_.clear();
    testResults_->clear();
    setVisible(false);
}

QString AncillaryFileTest::formatPassFail(bool test_result)
{
    return (test_result ? QStringLiteral("<font color=\"#0066FF\">pass</font>")
                        : QStringLiteral("<font color=\"#FF3300\">fail</font>"));
}

bool AncillaryFileTest::makeTest()
{
    auto result = testFile();
    if (!result)
    {
        setVisible(true);
        testResults_->moveCursor(QTextCursor::End);
    }
    return result;
}

bool AncillaryFileTest::testFile()
{
    bool parseResult = false;
    bool formalResult = false;
    bool scientificResult = false;

    const auto parseErrorStr_1 =
            tr("<b>The formatting and content of the selected file "
               "could not be assessed due to missing "
               "template files. Please, re-install the software.</b>");

    const auto parseErrorStr_2 =
            tr("<b>Unable to open the selected file or the file "
               "is empty. Please, select another file.</b>");

    // if not already read the template file
    if (templateLines_.isEmpty())
    {
        // test presence of the template file and read it
        parseResult
            = parseFile(testFileMap_.value(type_).filepath, &templateLines_);
        if (!parseResult)
        {
            testResults_->append(parseErrorStr_1);
            return false;
        }
    }

    parseResult = parseFile(name_, &actualLines_);
    if (!parseResult)
    {
        testResults_->append(parseErrorStr_2);
        return false;
    }

    testResults_->append(QLatin1String("<b>FORMAT test</b>"));
    formalResult =
            (this->*testFileMap_.value(type_).formalTest)(templateLines_,
                                                          actualLines_);
    const auto formalErrorStr =
            tr("<b>FORMAT test <font color=\"#FF3300\">failed</font>.</b><br />");
    const auto formalSuccessStr =
            tr("<b>FORMAT test <font color=\"#0066FF\">passed</font>.</b><br />");
    const auto finalErrorStr =
            tr("<b>The selected file does not match the expected "
               "formatting or scientific content. "
               "<p>If you would like to upload a different file or choose an alternate method, please click <i>Cancel</i>. "
               "If you click <i>Continue</i>, EddyFlow will probably not use the file and will resort to the default method.</p>"
               "<p>More information about the testing performed "
               "can be found in the help.</b>&nbsp;"
               "<a href=\"%1\"><img src=\"qrc:/icons/qm-enabled\"></img></a>").arg(helpPage);

    if (!formalResult)
    {
        testResults_->append(formalErrorStr);
        testResults_->append(finalErrorStr);
    }
    else
    {
        testResults_->append(formalSuccessStr);
        testResults_->insertHtml(QStringLiteral("<br>"));

        testResults_->append(tr("<b>SCIENTIFIC test</b>"));
        scientificResult =
                (this->*testFileMap_.value(type_).scientificTest)(actualLines_);
        const auto scientificErrorStr =
                tr("<b>SCIENTIFIC test <font color=\"#FF3300\">failed</font>.</b><br />");

        if (!scientificResult)
        {
            testResults_->append(scientificErrorStr);
            testResults_->append(finalErrorStr);
        }
    }

    return (formalResult && scientificResult);
}

bool AncillaryFileTest::parseFile(const QString& filename, LineList *lines)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "parseFile error: file open" << filename;
        return false;
    }

    QTextStream in(&file);
    QString line;

    line = in.readLine();
    if (line.isNull()) { return false; }

    const auto space = QLatin1Char(' ');
    *lines << line.split(space, Qt::SkipEmptyParts);
    while (!line.isNull())
    {
        line = in.readLine();
        *lines << line.split(space, Qt::SkipEmptyParts);
    }

    file.close();
    return true;
}

bool AncillaryFileTest::testSpectraF(const LineList& templateList, const LineList& actualList)
{
    // test total number of rows
    auto rowCountTest = (actualList.size() == templateList.size());
    testResults_->append(QLatin1String("Number of rows [")
                  + QString::number(actualList.size())
                  + QStringLiteral("]: ")
                  + formatPassFail(rowCountTest));
    if (!rowCountTest)
    {
        //> The commonest cause, and the one a bare row count does not
        //> explain: the file carries fourteen rows per non-water gas, and the
        //> shipped template describes three of them. A project with more
        //> gases produces a longer file that is not wrong, only longer.
        const auto blocks = tfpGasSlots(ecProject_).size();
        const auto templateBlocks =
            (templateList.size() - kSpectraFixedRows) / kSpectraGasBlockRows;
        if (blocks != templateBlocks)
        {
            testResults_->append(
                tr("The sample file describes %1 gases and this project has "
                   "%2. Each gas adds %3 rows, so the counts differ by design "
                   "— compare against a sample from a project with the same "
                   "gases.")
                    .arg(templateBlocks).arg(blocks).arg(kSpectraGasBlockRows));
        }
        return false;
    }

    // other tests
    QList<bool> test;
    auto last_test = [&](){ return test.value(test.size() - 1); };

    // test header, rows 1-7
    test << ContainerHelper::rangeEqual(templateList, actualList, 0, 7);
    testResults_->append(QLatin1String("Header, rows 1-7: ")
                                  + formatPassFail(last_test()));

    // test water vapour TFP labels, rows 8-16
    for (auto i = 7; i < 16; ++i)
    {
        test << (StringUtils::subStringList(templateList.value(i), 0, 6)
                == StringUtils::subStringList(actualList.value(i), 0, 6));
        testResults_->append(QLatin1String("<u>H<sub>2</sub>O</u> TFP label, row ")
                                      + QString::number(i + 1)
                                      + QStringLiteral(": ")
                                      + formatPassFail(last_test()));
    }

    // test header rows 17-18
    test << ContainerHelper::rangeEqual(templateList, actualList, 16, 18);
    testResults_->append(QLatin1String("Header rows 17-18: ")
                                  + formatPassFail(last_test()));

    //> One transfer-function block per non-water gas: twelve label rows, then
    //> two header rows, repeating. The first begins at row 19.
    //>
    //> Walked with a cursor rather than at three spelled-out positions. Those
    //> positions - 19-30, 33-44, 47-58 - are only the CO2, CH4 and fourth-gas
    //> blocks on a project laid out in that order, and every row after them
    //> shifts by fourteen for each additional gas.
    const auto tfpSlots = tfpGasSlots(ecProject_);
    auto row = 18;
    for (auto k = 0; k < tfpSlots.size(); ++k)
    {
        const auto slot = tfpSlots.at(k);
        if (row + 12 > templateList.size()) { break; }

        if (!gasSlotConfigured(ecProject_, slot))
        {
            testResults_->append(tr("<u>%1</u>: not configured in this project — <b>skipped</b>")
                                 .arg(gasSlotName(ecProject_, slot)));
        }
        else
        {
            for (auto i = row; i < row + 12; ++i)
            {
                test << (StringUtils::subStringList(templateList.value(i), 0, 2)
                        == StringUtils::subStringList(actualList.value(i), 0, 2));

                testResults_->append(QLatin1String("<u>")
                                              + gasSlotName(ecProject_, slot)
                                              + QLatin1String("</u> TFP label, row ")
                                              + QString::number(i + 1)
                                              + QStringLiteral(": ")
                                              + formatPassFail(last_test()));
            }
        }
        row += 12;

        //> The two header rows that separate this block from the next.
        test << ContainerHelper::rangeEqual(templateList, actualList, row, row + 2);
        testResults_->append(QLatin1String("Header rows ")
                                      + QString::number(row + 1)
                                      + QLatin1String("-")
                                      + QString::number(row + 2)
                                      + QStringLiteral(": ")
                                      + formatPassFail(last_test()));
        row += 2;
    }

    //> The fixed tail: the RH/fc exponential fit block and the high-pass
    //> parameters. Offsets from the cursor, so they follow however many gas
    //> blocks came before.
    test << ContainerHelper::rangeEqual(templateList, actualList, row, row + 2);
    testResults_->append(QLatin1String("Header, rows ")
                                  + QString::number(row + 1)
                                  + QLatin1String("-")
                                  + QString::number(row + 2)
                                  + QStringLiteral(": ")
                                  + formatPassFail(last_test()));
    row += 2;

    test << ContainerHelper::rangeEqual(templateList, actualList, row + 1, row + 7);
    testResults_->append(QLatin1String("Header, rows ")
                                  + QString::number(row + 2)
                                  + QLatin1String("-")
                                  + QString::number(row + 7)
                                  + QStringLiteral(": ")
                                  + formatPassFail(last_test()));
    row += 7;

    // test high pass parameters labels
    for (auto i = row; i < row + 2 && i < templateList.size(); ++i)
    {
        test << (StringUtils::subStringList(templateList.value(i), 0, 2)
                == StringUtils::subStringList(actualList.value(i), 0, 2));

        testResults_->append(QLatin1String("HP correction parameters label, row ")
                                      + QString::number(i + 1)
                                      + QStringLiteral(": ")
                                      + formatPassFail(last_test()));
    }

    auto res = true;
    for (auto i = 0; i < test.size(); ++i)
    {
        res &= test.value(i);
    }

    return res;
}

bool AncillaryFileTest::testSpectraS(const LineList &actualList)
{
    // get numbers
    QVector<double> FnH2o;
    QVector<double> fcH2o;
    QVector<double> numerosity;
    for (auto i = 7; i < 16; ++i)
    {
        FnH2o << StringUtils::subStringList(actualList.value(i), 6, 7).value(0).toDouble();
        fcH2o << StringUtils::subStringList(actualList.value(i), 7, 8).value(0).toDouble();
        numerosity << StringUtils::subStringList(actualList.value(i), 8, 9).value(0).toDouble();
    }
    //> Block k of the transfer-function table, twelve rows starting at 19.
    //> Positions computed rather than spelled out: every row after the blocks
    //> shifts by kSpectraGasBlockRows for each gas the project adds, so the
    //> fit and model parameters below are offsets from the end of the blocks
    //> and not the absolute rows 63, 70 and 71.
    const auto tfpSlots = tfpGasSlots(ecProject_);
    const auto blockRow = [](int k) { return 18 + kSpectraGasBlockRows * k; };

    QVector<double> FnCo2;
    QVector<double> fcCo2;
    for (auto i = blockRow(0); i < blockRow(0) + 12; ++i)
    {
        FnCo2 << StringUtils::subStringList(actualList.value(i), 2, 3).value(0).toDouble();
        fcCo2 << StringUtils::subStringList(actualList.value(i), 3, 4).value(0).toDouble();
    }
    QVector<double> FnCh4;
    QVector<double> fcCh4;
    for (auto i = blockRow(1); i < blockRow(1) + 12; ++i)
    {
        FnCh4 << StringUtils::subStringList(actualList.value(i), 2, 3).value(0).toDouble();
        fcCh4 << StringUtils::subStringList(actualList.value(i), 3, 4).value(0).toDouble();
    }

    //> The tail: two header rows after the last block, then the RH/fc
    //> exponential fit, then the high-pass parameters seven rows on.
    const auto tailRow = blockRow(tfpSlots.isEmpty() ? 3 : tfpSlots.size()) + 2;
    QVector<double> fitParameters;
    for (auto i = 0; i < 3; ++i)
    {
        fitParameters << actualList.value(tailRow).value(i).toDouble();
    }
    QVector<double> modelParameters;
    modelParameters << actualList.value(tailRow + 7).value(2).toDouble();
    modelParameters << actualList.value(tailRow + 7).value(3).toDouble();
    modelParameters << actualList.value(tailRow + 8).value(2).toDouble();
    modelParameters << actualList.value(tailRow + 8).value(3).toDouble();

    // test criteria
    QList<bool> test;
    auto last_test_index = [&](){ return (test.size() - 1); };
    auto last_test = [&](){ return test.value(test.size() - 1); };

    // test a.1
    test << std::any_of(fcH2o.begin(), fcH2o.end(),
                        [](double d){ return (d >= 0.001 && d <= 10.0); });
    auto a1_label = QStringLiteral("<u>H<sub>2</sub>O</u> Column 'fc' "
                                   "shall have at least 1 value in the range [0.001; 10.0]: ");
    testResults_->append(a1_label + formatPassFail(last_test()));

    // test a.2
    test << std::any_of(fcH2o.begin(), fcH2o.end(),
                        [](double d){ return !qFuzzyCompare(d, -9999.0); });
    auto a2_label = QStringLiteral("<u>H<sub>2</sub>O</u> Column 'fc' "
                                   "shall not have all values set to -9999: ");
    testResults_->append(a2_label + formatPassFail(last_test()));

    // test a.3
    test << std::any_of(numerosity.begin(), numerosity.end(),
                        [](int i){ return (i > 0); });
    auto a3_label = QStringLiteral("<u>H<sub>2</sub>O</u> Column 'numerosity'' "
                                   "shall have at least 1 value > 0: ");
    testResults_->append(a3_label + formatPassFail(last_test()));

    // test a.4
    test << true;
    for (auto i = 0; i < 10; ++i)
    {
        if (fcH2o.value(i) >= 0.001 && fcH2o.value(i) <= 10.0)
        {
            if (FnH2o.value(i) < 0.01 || FnH2o.value(i) > 10.0)
            {
                test.replace(last_test_index(), false);
                break;
            }
        }
    }
    auto a4_label = QStringLiteral("<u>H<sub>2</sub>O</u> Column 'Fn' shall be "
                                   "in the range [0.01; 10.0] for good values of column 'fc': ");
    testResults_->append(a4_label + formatPassFail(last_test()));

    // test b.1
    test << std::all_of(fitParameters.begin(), fitParameters.end(),
                [](double d){ return !qFuzzyCompare(d, -9999.0); });
    auto b1_label = QStringLiteral("<u>H<sub>2</sub>O</u> All spectral corrections RH/fc "
                                      "exponential fit parameters shall be != -9999.0: ");
    testResults_->append(b1_label + formatPassFail(last_test()));

    // test c.2
    test << std::all_of(fcCo2.begin(), fcCo2.end(),
                        [](double d){ return (d >= 0.001 && d <= 10.0); });
    auto c2_label = QStringLiteral("<u>CO<sub>2</sub></u> All column 'fc' values "
                                   "shall be in the range [0.001; 10.0]: ");
    testResults_->append(c2_label + formatPassFail(last_test()));

    // test c.3
//    if (last_test())
//    {
//        test << std::all_of(FnCo2.begin(), FnCo2.end(),
//                            [](double d){ return (d >= 0.01 && d <= 10.0); });
//    }

//    test << std::equal(fcCo2.begin(), fcCo2.end(), FnCo2.begin(),
//                       [](double d1, double d2){});

    test << true;
    for (auto i = 0; i < 12; ++i)
    {
        if (fcCo2.value(i) >= 0.001 && fcCo2.value(i) <= 10.0)
        {
            if (FnCo2.value(i) < 0.01 || FnCo2.value(i) > 10.0)
            {
                test.replace(last_test_index(), false);
                break;
            }
        }
    }
    auto c3_label = QStringLiteral("<u>CO<sub>2</sub></u> All column 'Fn' shall "
                                   "be in the range [0.01; 10.0] for good values of column 'fc': ");
    testResults_->append(c3_label + formatPassFail(last_test()));

    // test d.2 and d.3 — skip if CH4 not configured
    //
    // Asked by species. This used to ask for record two, which was methane
    // only while every project reserved that position for it whether or not
    // the site measured any.
    if (!gasSpeciesConfigured(ecProject_, QStringLiteral("ch4")))
    {
        testResults_->append(tr("<u>%1</u>: not configured in this project — <b>skipped</b>")
                             .arg(QStringLiteral("CH4")));
    }
    else
    {
        // test d.2
        test << std::all_of(fcCh4.begin(), fcCh4.end(),
                            [](double d){ return (d >= 0.001 && d <= 10); });
        auto d2_label = QStringLiteral("<u>CH<sub>4</sub></u> All column 'fc' values "
                                       "shall be in the range [0.001; 10.0]: ");
        testResults_->append(d2_label + formatPassFail(last_test()));

        // test d.3
        test << true;
        for (auto i = 0; i < 12; ++i)
        {
            if (fcCh4.value(i) >= 0.001 && fcCh4.value(i) <= 10.0)
            {
                if (FnCh4.value(i) < 0.01 || FnCh4.value(i) > 10.0)
                {
                    test.replace(last_test_index(), false);
                    break;
                }
            }
        }
        auto d3_label = QStringLiteral("<u>CH<sub>4</sub></u> All column 'Fn' "
                                       "shall be in the range [0.01; 10.0] for good values of column 'fc': ");
        testResults_->append(d3_label + formatPassFail(last_test()));
    }

    // test e.1
    test << std::all_of(modelParameters.begin(), modelParameters.end(),
                        [](double d){ return (d >= 0.0 && d <= 1.0); });
    auto e1_label = QStringLiteral("<u>H<sub>2</sub>O or CO<sub>2</sub> or CH<sub>4</sub></u> "
                                   "All high-pass correction factor model parameters "
                                   "shall be within the range [0; 1]: ");
    testResults_->append(e1_label + formatPassFail(last_test()));

    auto res = true;
    for (auto i = 0; i < test.size(); ++i)
    {
        res &= test.value(i);
    }

    return res;
}

bool AncillaryFileTest::testPlanarFitF(const LineList &templateList, const LineList &actualList)
{
    // preliminary test, number of rows
    auto rowCountTest = (actualList.size() > 2);
    testResults_->append(QLatin1String("Number of rows [")
                                 + QString::number(actualList.size())
                                 + QStringLiteral("]: ")
                                 + formatPassFail(rowCountTest));
    if (!rowCountTest) { return false; }

    // other tests
    QList<bool> test;
    auto last_test_index = [&](){ return (test.size() - 1); };
    auto last_test = [&](){ return test.value(test.size() - 1); };

    // test a, header rows 1-7
    for (auto i = 0; i < 6; ++i)
    {
        test << (StringUtils::subStringList(templateList.value(i), 0, 1)
                == StringUtils::subStringList(actualList.value(i), 0, 1));
        testResults_->append(QLatin1String("Header, row ")
                             + QString::number(i + 1)
                             + QStringLiteral(": ")
                             + formatPassFail(last_test()));
    }

    // test a, header rows 8-10
    test << ContainerHelper::rangeEqual(templateList, actualList, 7, 10);
    testResults_->append(QLatin1String("Header, rows 8-10: ")
                         + formatPassFail(last_test()));

    // wind sectors > 0
    auto windSectorsStr = StringUtils::subStringList(actualList.value(1), 1, 2).value(0);
    auto windSectors = windSectorsStr.toInt();
    test << (windSectors > 0);
    testResults_->append(QLatin1String("Wind sectors [")
                         + QString::number(windSectors)
                         + QStringLiteral("]: ")
                         + formatPassFail(last_test()));
    if (!last_test()) { return false; }

    // test e, total number of rows (depending from wind sectors)
    rowCountTest = ((5 * windSectors + 13) == actualList.size());
    testResults_->append(QLatin1String("Total number of rows [")
                         + QString::number(actualList.size())
                         + QStringLiteral("]: ")
                         + formatPassFail(rowCountTest));
    if (!rowCountTest) { return false; }

    // test b, rows 11-14 formal test
    test << true;
    for (auto i = 0; i < windSectors; ++i)
    {
        // column 1
        if (actualList.value(10 + i).value(0).toInt() != i + 1)
        {
            test.replace(last_test_index(), false);
            break;
        }

        // columns 3-5
        auto conversionToDouble = false;
        for (auto j = 3; j < 6; ++j)
        {
            actualList.value(10 + i).value(j).toDouble(&conversionToDouble);

            if (!conversionToDouble)
            {
                test.replace(last_test_index(), false);
                break;
            }
        }
    }
    testResults_->append(QLatin1String("Wind sectors coefficients formal structure: ")
                         + formatPassFail(last_test()));

    // test c, header rows (11-12 + windSectors)
    test << std::equal(templateList.begin() + 10 + 4, templateList.begin() + 10 + 6,
                       actualList.begin() + 10 + windSectors);
    testResults_->append(QLatin1String("Header, rows ")
                         + QString::number(11 + windSectors)
                         + QStringLiteral("-")
                         + QString::number(12 + windSectors)
                         + QStringLiteral(": ")
                         + formatPassFail(last_test()));

    // test d1
    test << true;
    for (auto i = 0; i < windSectors; ++i)
    {
        if (StringUtils::subStringList(templateList.value(16), 0, 2)
            != StringUtils::subStringList(actualList.value(12 + windSectors + 4 * i), 0, 2))
        {
            test.replace(last_test_index(), false);
            break;
        }
        if (StringUtils::subStringList(templateList.value(16), 3, 4)
            != StringUtils::subStringList(actualList.value(12 + windSectors + 4 * i), 3, 4))
        {
            test.replace(last_test_index(), false);
            break;
        }
        if (StringUtils::subStringList(templateList.value(16), 7, 9)
            != StringUtils::subStringList(actualList.value(12 + windSectors + 4 * i), 7, 9))
        {
            test.replace(last_test_index(), false);
            break;
        }
    }
    testResults_->append(QLatin1String("Rotation matrices formal structure 1: ")
                         + formatPassFail(last_test()));

    // test d2
    test << true;
    for (auto i = 0; i < windSectors; ++i)
    {
        // columns 1-3
        auto conversionToDouble = false;
        for (auto j = 0; j < 3; ++j)
        {
            for (auto k = 0; k < 3; ++k)
            {
                actualList.value(13 + j + windSectors + 4 * i).value(k).toDouble(&conversionToDouble);

                if (!conversionToDouble)
                {
                    test.replace(last_test_index(), false);
                    break;
                }
            }
        }
    }
    testResults_->append(QLatin1String("Rotation matrices formal structure 2: ")
                         + formatPassFail(last_test()));

    // test d3
    test << true;
    for (auto i = 0; i < windSectors; ++i)
    {
        if (actualList.value(14 + windSectors + 4 * i).value(0).toDouble() != 0.0)
        {
            test.replace(last_test_index(), false);
            break;
        }
    }
    testResults_->append(QLatin1String("Rotation matrices formal structure 3: ")
                         + formatPassFail(last_test()));

    auto res = true;
    for (auto i = 0; i < test.size(); ++i)
    {
        res &= test.value(i);
    }

    return res;
}

bool AncillaryFileTest::testPlanarFitS(const LineList &actualList)
{
    auto windSectorsStr = StringUtils::subStringList(actualList.value(1), 1, 2).value(0);
    auto windSectors = windSectorsStr.toInt();

    // QGenericMatrix
    QVector<QVector<double>> fitParameters(windSectors);
    for (auto i = 0; i < windSectors; ++i)
    {
        fitParameters[i].resize(3);
        fitParameters[i][0] = actualList.value(10 + i).value(3).toDouble();
        fitParameters[i][1] = actualList.value(10 + i).value(4).toDouble();
        fitParameters[i][2] = actualList.value(10 + i).value(5).toDouble();
    }

    // QMatrix3x3
    QList<QVector<QVector<double>>> rotMatrices;
    rotMatrices.reserve(windSectors);
    for (int i = 0; i < windSectors; ++i)
    {
        QVector<QVector<double>> matrix(3);
        for (int j = 0; j < 3; ++j)
        {
            matrix[j].resize(3);
            matrix[j][0] = actualList.value(13 + j + windSectors + 4 * i).value(0).toDouble();
            matrix[j][1] = actualList.value(13 + j + windSectors + 4 * i).value(1).toDouble();
            matrix[j][2] = actualList.value(13 + j + windSectors + 4 * i).value(2).toDouble();
        }
        rotMatrices << matrix;
    }

    auto test_full = false;
    QList<bool> test_detail;
    test_detail << false << true << false;

    for (auto i = 0; i < windSectors; ++i)
    {
        // init test results
        test_detail.replace(0, false);
        test_detail.replace(1, true);
        test_detail.replace(2, false);

        // test a
//        if (std::all_of(fitParameters[i].begin(), fitParameters[i].end()),
//                            [](double d){ return (d != -9999.0); })
        if (!qFuzzyCompare(fitParameters[i][0], -9999.0)
            && !qFuzzyCompare(fitParameters[i][1], -9999.0)
            && !qFuzzyCompare(fitParameters[i][2], -9999.0))
        {
            test_full = true;
            test_detail.replace(0, true);

            // test b.1
            for (auto j = 0; j < 3; ++j)
            {
                for (auto k = 0; k < 3; ++k)
                {
                    if (qFuzzyCompare(rotMatrices.value(i)[j][k], -9999.0))
                    {
                        test_full = false;
                        test_detail.replace(1, false);

                        // not leaving the outermost for loop
                        // is not efficient, but acceptable
                        break;
                    }
                }
            }
//            if (!std::all_of(rotMatrices.value(i).begin(), rotMatrices.value(i).end(),
//                                [](double d){ return (d != -9999.0); }))
//            {
//                test.replace(last_test_index(), false);
//                break;
//            }

            // test b.2
            for (auto j = 0; j < 3; ++j)
            {
                for (auto k = 0; k < 3; ++k)
                {
                    if (rotMatrices.value(i)[j][k] != 0.0)
                    {
                        test_full = test_full && true;
                        test_detail.replace(2, true);
                        // not leaving the outermost for loop
                        // is not efficient, but acceptable
                        break;
                    }
                }
            }
//            if (!std::any_of(rotMatrices.value(i).begin(), rotMatrices.value(i).end(),
//                                [](double d){ return (d != 0.0); }))
//            {
//                test.replace(last_test_index(), false);
//                break;
//            }
        }

        // print results
        auto wind_sector_test = std::all_of(test_detail.begin(), test_detail.end(),
                                            [](bool res){ return (res); });
        testResults_->append(QLatin1String("<u>Wind sector ")
                             + QString::number(i + 1)
                             + QStringLiteral("</u>: ")
                             + formatPassFail(wind_sector_test));
        if (!wind_sector_test)
        {
            if (!test_detail.value(0))
            {
                testResults_->append(QLatin1String("At least one wind sector "
                                     "should have all three coefficients "
                                     "!= -9999.0: ") + formatPassFail(false));
            }
            else
            {
                if (!test_detail.value(1))
                {
                    testResults_->append(QLatin1String("A wind sector having valid coefficients "
                                         "shall have all rotations values "
                                         "!= -9999.0") + formatPassFail(false));
                }
                if (!test_detail.value(2))
                {
                    testResults_->append(QLatin1String("A wind sector having valid coefficients "
                                         "shall have at least one rotation value "
                                         "!= 0.0: ") + formatPassFail(false));
                }
            }
        }
        else
        {
            break;
        }
    }

    return test_full;
}

bool AncillaryFileTest::testTimeLagF(const LineList &templateList, const LineList &actualList)
{
    Q_UNUSED(templateList);

    // preliminary test, number of rows
    auto rowCountTest = (actualList.size() > 2);
    testResults_->append(QLatin1String("Number of rows [")
                                 + QString::number(actualList.size())
                                 + QStringLiteral("]: ")
                                 + formatPassFail(rowCountTest));
    if (!rowCountTest) { return false; }

    QList<bool> test;
    auto last_test = [&](){ return test.value(test.size() - 1); };

    // test a
    for (auto i = 0; i < 5; ++i)
    {
        test << matchesTimelagHeaderRow(actualList.value(i), i);
        testResults_->append(QLatin1String("Header, row ")
                             + QString::number(i + 1)
                             + QStringLiteral(": ")
                             + formatPassFail(last_test()));
    }

    // test b
    auto gasCount = 0;
    timelagValues.resize(3);
    while (matchesGasTimelagBlock(actualList, 5 + 5 * gasCount))
    {
        ++gasCount;

        // collect values
        timelagValues[0].resize(gasCount);
        timelagValues[1].resize(gasCount);
        timelagValues[2].resize(gasCount);
        timelagValues[0][gasCount - 1] = actualList.value(6 + 5 * (gasCount - 1)).value(1).toDouble();
        timelagValues[1][gasCount - 1] = actualList.value(7 + 5 * (gasCount - 1)).value(1).toDouble();
        timelagValues[2][gasCount - 1] = actualList.value(8 + 5 * (gasCount - 1)).value(1).toDouble();
    }

    // test c1
    // compare 3 lines of RH headers
    if (matchesRhTimelagHeader(actualList, 5 + 5 * gasCount))
    {
        test << true;
        testResults_->append(QLatin1String("Header of RH sorted H<sub>2</sub>O classes (3 rows): ")
                             + formatPassFail(last_test()));

        // test c1' (moved from scientific to formal)
        auto rhClassCount = 0;

        while (!actualList.value(8 + 5 * gasCount + rhClassCount).isEmpty())
        {
            ++rhClassCount;
            auto actualRhlClassIndex = actualList.value(8 + 5 * gasCount + rhClassCount - 1).value(0).toInt();
            test << (rhClassCount == actualRhlClassIndex);
            testResults_->append(QLatin1String("Consistent RH index [")
                                 + QString::number(actualRhlClassIndex)
                                 + QStringLiteral("]: ")
                                 + formatPassFail(last_test()));
        }

        // test c2
        if (rhClassCount <= 20)
        {
            test << (actualList.value(8 + 5 * gasCount).value(1) == QLatin1String("0")
                     && actualList.value(8 + 5 * gasCount + rhClassCount - 1).value(3) == QLatin1String("100%"));

            // collect values
            h2oTimelagValues.resize(4);
            h2oTimelagValues[0].resize(rhClassCount);
            h2oTimelagValues[1].resize(rhClassCount);
            h2oTimelagValues[2].resize(rhClassCount);
            h2oTimelagValues[3].resize(rhClassCount);
            for (auto i = 0; i < rhClassCount; ++i)
            {
                h2oTimelagValues[0][i] = actualList.value(8 + 5 * gasCount + i).value(4).toDouble();
                h2oTimelagValues[1][i] = actualList.value(8 + 5 * gasCount + i).value(5).toDouble();
                h2oTimelagValues[2][i] = actualList.value(8 + 5 * gasCount + i).value(6).toDouble();
                h2oTimelagValues[3][i] = actualList.value(8 + 5 * gasCount + i).value(7).toDouble();
            }

            testResults_->append(QStringLiteral("Consistent RH ranges: ") + formatPassFail(last_test()));
        }
        else
        {
            test << false;
            testResults_->append(QLatin1String("RH classes <= 20: ") + formatPassFail(last_test()));
        }
    }
    else
    {
        auto rhIsEmpty = actualList.value(5 + 5 * gasCount).isEmpty()
                         && actualList.value(6 + 5 * gasCount).isEmpty()
                         && actualList.value(7 + 5 * gasCount).isEmpty();

        // with no gases and no rh classes
        if (!gasCount && rhIsEmpty)
        {
            test << false;
            testResults_->append(QLatin1String("Number of gases [0] and header of "
                                                "RH sorted H<sub>2</sub>O classes (3 rows): ")
                          + formatPassFail(last_test()));
        }
        // with no gases and > 20 rh classes
        else if (!gasCount && !rhIsEmpty)
        {
            test << false;
            testResults_->append(QLatin1String("Header of RH sorted H<sub>2</sub>O classes (3 rows): ")
                          + formatPassFail(last_test()));
        }
        // with gases and > 20 rh classes
        else if (gasCount && !rhIsEmpty)
        {
            test << false;
            testResults_->append(QLatin1String("Header of gases or RH sorted H<sub>2</sub>O classes (3 rows): ")
                          + formatPassFail(last_test()));
        }
        // with gases and no rh classes
        else if (gasCount && rhIsEmpty)
        {
            test << true;
            testResults_->append(QStringLiteral("Number of gases [0]: ")
                          + formatPassFail(last_test()));
        }
    }

    auto res = true;
    for (auto i = 0; i < test.size(); ++i)
    {
        res &= test.value(i);
    }
    return res;
}

bool AncillaryFileTest::testTimeLagS(const LineList &actualList)
{
    Q_UNUSED(actualList);

    QList<bool> test;
    auto last_test_index = [&](){ return (test.size() - 1); };
    auto last_test = [&](){ return test.value(test.size() - 1); };
    test << true;

    // test a
    auto gasCount = timelagValues[0].size();
    if (gasCount > 0)
    {
        for (auto j = 0; j < gasCount; ++j)
        {
            if (!((timelagValues[0][j] >= timelagValues[1][j])
                && timelagValues[0][j] <= timelagValues[2][j]))
            {
                test.replace(last_test_index(), false);
            }
        }
        testResults_->append(QLatin1String("Gas time-lag median values inside the "
                             "[minimum; maximum] range: ")
                             + formatPassFail(last_test()));

        // test b
        test << true;
        for (auto i = 0; i < 3; ++i)
        {
            for (auto j = 0; j < gasCount; ++j)
            {
                if (timelagValues[i][j] > 60.0)
                {
                    test.replace(last_test_index(), false);
                    goto end_loop;
                }
            }
        }
        end_loop:
        testResults_->append(QLatin1String("Time-lag values not larger than 60 seconds: ")
                             + formatPassFail(last_test()));
    }

    // if there are RH classes
    if (h2oTimelagValues.size())
    {
        // test c.2
        test << true;
        auto rhClassCount = h2oTimelagValues[0].size();
        for (auto i = 0; i < rhClassCount; ++i)
        {
            if (!((h2oTimelagValues[0][i] >= h2oTimelagValues[1][i])
                  && (h2oTimelagValues[0][i] <= h2oTimelagValues[2][i])))
            {
                test.replace(last_test_index(), false);
                break;
            }
        }
        testResults_->append(QStringLiteral("H<sub>2</sub>O RH-sorted median values inside the "
                             "[minimum; maximum] range: ")
                             + formatPassFail(last_test()));

        // test c.3
        test << false;
        auto classNumCount = 0;
        for (auto i = 0; i < rhClassCount; ++i)
        {
            if (h2oTimelagValues[3][i] > 30)
            {
                ++classNumCount;
            }
            if (classNumCount >= 3)
            {
                test.replace(last_test_index(), true);
                break;
            }
        }
        testResults_->append(QStringLiteral("At least 3 H<sub>2</sub>O classes with numerosity > 30: ")
                             + formatPassFail(last_test()));
    }

    auto res = true;
    for (auto i = 0; i < test.size(); ++i)
    {
        res &= test.value(i);
    }
    return res;
}

QString AncillaryFileTest::typeToString(FileType type)
{
    switch (type)
    {
    case FileType::Spectra:
        return QStringLiteral("spectral-assessment-file-check");
    case FileType::PlanarFit:
        return QStringLiteral("planar-fit-assessment-file-check");
    case FileType::TimeLag:
        return QStringLiteral("time-lag-assessment-file-check");
    }
    return QString();
}

// TODO: Use sheet on Mac with getSaveFileName
void AncillaryFileTest::saveResults()
{
    auto timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-ddThhmmss"));
    auto filenameHint =
            WidgetUtils::getDialogPathHint(QStringLiteral("ancillary_test_results_save"))
            + QStringLiteral("/")
            + typeToString(type_)
            + QStringLiteral("-")
            + timestamp
            + Defs::TEMPLATE_FILE_EXT;
    auto filename = QFileDialog::getSaveFileName(this,
                                         tr("Save the test results as..."),
                                         filenameHint,
                                         tr("%1 assessment file check results (*.txt);;All files (*)").arg(Defs::APP_NAME));

    if (!filename.isEmpty())
    {
        QSaveFile file(filename);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return;

        QTextStream out(&file);

        // add header
        out << tr("%1 check of assessment file %2 against %3").arg(Defs::APP_NAME)
               .arg(name_)
               .arg(testFileMap_.value(type_).filepath);
        out << "\n\n";

        // get text
        auto text = testResults_->toPlainText();

        // remove question mark
        text.chop(1);

        // write text
        out << text;

        // add online help address
        out << "\nSee ";
        out << helpPage;
        out << ".\n";

        // flush data to file
        if (file.commit())
        {
            WidgetUtils::rememberDialogPath(QStringLiteral("ancillary_test_results_save"), filename, true);
        }
    }
}

