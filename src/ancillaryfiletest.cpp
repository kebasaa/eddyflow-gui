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
#include <QMap>
#include <QTextBrowser>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QVBoxLayout>
#include <QVector>

#include <algorithm>
#include <functional>

#include "ecproject.h"
#include "stringutils.h"
#include "container_helpers.h"
#include "widget_utils.h"

const auto helpPage = QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Assessment_Tests.html");

namespace {

using Lines = QList<QStringList>;

/// Whether record \a slot, a 0-based index into the gas record list, has a
/// column in the raw data.
///
/// Every per-gas verdict below turns on this. The engine writes a block for
/// each record, measured or not - an unmeasured one filled with -9999 - and
/// reads such a block back only to discard it, so only a measured gas can make
/// a file unusable. A name no record claims (\a slot -1) is unmeasured.
///
/// Without a project there is nothing to ask, and every gas is taken as
/// measured: checking too much is the safe way to be wrong here.
bool gasMeasured(const EcProject* project, int slot)
{
    if (!project) { return true; }
    const auto& gases = project->gasColumns();
    return slot >= 0 && slot < gases.size() && gases.at(slot).rawColumn > 0;
}

/// Record indices naming \a species, in record order.
QVector<int> recordsOfSpecies(const EcProject* project, const QString& species)
{
    QVector<int> found;
    if (!project) { return found; }
    const auto& gases = project->gasColumns();
    for (int i = 0; i < gases.size(); ++i)
    {
        if (gases.at(i).slug.compare(species, Qt::CaseInsensitive) == 0) { found << i; }
    }
    return found;
}

/// The primary hygrometer: the first water record with a column.
///
/// Its cutoffs are the one TFP table no header names, and its time lags the
/// RH-sorted table. This is the engine's DesignatedGasSlot('H2O') for a
/// project that flags none, and the interface has no flag to set - records
/// without a column never count, so an unmeasured first water record does
/// not push the real one into a named block.
int primaryWaterSlot(const EcProject* project)
{
    for (const auto slot : recordsOfSpecies(project, QStringLiteral("h2o")))
    {
        if (gasMeasured(project, slot)) { return slot; }
    }
    return -1;
}

/// The name the engine gives record \a slot in both files (FullOutputGasTags):
/// the species, bare when the project names it once, and numbered by
/// occurrence - CO2_1, CO2_2 - when it names it more than once. Uppercase, as
/// the spectral file writes it; the time-lag file lowercases the same name.
QString gasFileName(const EcProject* project, int slot)
{
    const auto& slug = project->gasColumns().at(slot).slug;
    const auto same = recordsOfSpecies(project, slug);
    if (same.size() <= 1) { return slug.toUpper(); }
    return slug.toUpper() + QLatin1Char('_') + QString::number(same.indexOf(slot) + 1);
}

/// The record a block or row name refers to, or -1 - gasFileName inverted.
///
/// A bare name in a project naming that species more than once is its first
/// record. The engine now numbers every occurrence, but it used to leave the
/// first bare - `co2`, then `co2_2` - and files written then say so.
int slotForGasName(const EcProject* project, const QString& name)
{
    if (!project || name.isEmpty()) { return -1; }
    const auto whole = recordsOfSpecies(project, name);
    if (!whole.isEmpty()) { return whole.first(); }

    static const QRegularExpression numbered(QStringLiteral("^(.+)_(\\d+)$"));
    const auto match = numbered.match(name);
    if (!match.hasMatch()) { return -1; }
    const auto same = recordsOfSpecies(project, match.captured(1));
    const auto k = match.captured(2).toInt();
    //> Accepted even when the project now names the species once: a file
    //> fitted while a second analyser was configured still says CO2_1.
    return (k >= 1 && k <= same.size()) ? same.at(k - 1) : -1;
}

/// Rows in the two shapes of transfer-function block.
///
/// The file used to be walked by arithmetic - fourteen rows per non-water gas
/// from a fixed row 19, and a fixed twenty-nine rows of everything else. That
/// held only while every block had the same height and only non-water gases
/// had one. Neither is true: a hygrometer past the primary gets a nine-row
/// RH-class block of its own, so no single multiply describes the layout, and
/// the arithmetic silently pointed the tail checks at a hygrometer's class
/// rows instead.
///
/// The engine's own reader has never counted. It finds a block by the word
/// TFP in its header and takes the block's shape from whether that header also
/// says `numerosity` - nine humidity classes if it does, twelve months if it
/// does not. Those two rules are what this file is, so they are what is used
/// here, and a block count or a new trailing token cannot break them.
constexpr int kSpectraRhRows = 9;
constexpr int kSpectraMonthRows = 12;

/// The name a block header states: everything before the word TFP.
///
/// Headers carry `groups=`, `var=`, `instr=` and `exp=` after the columns, and
/// more may follow. They are deliberately past the point every reader of this
/// format stops, so the name is taken the same way rather than by comparing
/// the line whole.
QString spectraBlockName(const QStringList& line)
{
    QStringList words;
    for (const auto& word : line)
    {
        if (word == QLatin1String("TFP")) { break; }
        words << word;
    }
    return words.join(QLatin1Char(' '));
}

/// One transfer-function block: where its header sits and how many rows follow.
struct SpectraBlock
{
    int header = -1;
    int rows = 0;
    bool rhClasses = false;
    QString name;
};

/// Every TFP block in \a lines, in file order.
QVector<SpectraBlock> spectraBlocks(const QList<QStringList>& lines)
{
    QVector<SpectraBlock> found;
    for (auto i = 0; i < lines.size(); ++i)
    {
        if (!lines.at(i).contains(QStringLiteral("TFP"))) { continue; }
        //> The preamble names TFP too, in prose, on a line that is one word.
        if (lines.at(i).size() < 3) { continue; }
        SpectraBlock block;
        block.header = i;
        block.name = spectraBlockName(lines.at(i));
        block.rhClasses = lines.at(i).contains(QStringLiteral("numerosity"));
        block.rows = block.rhClasses ? kSpectraRhRows : kSpectraMonthRows;
        found << block;
    }
    return found;
}

/// The first row whose first word is \a word, or -1.
int spectraRowStarting(const QList<QStringList>& lines, const QString& word)
{
    for (auto i = 0; i < lines.size(); ++i)
    {
        if (lines.at(i).value(0).startsWith(word)) { return i; }
    }
    return -1;
}

/// \a line without the `key=value` tokens the engine appends to a block header
/// (`groups=`, `var=`, `instr=`, `exp=`, and whatever follows them). They sit
/// past the columns on purpose, where every reader of this file stops.
QStringList withoutStamps(const QStringList& line)
{
    QStringList kept;
    for (const auto& word : line)
    {
        if (word.size() > 1 && word.contains(QLatin1Char('='))) { continue; }
        kept << word;
    }
    return kept;
}

bool isDashRule(const QString& word)
{
    return !word.isEmpty() && word.count(QLatin1Char('-')) == word.size();
}

bool isNumber(const QString& word)
{
    bool ok = false;
    word.toDouble(&ok);
    return ok;
}

/// Whether \a actual carries the labels the template row \a model does.
///
/// This is upstream's row-by-row comparison, made per kind of row so that it
/// holds wherever the row sits rather than at one fixed offset:
/// - a row with a lone `=` is compared up to and including it - the RH-class
///   and month rows and the high-pass pair; what follows is data;
/// - a row of numbers only needs as many numbers;
/// - any other row is a label row and is compared whole.
///
/// \a name stands in for the template's `<GAS>` placeholder. A run of dashes
/// matches any run of dashes: the engine's separators have changed length
/// without changing meaning.
bool sameLabels(const QStringList& model, const QStringList& actualLine,
                const QString& name = QString())
{
    QStringList expected;
    for (const auto& word : model)
    {
        if (word == QLatin1String("<GAS>"))
        {
            expected << name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        }
        else
        {
            expected << word;
        }
    }
    const auto actual = withoutStamps(actualLine);

    const auto equals = expected.indexOf(QStringLiteral("="));
    if (equals >= 0)
    {
        return actual.mid(0, equals + 1) == expected.mid(0, equals + 1);
    }
    if (!expected.isEmpty() && std::all_of(expected.cbegin(), expected.cend(), isNumber))
    {
        return actual.size() == expected.size();
    }
    if (actual.size() != expected.size()) { return false; }
    for (auto i = 0; i < expected.size(); ++i)
    {
        const auto same = actual.at(i) == expected.at(i)
                          || (isDashRule(actual.at(i)) && isDashRule(expected.at(i)));
        if (!same) { return false; }
    }
    return true;
}

/// The spectral template, cut into the pieces an N-gas file is built from.
///
/// Upstream's template was the whole file for a fixed set of gases - H2O, CO2,
/// CH4 and one other - and could be compared row for row. A file now carries
/// one block per gas the project names, so the template carries one
/// *prototype* of each kind of block instead, headed `<GAS>`, and every block
/// in a file is held against the prototype of its kind.
struct SpectraTemplate
{
    /// The header rows and the primary hygrometer's table.
    Lines preamble;
    QStringList monthHeader;
    Lines monthRows;
    QStringList rhHeader;
    Lines rhRows;
    /// From the exponential-fit title to the end.
    Lines tail;

