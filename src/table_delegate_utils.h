/***************************************************************************
  table_delegate_utils.h
  -------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller

  This file is part of EddyFlow.
****************************************************************************/

#ifndef TABLE_DELEGATE_UTILS_H
#define TABLE_DELEGATE_UTILS_H

#include <QAbstractItemView>
#include <QApplication>
#include <QBrush>
#include <QComboBox>
#include <QPainter>
#include <QPointer>
#include <QStyle>
#include <QStyleOptionComboBox>
#include <QStyleOptionViewItem>
#include <QTimer>

namespace TableDelegateUtils {

inline QColor textColor(const QPalette& palette, bool selected = false, bool enabled = true)
{
    if (!enabled)
    {
        return palette.color(QPalette::Disabled, QPalette::Text);
    }
    return selected ? palette.color(QPalette::HighlightedText)
                    : palette.color(QPalette::Text);
}

inline QColor baseColor(const QPalette& palette, bool selected = false)
{
    return selected ? palette.color(QPalette::Highlight)
                    : palette.color(QPalette::Base);
}

inline void applyCellPalette(QPalette* palette, bool selected, bool enabled)
{
    if (!palette) { return; }

    const QColor bg = baseColor(*palette, selected);
    const QColor fg = textColor(*palette, selected, enabled);
    const QPalette::ColorGroup group = enabled ? QPalette::Active : QPalette::Disabled;

    palette->setColor(group, QPalette::Base, bg);
    palette->setColor(group, QPalette::Button, bg);
    palette->setColor(group, QPalette::Window, bg);
    palette->setColor(group, QPalette::Text, fg);
    palette->setColor(group, QPalette::ButtonText, fg);
    palette->setColor(group, QPalette::WindowText, fg);
    palette->setColor(group, QPalette::HighlightedText, textColor(*palette, true, enabled));
}

inline void paintComboCell(QPainter* painter,
                           const QStyleOptionViewItem& option,
                           const QModelIndex& index)
{
    const bool selected = option.state & QStyle::State_Selected;
    const bool enabled = option.state & QStyle::State_Enabled;

    QStyleOptionComboBox comboOption;
    comboOption.rect = option.rect.adjusted(2, 1, -2, -1);
    comboOption.currentText = index.data(Qt::DisplayRole).toString();
    comboOption.state = option.state;
    comboOption.state |= QStyle::State_Enabled;
    comboOption.palette = option.palette;
    applyCellPalette(&comboOption.palette, selected, enabled);

    painter->fillRect(option.rect, baseColor(comboOption.palette, selected));

    auto style = option.widget ? option.widget->style() : QApplication::style();
    style->drawComplexControl(QStyle::CC_ComboBox, &comboOption, painter, option.widget);
    style->drawControl(QStyle::CE_ComboBoxLabel, &comboOption, painter, option.widget);
}

inline void prepareSelectedOption(QStyleOptionViewItem* selectedOption, bool enabled)
{
    if (!selectedOption) { return; }

    applyCellPalette(&selectedOption->palette, true, enabled);
    selectedOption->state &= ~QStyle::State_Selected;
    selectedOption->backgroundBrush = QBrush(baseColor(selectedOption->palette, true));
}

inline void prepareComboEditor(QComboBox* combo, QWidget* parent)
{
    if (!combo) { return; }

    QPalette palette = parent ? parent->palette() : QApplication::palette();
    applyCellPalette(&palette, false, combo->isEnabled());
    combo->setPalette(palette);
    combo->setAutoFillBackground(true);
    if (combo->view())
    {
        combo->view()->setPalette(palette);
        combo->view()->viewport()->setAutoFillBackground(true);
        combo->view()->viewport()->setPalette(palette);
    }
}

inline void showPopupQueued(QComboBox* combo)
{
    QPointer<QComboBox> guarded(combo);
    QTimer::singleShot(0, combo, [guarded]() {
        if (guarded)
        {
            guarded->showPopup();
        }
    });
}

} // namespace TableDelegateUtils

#endif // TABLE_DELEGATE_UTILS_H
