/***************************************************************************
  timelagpreassessdialog.cpp
  --------------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "timelagpreassessdialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

#include "defs.h"
#include "dlproject.h"
#include "ecinidefs.h"
#include "ecproject.h"
#include "engineprogress.h"
#include "irga_desc.h"
#include "laghistogramwidget.h"
#include "logpercentslider.h"
#include "measurement_record.h"
#include "process.h"
#include "remotesource.h"
#include "variable_desc.h"
#include "widget_utils.h"

namespace {

//> tlag_meth as the project file has it.
constexpr int TlagConstant = 1;
constexpr int TlagOptimizer = 4;
constexpr int TlagPwb = 5;

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

//> The fewest half-hours drawn, however small the share.
constexpr int MinSampled = 10;

//> More gas columns than this and the tables scroll sideways rather than
//> squeeze every column.
constexpr int MaxStretchedColumns = 6;

//> Size of a saved chart.
const QSize FigureSize(1000, 520);

//> Round a window outwards to the tenth of a second the project file keeps.
double floorTenth(double x) { return std::floor(x * 10.0 + 1e-9) / 10.0; }
double ceilTenth(double x) { return std::ceil(x * 10.0 - 1e-9) / 10.0; }

//> The raw file extension filter, as the basic settings page forms it.
QString rawFileFilter(const EcProject* ec)
{
    const QString proto = ec->generalFilePrototype();
    if (proto.isEmpty())
    {
        return ec->generalFileType() == Defs::RawFileType::GHG
                   ? QStringLiteral("*.") + Defs::GHG_NATIVE_DATA_FILE_EXT
                   : QStringLiteral("*");
    }
    return QStringLiteral("*.") + proto.mid(proto.lastIndexOf(QLatin1Char('.')) + 1);
}

QString formatPercent(double pct)
{
    return QString::number(pct, 'f', pct < 10.0 ? 1 : 0) + QStringLiteral(" %");
}

//> A time lag in a table cell: edited with a spin box, shown with a fixed
//> number of decimals. Cells holding text - "-", "37 of 39" - are shown as
//> they are.
class LagCellDelegate : public QStyledItemDelegate
{
public:
    LagCellDelegate(int decimals, QObject* parent) :
        QStyledItemDelegate(parent), decimals_(decimals) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&,
                          const QModelIndex&) const override
    {
        auto spin = new QDoubleSpinBox(parent);
        spin->setDecimals(decimals_);
        spin->setRange(-100.0, 100.0);
        spin->setSingleStep(0.1);
        spin->setFrame(false);
        spin->setAlignment(Qt::AlignCenter);
        return spin;
    }
    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        static_cast<QDoubleSpinBox*>(editor)->setValue(index.data(Qt::EditRole).toDouble());
    }
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override
    {
        auto spin = static_cast<QDoubleSpinBox*>(editor);
        spin->interpretText();
        model->setData(index, spin->value(), Qt::EditRole);
    }
    QString displayText(const QVariant& value, const QLocale& locale) const override
    {
        if (value.typeId() == QMetaType::Double)
        {
            return locale.toString(value.toDouble(), 'f', decimals_);
        }
        return QStyledItemDelegate::displayText(value, locale);
    }

private:
    int decimals_;
};

QTableWidget* createGasTable(const QStringList& rowLabels, int decimals, QWidget* parent)
{
    auto table = new QTableWidget(rowLabels.size(), 0, parent);
    table->setVerticalHeaderLabels(rowLabels);
    table->setItemDelegate(new LagCellDelegate(decimals, table));
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setSelectionBehavior(QAbstractItemView::SelectItems);
    table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked
                           | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table->horizontalHeader()->setMinimumSectionSize(90);
    table->horizontalHeader()->setDefaultSectionSize(110);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return table;
}

}  // namespace

TimelagPreassessDialog::TimelagPreassessDialog(QWidget* parent,
                                               DlProject* dlProject,
                                               EcProject* ecProject) :
    QDialog(parent),
    dlProject_(dlProject),
    ecProject_(ecProject)
{
    //> Modal: the assessment saves a copy of the project as it stands, so
    //> the project should not change under it while it runs.
    setWindowModality(Qt::WindowModal);
    setWindowTitle(tr("Timelag Preliminary Assessment"));
    WidgetUtils::removeContextHelpButton(this);

    auto intro = new QLabel(tr(
        "Detects the time lag of a random sample of the half-hours, with the "
        "nominal, minimum and maximum time lags set aside for a wide search "
        "window, using the time lag method selected (covariance maximization "
        "for any method but pre-whitening block-bootstrap). Where most lags fall "
        "is the plausible window; the suggestions below can be edited and then "
        "written to the metadata."));
    intro->setWordWrap(true);

    // --- Sample -----------------------------------------------------------
    auto sampleTitle = WidgetUtils::createBlueLabel(this, tr("Sample"));
    slider_ = new LogPercentSlider;
    sampleLabel_ = new QLabel;
    sampleLabel_->setMinimumWidth(sampleLabel_->fontMetrics().horizontalAdvance(
        QStringLiteral("100 % - all 17520 half-hours with raw data")));
    wholeCheck_ = new QCheckBox(tr("Run entire dataset"));
    wholeCheck_->setToolTip(tr(
        "<b>Run entire dataset:</b> assess every half-hour with raw data rather "
        "than a sample. It takes as long as the time lag pre-pass of a full run, "
        "and it is one: the time lag optimization file it produces is left in "
        "the output folder whether or not the outputs are kept, and can then be "
        "used by the project, so its own run need not repeat the pre-pass."));
    auto sampleRow = new QHBoxLayout;
    sampleRow->addWidget(new QLabel(tr("Half-hours to assess :")));
    sampleRow->addWidget(slider_, 1);
    sampleRow->addWidget(sampleLabel_);
    sampleRow->addSpacing(12);
    sampleRow->addWidget(wholeCheck_);

    // --- Search windows ----------------------------------------------------
    auto windowTitle = WidgetUtils::createBlueLabel(this, tr("Search window"));
    windowTable_ = createGasTable({ tr("From [s]"), tr("To [s]") }, 1, this);
    windowTable_->setToolTip(tr(
        "<b>Search window:</b> the time lags searched, per gas. The nominal, "
        "minimum and maximum time lags of the metadata are not used. A wider "
        "window costs time, and with pre-whitening block-bootstrap also widens "
        "its resampling blocks."));

    keepCheck_ = new QCheckBox(tr("Keep pre-assessment outputs"));
    keepCheck_->setToolTip(tr(
        "<b>Keep pre-assessment outputs:</b> the assessment runs on a "
        "throw-away copy of the project in a temporary folder. Unticked, that "
        "folder - the copy of the project, the table of detected lags and the "
        "run log - is removed when this window closes. Ticked, it is copied "
        "first to a folder named tlag_preassessment_<date-time> in the "
        "project's output folder, together with each gas's chart as a PNG and "
        "the suggested time lags."));

    runButton_ = new QPushButton(tr("Run pre-assessment"));
    runButton_->setProperty("mdButton", true);
    progressBar_ = new QProgressBar;
    progressBar_->setTextVisible(true);
    progressBar_->setRange(0, 1);
    progressBar_->setValue(0);
    statusLabel_ = new QLabel;
    statusLabel_->setWordWrap(true);
    statusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    logToggle_ = new QPushButton(tr("Show engine output"));
    logToggle_->setCheckable(true);
    logToggle_->setFlat(true);
    logToggle_->hide();
    engineLog_ = new QPlainTextEdit;
    engineLog_->setReadOnly(true);
    engineLog_->setMaximumBlockCount(5000);
    engineLog_->setMinimumHeight(120);
    engineLog_->hide();

    auto runRow = new QHBoxLayout;
    runRow->addWidget(runButton_);
    runRow->addWidget(progressBar_, 1);
    runRow->addWidget(keepCheck_);

    // --- Results -----------------------------------------------------------
    auto resultTitle = WidgetUtils::createBlueLabel(this, tr("Detected time lags"));
    gasCombo_ = new QComboBox;
    //> As wide as its longest gas name - "CO2 (li7200_1)" and the like - not
    //> the width of the first item it was laid out with.
    gasCombo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    gasCombo_->setMinimumContentsLength(8);
    gasCombo_->setToolTip(tr("<b>Gas:</b> whose detected time lags the chart shows."));
    binSpin_ = new QDoubleSpinBox;
    binSpin_->setDecimals(2);
    binSpin_->setRange(0.01, 10.0);
    binSpin_->setSingleStep(0.1);
    binSpin_->setValue(0.5);
    binSpin_->setSuffix(tr("  [s]", "Second"));
    binSpin_->setToolTip(tr("<b>Bin width:</b> the width of each bar of the chart."));
    viewCombo_ = new QComboBox;
    viewCombo_->addItem(tr("Histogram"));
    viewCombo_->addItem(tr("Time series"));
    viewCombo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    viewCombo_->setToolTip(tr(
        "<b>View:</b> how the detected time lags are shown. <b>Histogram</b>: how "
        "many half-hours found each lag, which is where the suggestion comes "
        "from. <b>Time series</b>: each half-hour's lag at its time, which shows "
        "whether the lag drifts, jumps - a tube or flow change - or follows the "
        "time of day."));
    edgeCheck_ = new QCheckBox(tr("Include lags at the window edges"));
    edgeCheck_->setToolTip(tr(
        "<b>Include lags at the window edges:</b> a lag found at either end of "
        "the search window is where the covariance kept rising, not a peak. "
        "Such lags are always drawn, stacked in grey; ticked, they also count "
        "towards the suggestions."));
    fluxCheck_ = new QCheckBox(tr("Only half-hours above the flux threshold"));
    fluxCheck_->setToolTip(tr(
        "<b>Only half-hours above the flux threshold:</b> use only the "
        "half-hours whose flux cleared the minimum flux the automatic time lag "
        "optimization uses. A weak flux gives a poorly defined lag, but for a "
        "gas that is mostly taken up rather than emitted the signed threshold "
        "may drop every half-hour. Not applied to pre-whitening "
        "block-bootstrap, which judges each detection itself."));

    auto chartControls = new QHBoxLayout;
    chartControls->addWidget(new QLabel(tr("Gas :")));
    chartControls->addWidget(gasCombo_);
    chartControls->addSpacing(12);
    chartControls->addWidget(new QLabel(tr("Bin width :")));
    chartControls->addWidget(binSpin_);
    chartControls->addSpacing(12);
    chartControls->addWidget(new QLabel(tr("View :")));
    chartControls->addWidget(viewCombo_);
    chartControls->addSpacing(12);
    chartControls->addWidget(edgeCheck_);
    chartControls->addWidget(fluxCheck_);
    chartControls->addStretch(1);

    histogram_ = new LagHistogramWidget;
    histogramNote_ = new QLabel;
    histogramNote_->setWordWrap(true);

    auto suggestTitle = WidgetUtils::createBlueLabel(this, tr("Suggested time lags"));
    resultTable_ = createGasTable({ tr("Lags used"), tr("Nominal [s]"), tr("Minimum [s]"), tr("Maximum [s]"),
                                    tr("Same analyser") },
                                  2, this);
    resultTable_->setToolTip(tr(
        "<b>Suggested time lags:</b> taken from the peak of the lags found: the "
        "median of the lags around it, and a window of the automatic time lag "
        "optimization's width either side. Edit any value before setting it in "
        "the metadata. A gas with a remark shows an icon in its heading; hover "
        "over the column to read it. A gas with no clear peak of its own can "
        "borrow the suggestion of a gas on the same analyser."));

    setButton_ = new QPushButton(tr("Set in metadata"));
    setButton_->setProperty("mdButton", true);
    setButton_->setEnabled(false);
    setButton_->setToolTip(tr(
        "<b>Set in metadata:</b> writes each gas's nominal, minimum and maximum "
        "time lag to its column in the metadata. Under automatic time lag "
        "optimization or pre-whitening block-bootstrap, which search a window "
        "of their own, the minimum and maximum also become that window."));
    useFileButton_ = new QPushButton(tr("Use time lag file in project"));
    useFileButton_->setProperty("mdButton", true);
    useFileButton_->hide();
    auto closeButton = new QPushButton(tr("Close"));

    auto bottomRow = new QHBoxLayout;
    bottomRow->addWidget(WidgetUtils::createQuestionMarkButton(this,
        QStringLiteral("https://kebasaa.github.io/eddyflow-documentation/topics/timelag-preliminary-assessment/")),
        0, Qt::AlignVCenter);
    bottomRow->addWidget(setButton_);
    bottomRow->addWidget(useFileButton_);
    bottomRow->addStretch(1);
    bottomRow->addWidget(closeButton);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addWidget(sampleTitle);
    layout->addLayout(sampleRow);
    layout->addWidget(windowTitle);
    layout->addWidget(windowTable_);
    layout->addLayout(runRow);
    layout->addWidget(statusLabel_);
    layout->addWidget(logToggle_, 0, Qt::AlignLeft);
    layout->addWidget(engineLog_);
    layout->addWidget(resultTitle);
    layout->addLayout(chartControls);
    layout->addWidget(histogram_, 1);
    layout->addWidget(histogramNote_);
    layout->addWidget(suggestTitle);
    layout->addWidget(resultTable_);
    layout->addLayout(bottomRow);

    connect(slider_, &QSlider::valueChanged, this, &TimelagPreassessDialog::updateSampleLabel);
    connect(wholeCheck_, &QCheckBox::toggled, this, &TimelagPreassessDialog::wholeToggled);
    connect(runButton_, &QPushButton::clicked, this, &TimelagPreassessDialog::runOrCancel);
    connect(logToggle_, &QPushButton::toggled, engineLog_, &QWidget::setVisible);
    connect(gasCombo_, &QComboBox::currentIndexChanged, this, &TimelagPreassessDialog::showGas);
    connect(binSpin_, &QDoubleSpinBox::valueChanged, this, &TimelagPreassessDialog::redrawCurrent);
    //> The bins are the histogram's alone.
    connect(viewCombo_, &QComboBox::currentIndexChanged, this, [this](int view) {
        binSpin_->setEnabled(view == ViewHistogram);
        histogram_->resetZoom();
        redrawCurrent();
    });
    connect(edgeCheck_, &QCheckBox::toggled, this, [this] { updateSuggestions(); redrawCurrent(); });
    connect(fluxCheck_, &QCheckBox::toggled, this, [this] { updateSuggestions(); redrawCurrent(); });
    //> An edited suggestion moves its marker on the chart.
    connect(resultTable_, &QTableWidget::itemChanged, this, &TimelagPreassessDialog::redrawCurrent);
    //> Clicking a gas's column shows its chart.
    connect(resultTable_, &QTableWidget::currentCellChanged, this,
            [this](int, int column, int, int) { selectGasColumn(column); });
    connect(resultTable_->horizontalHeader(), &QHeaderView::sectionClicked,
            this, &TimelagPreassessDialog::selectGasColumn);
    connect(setButton_, &QPushButton::clicked, this, &TimelagPreassessDialog::setInMetadata);
    connect(useFileButton_, &QPushButton::clicked, this, &TimelagPreassessDialog::useFileInProject);
    connect(keepCheck_, &QCheckBox::toggled, this, &TimelagPreassessDialog::keepToggled);
    connect(closeButton, &QPushButton::clicked, this, &TimelagPreassessDialog::reject);

    //> Room for the chart from the start: up to 1280 x 1000, but never more
    //> than nine tenths of the screen the window opens on.
    const QScreen* screen = parent ? parent->screen() : QGuiApplication::primaryScreen();
    const QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 1000);
    resize(std::min(1280, avail.width() * 9 / 10), std::min(1000, avail.height() * 9 / 10));
}

TimelagPreassessDialog::~TimelagPreassessDialog()
{
    if (engine_ && engine_->isRunning()) { engine_->processStop(); }
    discardOutputs();
}

QString TimelagPreassessDialog::rawDataProblem(const EcProject* ecProject)
{
    const QString path = ecProject->screenDataPath().trimmed();
    if (path.isEmpty())
    {
        return tr("No raw data folder is set.");
    }
    //> A shared link is listed by its provider when the run starts; there is
    //> nothing on this disk to look at first.
    if (RemoteSource::isRemote(path)) { return QString(); }
    if (!QFileInfo(path).isDir())
    {
        return tr("The raw data folder %1 does not exist.").arg(QDir::toNativeSeparators(path));
    }
    const auto flag = ecProject->screenRecurse() ? QDirIterator::Subdirectories
                                                 : QDirIterator::NoIteratorFlags;
    QDirIterator it(path, { rawFileFilter(ecProject) }, QDir::Files, flag);
    if (!it.hasNext())
    {
        return tr("The raw data folder %1 holds no raw files (%2).")
            .arg(QDir::toNativeSeparators(path), rawFileFilter(ecProject));
    }
    return QString();
}

void TimelagPreassessDialog::refresh()
{
    if (engine_ && engine_->isRunning()) { return; }
    periodsEstimate_ = estimatePeriods();
    const double pct = LagStats::defaultSamplePercent(periodsEstimate_ > 0 ? periodsEstimate_ : 0);
    slider_->setDefaultPercent(pct);
    if (wholeCheck_->isChecked())
    {
        percentBeforeWhole_ = pct;
    }
    else
    {
        slider_->setPercent(pct);
    }
    updateSampleLabel();
    buildGasRows();
    if (table_.isEmpty())
    {
        histogram_->setMessage(QString());
        statusLabel_->clear();
    }
    else
    {
        updateSuggestions();
        redrawCurrent();
    }
}

qint64 TimelagPreassessDialog::estimatePeriods() const
{
    const QString path = ecProject_->screenDataPath().trimmed();
    if (path.isEmpty() || RemoteSource::isRemote(path) || !QFileInfo(path).isDir()) { return -1; }

    const auto flag = ecProject_->screenRecurse() ? QDirIterator::Subdirectories
                                                  : QDirIterator::NoIteratorFlags;
    qint64 files = 0;
    QDirIterator it(path, { rawFileFilter(ecProject_) }, QDir::Files, flag);
    while (it.hasNext()) { it.next(); ++files; }

    //> One file holds fileDuration / avrg_len averaging periods.
    const int fileMinutes = dlProject_ ? dlProject_->fileDuration() : 0;
    const int avgMinutes = ecProject_->screenAvrgLen();
    qint64 perFile = 1;
    if (fileMinutes > 0 && avgMinutes > 0) { perFile = std::max(1, fileMinutes / avgMinutes); }
    return files * perFile;
}

void TimelagPreassessDialog::updateSampleLabel()
{
    if (wholeCheck_->isChecked())
    {
        sampleLabel_->setText(periodsEstimate_ > 0
            ? tr("100 % - all %1 half-hours with raw data").arg(periodsEstimate_)
            : tr("100 % - all half-hours with raw data"));
        return;
    }
    const double pct = slider_->percent();
    if (periodsEstimate_ <= 0)
    {
        sampleLabel_->setText(tr("%1 - of the half-hours with raw data (at least %2)")
                                  .arg(formatPercent(pct)).arg(MinSampled));
        return;
    }
    const qint64 n = LagStats::sampledCount(periodsEstimate_, pct, MinSampled);
    sampleLabel_->setText(tr("%1 - about %2 of %3 half-hours (at least %4)")
                              .arg(formatPercent(pct)).arg(n).arg(periodsEstimate_).arg(MinSampled));
}

void TimelagPreassessDialog::wholeToggled(bool whole)
{
    //> The slider shows 100 % while the box is ticked, and goes back to
    //> where it was when it is unticked.
    if (whole)
    {
        percentBeforeWhole_ = slider_->percent();
        slider_->setPercent(LogPercentSlider::MaxPercent);
    }
    else
    {
        slider_->setPercent(percentBeforeWhole_);
    }
    slider_->setEnabled(!whole && !(engine_ && engine_->isRunning()));
    updateSampleLabel();
}

void TimelagPreassessDialog::fitTableHeight(QTableWidget* table) const
{
    //> Every row shown, never a vertical scroll bar; sideways scrolling only
    //> when there are more gases than fit.
    const bool scroll = table->columnCount() > MaxStretchedColumns;
    table->horizontalHeader()->setSectionResizeMode(scroll ? QHeaderView::Interactive
                                                           : QHeaderView::Stretch);
    int h = table->horizontalHeader()->sizeHint().height() + table->verticalHeader()->length()
            + 2 * table->frameWidth();
    if (scroll) { h += table->horizontalScrollBar()->sizeHint().height(); }
    table->setFixedHeight(h);
}

double TimelagPreassessDialog::cellValue(QTableWidget* table, int row, int column) const
{
    const auto* item = table->item(row, column);
    return item ? item->data(Qt::EditRole).toDouble() : NaN;
}

void TimelagPreassessDialog::setValueCell(QTableWidget* table, int row, int column,
                                          double value, bool editable)
{
    auto item = new QTableWidgetItem;
    item->setData(Qt::EditRole, value);
    item->setTextAlignment(Qt::AlignCenter);
    item->setFlags(editable ? Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable
                            : Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    table->setItem(row, column, item);
}

void TimelagPreassessDialog::setTextCell(QTableWidget* table, int row, int column, const QString& text)
{
    auto item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    table->setItem(row, column, item);
}

void TimelagPreassessDialog::applyNote(int column)
{
    //> The remark goes where the user is looking: on the column's heading,
    //> with an icon saying there is one, and on every cell of it.
    const auto& row = rows_.at(column);
    auto header = resultTable_->horizontalHeaderItem(column);
    if (!header) { return; }
    header->setToolTip(row.note);
    header->setIcon(row.note.isEmpty()
        ? QIcon()
        : style()->standardIcon(row.noteIsWarning ? QStyle::SP_MessageBoxWarning
                                                  : QStyle::SP_MessageBoxInformation));
    for (int r = 0; r < resultTable_->rowCount(); ++r)
    {
        if (auto item = resultTable_->item(r, column)) { item->setToolTip(row.note); }
    }
}

void TimelagPreassessDialog::buildGasRows()
{
    rows_.clear();
    const auto& gases = ecProject_->gasColumns();
    const VariableDescList* vars = dlProject_ ? dlProject_->variables() : nullptr;
    static const QRegularExpression irgaRef(QStringLiteral("^\\s*Irga\\s+(\\d+)\\s*:"));

    QHash<QString, int> slugCount;
    for (const auto& g : gases) { ++slugCount[g.slug]; }

    QVector<QPair<double, double>> windows;
    for (int i = 0; i < gases.size(); ++i)
    {
        const auto& gas = gases.at(i);
        if (!gas.isValid()) { continue; }
        GasRow row;
        row.record = i + 1;
        row.name = gas.slug.toUpper();
        row.instrumentId = gas.instrumentId;
        row.isWater = gas.slug == QLatin1String("h2o");
        if (slugCount.value(gas.slug) > 1)
        {
            row.name += QStringLiteral(" (%1)").arg(gas.instrumentId.isEmpty()
                                                        ? tr("column %1").arg(gas.rawColumn)
                                                        : gas.instrumentId);
        }

        //> The analyser behind the column, for the path type and the tube.
        double tubeL = 0.0, tubeD = 0.0, flow = 0.0;
        const int index = gas.rawColumn - 1;
        if (vars && index >= 0 && index < vars->size() && dlProject_->irgas())
        {
            const auto m = irgaRef.match(vars->at(index).instrument());
            const int k = m.hasMatch() ? m.captured(1).toInt() - 1 : -1;
            const auto* irgas = dlProject_->irgas();
            if (k >= 0 && k < irgas->size())
            {
                const auto& irga = irgas->at(k);
                row.closedPath = !IrgaDesc::isOpenPathModel(irga.model());
                tubeL = irga.tubeLength();
                tubeD = irga.tubeDiameter();
                flow = irga.tubeFlowRate();
            }
        }
        double lo = 0.0, hi = 0.0;
        LagStats::defaultSearchWindow(row.closedPath, tubeL, tubeD, flow, &lo, &hi);
        if (row.closedPath && tubeL > 0.0 && tubeD > 0.0 && flow > 0.0)
        {
            row.transit = (hi - 10.0) / 3.0;
        }
        rows_.append(row);
        windows.append({ lo, hi });
    }

    QStringList names;
    for (const auto& row : rows_) { names << row.name; }

    {
        const QSignalBlocker block(windowTable_);
        windowTable_->clearContents();
        windowTable_->setColumnCount(rows_.size());
        windowTable_->setHorizontalHeaderLabels(names);
        for (int c = 0; c < rows_.size(); ++c)
        {
            setValueCell(windowTable_, WindowFrom, c, windows.at(c).first, true);
            setValueCell(windowTable_, WindowTo, c, windows.at(c).second, true);
            const auto& row = rows_.at(c);
            QString tip = row.closedPath ? tr("Closed path") : tr("Open path");
            if (row.transit >= 0.0)
            {
                tip += QStringLiteral(", ") + tr("tube transit time about %1 s").arg(row.transit, 0, 'f', 1);
            }
            windowTable_->horizontalHeaderItem(c)->setToolTip(tip);
        }
        fitTableHeight(windowTable_);
    }
    {
        const QSignalBlocker block(resultTable_);
        resultTable_->clearContents();
        resultTable_->setColumnCount(rows_.size());
        resultTable_->setHorizontalHeaderLabels(names);
        for (int c = 0; c < rows_.size(); ++c)
        {
            for (int r = 0; r < resultTable_->rowCount(); ++r)
            {
                setTextCell(resultTable_, r, c, QStringLiteral("-"));
            }
        }
        fitTableHeight(resultTable_);
    }

    if (rows_.isEmpty())
    {
        statusLabel_->setText(tr("The project has no gas selected on the Basic Settings page, "
                                 "so there is no time lag to assess."));
    }
    runButton_->setEnabled(!rows_.isEmpty());
}

void TimelagPreassessDialog::setRunning(bool running)
{
    runButton_->setText(running ? tr("Cancel") : tr("Run pre-assessment"));
    slider_->setEnabled(!running && !wholeCheck_->isChecked());
    wholeCheck_->setEnabled(!running);
    keepCheck_->setEnabled(!running);
    windowTable_->setEnabled(!running);
    setButton_->setEnabled(!running && std::any_of(rows_.cbegin(), rows_.cend(),
                                                   [](const GasRow& r) { return r.suggestion; }));
    useFileButton_->setEnabled(!running);
}

void TimelagPreassessDialog::runOrCancel()
{
    if (engine_ && engine_->isRunning())
    {
        cancelled_ = true;
        engine_->processStop();
        engineFinished();
        return;
    }

    //> The folder may have changed since the window opened.
    const QString problem = rawDataProblem(ecProject_);
    if (!problem.isEmpty())
    {
        WidgetUtils::warning(this, tr("Raw Data Folder Not Set"), problem,
                             tr("Set the raw data folder on the Basic Settings page first."));
        return;
    }
    for (int c = 0; c < rows_.size(); ++c)
    {
        if (cellValue(windowTable_, WindowTo, c) <= cellValue(windowTable_, WindowFrom, c))
        {
            WidgetUtils::warning(this, tr("Search Window"),
                                 tr("The search window of %1 ends before it starts.").arg(rows_.at(c).name));
            return;
        }
    }

    const QString workingDir = qApp->applicationDirPath() + QLatin1Char('/') + Defs::BIN_FILE_DIR;
    const QString enginePath = workingDir + QLatin1Char('/') + Defs::ENGINE_RP;
    if (!QFileInfo::exists(enginePath))
    {
        WidgetUtils::warning(this, tr("Engine not found"),
                             tr("Processing engine not found at:\n%1").arg(QDir::toNativeSeparators(enginePath)));
        return;
    }

    discardOutputs();
    workDir_ = std::make_unique<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/eddyflow_tlag_XXXXXX"));
    if (!workDir_->isValid())
    {
        WidgetUtils::warning(this, tr("Timelag Preliminary Assessment"),
                             tr("Could not create a temporary folder:\n%1").arg(workDir_->errorString()));
        workDir_.reset();
        return;
    }

    //> What this run is asked, kept: the controls may change before it ends.
    runWhole_ = wholeCheck_->isChecked();
    runMethod_ = ecProject_->screenTlagMeth();
    runWindows_.clear();
    for (int c = 0; c < rows_.size(); ++c)
    {
        runWindows_.insert(rows_.at(c).record, { cellValue(windowTable_, WindowFrom, c),
                                                 cellValue(windowTable_, WindowTo, c) });
    }

    const int seed = static_cast<int>(QRandomGenerator::global()->bounded(1, 2147483646));
    QString projectPath, error;
    if (!writeRunProject(workDir_->path(), seed, &projectPath, &error))
    {
        WidgetUtils::warning(this, tr("Timelag Preliminary Assessment"), error);
        discardOutputs();
        return;
    }

    table_.clear();
    gasCombo_->clear();
    histogram_->setMessage(tr("Assessing..."));
    histogramNote_->clear();
    for (auto& row : rows_)
    {
        row.suggestion = false;
        row.note.clear();
        row.noteIsWarning = false;
        row.peak = LagStats::PeakSuggestion();
        row.borrowedFrom = 0;
    }
    {
        const QSignalBlocker block(resultTable_);
        for (int c = 0; c < rows_.size(); ++c)
        {
            resultTable_->removeCellWidget(ResultBorrow, c);
            for (int r = 0; r < resultTable_->rowCount(); ++r)
            {
                setTextCell(resultTable_, r, c, QStringLiteral("-"));
            }
            applyNote(c);
        }
    }
    useFileButton_->hide();
    optFileForProject_.clear();
    engineLog_->clear();
    pendingOutput_.clear();
    cancelled_ = false;
    keptDir_.clear();

    startEngine(projectPath);
}

bool TimelagPreassessDialog::writeRunProject(const QString& dir, int seed,
                                             QString* projectPath, QString* error)
{
    const QString home = dir + QStringLiteral("/home");
    const QString out = dir + QStringLiteral("/out");
    const QStringList subs{ home + QStringLiteral("/ini"), home + QStringLiteral("/tmp"),
                            home + QStringLiteral("/log"), out };
    for (const auto& sub : subs)
    {
        if (!QDir().mkpath(sub))
        {
            *error = tr("Could not create %1.").arg(QDir::toNativeSeparators(sub));
            return false;
        }
    }

    //> The copies are written by the same code as the real files, so they
    //> say exactly what the project holds now - saved or not - and leave the
    //> user's own files, and whether they count as saved, alone.
    QString metadataPath;
    if (ecProject_->generalUseAltMdFile() && dlProject_)
    {
        metadataPath = dir + QStringLiteral("/preassess.metadata");
        if (!dlProject_->saveCopy(metadataPath))
        {
            *error = tr("Could not write a copy of the metadata.");
            return false;
        }
    }

    *projectPath = dir + QStringLiteral("/preassess.") + Defs::PROJECT_FILE_EXT;
    if (!ecProject_->saveEcProjectCopy(*projectPath))
    {
        *error = tr("Could not write a copy of the project.");
        return false;
    }

    const bool pwb = runMethod_ == TlagPwb;
    QSettings ini(*projectPath, QSettings::IniFormat);

    ini.beginGroup(EcIni::INIGROUP_PROJECT);
        //> Advanced: express mode replaces the time lag method with its own.
        ini.setValue(EcIni::INI_PROJECT_33, 0);
        ini.setValue(EcIni::INI_PROJECT_40, 0);
        ini.setValue(EcIni::INI_PROJECT_51, QString(out + QLatin1Char('/')));
        //> The whole dataset, not the processing period.
        ini.setValue(EcIni::INI_PROJECT_54, 0);
        //> Ensemble spectra hand the outputs to FCC, which does not run here.
        ini.setValue(EcIni::INI_PROJECT_60, 0);
        ini.setValue(EcIni::INI_PROJECT_65, 0);
        if (!metadataPath.isEmpty()) { ini.setValue(EcIni::INI_PROJECT_10, metadataPath); }
    ini.endGroup();

    ini.beginGroup(EcIni::INIGROUP_SCREEN_SETTINGS);
        //> Covariance maximization over the search window for every method
        //> but PWB, which detects with its own procedure.
        ini.setValue(EcIni::INI_SCREEN_SETTINGS_7, pwb ? TlagPwb : TlagOptimizer);
    ini.endGroup();

    ini.beginGroup(EcIni::INIGROUP_TIMELAG_OPT);
        ini.setValue(EcIni::INI_TIMELAG_OPT_2, 1);
        ini.setValue(EcIni::INI_TIMELAG_OPT_18, 0);
        ini.setValue(EcIni::INI_TIMELAG_OPT_21, 1);
        ini.setValue(QStringLiteral("to_dump_lags"), 1);
        if (!runWhole_)
        {
            //> One humidity class: water's lag is assessed as one population.
            //> Not for the entire dataset, whose optimisation file is meant
            //> for the project and so keeps its classes.
            ini.setValue(QStringLiteral("to_h2o_nclass"), 1);
            //> The preliminary assessment's own keys. Never in a user's
            //> project, and left out for the entire dataset, which the engine
            //> then walks in full.
            ini.setValue(QStringLiteral("to_sample_frac"), slider_->percent() / 100.0);
            ini.setValue(QStringLiteral("to_sample_min_n"), MinSampled);
            ini.setValue(QStringLiteral("to_sample_seed"), seed);
        }
        if (!pwb)
        {
            for (const auto& row : rows_)
            {
                const auto& w = runWindows_.value(row.record);
                const auto pfx = QStringLiteral("gas_%1_to_").arg(row.record);
                ini.setValue(pfx + QStringLiteral("min_lag"), QString::number(w.first, 'f', 1));
                ini.setValue(pfx + QStringLiteral("max_lag"), QString::number(w.second, 'f', 1));
            }
        }
    ini.endGroup();

    if (pwb)
    {
        ini.beginGroup(EcIni::INIGROUP_PWB_TIMELAG);
            for (const auto& row : rows_)
            {
                const auto& w = runWindows_.value(row.record);
                const auto pfx = QStringLiteral("gas_%1_pwb_").arg(row.record);
                ini.setValue(pfx + QStringLiteral("min_lag"), QString::number(w.first, 'f', 1));
                ini.setValue(pfx + QStringLiteral("max_lag"), QString::number(w.second, 'f', 1));
            }
        ini.endGroup();
    }
    ini.sync();
    if (ini.status() != QSettings::NoError)
    {
        *error = tr("Could not write a copy of the project.");
        return false;
    }
    return true;
}

void TimelagPreassessDialog::startEngine(const QString& projectPath)
{
    const QString workingDir = qApp->applicationDirPath() + QLatin1Char('/') + Defs::BIN_FILE_DIR;
    const QString enginePath = workingDir + QLatin1Char('/') + Defs::ENGINE_RP;

    delete engine_;
    engine_ = new Process(this);
    engine_->setChannelsMode(QProcess::MergedChannels);
    engine_->setReadChannels(QProcess::StandardOutput);
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("GFORTRAN_UNBUFFERED_PRECONNECTED"), QStringLiteral("y"));
    env.insert(QStringLiteral("GFORTRAN_SHOW_LOCUS"), QStringLiteral("n"));
    engine_->process()->setProcessEnvironment(env);
    connect(engine_, &Process::readyReadStdOut, this, &TimelagPreassessDialog::readEngineOutput);
    connect(engine_, &Process::processSuccess, this, &TimelagPreassessDialog::engineFinished);
    connect(engine_, &Process::processFailure, this, &TimelagPreassessDialog::engineFinished);

    //> Its own home folder: the engine's temporary files must not meet those
    //> of a run the main window starts.
    //>
    //> Always split across the cores ("-j 0"), whatever the main window's
    //> parallel pre-pass setting says: the assessment is waited for, and its
    //> answer does not depend on how the periods were shared out.
    const QStringList args{ QStringLiteral("-c"), QStringLiteral("gui"),
                            QStringLiteral("-s"), Defs::HOST_OS,
                            QStringLiteral("-e"), QDir::toNativeSeparators(workDir_->path() + QStringLiteral("/home")),
                            QStringLiteral("-j"), QStringLiteral("0"),
                            QDir::toNativeSeparators(projectPath) };

    progressBar_->setRange(0, 0);   // busy until the first progress line
    statusLabel_->setText(tr("Listing the raw files and reading the metadata..."));
    runClock_.start();
    setRunning(true);
    if (!engine_->engineProcessStart(enginePath, workingDir, args))
    {
        statusLabel_->setText(tr("The engine could not be started."));
        progressBar_->setRange(0, 1);
        progressBar_->setValue(0);
        setRunning(false);
    }
}

void TimelagPreassessDialog::readEngineOutput()
{
    if (!engine_) { return; }
    pendingOutput_ += engine_->readAllStdOut();
    int nl;
    while ((nl = pendingOutput_.indexOf('\n')) >= 0)
    {
        QByteArray line = pendingOutput_.left(nl);
        pendingOutput_.remove(0, nl + 1);
        if (line.endsWith('\r')) { line.chop(1); }

        QString tag;
        qint64 done = 0, total = 0;
        if (EngineProgress::parse(line, &tag, &done, &total))
        {
            if (tag == QLatin1String("to") && total > 0)
            {
                progressBar_->setRange(0, static_cast<int>(total));
                progressBar_->setValue(static_cast<int>(done));
                QString text = tr("Assessing %1 of %2 half-hours...").arg(done).arg(total);
                const qint64 ms = runClock_.elapsed();
                if (done > 0 && done < total)
                {
                    const qint64 left = ms * (total - done) / done / 1000;
                    text += QLatin1Char(' ')
                            + tr("About %1 min %2 s left.").arg(left / 60).arg(left % 60, 2, 10, QLatin1Char('0'));
                }
                statusLabel_->setText(text);
            }
            continue;
        }
        if (line.startsWith(QByteArrayLiteral("  Run plan:"))) { continue; }
        engineLog_->appendPlainText(QString::fromLocal8Bit(line));
    }
}

void TimelagPreassessDialog::engineFinished()
{
    if (!engine_) { return; }
    readEngineOutput();
    if (!pendingOutput_.isEmpty()) { engineLog_->appendPlainText(QString::fromLocal8Bit(pendingOutput_)); }
    pendingOutput_.clear();
    progressBar_->setRange(0, 1);
    const bool success = !cancelled_ && engine_->processExit() == Process::ExitStatus::Success;
    engine_->deleteLater();
    engine_ = nullptr;

    if (cancelled_)
    {
        progressBar_->setValue(0);
        statusLabel_->setText(tr("Cancelled."));
        histogram_->setMessage(tr("The assessment was cancelled."));
        setRunning(false);
        discardOutputs();
        return;
    }

    progressBar_->setValue(1);
    if (success) { loadResults(); }
    if (!success || table_.isEmpty())
    {
        progressBar_->setValue(0);
        statusLabel_->setText(success
            ? tr("The engine finished but detected no time lag. See the engine output for why.")
            : tr("The assessment failed. See the engine output for why."));
        histogram_->setMessage(tr("No time lags to show."));
        logToggle_->show();
        logToggle_->setChecked(true);
    }
    //> A run over every half-hour is a time lag pre-pass like any other, and
    //> leaves what one leaves, kept or not.
    if (success && runWhole_) { copyOptimisationFiles(); }
    setRunning(false);
    if (keepCheck_->isChecked()) { keepOutputs(); }
}

void TimelagPreassessDialog::loadResults()
{
    const QDir out(workDir_->path() + QStringLiteral("/out"));
    const auto files = out.entryInfoList({ QStringLiteral("*_tlag_preassess_*.csv") }, QDir::Files, QDir::Time);
    if (files.isEmpty()) { return; }
    QString error;
    table_ = LagStats::readTable(files.first().absoluteFilePath(), &error);
    if (table_.isEmpty()) { return; }

    QSet<QString> periods;
    for (const auto& r : table_) { periods.insert(r.date + r.time); }
    const qint64 secs = runClock_.elapsed() / 1000;
    statusLabel_->setText(tr("Done: %1 half-hours assessed in %2 min %3 s.")
                              .arg(periods.size()).arg(secs / 60).arg(secs % 60, 2, 10, QLatin1Char('0')));

    //> One chart per gas the table has, in the project's order.
    gasCombo_->blockSignals(true);
    gasCombo_->clear();
    for (const auto& row : rows_)
    {
        for (const auto& r : table_)
        {
            if (r.record == row.record)
            {
                gasCombo_->addItem(row.name, row.record);
                break;
            }
        }
    }
    gasCombo_->blockSignals(false);

    //> PWB judges no flux.
    const bool anyFlux = std::any_of(table_.cbegin(), table_.cend(),
                                     [](const LagStats::Row& r) { return r.hasFlux; });
    fluxCheck_->setEnabled(anyFlux);
    if (!anyFlux) { fluxCheck_->setChecked(false); }

    updateSuggestions();
    showGas(gasCombo_->currentIndex());
}

QVector<const LagStats::Row*> TimelagPreassessDialog::usableRows(int record, bool forSuggestion) const
{
    QVector<const LagStats::Row*> out;
    for (const auto& r : table_)
    {
        if (r.record != record) { continue; }
        if (fluxCheck_->isChecked() && !r.fluxOk) { continue; }
        if (forSuggestion && r.atEdge && !edgeCheck_->isChecked()) { continue; }
        out.append(&r);
    }
    return out;
}

void TimelagPreassessDialog::updateSuggestions()
{
    if (table_.isEmpty()) { return; }
    const double pgRange = ecProject_->timelagOptPgRange() > 0.0 ? ecProject_->timelagOptPgRange() : 1.5;
    const QSignalBlocker block(resultTable_);

    //> Every gas's own suggestion first: the remarks and the offers to
    //> borrow look across the gases of an analyser.
    for (int c = 0; c < rows_.size(); ++c)
    {
        auto& row = rows_[c];
        row.borrowedFrom = 0;
        QVector<double> lags;
        for (const auto* r : usableRows(row.record, true)) { lags.append(r->lag); }
        int total = 0;
        double searchLo = NaN, searchHi = NaN;
        for (const auto& r : table_)
        {
            if (r.record != row.record) { continue; }
            ++total;
            searchLo = r.windowMin;
            searchHi = r.windowMax;
        }

        auto p = LagStats::suggestFromPeak(lags, pgRange, row.closedPath, searchLo, searchHi);
        //> Nothing outside what was searched: a lag there was never looked for.
        if (p.ok && std::isfinite(searchLo) && std::isfinite(searchHi) && searchHi > searchLo)
        {
            p.minimum = std::max(p.minimum, searchLo);
            p.maximum = std::min(p.maximum, searchHi);
        }
        row.peak = p;
        row.suggestion = p.ok;
        setTextCell(resultTable_, ResultUsed, c,
                    p.ok ? tr("%1 of %2 (peak)").arg(p.used).arg(total)
                         : tr("%1 of %2").arg(lags.size()).arg(total));
        if (p.ok)
        {
            showSuggestion(c, p.nominal, p.minimum, p.maximum);
        }
        else
        {
            setTextCell(resultTable_, ResultNominal, c, QStringLiteral("-"));
            setTextCell(resultTable_, ResultMinimum, c, QStringLiteral("-"));
            setTextCell(resultTable_, ResultMaximum, c, QStringLiteral("-"));
        }
    }
    for (int c = 0; c < rows_.size(); ++c)
    {
        applyRemark(c);
        updateBorrowCell(c);
    }
    setButton_->setEnabled(std::any_of(rows_.cbegin(), rows_.cend(),
                                       [](const GasRow& r) { return r.suggestion; }));
}

void TimelagPreassessDialog::showSuggestion(int column, double nominal, double minimum, double maximum)
{
    setValueCell(resultTable_, ResultNominal, column, nominal, true);
    setValueCell(resultTable_, ResultMinimum, column, minimum, true);
    setValueCell(resultTable_, ResultMaximum, column, maximum, true);
}

QDateTime TimelagPreassessDialog::periodEnd(const LagStats::Row& row)
{
    //> Read as UTC so no daylight-saving shift is applied: the timestamps
    //> are shown exactly as the raw files have them.
    auto t = QDateTime::fromString(row.date + QLatin1Char(' ') + row.time, QStringLiteral("yyyy-MM-dd HH:mm"));
    t.setTimeZone(QTimeZone::UTC);
    return t;
}

QString TimelagPreassessDialog::nameOfRecord(int record) const
{
    for (const auto& row : rows_)
    {
        if (row.record == record) { return row.name; }
    }
    return QString();
}

int TimelagPreassessDialog::donorFor(int column) const
{
    //> Only a gas on the same analyser - the same intake tube, so the same
    //> lag - and never water vapour, whose lag grows with humidity; the
    //> engine's PWB borrows by the same rule (SameAnalyser).
    const auto& me = rows_.at(column);
    if (me.isWater || !MeasurementRecords::isRealInstrument(me.instrumentId)) { return -1; }
    int best = -1;
    for (int d = 0; d < rows_.size(); ++d)
    {
        const auto& other = rows_.at(d);
        if (d == column || other.isWater || other.instrumentId != me.instrumentId) { continue; }
        if (!other.peak.ok || !other.peak.clear || other.borrowedFrom != 0) { continue; }
        if (best < 0 || other.peak.share > rows_.at(best).peak.share) { best = d; }
    }
    return best;
}

void TimelagPreassessDialog::applyRemark(int column)
{
    auto& row = rows_[column];
    const auto& p = row.peak;
    row.note.clear();
    row.noteIsWarning = false;
    bool mentionTubeMate = false;

    if (row.borrowedFrom != 0)
    {
        row.note = tr("Borrowed from %1 on the same analyser; this gas showed no clear peak of its own.")
                       .arg(nameOfRecord(row.borrowedFrom));
    }
    else if (!p.ok)
    {
        row.note = p.total == 0 && std::none_of(table_.cbegin(), table_.cend(),
                                                [&row](const LagStats::Row& r) { return r.record == row.record; })
            ? tr("Not detected in any half-hour.")
            : tr("Too few usable lags (fewer than %1) to suggest anything.").arg(LagStats::MinLagsForSuggestion);
        row.noteIsWarning = true;
        mentionTubeMate = true;
    }
    else if (!p.clear)
    {
        row.note = tr("No clear peak: the lags spread over the search window.");
        row.noteIsWarning = true;
        mentionTubeMate = true;
    }
    else if (p.share < 0.5)
    {
        row.note = tr("Peak at %1 s, found by %2 of %3 half-hours (%4 %); the rest are scattered over the "
                      "search window.")
                       .arg(p.peak, 0, 'f', 1).arg(p.used).arg(p.total).arg(std::lround(100.0 * p.share));
        mentionTubeMate = true;
    }
    else if (row.isWater)
    {
        row.note = tr("Water vapour's lag grows with relative humidity; one window is a compromise.");
    }

    if (mentionTubeMate)
    {
        const int donor = donorFor(column);
        if (donor >= 0)
        {
            const auto& d = rows_.at(donor);
            row.note += QLatin1Char(' ')
                + tr("%1 on the same analyser: %2 s (%3 % of its half-hours).")
                      .arg(d.name)
                      .arg(cellValue(resultTable_, ResultNominal, donor), 0, 'f', 2)
                      .arg(std::lround(100.0 * d.peak.share));
        }
    }
    applyNote(column);
}

void TimelagPreassessDialog::updateBorrowCell(int column)
{
    resultTable_->removeCellWidget(ResultBorrow, column);
    const auto& row = rows_.at(column);
    const int donor = donorFor(column);
    //> Offered only where the gas has no clear peak of its own.
    const bool offer = donor >= 0 && (!row.peak.ok || !row.peak.clear);
    setTextCell(resultTable_, ResultBorrow, column,
                offer || row.borrowedFrom != 0 ? QString() : QStringLiteral("-"));
    if (auto item = resultTable_->item(ResultBorrow, column)) { item->setToolTip(row.note); }
    if (!offer && row.borrowedFrom == 0) { return; }

    auto button = new QPushButton(row.borrowedFrom != 0
                                      ? tr("Use own suggestion")
                                      : tr("Borrow from %1").arg(rows_.at(donor).name));
    button->setToolTip(row.borrowedFrom != 0
        ? tr("Put back this gas's own suggestion.")
        : tr("Fill this gas's nominal, minimum and maximum time lag with those of %1, measured by the "
             "same analyser and so through the same intake tube, which shows a clear peak.")
              .arg(rows_.at(donor).name));
    connect(button, &QPushButton::clicked, this, [this, column] { toggleBorrow(column); });
    resultTable_->setCellWidget(ResultBorrow, column, button);
}

void TimelagPreassessDialog::toggleBorrow(int column)
{
    auto& row = rows_[column];
    {
        const QSignalBlocker block(resultTable_);
        if (row.borrowedFrom != 0)
        {
            row.borrowedFrom = 0;
            row.suggestion = row.peak.ok;
            if (row.peak.ok)
            {
                showSuggestion(column, row.peak.nominal, row.peak.minimum, row.peak.maximum);
            }
            else
            {
                setTextCell(resultTable_, ResultNominal, column, QStringLiteral("-"));
                setTextCell(resultTable_, ResultMinimum, column, QStringLiteral("-"));
                setTextCell(resultTable_, ResultMaximum, column, QStringLiteral("-"));
            }
        }
        else
        {
            const int donor = donorFor(column);
            if (donor < 0) { return; }
            //> The donor's values as they stand, edits included.
            showSuggestion(column, cellValue(resultTable_, ResultNominal, donor),
                           cellValue(resultTable_, ResultMinimum, donor),
                           cellValue(resultTable_, ResultMaximum, donor));
            row.borrowedFrom = rows_.at(donor).record;
            row.suggestion = true;
        }
        applyRemark(column);
        updateBorrowCell(column);
    }
    setButton_->setEnabled(std::any_of(rows_.cbegin(), rows_.cend(),
                                       [](const GasRow& r) { return r.suggestion; }));
    selectGasColumn(column);
    redrawCurrent();
}

void TimelagPreassessDialog::showGas(int comboIndex)
{
    Q_UNUSED(comboIndex)
    //> Another gas's lags may lie anywhere; start from all of them.
    histogram_->resetZoom();
    redrawCurrent();
}

void TimelagPreassessDialog::selectGasColumn(int column)
{
    if (column < 0 || column >= rows_.size()) { return; }
    const int index = gasCombo_->findData(rows_.at(column).record);
    if (index >= 0) { gasCombo_->setCurrentIndex(index); }
}

void TimelagPreassessDialog::redrawCurrent()
{
    if (table_.isEmpty() || gasCombo_->currentIndex() < 0) { return; }
    const int record = gasCombo_->currentData().toInt();

    QVector<double> lags;
    QVector<bool> edge;
    QVector<QDateTime> times;
    int nEdge = 0;
    const LagStats::Row* any = nullptr;
    for (const auto* r : usableRows(record, false))
    {
        lags.append(r->lag);
        edge.append(r->atEdge);
        times.append(periodEnd(*r));
        nEdge += r->atEdge ? 1 : 0;
        any = r;
    }
    if (lags.isEmpty())
    {
        histogram_->setMessage(tr("No half-hour left to show for this gas."));
        histogramNote_->clear();
        return;
    }
    if (viewCombo_->currentIndex() == ViewTimeSeries)
    {
        histogram_->setTimeSeries(times, lags, edge, any->windowMin, any->windowMax);
    }
    else
    {
        histogram_->setHistogram(LagStats::bin(lags, edge, binSpin_->value()));
    }
    int column = -1;
    for (int c = 0; c < rows_.size(); ++c)
    {
        if (rows_.at(c).record == record) { column = c; }
    }
    if (column >= 0 && rows_.at(column).suggestion)
    {
        histogram_->setMarkers(cellValue(resultTable_, ResultNominal, column),
                               cellValue(resultTable_, ResultMinimum, column),
                               cellValue(resultTable_, ResultMaximum, column));
    }
    else
    {
        histogram_->setMarkers(NaN, NaN, NaN);
    }

    QString note = tr("%1 half-hours, searched from %2 s to %3 s.")
                       .arg(lags.size()).arg(any->windowMin).arg(any->windowMax);
    if (nEdge > 0)
    {
        note += QLatin1Char(' ') + tr("%1 of them at a window edge (grey): if many are, widen the search window and run again.")
                                       .arg(nEdge);
    }
    histogramNote_->setText(note);
}

void TimelagPreassessDialog::setInMetadata()
{
    const int meth = ecProject_->screenTlagMeth();
    const bool useMetadata = ecProject_->generalUseAltMdFile() && dlProject_ && dlProject_->variables();
    auto gases = ecProject_->gasColumns();
    QStringList done;
    bool windowChanged = false;

    //> A project reading this assessment's PWB table cannot take new PWB
    //> windows: the table records the windows it was detected with, and the
    //> engine refuses a table whose windows differ from the project's.
    bool pwbWindows = meth == TlagPwb;
    bool leftTable = false;
    if (pwbWindows && !optFileForProject_.isEmpty() && ecProject_->timelagOptMode() == 0
        && QFileInfo(ecProject_->timelagOptFile()) == QFileInfo(optFileForProject_))
    {
        pwbWindows = WidgetUtils::yesNoQuestion(this, tr("Set in Metadata"),
            tr("The project reads the PWB time lag table of this assessment."),
            tr("That table was detected with the search window, and the engine refuses it "
               "once the PWB windows differ. Set the suggested windows anyway, and let "
               "the project detect its time lags itself again? Answer No to write the "
               "metadata only and keep using the table."));
        leftTable = pwbWindows;
    }

    for (int c = 0; c < rows_.size(); ++c)
    {
        const auto& row = rows_.at(c);
        if (!row.suggestion) { continue; }
        const double nom = cellValue(resultTable_, ResultNominal, c);
        const double lo = cellValue(resultTable_, ResultMinimum, c);
        const double hi = cellValue(resultTable_, ResultMaximum, c);
        if (hi <= lo)
        {
            WidgetUtils::warning(this, tr("Set in Metadata"),
                                 tr("The window of %1 ends before it starts; nothing was written.").arg(row.name));
            return;
        }
        const int i = row.record - 1;
        if (i < 0 || i >= gases.size()) { continue; }

        if (useMetadata)
        {
            auto* vars = dlProject_->variables();
            const int index = gases.at(i).rawColumn - 1;
            if (index >= 0 && index < vars->size())
            {
                (*vars)[index].setNomTimelag(nom);
                (*vars)[index].setMinTimelag(lo);
                (*vars)[index].setMaxTimelag(hi);
            }
        }
        //> These methods search a window of their own, which would otherwise
        //> go on overriding what was just written.
        if (meth == TlagOptimizer)
        {
            gases[i].proc.toMinLag = floorTenth(lo);
            gases[i].proc.toMaxLag = ceilTenth(hi);
            windowChanged = true;
        }
        else if (pwbWindows)
        {
            gases[i].proc.pwbMinLag = floorTenth(lo);
            gases[i].proc.pwbMaxLag = ceilTenth(hi);
            windowChanged = true;
        }
        QString line = tr("%1: nominal %2 s, from %3 s to %4 s")
                           .arg(row.name, QString::number(nom, 'f', 2), QString::number(lo, 'f', 2),
                                QString::number(hi, 'f', 2));
        if (row.borrowedFrom != 0)
        {
            line += QLatin1Char(' ') + tr("(borrowed from %1)").arg(nameOfRecord(row.borrowedFrom));
        }
        done << line;
    }
    if (done.isEmpty()) { return; }

    if (useMetadata) { dlProject_->setModified(true); }
    if (windowChanged) { ecProject_->setGasColumns(gases); }
    if (leftTable) { ecProject_->setTimelagOptMode(1); }

    QString where;
    if (useMetadata)
    {
        where = tr("Written to the metadata.");
    }
    else
    {
        where = tr("The project reads its metadata from the raw files, so the metadata was not changed.");
    }
    if (meth == TlagOptimizer)
    {
        where += QLatin1Char(' ') + tr("The minimum and maximum are now also the automatic time lag optimization's search window.");
    }
    else if (pwbWindows)
    {
        where += QLatin1Char(' ') + tr("The minimum and maximum are now also the pre-whitening block-bootstrap search window.");
        if (leftTable)
        {
            where += QLatin1Char(' ') + tr("The project detects its time lags itself again.");
        }
    }
    else if (meth == TlagPwb)
    {
        where += QLatin1Char(' ') + tr("The PWB search windows were left as they were, so the project keeps using the table.");
    }
    else if (meth == TlagConstant)
    {
        where += QLatin1Char(' ') + tr("With constant time lags only the nominal lag is used.");
    }
    WidgetUtils::information(this, tr("Set in Metadata"), where, done.join(QLatin1Char('\n')));
}

QString TimelagPreassessDialog::outputBaseDir() const
{
    QString base = ecProject_->generalOutPath().trimmed();
    if (base.isEmpty() || RemoteSource::isRemote(base) || !QFileInfo(base).isDir())
    {
        base = QFileInfo(ecProject_->generalFileName()).absolutePath();
    }
    return QDir::cleanPath(base);
}

void TimelagPreassessDialog::copyOptimisationFiles()
{
    if (!workDir_) { return; }
    const QDir out(workDir_->path() + QStringLiteral("/out"));
    const bool pwb = runMethod_ == TlagPwb;
    //> What a normal time lag pre-pass leaves in the output folder.
    const QStringList patterns = pwb
        ? QStringList{ QStringLiteral("*_pwb_timelag_*.csv"), QStringLiteral("*_pwb_timelag_opt_*.txt") }
        : QStringList{ QStringLiteral("*_timelag_opt_*.txt") };
    const QString base = outputBaseDir();
    QDir().mkpath(base);

    QStringList copied;
    for (const auto& info : out.entryInfoList(patterns, QDir::Files, QDir::Name))
    {
        const QString dest = base + QLatin1Char('/') + info.fileName();
        if (QFile::exists(dest)) { QFile::remove(dest); }
        if (!QFile::copy(info.absoluteFilePath(), dest)) { continue; }
        copied << info.fileName();
        //> The file the project would read: the half-hourly table under PWB,
        //> the optimisation summary otherwise.
        const bool usable = pwb ? info.fileName().endsWith(QStringLiteral(".csv"))
                                : info.fileName().contains(QStringLiteral("_timelag_opt_"));
        if (usable) { optFileForProject_ = dest; }
    }
    if (copied.isEmpty())
    {
        statusLabel_->setText(statusLabel_->text() + QLatin1Char(' ')
            + tr("No time lag optimization file was written; see the engine output."));
        logToggle_->show();
        return;
    }
    statusLabel_->setText(statusLabel_->text() + QLatin1Char(' ')
        + tr("Time lag optimization file written to %1: %2.")
              .arg(QDir::toNativeSeparators(base), copied.join(QStringLiteral(", "))));

    //> Only the automatic optimization and PWB read such a file.
    const int meth = ecProject_->screenTlagMeth();
    if (!optFileForProject_.isEmpty() && (meth == TlagOptimizer || meth == TlagPwb) && meth == runMethod_)
    {
        useFileButton_->setToolTip(meth == TlagPwb
            ? tr("<b>Use time lag file in project:</b> the project's run reads the time lag of "
                 "every half-hour from this table instead of detecting it again. The PWB "
                 "search windows are set to the ones searched here, which the table was "
                 "detected with.")
            : tr("<b>Use time lag file in project:</b> the project's run reads the nominal "
                 "time lags and windows from this file instead of running its own time lag "
                 "optimization pre-pass."));
        useFileButton_->show();
    }
    else
    {
        useFileButton_->hide();
        statusLabel_->setText(statusLabel_->text() + QLatin1Char(' ')
            + tr("Only automatic time lag optimization and pre-whitening block-bootstrap read such a file."));
    }
}

void TimelagPreassessDialog::useFileInProject()
{
    if (optFileForProject_.isEmpty() || !QFileInfo::exists(optFileForProject_)) { return; }
    const int meth = ecProject_->screenTlagMeth();
    ecProject_->setTimelagOptMode(0);
    ecProject_->setTimelagOptFile(optFileForProject_);

    QString text = tr("The project now reads its time lags from %1.")
                       .arg(QDir::toNativeSeparators(optFileForProject_));
    if (meth == TlagPwb)
    {
        //> The table records the windows it was detected with, and the
        //> engine refuses it under any others.
        auto gases = ecProject_->gasColumns();
        for (auto it = runWindows_.cbegin(); it != runWindows_.cend(); ++it)
        {
            const int i = it.key() - 1;
            if (i < 0 || i >= gases.size()) { continue; }
            gases[i].proc.pwbMinLag = it.value().first;
            gases[i].proc.pwbMaxLag = it.value().second;
        }
        ecProject_->setGasColumns(gases);
        text += QLatin1Char(' ') + tr("Its PWB search windows are set to the ones searched here, "
                                      "which the table was detected with.");
    }
    WidgetUtils::information(this, tr("Use Time Lag File"), text);
}

void TimelagPreassessDialog::keepToggled(bool keep)
{
    if (keep && workDir_ && !(engine_ && engine_->isRunning())) { keepOutputs(); }
}

void TimelagPreassessDialog::keepOutputs()
{
    if (!workDir_) { return; }
    if (keptDir_.isEmpty())
    {
        const QString dest = outputBaseDir() + QStringLiteral("/tlag_preassessment_")
                             + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmm"));
        const QDir src(workDir_->path());
        QDirIterator it(src.path(), QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        bool ok = QDir().mkpath(dest);
        while (ok && it.hasNext())
        {
            const QString from = it.next();
            const QString to = dest + QLatin1Char('/') + src.relativeFilePath(from);
            ok = QDir().mkpath(QFileInfo(to).absolutePath()) && QFile::copy(from, to);
        }
        if (!ok)
        {
            statusLabel_->setText(statusLabel_->text() + QLatin1Char(' ')
                                  + tr("The outputs could not be kept in %1.").arg(QDir::toNativeSeparators(dest)));
            return;
        }
        keptDir_ = dest;
        statusLabel_->setText(statusLabel_->text() + QLatin1Char(' ')
                              + tr("Outputs kept in %1.").arg(QDir::toNativeSeparators(dest)));
    }
    saveFigures(keptDir_);
}

void TimelagPreassessDialog::saveFigures(const QString& dir)
{
    if (table_.isEmpty() || dir.isEmpty()) { return; }
    static const QRegularExpression unsafe(QStringLiteral("[^A-Za-z0-9_-]+"));

    //> Each chart as it stands in the window: the same bins, the same
    //> half-hours left out, the suggestion as edited.
    LagHistogramWidget figure;
    for (int c = 0; c < rows_.size(); ++c)
    {
        const auto& row = rows_.at(c);
        QVector<double> lags;
        QVector<bool> edge;
        QVector<QDateTime> times;
        double windowMin = NaN, windowMax = NaN;
        for (const auto* r : usableRows(row.record, false))
        {
            lags.append(r->lag);
            edge.append(r->atEdge);
            times.append(periodEnd(*r));
            windowMin = r->windowMin;
            windowMax = r->windowMax;
        }
        if (lags.isEmpty()) { continue; }
        if (row.suggestion)
        {
            figure.setMarkers(cellValue(resultTable_, ResultNominal, c),
                              cellValue(resultTable_, ResultMinimum, c),
                              cellValue(resultTable_, ResultMaximum, c));
        }
        else
        {
            figure.setMarkers(NaN, NaN, NaN);
        }
        figure.setTitle(tr("%1 - %2 half-hours").arg(row.name).arg(lags.size()));
        const QString name = QString(row.name).replace(unsafe, QStringLiteral("_")).toLower();
        //> Both views, whichever is on screen.
        figure.setHistogram(LagStats::bin(lags, edge, binSpin_->value()));
        figure.renderImage(FigureSize).save(dir + QLatin1Char('/') + name + QStringLiteral("_timelag_histogram.png"));
        figure.setTimeSeries(times, lags, edge, windowMin, windowMax);
        figure.renderImage(FigureSize).save(dir + QLatin1Char('/') + name + QStringLiteral("_timelag_timeseries.png"));
    }

    //> The suggestions as they stand, beside the charts.
    QFile file(dir + QStringLiteral("/suggested_timelags.csv"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) { return; }
    QTextStream csv(&file);
    csv << "gas,gas_record,lags_used,peak_share,nominal_s,minimum_s,maximum_s,borrowed_from,note\n";
    for (int c = 0; c < rows_.size(); ++c)
    {
        const auto& row = rows_.at(c);
        auto value = [&](int r) {
            return row.suggestion ? QString::number(cellValue(resultTable_, r, c), 'f', 2) : QString();
        };
        const auto* used = resultTable_->item(ResultUsed, c);
        csv << row.name << ',' << row.record << ',' << (used ? used->text() : QString()) << ','
            << (row.peak.ok ? QString::number(row.peak.share, 'f', 3) : QString()) << ','
            << value(ResultNominal) << ',' << value(ResultMinimum) << ',' << value(ResultMaximum) << ','
            << nameOfRecord(row.borrowedFrom) << ','
            << '"' << QString(row.note).replace(QLatin1Char('"'), QStringLiteral("\"\"")) << "\"\n";
    }
}

void TimelagPreassessDialog::discardOutputs()
{
    //> QTemporaryDir removes the folder and all in it.
    workDir_.reset();
}

void TimelagPreassessDialog::reject()
{
    if (engine_ && engine_->isRunning())
    {
        if (!WidgetUtils::yesNoQuestion(this, tr("Timelag Preliminary Assessment"),
                                        tr("The assessment is still running. Stop it and close?")))
        {
            return;
        }
        cancelled_ = true;
        engine_->processStop();
        engineFinished();
    }
    //> Kept again on closing, so the charts and suggestions saved are the
    //> ones the user ended with.
    if (keepCheck_->isChecked()) { keepOutputs(); }
    discardOutputs();
    QDialog::reject();
}
