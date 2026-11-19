/***************************************************************************
  layoutreport.cpp
  ----------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "layoutreport.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QGridLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextDocumentFragment>
#include <QTextStream>
#include <QToolBox>

#include <algorithm>

#include "advoutputoptions.h"
#include "advprocessingoptions.h"
#include "advsettingscontainer.h"
#include "advspectraloptions.h"
#include "advstatisticaloptions.h"

namespace
{
    /// What a widget shows, short, so a line of the report can be found on
    /// the page.
    QString describe(const QWidget* w)
    {
        QString text;
        if (auto b = qobject_cast<const QAbstractButton*>(w)) { text = b->text(); }
        else if (auto l = qobject_cast<const QLabel*>(w)) { text = l->text(); }
        else if (auto c = qobject_cast<const QComboBox*>(w))
        {
            //> The widest item, which is what a combo sizes itself to.
            int widest = -1;
            for (int i = 0; i < c->count(); ++i)
            {
                if (widest < 0 || c->itemText(i).size() > c->itemText(widest).size())
                {
                    widest = i;
                }
            }
            text = widest >= 0 ? QStringLiteral("[%1 items] ").arg(c->count())
                                     + c->itemText(widest)
                               : QString();
        }
        text = QTextDocumentFragment::fromHtml(text).toPlainText().simplified();
        if (text.size() > 70) { text = text.left(67) + QStringLiteral("..."); }
        QString name = QString::fromLatin1(w->metaObject()->className());
        if (!w->objectName().isEmpty()) { name += QLatin1Char('#') + w->objectName(); }
        return text.isEmpty() ? name : name + QStringLiteral(" \"") + text + QLatin1Char('"');
    }

    int minWidth(const QWidget* w)
    {
        return std::max(w->minimumSizeHint().width(), w->minimumWidth());
    }

    /// The widgets a layout is made of, rather than the frames around them:
    /// no visible child widget of their own, or a control whose children are
    /// its own internals.
    bool isLeaf(const QWidget* w)
    {
        if (qobject_cast<const QAbstractItemView*>(w)
            || qobject_cast<const QComboBox*>(w)
            || qobject_cast<const QAbstractSpinBox*>(w)
            || qobject_cast<const QLineEdit*>(w))
        {
            return true;
        }
        const auto kids = w->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        return std::none_of(kids.begin(), kids.end(),
                            [](const QWidget* k) { return k->isVisibleTo(k->parentWidget()); });
    }

    void settle()
    {
        for (int i = 0; i < 3; ++i)
        {
            QApplication::sendPostedEvents();
            QApplication::processEvents();
        }
    }

    void measure(QTextStream& out, const QString& title, QWidget* page)
    {
        settle();
        out << "\n== " << title << "\n";
        const auto areas = page->findChildren<QScrollArea*>();
        int worst = 0;
        for (auto area : areas)
        {
            if (!area->isVisible() || !area->widget()) { continue; }
            const int content = minWidth(area->widget());
            const int viewport = area->viewport()->width();
            const int over = area->horizontalScrollBar()->maximum();
            worst = std::max(worst, over);
            out << "  scroll area: content needs " << content << " px, viewport "
                << viewport << " px -> "
                << (over > 0 ? QStringLiteral("SCROLLS SIDEWAYS by %1 px").arg(over)
                             : QStringLiteral("fits"))
                << "\n";
        }
        out << "  verdict: " << (worst > 0 ? "OVERFLOWS" : "fits") << "\n";

        QList<QWidget*> leaves;
        for (auto w : page->findChildren<QWidget*>())
        {
            if (w->isVisible() && isLeaf(w)) { leaves << w; }
        }
        std::sort(leaves.begin(), leaves.end(),
                  [](const QWidget* a, const QWidget* b) { return minWidth(a) > minWidth(b); });
        out << "  widest widgets (minimum width):\n";
        for (int i = 0; i < leaves.size() && i < 15; ++i)
        {
            out << "    " << minWidth(leaves.at(i)) << "  " << describe(leaves.at(i)) << "\n";
        }

        QList<QLayout*> layouts;
        for (auto l : page->findChildren<QLayout*>())
        {
            if (l->parentWidget() && l->parentWidget()->isVisible()) { layouts << l; }
        }
        std::sort(layouts.begin(), layouts.end(),
                  [](QLayout* a, QLayout* b) { return a->minimumSize().width() > b->minimumSize().width(); });
        out << "  widest layouts (minimum width, first widget in it):\n";
        for (int i = 0; i < layouts.size() && i < 6; ++i)
        {
            QString first;
            for (int k = 0; k < layouts.at(i)->count(); ++k)
            {
                if (auto w = layouts.at(i)->itemAt(k)->widget()) { first = describe(w); break; }
            }
            out << "    " << layouts.at(i)->minimumSize().width() << "  "
                << layouts.at(i)->metaObject()->className() << "  " << first << "\n";
        }

        //> A grid is as wide as its columns added up, so the widest grids are
        //> broken down by column: how wide each is laid out, and the widest
        //> item that sits in that column alone - which is what sets it.
        int grids = 0;
        for (auto l : layouts)
        {
            auto grid = qobject_cast<QGridLayout*>(l);
            if (!grid || grids >= 3) { continue; }
            ++grids;
            out << "  columns of the grid starting with ";
            for (int k = 0; k < grid->count(); ++k)
            {
                if (auto w = grid->itemAt(k)->widget()) { out << describe(w); break; }
            }
            out << " (" << grid->minimumSize().width() << " px):\n";
            for (int c = 0; c < grid->columnCount(); ++c)
            {
                int widest = 0;
                QString what;
                for (int k = 0; k < grid->count(); ++k)
                {
                    int row = 0, col = 0, rowSpan = 0, colSpan = 0;
                    grid->getItemPosition(k, &row, &col, &rowSpan, &colSpan);
                    if (col != c || colSpan != 1) { continue; }
                    auto item = grid->itemAt(k);
                    if (item->isEmpty()) { continue; }
                    const int w = item->minimumSize().width();
                    if (w > widest)
                    {
                        widest = w;
                        what = item->widget() ? describe(item->widget())
                                              : QStringLiteral("(layout, row %1)").arg(row);
                    }
                }
                out << "    col " << c << ": laid out " << grid->cellRect(0, c).width()
                    << " px, min " << grid->columnMinimumWidth(c)
                    << ", widest alone " << widest << "  " << what << "\n";
            }
        }
    }
}

namespace LayoutReport
{

bool write(const QString& path, QWidget* window, AdvSettingsContainer* pages)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) { return false; }
    QTextStream out(&file);
    out << "EddyFlow layout report. Window client width " << window->width()
        << " px (logical).\n";

    struct Page { const char* title; QWidget* widget; };
    const Page all[] = {
        { "Processing Options", pages->processingOptions() },
        { "Statistical Analysis", pages->statisticalOptions() },
        { "Spectral Corrections", pages->spectralOptions() },
        { "Output Files", pages->outputOptions() },
    };
    for (int index = 0; index < 4; ++index)
    {
        pages->setCurrentPage(index);
        settle();
        for (const auto& page : all)
        {
            if (!page.widget || !page.widget->isVisible()) { continue; }
            const auto boxes = page.widget->findChildren<QToolBox*>();
            if (boxes.isEmpty())
            {
                measure(out, QString::fromLatin1(page.title), page.widget);
                continue;
            }
            //> A tool box lays out only the item that is open.
            for (auto box : boxes)
            {
                const int was = box->currentIndex();
                for (int item = 0; item < box->count(); ++item)
                {
                    box->setCurrentIndex(item);
                    measure(out, QStringLiteral("%1 / %2").arg(QString::fromLatin1(page.title),
                                                               box->itemText(item).remove(QLatin1Char('&'))),
                            page.widget);
                }
                box->setCurrentIndex(was);
            }
        }
    }
    return true;
}

}  // namespace LayoutReport
