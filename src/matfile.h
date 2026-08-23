/***************************************************************************
  matfile.h
  -------------------
  A read-only reader for MATLAB level 5 (.mat, "v7") files
  -------------------
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

#ifndef MATFILE_H
#define MATFILE_H

#include <QByteArray>
#include <QMap>
#include <QSharedDataPointer>
#include <QString>
#include <QStringList>
#include <QVector>

///
/// \class MatValue
/// \brief One MATLAB variable, as a value tree.
///
/// Enough of the format to read a settings file, and no more. There is no
/// writer, no complex arithmetic, no sparse support and no MATLAB object
/// support - an EddyUH project holds numbers, strings, cells and structs, and
/// anything else in one is a file this was not written for.
///
/// Every integer class is promoted to double on the way in. MATLAB stores a
/// whole number in the narrowest type that fits, so the same field is uint8 in
/// one project and double in another purely because of the value it happens to
/// hold. Reading the width as if it meant something is how a lag in seconds
/// gets mistaken for a lag in samples.
///
class MatValue
{
public:
    enum class Type
    {
        Empty,   ///< absent, or a 0x0 array - MATLAB writes [] for both
        Number,  ///< any numeric or logical class, promoted to double
        Text,    ///< a char array, one entry per row
        Cell,    ///< a cell array, in column-major order
        Struct   ///< a struct, or a struct array with more than one element
    };

    MatValue();
    MatValue(const MatValue& other);
    MatValue& operator=(const MatValue& other);
    ~MatValue();

    Type type() const;
    bool isEmpty() const;

    int rows() const;
    int columns() const;
    /// Elements in the array - for a struct array, how many structs.
    int count() const;

    /// Every number, in MATLAB's column-major order. Empty for other types.
    QVector<double> numbers() const;
    /// The first number, or \a fallback when there is none.
    double toDouble(double fallback = 0.0) const;
    /// The first number rounded, or \a fallback when there is none.
    int toInt(int fallback = 0) const;

    /// A char array's rows. A one-row array is the usual case.
    QStringList lines() const;
    /// The first row of a char array, trimmed. Also unwraps a 1x1 cell
    /// holding a char array, which is how EddyUH stores several of its
    /// names - set_sonic.Name is a cell, set_Gan.Name is not.
    QString toString() const;

    /// Element \a i of a cell or struct array. Out of range gives Empty.
    MatValue at(int i) const;
    /// Field \a name of element \a i. Absent gives Empty.
    MatValue field(const QString& name, int i = 0) const;
    /// True when element \a i has a field called \a name.
    bool hasField(const QString& name, int i = 0) const;
    /// The field names of element \a i, in the order MATLAB stored them.
    QStringList fieldNames(int i = 0) const;

private:
    friend class MatFileReader;
    class Data;
    QSharedDataPointer<Data> d;
};

///
/// \class MatFile
/// \brief The top-level variables of one level 5 MAT file.
///
/// Handles the v7 flavour, in which each top-level variable is a separately
/// zlib-compressed element. v7.3 is HDF5 and is refused by name rather than
/// misread.
///
class MatFile
{
public:
    MatFile();

    /// Reads \a path whole. Returns false and sets \a error on any problem;
    /// a partially read file is discarded rather than half-offered.
    bool read(const QString& path, QString* error = nullptr);

    /// The header text MATLAB wrote, which names the platform and the date.
    QString description() const { return description_; }

    QStringList names() const { return order_; }
    bool contains(const QString& name) const { return vars_.contains(name); }
    /// The named variable, or an Empty value when there is none.
    MatValue value(const QString& name) const { return vars_.value(name); }

private:
    QString description_;
    QStringList order_;
    QMap<QString, MatValue> vars_;
};

#endif  // MATFILE_H