    bool isValid() const
    {
        return !preamble.isEmpty() && !monthHeader.isEmpty()
               && !rhHeader.isEmpty() && !tail.isEmpty();
    }
};

/// Found by the prototypes' own headers, not by row number, so the template
/// can gain or lose a line without this changing.
SpectraTemplate spectraTemplateParts(const Lines& lines)
{
    SpectraTemplate parts;
    const auto blocks = spectraBlocks(lines);
    const auto tailRow = spectraRowStarting(
        lines, QStringLiteral("RH/fc_exponential_fit_parameters"));
    if (blocks.isEmpty() || tailRow < 0) { return parts; }

    const auto& water = blocks.first();
    parts.preamble = lines.mid(0, water.header + 1 + water.rows);
    for (const auto& block : blocks)
    {
        if (block.name != QLatin1String("<GAS>")) { continue; }
        const auto rows = lines.mid(block.header + 1, block.rows);
        if (block.rhClasses)
        {
            parts.rhHeader = lines.at(block.header);
            parts.rhRows = rows;
        }
        else
        {
            parts.monthHeader = lines.at(block.header);
            parts.monthRows = rows;
        }
    }
    parts.tail = lines.mid(tailRow);
    while (!parts.tail.isEmpty() && parts.tail.last().isEmpty())
    {
        parts.tail.removeLast();
    }
    return parts;
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

/// The time-lag template, cut the same way as the spectral one: the header,
/// one gas block with `<gas>` for its name, and the header of the RH-sorted
/// water table.
struct TimelagTemplate
{
    Lines header;
    Lines gasBlock;
    Lines rhHeader;
    /// Rows of gasBlock holding the median, minimum and maximum.
    int valueRows[3] = { -1, -1, -1 };

    bool isValid() const
    {
        return !header.isEmpty() && rhHeader.size() == 3
               && valueRows[0] >= 0 && valueRows[1] >= 0 && valueRows[2] >= 0;
    }
};

TimelagTemplate timelagTemplateParts(const Lines& lines)
{
    TimelagTemplate parts;
    auto gasRow = -1;
    auto rhRow = -1;
    for (auto i = 0; i < lines.size(); ++i)
    {
        if (gasRow < 0 && joinedLine(lines.at(i)).contains(QStringLiteral("<gas>")))
        {
            gasRow = i;
        }
        if (normalizedTimelagLabel(firstField(lines.at(i)))
            == QLatin1String("h2o_timelag_determinations_as_a_function_of_relative_humidity"))
        {
            rhRow = i;
        }
    }
    if (gasRow < 0 || rhRow < gasRow) { return parts; }

    parts.header = lines.mid(0, gasRow);
    parts.gasBlock = lines.mid(gasRow, rhRow - gasRow);
    parts.rhHeader = lines.mid(rhRow, 3);

    const QString prefixes[3] = { QStringLiteral("median_"),
                                  QStringLiteral("minimum_"),
                                  QStringLiteral("maximum_") };
    for (auto i = 0; i < parts.gasBlock.size(); ++i)
    {
        const auto label = normalizedTimelagLabel(firstField(parts.gasBlock.at(i)));
        for (auto k = 0; k < 3; ++k)
        {
            if (label.startsWith(prefixes[k])) { parts.valueRows[k] = i; }
        }
    }
    return parts;
}

/// Whether row \a actual carries the label of template row \a model, with
/// `<gas>` read as \a gas. Blank rows must stay blank.
bool sameTimelagLabel(const QStringList& model, const QStringList& actual,
                      const QString& gas = QString())
{
    if (model.isEmpty()) { return joinedLine(actual).isEmpty(); }
    auto expected = normalizedTimelagLabel(firstField(model));
    expected.replace(QStringLiteral("<gas>"), gas);
    return normalizedTimelagLabel(firstField(actual)) == expected;
}

/// The gas named by the block starting at \a start, if its rows carry the
/// template block's labels; empty if they do not.
///
/// The name is taken from the first row and must then be the same in every
/// other one. It is not split on underscores, which is what upstream did and
/// what made `co2_2` or `alpha_pinene` unreadable.
QString timelagBlockGas(const Lines& model, const Lines& lines, int start)
{
    QString gas;
    if (!parseNumberOfTimelagsLabel(lines.value(start), &gas)) { return {}; }
    for (auto i = 0; i < model.size(); ++i)
    {
        //> The template's last row is the blank after the block; the file's
        //> may instead simply end.
        if (start + i >= lines.size() && model.at(i).isEmpty()) { continue; }
        if (!sameTimelagLabel(model.at(i), lines.value(start + i), gas)) { return {}; }
    }
    return gas;
}

/// \a text with every run of digits replaced by `#`. The RH-table note states
/// the engine's minimum class numerosity, which has changed (30, then 15);
/// the sentence is the format, the number is a setting.
QString withoutNumbers(QString text)
{
    static const QRegularExpression digits(QStringLiteral("\\d+"));
    return text.replace(digits, QStringLiteral("#"));
}

bool matchesRhTimelagHeader(const Lines& model, const Lines& lines, int start)
{
    const auto columns = whitespaceFields(lines.value(start + 2));
    const auto modelColumns = whitespaceFields(model.value(2));
    if (columns.size() != modelColumns.size()) { return false; }
    for (auto i = 0; i < columns.size(); ++i)
    {
        if (columns.at(i).compare(modelColumns.at(i), Qt::CaseInsensitive) != 0) { return false; }
    }
    return sameTimelagLabel(model.value(0), lines.value(start))
           && withoutNumbers(normalizedTimelagLine(model.value(1)))
                  == withoutNumbers(normalizedTimelagLine(lines.value(start + 1)));
}

/// The numerosity the RH-table note says a class needs to be determined rather
/// than inferred, or \a fallback if it states none.
int statedMinClassNumerosity(const QStringList& note, int fallback)
{
    static const QRegularExpression number(QStringLiteral("\\d+"));
    const auto match = number.match(joinedLine(note));
    return match.hasMatch() ? match.captured(0).toInt() : fallback;
}

/// Whether \a line is one of the provenance rows the engine adds in PWB
/// aggregate mode - `PWB_aggregate_summary:` under the title and
/// `PWB_summary_source_for_<gas>:` inside a block and above the RH table.
/// They say where a summary came from; they are not part of the layout.
bool isPwbProvenance(const QStringList& line)
{
    return line.value(0).startsWith(QLatin1String("PWB_"), Qt::CaseInsensitive);
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
    //> Checked against the template again, as upstream did, but block by block.
    //>
    //> Upstream compared the whole file row for row with a sample carrying
    //> exactly H2O, CO2, CH4 and one other gas, which no longer describes any
    //> file: the engine writes one block per gas the project names, numbers a
    //> species measured twice, adds a named RH block per extra hygrometer, and
    //> appends `groups=`/`var=`/`instr=`/`exp=` past the columns. For a while
    //> that pushed this test onto hand-written rules with the template unused.
    //> Now the template carries one prototype of each kind of block, every
    //> block is held against the prototype of its kind wherever it sits, and
    //> the text of every label comes from the template rather than from here.
    const auto model = spectraTemplateParts(templateList);
    if (!model.isValid())
    {
        testResults_->append(tr("<b>The spectral assessment template is incomplete. "
                                "Please, re-install the software.</b>"));
        return false;
    }

    QList<bool> test;
    auto last_test = [&](){ return test.value(test.size() - 1); };
    auto check = [&](bool ok, const QString& what)
    {
        test << ok;
        testResults_->append(what + QStringLiteral(": ") + formatPassFail(last_test()));
    };

    //> The header rows, then the primary hygrometer's table - the one block no
    //> header names, which is why it is part of the preamble rather than a
    //> block to be matched.
    const auto waterHeader = model.preamble.size() - kSpectraRhRows - 1;
    auto headerOk = true;
    for (auto i = 0; i < waterHeader; ++i)
    {
        headerOk &= sameLabels(model.preamble.at(i), actualList.value(i));
    }
    check(headerOk, tr("Header, rows 1-%1").arg(waterHeader));

    auto waterOk = true;
    for (auto i = waterHeader; i < model.preamble.size(); ++i)
    {
        waterOk &= sameLabels(model.preamble.at(i), actualList.value(i));
    }
    check(waterOk, tr("<u>H<sub>2</sub>O</u> TFP labels, rows %1-%2")
                       .arg(waterHeader + 1).arg(model.preamble.size()));

    //> Every other block, against its prototype.
    const auto blocks = spectraBlocks(actualList);
    QVector<int> covered;
    for (auto b = 1; b < blocks.size(); ++b)
    {
        const auto& block = blocks.at(b);
        const auto& header = block.rhClasses ? model.rhHeader : model.monthHeader;
        const auto& rows = block.rhClasses ? model.rhRows : model.monthRows;
        const auto label = block.name.isEmpty() ? tr("(unnamed)") : block.name;

        auto ok = !block.name.isEmpty()
                  && block.header + rows.size() < actualList.size()
                  && sameLabels(header, actualList.value(block.header), block.name);
        for (auto i = 0; ok && i < rows.size(); ++i)
        {
            ok &= sameLabels(rows.at(i), actualList.value(block.header + 1 + i));
        }
        check(ok, tr("<u>%1</u> TFP labels, rows %2-%3")
                      .arg(label).arg(block.header + 1).arg(block.header + 1 + rows.size()));

        const auto slot = slotForGasName(ecProject_, block.name);
        if (slot >= 0) { covered << slot; }
    }

    //> Every gas the raw data holds needs its block. The engine reads a file
    //> that lacks one, but gives that gas no transfer function and raises
    //> Alert 65 mid-run - the kind of thing this test exists to say first.
    //> A gas the project names but does not measure needs nothing.
    if (ecProject_)
    {
        const auto water = primaryWaterSlot(ecProject_);
        const auto& gases = ecProject_->gasColumns();
        for (auto i = 0; i < gases.size(); ++i)
        {
            if (i == water || !gasMeasured(ecProject_, i)) { continue; }
            check(covered.contains(i), tr("<u>%1</u> is measured and has a TFP block")
                                           .arg(gasFileName(ecProject_, i)));
        }
    }

    //> The tail, from its own title on, after the last block.
    const auto tailRow = spectraRowStarting(
        actualList, QStringLiteral("RH/fc_exponential_fit_parameters"));
    const auto blocksEnd = blocks.isEmpty() ? 0 : blocks.last().header + blocks.last().rows;
    check(tailRow > blocksEnd, tr("Exponential-fit and high-pass sections after the blocks"));
    if (tailRow > blocksEnd)
    {
        auto tailOk = true;
        for (auto i = 0; i < model.tail.size(); ++i)
        {
            tailOk &= sameLabels(model.tail.at(i), actualList.value(tailRow + i));
        }
        check(tailOk, tr("Exponential-fit and high-pass labels, rows %1-%2")
                          .arg(tailRow + 1).arg(tailRow + model.tail.size()));
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
    //> Per gas, and by name. This used to read the first monthly block as CO2
    //> and the second as CH4 whatever their headers said, so on a site writing
    //> CO2, N2O, CH4 the N2O values were reported as methane's, and nothing
    //> past the second block was ever looked at.
    const auto blocks = spectraBlocks(actualList);

    QList<bool> test;
    auto last_test = [&](){ return test.value(test.size() - 1); };
    auto check = [&](bool ok, const QString& what)
    {
        test << ok;
        testResults_->append(what + QStringLiteral(": ") + formatPassFail(last_test()));
    };
    auto skip = [&](const QString& label)
    {
        testResults_->append(tr("<u>%1</u>: not in this project's raw data — <b>skipped</b>")
                                 .arg(label));
    };

    const auto columnOf = [&actualList](const SpectraBlock& block, int column)
    {
        QVector<double> values;
        for (auto i = 0; i < block.rows; ++i)
        {
            values << actualList.value(block.header + 1 + i).value(column).toDouble();
        }
        return values;
    };
    const auto fcGood = [](double d){ return d >= 0.001 && d <= 10.0; };
    const auto fnGood = [](double d){ return d >= 0.01 && d <= 10.0; };
    //> Fn is only meaningful where fc is.
    const auto fnGoodWhereFcIs = [&](const QVector<double>& fn, const QVector<double>& fc)
    {
        for (auto i = 0; i < fc.size(); ++i)
        {
            if (fcGood(fc.at(i)) && !fnGood(fn.value(i))) { return false; }
        }
        return true;
    };

    //> A hygrometer's nine RH classes - the primary's table and every other
    //> hygrometer's named block alike. A class may be empty; the table may not.
    const auto testRhBlock = [&](const SpectraBlock& block, const QString& label)
    {
        const auto fn = columnOf(block, 6);
        const auto fc = columnOf(block, 7);
        const auto numerosity = columnOf(block, 8);
        check(std::any_of(fc.begin(), fc.end(), fcGood),
              tr("<u>%1</u> Column 'fc' shall have at least 1 value "
                 "in the range [0.001; 10.0]").arg(label));
        check(std::any_of(fc.begin(), fc.end(),
                          [](double d){ return !qFuzzyCompare(d, -9999.0); }),
              tr("<u>%1</u> Column 'fc' shall not have all values set to -9999").arg(label));
        check(std::any_of(numerosity.begin(), numerosity.end(),
                          [](double d){ return d > 0; }),
              tr("<u>%1</u> Column 'numerosity' shall have at least 1 value > 0").arg(label));
        check(fnGoodWhereFcIs(fn, fc),
              tr("<u>%1</u> Column 'Fn' shall be in the range [0.01; 10.0] "
                 "for good values of column 'fc'").arg(label));
    };

    //> A gas's twelve months. Every month must carry a fit, so a gas the raw
    //> data holds but the assessment could not fit fails here, as CO2 always
    //> did.
    const auto testMonthBlock = [&](const SpectraBlock& block, const QString& label)
    {
        const auto fn = columnOf(block, 2);
        const auto fc = columnOf(block, 3);
        check(std::all_of(fc.begin(), fc.end(), fcGood),
              tr("<u>%1</u> All column 'fc' values shall be in the range [0.001; 10.0]")
                  .arg(label));
        check(fnGoodWhereFcIs(fn, fc),
              tr("<u>%1</u> All column 'Fn' shall be in the range [0.01; 10.0] "
                 "for good values of column 'fc'").arg(label));
    };

    const auto h2oLabel = QStringLiteral("H<sub>2</sub>O");
    if (gasMeasured(ecProject_, primaryWaterSlot(ecProject_)))
    {
        if (!blocks.isEmpty()) { testRhBlock(blocks.first(), h2oLabel); }

        //> The humidity relation belongs to the primary hygrometer.
        const auto expTitle = spectraRowStarting(
            actualList, QStringLiteral("RH/fc_exponential_fit_parameters"));
        QVector<double> fitParameters;
        for (auto i = 0; expTitle >= 0 && i < 3; ++i)
        {
            fitParameters << actualList.value(expTitle + 3).value(i).toDouble();
        }
        check(std::all_of(fitParameters.begin(), fitParameters.end(),
                          [](double d){ return !qFuzzyCompare(d, -9999.0); }),
              tr("<u>%1</u> All spectral corrections RH/fc exponential fit "
                 "parameters shall be != -9999.0").arg(h2oLabel));
    }
    else
    {
        skip(h2oLabel);
    }

    for (auto b = 1; b < blocks.size(); ++b)
    {
        const auto& block = blocks.at(b);
        if (!gasMeasured(ecProject_, slotForGasName(ecProject_, block.name)))
        {
            skip(block.name);
            continue;
        }
        if (block.rhClasses) { testRhBlock(block, block.name); }
        else { testMonthBlock(block, block.name); }
    }

    //> The high-pass model is one for the whole site.
    QVector<double> modelParameters;
    if (spectraRowStarting(actualList,
                           QStringLiteral("High-pass_correction_factor_model_parameters")) >= 0)
    {
        const auto unstable = spectraRowStarting(actualList, QStringLiteral("unstable"));
        const auto stable = spectraRowStarting(actualList, QStringLiteral("stable"));
        modelParameters << actualList.value(unstable).value(2).toDouble();
        modelParameters << actualList.value(unstable).value(3).toDouble();
        modelParameters << actualList.value(stable).value(2).toDouble();
        modelParameters << actualList.value(stable).value(3).toDouble();
    }
    check(std::all_of(modelParameters.begin(), modelParameters.end(),
                      [](double d){ return d >= 0.0 && d <= 1.0; }),
          tr("All high-pass correction factor model parameters "
             "shall be within the range [0; 1]"));

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
    timelagGases_.clear();
    timelagValues = QVector<QVector<double>>(3);
    h2oTimelagValues.clear();
    h2oMinClassNumerosity_ = kDefaultMinClassNumerosity;

    const auto model = timelagTemplateParts(templateList);
    if (!model.isValid())
    {
        testResults_->append(tr("<b>The time-lag template is incomplete. "
                                "Please, re-install the software.</b>"));
        return false;
    }

    //> The provenance rows of PWB aggregate mode are left out before anything
    //> is counted: they sit between the rows of the layout, and left in they
    //> shift every row after them.
    LineList lines;
    for (const auto& line : actualList)
    {
        if (!isPwbProvenance(line)) { lines << line; }
    }

    // preliminary test, number of rows
    auto rowCountTest = (lines.size() > 2);
    testResults_->append(QLatin1String("Number of rows [")
                                 + QString::number(lines.size())
                                 + QStringLiteral("]: ")
                                 + formatPassFail(rowCountTest));
    if (!rowCountTest) { return false; }

    QList<bool> test;
    auto last_test = [&](){ return test.value(test.size() - 1); };
    auto check = [&](bool ok, const QString& what)
    {
        test << ok;
        testResults_->append(what + QStringLiteral(": ") + formatPassFail(last_test()));
    };

    // test a, the header rows against the template's
    for (auto i = 0; i < model.header.size(); ++i)
    {
        check(sameTimelagLabel(model.header.at(i), lines.value(i)),
              QLatin1String("Header, row ") + QString::number(i + 1));
    }

    // test b, one block per gas against the template's block, whatever the gas
    auto cursor = static_cast<int>(model.header.size());
    for (auto gas = timelagBlockGas(model.gasBlock, lines, cursor);
         !gas.isEmpty();
         gas = timelagBlockGas(model.gasBlock, lines, cursor))
    {
        timelagGases_ << gas;
        for (auto k = 0; k < 3; ++k)
        {
            timelagValues[k] << lines.value(cursor + model.valueRows[k]).value(1).toDouble();
        }
        cursor += model.gasBlock.size();
    }
    testResults_->append(tr("Gas blocks found: %1 (%2)")
                             .arg(timelagGases_.size())
                             .arg(timelagGases_.join(QStringLiteral(", "))));

    // test c1, the RH-sorted water table
    const auto hasRhTable = matchesRhTimelagHeader(model.rhHeader, lines, cursor);
    if (hasRhTable)
    {
        check(true, QLatin1String("Header of RH sorted H<sub>2</sub>O classes (3 rows)"));
        h2oMinClassNumerosity_ = statedMinClassNumerosity(lines.value(cursor + 1),
                                                          kDefaultMinClassNumerosity);

        // test c1' (moved from scientific to formal)
        const auto rhStart = cursor + 3;
        auto rhClassCount = 0;
        while (!lines.value(rhStart + rhClassCount).isEmpty())
        {
            ++rhClassCount;
            auto actualRhlClassIndex = lines.value(rhStart + rhClassCount - 1).value(0).toInt();
            check(rhClassCount == actualRhlClassIndex,
                  QLatin1String("Consistent RH index [")
                      + QString::number(actualRhlClassIndex) + QStringLiteral("]"));
        }

        // test c2
        if (rhClassCount <= 20)
        {
            h2oTimelagValues.resize(4);
            for (auto i = 0; i < rhClassCount; ++i)
            {
                for (auto k = 0; k < 4; ++k)
                {
                    h2oTimelagValues[k] << lines.value(rhStart + i).value(4 + k).toDouble();
                }
            }
            check(lines.value(rhStart).value(1) == QLatin1String("0")
                      && lines.value(rhStart + rhClassCount - 1).value(3) == QLatin1String("100%"),
                  QStringLiteral("Consistent RH ranges"));
        }
        else
        {
            check(false, QLatin1String("RH classes <= 20"));
        }
    }
    else
    {
        //> Without the table, the gas blocks have to be the whole file.
        QString stray;
        for (auto i = cursor; i < lines.size() && stray.isEmpty(); ++i)
        {
            stray = joinedLine(lines.at(i));
        }
        if (!stray.isEmpty())
        {
            check(false, tr("Unrecognised row after the gas blocks [%1]")
                             .arg(stray.toHtmlEscaped()));
        }
        else
        {
            check(!timelagGases_.isEmpty(),
                  tr("At least one gas block, or the RH sorted H<sub>2</sub>O classes"));
        }
    }

    //> Every gas the raw data holds needs a time lag here - the primary
    //> hygrometer's may be the RH table instead. The engine reads a file that
    //> lacks one and quietly uses that gas's default lag, which is not what a
    //> user selecting this file expects.
    if (ecProject_)
    {
        QVector<int> covered;
        for (const auto& gas : std::as_const(timelagGases_))
        {
            covered << slotForGasName(ecProject_, gas);
        }
        const auto water = primaryWaterSlot(ecProject_);
        if (hasRhTable) { covered << water; }

        const auto& gases = ecProject_->gasColumns();
        for (auto i = 0; i < gases.size(); ++i)
        {
            if (!gasMeasured(ecProject_, i)) { continue; }
            check(covered.contains(i), tr("<u>%1</u> is measured and has a time lag")
                                           .arg(gasFileName(ecProject_, i).toLower()));
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
    auto last_test = [&](){ return test.value(test.size() - 1); };
    auto check = [&](bool ok, const QString& what)
    {
        test << ok;
        testResults_->append(what + QStringLiteral(": ") + formatPassFail(last_test()));
    };

    //> Only gases the raw data holds. A block for any other - a gas the
    //> project names without a column, or one from the project the file was
    //> made for - is read and discarded by the engine, -9999 and all.
    QVector<int> measured;
    for (auto j = 0; j < timelagGases_.size(); ++j)
    {
        if (gasMeasured(ecProject_, slotForGasName(ecProject_, timelagGases_.at(j))))
        {
            measured << j;
        }
        else
        {
            testResults_->append(tr("<u>%1</u>: not in this project's raw data — <b>skipped</b>")
                                     .arg(timelagGases_.at(j)));
        }
    }

    //> The gases that fail \a rule, named, so a long list says which.
    const auto failing = [&](const std::function<bool(int)>& rule)
    {
        QStringList names;
        for (const auto j : std::as_const(measured))
        {
            if (!rule(j)) { names << timelagGases_.at(j); }
        }
        return names;
    };
    const auto verdict = [&](const QStringList& names, const QString& what)
    {
        check(names.isEmpty(), names.isEmpty()
                                   ? what
                                   : what + QStringLiteral(" [") + names.join(QStringLiteral(", "))
                                         + QStringLiteral("]"));
    };

    if (!measured.isEmpty())
    {
        const auto& median = timelagValues.at(0);
        const auto& minimum = timelagValues.at(1);
        const auto& maximum = timelagValues.at(2);

        // test a.0, a determination at all
        verdict(failing([&](int j){ return !qFuzzyCompare(median.at(j), -9999.0); }),
                QLatin1String("Every measured gas has a time-lag determination"));

        // test a
        verdict(failing([&](int j){ return median.at(j) >= minimum.at(j)
                                           && median.at(j) <= maximum.at(j); }),
                QLatin1String("Gas time-lag median values inside the [minimum; maximum] range"));

        // test b
        verdict(failing([&](int j){ return median.at(j) <= 60.0
                                           && minimum.at(j) <= 60.0
                                           && maximum.at(j) <= 60.0; }),
                QLatin1String("Time-lag values not larger than 60 seconds"));
    }

    // if there are RH classes, and the primary hygrometer is measured
    if (h2oTimelagValues.size() && !gasMeasured(ecProject_, primaryWaterSlot(ecProject_)))
    {
        testResults_->append(tr("<u>H<sub>2</sub>O</u> RH sorted classes: not in this "
                                "project's raw data — <b>skipped</b>"));
    }
    else if (h2oTimelagValues.size())
    {
        // test c.2
        auto rangeOk = true;
        auto rhClassCount = h2oTimelagValues[0].size();
        for (auto i = 0; i < rhClassCount; ++i)
        {
            if (!((h2oTimelagValues[0][i] >= h2oTimelagValues[1][i])
                  && (h2oTimelagValues[0][i] <= h2oTimelagValues[2][i])))
            {
                rangeOk = false;
                break;
            }
        }
        check(rangeOk, QStringLiteral("H<sub>2</sub>O RH-sorted median values inside the "
                                      "[minimum; maximum] range"));

        // test c.3, against the numerosity the file itself says a class needs
        const auto determined = std::count_if(
            h2oTimelagValues[3].begin(), h2oTimelagValues[3].end(),
            [this](double n){ return n >= h2oMinClassNumerosity_; });
        check(determined >= 3, QStringLiteral("At least 3 H<sub>2</sub>O classes with numerosity >= ")
                                   + QString::number(h2oMinClassNumerosity_));
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

