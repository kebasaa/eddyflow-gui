/***************************************************************************
  matfile.cpp
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

#include "matfile.h"

#include <QFile>
#include <QObject>
#include <QSharedData>
#include <QtEndian>

#include <zlib.h>

#include <cmath>
#include <cstring>
#include <limits>

namespace {

//> Data element types, from the level 5 MAT-file format specification.
enum MiType
{
    miINT8 = 1,
    miUINT8 = 2,
    miINT16 = 3,
    miUINT16 = 4,
    miINT32 = 5,
    miUINT32 = 6,
    miSINGLE = 7,
    miDOUBLE = 9,
    miINT64 = 12,
    miUINT64 = 13,
    miMATRIX = 14,
    miCOMPRESSED = 15,
    miUTF8 = 16,
    miUTF16 = 17,
    miUTF32 = 18
};

//> Array classes.
enum MxClass
{
    mxCELL = 1,
    mxSTRUCT = 2,
    mxOBJECT = 3,
    mxCHAR = 4,
    mxSPARSE = 5,
    mxDOUBLE = 6,
    mxSINGLE = 7,
    mxINT8 = 8,
    mxUINT8 = 9,
    mxINT16 = 10,
    mxUINT16 = 11,
    mxINT32 = 12,
    mxUINT32 = 13,
    mxINT64 = 14,
    mxUINT64 = 15
};

//> A whole file inflated into memory would be unbounded if the stream lied
//> about its size, so the inflate loop stops here. An EddyUH project is a
//> few kilobytes; anything approaching this is not one.
const qint64 kMaxInflated = 256LL * 1024 * 1024;

int elementSize(quint32 type)
{
    switch (type)
    {
    case miINT8:
    case miUINT8:
    case miUTF8:
        return 1;
    case miINT16:
    case miUINT16:
    case miUTF16:
        return 2;
    case miINT32:
    case miUINT32:
    case miSINGLE:
    case miUTF32:
        return 4;
    case miDOUBLE:
    case miINT64:
    case miUINT64:
        return 8;
    default:
        return 0;
    }
}

}  // namespace

///
/// The value tree's payload. One class for every kind, because the kinds are
/// few and a union of five small members costs less than the machinery to
/// keep them apart.
///
class MatValue::Data : public QSharedData
{
public:
    MatValue::Type type = MatValue::Type::Empty;
    int rows = 0;
    int columns = 0;
    QVector<double> numbers;
    QStringList lines;
    QVector<MatValue> cells;
    //> One map per element of a struct array, plus the field order MATLAB
    //> wrote, which a caller reporting what it found wants to preserve.
    QVector<QMap<QString, MatValue>> structs;
    QStringList fieldOrder;
};

MatValue::MatValue() : d(new Data) {}
MatValue::MatValue(const MatValue& other) = default;
MatValue& MatValue::operator=(const MatValue& other) = default;
MatValue::~MatValue() = default;

MatValue::Type MatValue::type() const { return d->type; }

bool MatValue::isEmpty() const
{
    switch (d->type)
    {
    case Type::Empty:
        return true;
    case Type::Number:
        return d->numbers.isEmpty();
    case Type::Text:
        return d->lines.isEmpty();
    case Type::Cell:
        return d->cells.isEmpty();
    case Type::Struct:
        return d->structs.isEmpty();
    }
    return true;
}

int MatValue::rows() const { return d->rows; }
int MatValue::columns() const { return d->columns; }

int MatValue::count() const
{
    switch (d->type)
    {
    case Type::Number:
        return d->numbers.size();
    case Type::Text:
        return d->lines.size();
    case Type::Cell:
        return d->cells.size();
    case Type::Struct:
        return d->structs.size();
    default:
        return 0;
    }
}

QVector<double> MatValue::numbers() const
{
    return d->type == Type::Number ? d->numbers : QVector<double>();
}

double MatValue::toDouble(double fallback) const
{
    if (d->type == Type::Number && !d->numbers.isEmpty())
    {
        return d->numbers.first();
    }
    //> A 1x1 cell holding one number, which is how EddyUH stores a planar-fit
    //> coefficient: fitcoeff.nobin is a 1x3 cell of 1x1 doubles.
    if (d->type == Type::Cell && d->cells.size() == 1)
    {
        return d->cells.first().toDouble(fallback);
    }
    return fallback;
}

int MatValue::toInt(int fallback) const
{
    const double v = toDouble(std::numeric_limits<double>::quiet_NaN());
    if (std::isnan(v))
    {
        return fallback;
    }
    return static_cast<int>(std::lround(v));
}

QStringList MatValue::lines() const
{
    return d->type == Type::Text ? d->lines : QStringList();
}

QString MatValue::toString() const
{
    if (d->type == Type::Text)
    {
        return d->lines.isEmpty() ? QString() : d->lines.first().trimmed();
    }
    //> EddyUH is inconsistent about this: set_sonic.Name is a 1x1 cell around
    //> a char array while set_Gan.Name is a char array, and both are read
    //> here by the same call.
    if (d->type == Type::Cell && d->cells.size() == 1)
    {
        return d->cells.first().toString();
    }
    return QString();
}

MatValue MatValue::at(int i) const
{
    if (d->type == Type::Cell && i >= 0 && i < d->cells.size())
    {
        return d->cells.at(i);
    }
    if (d->type == Type::Struct && i >= 0 && i < d->structs.size())
    {
        //> A struct-array element is itself a one-element struct.
        MatValue v;
        v.d->type = Type::Struct;
        v.d->rows = 1;
        v.d->columns = 1;
        v.d->structs.append(d->structs.at(i));
        v.d->fieldOrder = d->fieldOrder;
        return v;
    }
    return MatValue();
}

MatValue MatValue::field(const QString& name, int i) const
{
    if (d->type != Type::Struct || i < 0 || i >= d->structs.size())
    {
        return MatValue();
    }
    return d->structs.at(i).value(name);
}

bool MatValue::hasField(const QString& name, int i) const
{
    if (d->type != Type::Struct || i < 0 || i >= d->structs.size())
    {
        return false;
    }
    return d->structs.at(i).contains(name);
}

QStringList MatValue::fieldNames(int i) const
{
    if (d->type != Type::Struct || i < 0 || i >= d->structs.size())
    {
        return QStringList();
    }
    //> The stored order, filtered to what this element actually has.
    QStringList out;
    for (const auto& n : d->fieldOrder)
    {
        if (d->structs.at(i).contains(n))
        {
            out.append(n);
        }
    }
    return out;
}

///
/// \class MatFileReader
/// \brief The parser proper, kept out of the header.
///
/// Every read is bounds-checked against the buffer it is reading from, and
/// the first failure stops the file. A settings file is small and read once,
/// so there is nothing to gain by being clever about it and a great deal to
/// lose: this parses a binary format from outside the program.
///
class MatFileReader
{
public:
    QString error;

    bool parseTop(const QByteArray& buf, QStringList* order,
                  QMap<QString, MatValue>* out);

private:
    struct Tag
    {
        quint32 type = 0;
        quint32 bytes = 0;
        int headerBytes = 8;  //> 4 for the small-data-element form
    };

    bool readTag(const QByteArray& b, int at, Tag* tag);
    bool readMatrix(const QByteArray& b, int at, int bytes, QString* name,
                    MatValue* out);
    bool readNumeric(const QByteArray& b, int at, int limit,
                     QVector<double>* out, quint32* typeOut);
    bool fail(const QString& why);
};

bool MatFileReader::fail(const QString& why)
{
    if (error.isEmpty())
    {
        error = why;
    }
    return false;
}

bool MatFileReader::readTag(const QByteArray& b, int at, Tag* tag)
{
    if (at < 0 || at + 8 > b.size())
    {
        return fail(QObject::tr("the file ends in the middle of a tag"));
    }
    const auto* p = reinterpret_cast<const uchar*>(b.constData()) + at;
    const quint32 first = qFromLittleEndian<quint32>(p);
    //> The small data element form: up to four bytes of payload packed into
    //> the tag itself, with the byte count in the upper half-word. Missing
    //> this reads a field name as a type code.
    if ((first >> 16) != 0)
    {
        tag->type = first & 0xffff;
        tag->bytes = first >> 16;
        tag->headerBytes = 4;
        if (tag->bytes > 4)
        {
            return fail(QObject::tr("a compact tag claims more than four "
                                    "bytes of data"));
        }
        return true;
    }
    tag->type = first;
    tag->bytes = qFromLittleEndian<quint32>(p + 4);
    tag->headerBytes = 8;
    if (at + 8 + static_cast<qint64>(tag->bytes) > b.size())
    {
        return fail(QObject::tr("an element claims more bytes than the file "
                                "holds"));
    }
    return true;
}

bool MatFileReader::readNumeric(const QByteArray& b, int at, int limit,
                                QVector<double>* out, quint32* typeOut)
{
    Tag tag;
    if (!readTag(b, at, &tag))
    {
        return false;
    }
    const int start = at + tag.headerBytes;
    if (start + static_cast<qint64>(tag.bytes) > limit)
    {
        return fail(QObject::tr("a data element runs past its parent"));
    }
    if (typeOut)
    {
        *typeOut = tag.type;
    }
    const int width = elementSize(tag.type);
    if (width == 0)
    {
        return fail(QObject::tr("unsupported data type %1").arg(tag.type));
    }
    const int n = static_cast<int>(tag.bytes) / width;
    out->clear();
    out->reserve(n);
    const auto* p = reinterpret_cast<const uchar*>(b.constData()) + start;
    for (int i = 0; i < n; ++i)
    {
        const uchar* q = p + static_cast<qint64>(i) * width;
        switch (tag.type)
        {
        case miINT8:
            out->append(static_cast<double>(static_cast<qint8>(*q)));
            break;
        case miUINT8:
        case miUTF8:
            out->append(static_cast<double>(*q));
            break;
        case miINT16:
            out->append(qFromLittleEndian<qint16>(q));
            break;
        case miUINT16:
        case miUTF16:
            out->append(qFromLittleEndian<quint16>(q));
            break;
        case miINT32:
            out->append(qFromLittleEndian<qint32>(q));
            break;
        case miUINT32:
        case miUTF32:
            out->append(qFromLittleEndian<quint32>(q));
            break;
        case miINT64:
            out->append(static_cast<double>(qFromLittleEndian<qint64>(q)));
            break;
        case miUINT64:
            out->append(static_cast<double>(qFromLittleEndian<quint64>(q)));
            break;
        case miSINGLE:
        {
            //> No reinterpret_cast between float and its bytes: the union is
            //> undefined and memcpy is what compilers optimise to a move.
            const quint32 bits = qFromLittleEndian<quint32>(q);
            float f = 0.0f;
            std::memcpy(&f, &bits, sizeof f);
            out->append(static_cast<double>(f));
            break;
        }
        case miDOUBLE:
        {
            const quint64 bits = qFromLittleEndian<quint64>(q);
            double v = 0.0;
            std::memcpy(&v, &bits, sizeof v);
            out->append(v);
            break;
        }
        default:
            return fail(QObject::tr("unsupported data type %1").arg(tag.type));
        }
    }
    return true;
}

bool MatFileReader::readMatrix(const QByteArray& b, int at, int bytes,
                               QString* name, MatValue* out)
{
    const int end = at + bytes;
    if (end > b.size())
    {
        return fail(QObject::tr("an array runs past the end of the file"));
    }

    //> Subelements are padded to eight bytes, unlike the compressed elements
    //> at the top level.
    auto advance = [&](int from, const Tag& tag) {
        const qint64 n = from + tag.headerBytes + tag.bytes;
        return static_cast<int>(tag.headerBytes == 4
                                    ? from + 8
                                    : n + ((8 - tag.bytes % 8) % 8));
    };

    int p = at;

    //> 1. Array flags.
    Tag tag;
    if (!readTag(b, p, &tag))
    {
        return false;
    }
    if (tag.bytes < 8)
    {
        return fail(QObject::tr("the array flags are too short"));
    }
    const auto* flags = reinterpret_cast<const uchar*>(b.constData())
                        + p + tag.headerBytes;
    const quint32 word = qFromLittleEndian<quint32>(flags);
    const quint32 klass = word & 0xff;
    p = advance(p, tag);

    //> 2. Dimensions.
    QVector<double> dims;
    if (!readNumeric(b, p, end, &dims, nullptr))
    {
        return false;
    }
    if (!readTag(b, p, &tag))
    {
        return false;
    }
    p = advance(p, tag);
    int rows = dims.size() > 0 ? static_cast<int>(dims.at(0)) : 0;
    int columns = dims.size() > 1 ? static_cast<int>(dims.at(1)) : 0;
    //> More than two dimensions is flattened onto the second, since nothing
    //> in a project file is a cube and refusing outright would be worse than
    //> reading the numbers in the order they are stored.
    for (int i = 2; i < dims.size(); ++i)
    {
        columns *= static_cast<int>(dims.at(i));
    }
    if (rows < 0 || columns < 0)
    {
        return fail(QObject::tr("an array has a negative dimension"));
    }

    //> 3. Name.
    QVector<double> nameChars;
    if (!readNumeric(b, p, end, &nameChars, nullptr))
    {
        return false;
    }
    if (!readTag(b, p, &tag))
    {
        return false;
    }
    p = advance(p, tag);
    if (name)
    {
        name->clear();
        for (double c : nameChars)
        {
            name->append(QChar(static_cast<ushort>(c)));
        }
    }

    const qint64 total = static_cast<qint64>(rows) * columns;
    if (total > kMaxInflated)
    {
        return fail(QObject::tr("an array is implausibly large"));
    }

    MatValue v;
    v.d->rows = rows;
    v.d->columns = columns;

    switch (klass)
    {
    case mxCELL:
    {
        v.d->type = MatValue::Type::Cell;
        for (qint64 i = 0; i < total; ++i)
        {
            if (!readTag(b, p, &tag))
            {
                return false;
            }
            if (tag.type != miMATRIX)
            {
                return fail(QObject::tr("a cell holds something that is not "
                                        "an array"));
            }
            MatValue child;
            if (!readMatrix(b, p + tag.headerBytes,
                            static_cast<int>(tag.bytes), nullptr, &child))
            {
                return false;
            }
            v.d->cells.append(child);
            p = advance(p, tag);
        }
        break;
    }
    case mxSTRUCT:
    {
        v.d->type = MatValue::Type::Struct;

        //> Field name length, then all the names in one fixed-width block.
        QVector<double> lengthField;
        if (!readNumeric(b, p, end, &lengthField, nullptr))
        {
            return false;
        }
        if (!readTag(b, p, &tag))
        {
            return false;
        }
        p = advance(p, tag);
        const int width = lengthField.isEmpty()
                              ? 0
                              : static_cast<int>(lengthField.first());
        if (width <= 0 || width > 256)
        {
            return fail(QObject::tr("a struct declares an impossible field "
                                    "name length"));
        }

        QVector<double> nameBlock;
        if (!readNumeric(b, p, end, &nameBlock, nullptr))
        {
            return false;
        }
        if (!readTag(b, p, &tag))
        {
            return false;
        }
        p = advance(p, tag);

        const int nfields = nameBlock.size() / width;
        QStringList names;
        for (int f = 0; f < nfields; ++f)
        {
            QString n;
            for (int c = 0; c < width; ++c)
            {
                const auto ch = static_cast<ushort>(nameBlock.at(f * width + c));
                if (ch == 0)
                {
                    break;
                }
                n.append(QChar(ch));
            }
            names.append(n);
        }
        v.d->fieldOrder = names;

        //> A 0x0 struct still declares its fields. Reading it as one element
        //> would invent a struct that MATLAB says does not exist.
        const int elements = static_cast<int>(total);
        for (int e = 0; e < elements; ++e)
        {
            QMap<QString, MatValue> one;
            for (int f = 0; f < nfields; ++f)
            {
                if (!readTag(b, p, &tag))
                {
                    return false;
                }
                if (tag.type != miMATRIX)
                {
                    return fail(QObject::tr("a struct field holds something "
                                            "that is not an array"));
                }
                MatValue child;
                if (!readMatrix(b, p + tag.headerBytes,
                                static_cast<int>(tag.bytes), nullptr, &child))
                {
                    return false;
                }
                one.insert(names.at(f), child);
                p = advance(p, tag);
            }
            v.d->structs.append(one);
        }
        break;
    }
    case mxCHAR:
    {
        v.d->type = MatValue::Type::Text;
        QVector<double> chars;
        if (!readNumeric(b, p, end, &chars, nullptr))
        {
            return false;
        }
        //> Column-major, so a two-row char array interleaves its rows and
        //> has to be transposed back rather than read straight through.
        for (int r = 0; r < rows; ++r)
        {
            QString line;
            for (int c = 0; c < columns; ++c)
            {
                const qint64 idx = static_cast<qint64>(c) * rows + r;
                if (idx < chars.size())
                {
                    line.append(QChar(static_cast<ushort>(chars.at(idx))));
                }
            }
            v.d->lines.append(line);
        }
        break;
    }
    case mxOBJECT:
        return fail(QObject::tr("the file holds a MATLAB object, which this "
                                "reader does not understand"));
    case mxSPARSE:
        return fail(QObject::tr("the file holds a sparse array, which this "
                                "reader does not understand"));
    default:
    {
        v.d->type = MatValue::Type::Number;
        if (!readNumeric(b, p, end, &v.d->numbers, nullptr))
        {
            return false;
        }
        break;
    }
    }

    *out = v;
    return true;
}

bool MatFileReader::parseTop(const QByteArray& buf, QStringList* order,
                             QMap<QString, MatValue>* out)
{
    int p = 128;
    while (p < buf.size())
    {
        Tag tag;
        if (!readTag(buf, p, &tag))
        {
            return false;
        }

        QByteArray payload;
        const char* body = buf.constData() + p + tag.headerBytes;
        quint32 kind = tag.type;

        if (tag.type == miCOMPRESSED)
        {
            //> Inflate into a growing buffer rather than trusting a declared
            //> size, because there is none: a compressed element says only
            //> how many bytes it occupies, not what it becomes.
            z_stream zs;
            std::memset(&zs, 0, sizeof zs);
            if (inflateInit(&zs) != Z_OK)
            {
                return fail(QObject::tr("could not start decompression"));
            }
            zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(body));
            zs.avail_in = tag.bytes;
            QByteArray chunk(64 * 1024, Qt::Uninitialized);
            int rc = Z_OK;
            do
            {
                zs.next_out = reinterpret_cast<Bytef*>(chunk.data());
                zs.avail_out = static_cast<uInt>(chunk.size());
                rc = inflate(&zs, Z_NO_FLUSH);
                if (rc != Z_OK && rc != Z_STREAM_END && rc != Z_BUF_ERROR)
                {
                    inflateEnd(&zs);
                    return fail(QObject::tr("a compressed variable could not "
                                            "be decompressed"));
                }
                payload.append(chunk.constData(),
                               chunk.size() - static_cast<int>(zs.avail_out));
                if (payload.size() > kMaxInflated)
                {
                    inflateEnd(&zs);
                    return fail(QObject::tr("a compressed variable expands to "
                                            "an implausible size"));
                }
            } while (rc != Z_STREAM_END && zs.avail_in > 0);
            inflateEnd(&zs);
            if (rc != Z_STREAM_END)
            {
                return fail(QObject::tr("a compressed variable is truncated"));
            }

            Tag inner;
            if (!readTag(payload, 0, &inner))
            {
                return false;
            }
            if (inner.type != miMATRIX)
            {
                return fail(QObject::tr("a compressed variable does not hold "
                                        "an array"));
            }
            QString name;
            MatValue v;
            if (!readMatrix(payload, inner.headerBytes,
                            static_cast<int>(inner.bytes), &name, &v))
            {
                return false;
            }
            if (!name.isEmpty() && !out->contains(name))
            {
                order->append(name);
            }
            if (!name.isEmpty())
            {
                out->insert(name, v);
            }
        }
        else if (kind == miMATRIX)
        {
            //> An uncompressed v6 file, which MATLAB still writes on request.
            QString name;
            MatValue v;
            if (!readMatrix(buf, p + tag.headerBytes,
                            static_cast<int>(tag.bytes), &name, &v))
            {
                return false;
            }
            if (!name.isEmpty() && !out->contains(name))
            {
                order->append(name);
            }
            if (!name.isEmpty())
            {
                out->insert(name, v);
            }
        }
        else
        {
            return fail(QObject::tr("unexpected element of type %1 at the top "
                                    "level").arg(tag.type));
        }

        //> A compressed element is NOT padded to eight bytes, whatever the
        //> specification says about data elements in general - MATLAB's own
        //> writer does not pad it, and assuming otherwise desynchronises the
        //> walk at the second variable. Everything else is padded.
        const qint64 next = static_cast<qint64>(p) + tag.headerBytes + tag.bytes;
        p = static_cast<int>(tag.type == miCOMPRESSED
                                 ? next
                                 : next + ((8 - tag.bytes % 8) % 8));
    }
    return true;
}

MatFile::MatFile() = default;

bool MatFile::read(const QString& path, QString* error)
{
    description_.clear();
    order_.clear();
    vars_.clear();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (error)
        {
            *error = QObject::tr("Cannot open %1: %2")
                         .arg(path, f.errorString());
        }
        return false;
    }
    const QByteArray buf = f.readAll();
    f.close();

    if (buf.size() < 128)
    {
        if (error)
        {
            *error = QObject::tr("%1 is too short to be a MAT file.").arg(path);
        }
        return false;
    }

    //> v7.3 is an HDF5 container wearing a level 5 header, and reading it as
    //> level 5 produces confident nonsense. Named rather than guessed at.
    if (buf.startsWith(QByteArrayLiteral("\x89HDF")))
    {
        if (error)
        {
            *error = QObject::tr("%1 is a MATLAB v7.3 file (HDF5). Re-save it "
                                 "from MATLAB with -v7 and try again.")
                         .arg(path);
        }
        return false;
    }

    description_ = QString::fromLatin1(buf.left(116)).trimmed();
    if (!description_.startsWith(QLatin1String("MATLAB 5.0 MAT-file")))
    {
        if (error)
        {
            *error = QObject::tr("%1 does not start with a MATLAB level 5 "
                                 "header.").arg(path);
        }
        return false;
    }

    //> Byte order: "IM" little-endian, "MI" big-endian. Only the first is
    //> written by any machine this program runs on, and swapping every read
    //> for a case that cannot arise would be untested code.
    if (buf.mid(126, 2) != QByteArrayLiteral("IM"))
    {
        if (error)
        {
            *error = QObject::tr("%1 is big-endian, which this reader does "
                                 "not handle.").arg(path);
        }
        return false;
    }

    MatFileReader reader;
    QStringList order;
    QMap<QString, MatValue> vars;
    if (!reader.parseTop(buf, &order, &vars))
    {
        if (error)
        {
            *error = QObject::tr("%1 could not be read: %2")
                         .arg(path, reader.error);
        }
        return false;
    }

    order_ = order;
    vars_ = vars;
    return true;
}
