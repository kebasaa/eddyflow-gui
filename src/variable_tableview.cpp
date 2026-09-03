/***************************************************************************
  variable_tableview.cpp
  -------------------
  Copyright © 2007-2011, Eco2s team, Antonio Forgione
  Copyright © 2011-2018, LI-COR Biosciences, Antonio Forgione
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

#include "variable_tableview.h"

#include <QDebug>
#include <QHeaderView>
#include <QMouseEvent>
#include <QScrollBar>

#include "customheader.h"

VariableTableView::VariableTableView(QWidget *parent) :
    QTableView(parent)
{
    auto hHeaderView = horizontalHeader();
    hHeaderView->show();
    connect(hHeaderView, &QHeaderView::sectionClicked,
            this, &VariableTableView::hHeaderClicked);

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    m_header = new CustomHeader(this);
    m_header->addSection(tr("Ignore"), tr("<b>Ignore:</b> Select <i>no</i> to tell EddyFlow that a column (variable) is not purely numeric. Purely numeric variables are strings included within two consecutive field separators and containing only digits from 0 to 9 and, at most, the decimal dot. Any other character makes a variable a <i>non-numeric</i> one. For example, time stamps in the form of 2011-09-26 or times as 23:20:562 are not numeric variables. Note that if a variable is not numeric, this must be specified even if you set <i>yes</i> in the ignore field."), CustomHeader::QuestionMarkHint::QuestionMark, ClickLabel::VarIgnoreDesc);
    m_header->addSection(tr("Numeric"), tr("<b>Numeric:</b> Select 'no' to tell EddyFlow that a column is not purely numeric. Purely numeric variables are strings included within two consecutive field separators and containing only digits from 0 to 9 and, at most, the decimal dot. Any other character makes a variable a not numeric one. For example, time stamps in the form of 2011-09-26 or times as 23:20:562 are not numeric variables. Note that if a variable is not numeric, this must be specified even if you set 'yes' in the ignore field."), CustomHeader::QuestionMarkHint::QuestionMark, ClickLabel::VarNumericDesc);
    m_header->addSection(tr("Variable"), tr("<b>Variable:</b> Specify the variable that is contained in the current column of the raw files (or position, for binary files). Purely numerical variables only contain numbers and the decimal dot (full stop)."), CustomHeader::QuestionMarkHint::QuestionMark, ClickLabel::VarDesc);
    m_header->addSection(tr("Instrument"), tr("<b>Instrument:</b> Select the instrument that measured the current variable. Instruments listed here are those entered under the instruments tab."));
    m_header->addSection(tr("Measurement type"), tr("<b>Measurement type:</b> Only applicable to gas concentrations. Enter the description of the concentration measurement (either <i>Molar/Mass density</i>, <i>Mole fraction (wet)</i>, or <i>Mixing ratio (dry)</i>). For all other variables, either leave the field blank or select <i>Other</i>. <i>Molar/Mass density</i> is a measure of mass per unit volume of air. <i>Mole fraction (wet)</i> is a measure of mass per mass of <b>wet</b> air. <i>Mixing ratio (dry)</i> is a measure of mass per mass of <b>dry</b> air. Measures of mass can be expressed as number of moles, grams, etc.<br><br><b>Watch the wet/dry basis.</b> Much of the literature says \"mole fraction\" for what is really the <i>dry</i> mole fraction - that is <i>Mixing ratio (dry)</i> here, not <i>Mole fraction (wet)</i>. The distinction changes the result: a mixing ratio is already on a dry-air basis, so no WPL correction is applied to it, whereas a mole fraction gets one."));
    m_header->addSection(tr("Input unit"), tr("<b>Input unit:</b> Specify the units of the variable as it is stored in the raw file."));
    m_header->addSection(tr("Linear scaling"), tr("<b>Linear scaling:</b> Specify whether to perform a linear conversion to rescale data. Variables that are already in any of the supported physical units do not need to be rescaled."), CustomHeader::QuestionMarkHint::QuestionMark, ClickLabel::VarConv);
    m_header->addSection(tr("Output unit"), tr("<b>Output units:</b> Enter the output units (physical units after conversion). The following <b><i>Gain</i></b> and <b><i>Offset</i></b> values must be such that the input variable is converted into the selected output unit."));
    m_header->addSection(tr("Gain value"), tr("<b>Gain value:</b> Enter the gain (angular coefficient) of the linear relation between input and output units."));
    m_header->addSection(tr("Offset value"), tr("<b>Offset value:</b> Enter the offset (y-axis intercept) of the linear relation between input and output units."));
    m_header->addSection(tr("<i>Nominal time lag</i>"), tr("<b>Nominal time lag:</b> Enter the expected (nominal) time lag of the variable, with respect to the measurements of the anemometer that you plan to use for flux computation, as applicable. Time lags should be specified at least for gas concentrations and can be estimated based on instrument separation (open path) or on the sampling line characteristics and the flow rate (closed path)."), CustomHeader::QuestionMarkHint::QuestionMark, ClickLabel::VarNomTLag);
    m_header->addSection(tr("<i>Minimum time lag</i>"), tr("<b>Minimum time lag:</b> Enter the minimum expected time lag for the current variable, with respect to anemometric measurements."), CustomHeader::QuestionMarkHint::QuestionMark, ClickLabel::VarMinTLag);
    m_header->addSection(tr("<i>Maximum time lag</i>"), tr("<b>Maximum time lag:</b> Enter the maximum expected time lag for the current variable, with respect to anemometric measurements."), CustomHeader::QuestionMarkHint::QuestionMark, ClickLabel::VarMaxTLag);
    m_header->addSection(tr("Error value"), tr("<b>Error value:</b> The value this column carries when the instrument had no reading. -9999 by default, which EddyFlow always treats as missing - along with NaN, an unparseable word such as NA, and an empty field - so this only needs changing when the logger writes something else. Stated in the same unit as the column itself, because it is recognised in the raw file before any conversion."));
    m_header->addSection(tr("Spectroscopic <i>a</i>"), tr("<b>Spectroscopic coefficient a:</b> Water vapour broadens the absorption lines a laser analyser measures, so what it reports scales as 1 + <i>a</i>&#183;&#967;<sub>q</sub> + <i>b</i>&#183;&#967;<sub>q</sub>&#178; with &#967;<sub>q</sub> the water mole fraction in mol/mol (Peltola et al., 2014). Zero, the default, means there is nothing to remove and leaves the column untouched. This is <b>not</b> the <i>Gain value</i> above, which is the linear calibration. Only used when the spectroscopic correction is switched on, under Processing Options, and only for closed-path analysers.<br><br><b>Spectroscopic only.</b> EddyUH folds the dilution into the same polynomial, so its <i>a</i> = &minus;1 means pure dilution; EddyFlow corrects the density separately and its identity is zero. Add one to an EddyUH or Rella (2010) value before entering it here."));
    m_header->addSection(tr("Spectroscopic <i>b</i>"), tr("<b>Spectroscopic coefficient b:</b> The quadratic term of the same water-broadening polynomial. Zero, the default, reduces it to a linear dependence on humidity, which is what most analysers are characterised for. This is <b>not</b> the <i>Offset value</i> above."));
}

VariableTableView::~VariableTableView()
{
    delete m_header;
}

//> The labels are a widget of their own beside the table, so nothing ties them
//> to the rows unless we do it here: every section is pinned to one row height
//> and the block starts at the viewport's top, where row 0 starts. It used to
//> start half a row higher, and the labels were left to size themselves.
void VariableTableView::layoutHeader()
{
    const int rowH = this->rowHeight(0);
    const int top = this->rowHeight(0) + 6;
    m_header->setSectionHeight(rowH);
    setViewportMargins(m_header->sizeHint().width(), top, 0, 0);
    m_header->setGeometry(0,
                          top,
                          m_header->sizeHint().width() + 10,
                          rowH * m_header->sectionCount());
}

//> Ask for room for every row. The vertical scroll bar is off, so a row that
//> does not fit is a row nobody can reach - which is how two rows added to
//> these tables went missing behind a hard-coded container height.
QSize VariableTableView::minimumSizeHint() const
{
    const int rows = model() ? model()->rowCount() : 0;
    int rowH = rowHeight(0);
    if (rowH <= 0) { rowH = verticalHeader()->defaultSectionSize(); }
    return {QTableView::minimumSizeHint().width(),
            rowH * (rows + 1) + 8 + 2 * frameWidth()};
}

QSize VariableTableView::sizeHint() const
{
    return minimumSizeHint();
}

void VariableTableView::resizeEvent(QResizeEvent *event)
{
    layoutHeader();
    horizontalHeader()->setMinimumWidth(horizontalHeader()->count() * horizontalHeader()->sectionSize(1));
    horizontalScrollBar()->setMaximum((horizontalHeader()->count() - 1) * horizontalHeader()->sectionSize(1));
    horizontalScrollBar()->updateGeometry();
    viewport()->update();
    QWidget::resizeEvent(event);
}

void VariableTableView::showEvent(QShowEvent *event)
{
    layoutHeader();
    horizontalHeader()->setMinimumWidth(horizontalHeader()->count() * horizontalHeader()->sectionSize(1));
    horizontalScrollBar()->setMaximum((horizontalHeader()->count() - 1) * horizontalHeader()->sectionSize(1));
    horizontalScrollBar()->updateGeometry();
    viewport()->update();
    QWidget::showEvent(event);
}

void VariableTableView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        QModelIndex item = indexAt(event->pos());

        if (!item.isValid())
        {
            reset();
        }
    }

    QAbstractItemView::mousePressEvent(event);
}

// NOTE: to finish http://www.hardcoded.net/articles/how-to-customize-qtableview-editing-behavior
// http://stackoverflow.com/questions/12380107/in-qtableview-what-signal-triggers-the-editing-mode
void VariableTableView::closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint)
{
    if (hint == QAbstractItemDelegate::NoHint)
    {
        QAbstractItemView::closeEditor(editor,
                                       QAbstractItemDelegate::SubmitModelCache);
    }

//    else if (hint == QAbstractItemDelegate::EditNextItem
//             || hint == QAbstractItemDelegate::EditPreviousItem)
//    {
//        int editableIndex;
//        if (hint == QAbstractItemDelegate::EditNextItem)
//        {
//            editableIndex = nextEditableIndex(currentIndex());
//        }
//        else
//        {
//            editableIndex = previousEditableIndex(currentIndex());
//        }

//        if (editableIndex == -1)
//        {
//            closeEditor(editor, QAbstractItemDelegate::SubmitModelCache);
//        }
//        else
//        {
//            closeEditor(editor, 0);
//            setCurrentIndex(editableIndex);
//            edit(editableIndex);
//        }
//    }

    else
    {
        QAbstractItemView::closeEditor(editor, hint);
    }
}

// NOTE: to finish
void VariableTableView::hHeaderClicked(int section)
{
    if (!model()) { return; }

    selectColumn(section);
    setCurrentIndex(model()->index(0, section));
}

//void VariableTableView::firstEditableIndex(const QModelIndex& originalIndex,
//                                           columnIndexes)
//{

//}

// Returns the first editable index at the left of `originalIndex` or None.
void VariableTableView::previousEditableIndex(const QModelIndex& originalIndex)
{
    Q_UNUSED(originalIndex)
//    auto h = horizontalHeader();
//    auto myCol = originalIndex.column();
//    columnIndexes = [h.logicalIndex(i) for i in range(h.count())];

//    // keep only columns before myCol
//    columnIndexes = columnIndexes[:columnIndexes.index(myCol)];

//    // We want the previous item, the columns have to be in reverse order
//    columnIndexes = reversed(columnIndexes);

//    return firstEditableIndex(originalIndex, columnIndexes);
}

void VariableTableView::nextEditableIndex(const QModelIndex& originalIndex)
{
    Q_UNUSED(originalIndex)
}
