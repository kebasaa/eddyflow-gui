/***************************************************************************
  anem_tableview.cpp
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

#include "anem_tableview.h"

#include <QHeaderView>
#include <QMouseEvent>
#include <QScrollBar>

#include "clicklabel.h"
#include "customheader.h"

AnemTableView::AnemTableView(QWidget *parent) :
    QTableView(parent)
{
    horizontalHeader()->show();
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    m_header = new CustomHeader(this);
    m_header->addSection(tr("Manufacturer"), tr("<b>Manufacturer:</b> Specify the manufacturer of the anemometer among those supported. Choose <i>Other</i> for any manufacturer not explicitly listed. This field is mandatory."));
    m_header->addSection(tr("Model"), tr("<b>Model:</b> Identify the model of the anemometer. Choose <i>Generic Anemometer</i> for any model not explicitly listed. This field is mandatory."));
    m_header->addSection(tr("Embedded software version"), tr("<b>Embedded software version:</b> Identify the embedded software (firmware) version that was running on the selected anemometer. For Gill WindMaster and WindMaster Pro models, the firmware version is required in order to select the proper angle of attack correction. Storing other anemometers' firmware version is recommended for good recordkeeping."), CustomHeader::QuestionMarkHint::QuestionMark, ClickLabel::AnemSwVersion);
    m_header->addSection(tr("<i>Instrument ID</i>"), tr("<b>Instrument ID:</b> Enter an ID for the anemometer, to distinguish it from your other instruments. This is only for your records and providing it is optional."));
    m_header->addSection(tr("Height"), tr("<b> Height:</b> Enter the distance between the ground and the center of the device sampling volume. This field is mandatory."));
    m_header->addSection(tr("Wind data format"), tr("<b>Wind data format:</b> Specify the format in which the wind data are provided."));
    m_header->addSection(tr("North alignment"), tr("<b>North alignment:</b> Specify whether the anemometer\'s axes are aligned to transducers (<i>Axis</i>) or spars (<i>Spars</i>). For Gill anemometers only."), CustomHeader::QuestionMarkHint::QuestionMark, ClickLabel::AnemNAlign);
    m_header->addSection(tr("North off-set"), tr("<b>North offset:</b> Enter the anemometer\'s yaw offset with respect to local magnetic north (the one you assess with the compass), positive eastward."), CustomHeader::QuestionMarkHint::QuestionMark, ClickLabel::AnemNOffset);
    m_header->addSection(tr("Northward separation"), tr("<b>Northward separation:</b> Specify the distance between the current anemometer and the reference anemometer, as measured horizontally along the magnetic north-south axis (the one you assess with the compass). The distance is positive if the current anemometer is placed to the north of the reference anemometer. The reference anemometer is the first one you describe. For this anemometer you cannot enter the separation and you find the string Reference."), CustomHeader::QuestionMarkHint::QuestionMark, ClickLabel::AnemNSep);
    m_header->addSection(tr("Eastward separation"), tr("<b>Eastward separation:</b> Specify the distance between the current anemometer and reference anemometer, as measured horizontally along the east-west axis (the one you assess with the compass). The distance is positive if the current anemometer is placed to the east of the reference anemometer. The reference anemometer is the first one you describe. For this anemometer you cannot enter the separation and you find the string Reference."), CustomHeader::QuestionMarkHint::QuestionMark, ClickLabel::AnemESep);
    m_header->addSection(tr("Vertical separation"), tr("<b>Vertical separation:</b> Specify the distance between the current anemometer and the reference anemometer, as measured along the vertical axis. The distance is positive if the current anemometer is placed above the reference anemometer. The reference anemometer is the first one you describe. For this anemometer you cannot enter the separation and you find the string <i>Reference</i>."), CustomHeader::QuestionMarkHint::QuestionMark, ClickLabel::AnemVSep);
    m_header->addSection(tr("Longitudinal path length"), tr("<b>Longitudinal path length:</b> Path length in the direction defined by a pair of transducers. Consult the anemometer\'s specifications or user manual."));
    m_header->addSection(tr("Transversal path length"), tr("<b>Transversal path length:</b> Path length in the direction orthogonal to the longitudinal path length of the anemometer (e.g., as defined by the diameter of transducers)."));
    m_header->addSection(tr("Time response"), tr("<b>Time response:</b> Time response of the anemometer. Its inverse defines the maximum frequency of the atmospheric turbulence that the instrument is able to resolve. Consult the anemometer\'s specifications or user manual."));
    m_header->addSection(tr("Acquisition frequency"), tr("<b>Acquisition frequency:</b> The rate at which this instrument samples, if it is slower than the station's acquisition frequency. Leave it at the station's value unless the instrument really is slower: an instrument that has not been given a rate of its own follows the station, so changing the station's frequency changes this one too. A slower instrument leaves gaps between its samples in the raw file, and stating its rate is what stops those gaps being counted as missing data."));
    m_header->addSection(tr("Sampling"), tr("<b>Sampling:</b> Whether this instrument reports the value at an instant or the mean over its own sampling interval. It matters only when the instrument is slower than the station: the vertical wind is then paired with each of its samples the same way - one w at that instant, or w averaged over the interval the sample closes. Pairing an averaged wind against a point-sampled gas biases the covariance, so <i>Instantaneous</i> is the default and the average is stated deliberately."));

    verticalHeader()->hide();
}

AnemTableView::~AnemTableView()
{
    delete m_header;
}

//> The labels are a widget of their own beside the table, so nothing ties them
//> to the rows unless we do it here: every section is pinned to one row height
//> and the block starts at the viewport's top, where row 0 starts. It used to
//> start half a row higher, and the labels were left to size themselves.
void AnemTableView::layoutHeader()
{
    const int rowH = rowHeight(0);
    const int top = rowHeight(0) + 2;
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
QSize AnemTableView::minimumSizeHint() const
{
    const int rows = model() ? model()->rowCount() : 0;
    int rowH = rowHeight(0);
    if (rowH <= 0) { rowH = verticalHeader()->defaultSectionSize(); }
    return {QTableView::minimumSizeHint().width(),
            rowH * (rows + 1) + 8 + 2 * frameWidth()};
}

QSize AnemTableView::sizeHint() const
{
    return minimumSizeHint();
}

void AnemTableView::resizeEvent(QResizeEvent *event)
{
    layoutHeader();
    horizontalHeader()->setMinimumWidth(horizontalHeader()->count() * horizontalHeader()->sectionSize(1));
    horizontalScrollBar()->setMaximum((horizontalHeader()->count() - 1) * horizontalHeader()->sectionSize(1));
    horizontalScrollBar()->updateGeometry();
    viewport()->update();
    QWidget::resizeEvent(event);
}

void AnemTableView::showEvent(QShowEvent *event)
{
    layoutHeader();

    horizontalHeader()->setMinimumWidth(horizontalHeader()->count() * horizontalHeader()->sectionSize(1));
    horizontalScrollBar()->setMaximum((horizontalHeader()->count() - 1) * horizontalHeader()->sectionSize(1));
    horizontalScrollBar()->updateGeometry();
    viewport()->update();
    QWidget::showEvent(event);
}

void AnemTableView::mousePressEvent(QMouseEvent *event)
{
    QModelIndex item = indexAt(event->pos());
    if (!item.isValid())
    {
        reset();
    }

    QAbstractItemView::mousePressEvent(event);
}
