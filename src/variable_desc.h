/***************************************************************************
  variable_desc.h
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

#ifndef VARIABLE_DESC_H
#define VARIABLE_DESC_H

#include <QCoreApplication> // for Q_DECLARE_TR_FUNCTIONS
#include <QList>

class QString;
#include <QStringList>

class VariableDesc
{
    Q_DECLARE_TR_FUNCTIONS(RawfileDesc)

public:
    // raw file desc strings
    static const QString getVARIABLE_VAR_STRING_0();
    static const QString getVARIABLE_VAR_STRING_1();
    static const QString getVARIABLE_VAR_STRING_2();
    static const QString getVARIABLE_VAR_STRING_3();
    static const QString getVARIABLE_VAR_STRING_4();
    static const QString getVARIABLE_VAR_STRING_5();
    static const QString getVARIABLE_VAR_STRING_6();
    static const QString getVARIABLE_VAR_STRING_7();
    static const QString getVARIABLE_VAR_STRING_8();
    static const QString getVARIABLE_VAR_STRING_9();
    static const QString getVARIABLE_VAR_STRING_10();
    static const QString getVARIABLE_VAR_STRING_11();
    static const QString getVARIABLE_VAR_STRING_12();
    static const QString getVARIABLE_VAR_STRING_13();
    static const QString getVARIABLE_VAR_STRING_14();
    static const QString getVARIABLE_VAR_STRING_15();
    static const QString getVARIABLE_VAR_STRING_16();
    static const QString getVARIABLE_VAR_STRING_17();
    static const QString getVARIABLE_VAR_STRING_18();
    static const QString getVARIABLE_VAR_STRING_19();
    static const QString getVARIABLE_VAR_STRING_20();
    static const QString getVARIABLE_VAR_STRING_21();
    static const QString getVARIABLE_VAR_STRING_22();
    static const QString getVARIABLE_VAR_STRING_23();
    static const QString getVARIABLE_VAR_STRING_24();
    static const QString getVARIABLE_VAR_STRING_25();
    static const QString getVARIABLE_VAR_STRING_26();
    static const QString getVARIABLE_VAR_STRING_27();
    static const QString getVARIABLE_VAR_STRING_28();
    static const QString getVARIABLE_VAR_STRING_29();
    static const QString getVARIABLE_VAR_STRING_30();
    static const QString getVARIABLE_VAR_STRING_31();
    static const QString getVARIABLE_VAR_STRING_32();
    static const QString getVARIABLE_VAR_STRING_33();
    static const QString getVARIABLE_VAR_STRING_34();
    /// Analyser signal strength, the two spellings LI-COR uses. Offered here
    /// because the engine's conditional-eddy-covariance screen looks for a
    /// column named exactly AGC or RSSI on the gas's own analyser, and until
    /// they appeared in this list there was no way to declare one.
    static const QString getVARIABLE_VAR_STRING_35();
    static const QString getVARIABLE_VAR_STRING_36();

    static const QString getVARIABLE_MEASURE_TYPE_STRING_0();
    static const QString getVARIABLE_MEASURE_TYPE_STRING_1();
    static const QString getVARIABLE_MEASURE_TYPE_STRING_2();
    static const QString getVARIABLE_MEASURE_TYPE_STRING_3();

    static const QString getVARIABLE_MEASURE_UNIT_STRING_0();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_1();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_2();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_3();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_4();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_5();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_6();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_7();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_8();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_9();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_10();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_11();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_12();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_13();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_14();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_15();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_16();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_17();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_18();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_19();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_20();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_21();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_22();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_23();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_24();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_25();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_26();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_27();
    static const QString getVARIABLE_MEASURE_UNIT_STRING_28();

    static const QString getVARIABLE_CONVERSION_TYPE_STRING_0();
    static const QString getVARIABLE_CONVERSION_TYPE_STRING_1();
    static const QString getVARIABLE_CONVERSION_TYPE_STRING_2();

    enum class AnalogType { SLOW = 0, FAST };

    VariableDesc();

    VariableDesc(
                 const QString& ignore,
                 const QString& numeric,
                 const QString& variable,
                 const QString& instrument,
                 const QString& measureType,
                 const QString& inputUnit,
                 qreal minValue,
                 qreal maxValue,
                 const QString& conversionType,
                 const QString& outputUnit,
                 qreal aValue,
                 qreal bValue,
                 qreal nomTimelag,
                 qreal minTimelag,
                 qreal maxTimelag,
                 qreal errorValue,
                 qreal spectroA,
                 qreal spectroB
                 );

    VariableDesc(const VariableDesc& fileDesc);

    VariableDesc& operator=(const VariableDesc& fileDesc);
    bool operator==(const VariableDesc& fileDesc) const;
    bool operator<(const VariableDesc& fileDesc) const;

    ~VariableDesc();

    // get/set ignore
    const QString& ignore() const;
    void setIgnore(const QString& i);

    // get/set notNumeric
    const QString& numeric() const;
    void setNumeric(const QString& i);

    // get/set variable
    const QString& variable() const;
    void setVariable(const QString& v);

    // get/set instrument
    const QString& instrument() const;
    void setInstrument(const QString& i);

    // get/set measure type
    const QString& measureType() const;
    void setMeasureType(const QString& t);

    // get/set input unit
    const QString& inputUnit() const;
    void setInputUnit(const QString& u);

    // get/set minimum value
    qreal minValue() const;
    void setMinValue(qreal v);

    // get/set maximum value
    qreal maxValue() const;
    void setMaxValue(qreal v);

    // get/set conversion type
    const QString& conversionType() const;
    void setConversionType(const QString& t);

    // get/set output unit
    const QString& outputUnit() const;
    void setOutputUnit(const QString& u);

    // get/set A value
    qreal aValue() const;
    void setAValue(qreal v);

    // get/set B value
    qreal bValue() const;
    void setBValue(qreal v);

    // get/set
    qreal nomTimelag() const;
    void setNomTimelag(qreal l);

    // get/set
    qreal minTimelag() const;
    void setMinTimelag(qreal l);

    // get/set
    //> What this column writes when it has no reading, in the units the file
    //> is in. -9999 by default, which is also one of the values the engine
    //> always treats as missing - along with NaN, an unparseable token and an
    //> empty field - so a column only has to state this when its logger writes
    //> something else.
    qreal errorValue() const;
    void setErrorValue(qreal v);

    //> Spectroscopic coefficients after Peltola et al. (2014): water vapour
    //> broadens this column's absorption lines, so the analyser's sensitivity
    //> scales as 1 + a*chi_q + b*chi_q^2 and the correction is a division by
    //> that. Both zero - the default - makes it the identity, which is what
    //> every metadata file written before these keys existed says.
    //>
    //> Not the aValue and bValue above, which are the linear calibration gain
    //> and offset and have nothing to do with this.
    qreal spectroA() const;
    void setSpectroA(qreal v);
    qreal spectroB() const;
    void setSpectroB(qreal v);

    qreal maxTimelag() const;
    void setMaxTimelag(qreal l);

    // return a list of variable strings
    static const QStringList variableStringList();

    static const QStringList measureTypeStringList();
    static const QStringList inputUnitStringList();
    static const QStringList outputUnitStringList();
    static const QStringList conversionTypeStringList();

    // return a list of boolean strings
    // NOTE: duplicated from anemdesc.h
    static const QStringList yesNoStringList();

    static bool isGoodWindComponent(const VariableDesc& var);
    static bool isGoodGas(const VariableDesc& var, bool isCustom = false);
    static bool isGoodSonicTempOrSpeed(const VariableDesc& var);
    static bool isGoodTemperature(const VariableDesc& var, AnalogType type = AnalogType::SLOW);
    static bool isGoodPressure(const VariableDesc& var);

    static bool goodGainOffsetTest(const VariableDesc& var);

    static bool isGasVariable(const QString& var);
    static bool isCustomVariable(const QString& var);
    static bool isScalableVariable(const QString& inputUnit);
    static bool isVelocityVar(const QString& var);
    static bool isAngleVar(const QString& var);
    static bool isTemperatureVar(const QString& var);
    static bool isPressureVar(const QString& var);
    static bool isFlowRateVar(const QString& var);
    static bool isDiagnosticVar(const QString& var);

    static const QStringList angleInputUnitStringList();
    static const QStringList velocityInputUnitStringList();
    static const QStringList temperatureInputUnitStringList();
    static const QStringList pressureInputUnitStringList();
    static const QStringList gasInputUnitStringList();
    static const QStringList flowRateInputUnitStringList();

    static const QStringList angleOutputUnitStringList();
    static const QStringList velocityOutputUnitStringList();
    static const QStringList temperatureOutputUnitStringList();
    static const QStringList pressureOutputUnitStringList();
    static const QStringList gasOutputUnitStringList();
    static const QStringList flowRateOutputUnitStringList();

private:
    QString ignore_;
    QString numeric_;
    QString variable_;
    QString instrument_;
    QString measureType_;
    QString inputUnit_;
    qreal minValue_;
    qreal maxValue_;
    QString conversionType_;
    QString outputUnit_;
    qreal aValue_;
    qreal bValue_;
    //> Declared in the order all three constructors initialise them. They
    //> were not, and the compiler warned about it on every build of this file
    //> - harmlessly here, since these are plain reals that do not read each
    //> other, but a real -Wreorder is how a member gets initialised from one
    //> that is still garbage.
    qreal nomTimelag_;
    qreal minTimelag_;
    qreal maxTimelag_;
    qreal errorValue_;
    qreal spectroA_;
    qreal spectroB_;
};

using VariableDescList = QList<VariableDesc>;

// Inlined Methods
inline bool VariableDesc::operator<(const VariableDesc& fileDesc) const
    { return (variable_ < fileDesc.variable_); }

inline const QString& VariableDesc::ignore() const
    { return ignore_; }

inline void VariableDesc::setIgnore(const QString& i)
    { ignore_ = i; }

inline const QString& VariableDesc::numeric() const
    { return numeric_; }

inline void VariableDesc::setNumeric(const QString& i)
    { numeric_ = i; }

inline const QString& VariableDesc::variable() const
    { return variable_; }

inline void VariableDesc::setVariable(const QString& v)
    { variable_ = v; }

inline const QString& VariableDesc::instrument() const
    { return instrument_; }

inline void VariableDesc::setInstrument(const QString& i)
    { instrument_ = i; }

inline const QString& VariableDesc::measureType() const
    { return measureType_; }

inline void VariableDesc::setMeasureType(const QString& t)
    { measureType_ = t; }

inline const QString& VariableDesc::inputUnit() const
    { return inputUnit_; }

inline void VariableDesc::setInputUnit(const QString& u)
    { inputUnit_ = u; }

inline qreal VariableDesc::minValue() const
    { return minValue_; }

inline void VariableDesc::setMinValue(qreal v)
    { minValue_ = v; }

inline qreal VariableDesc::maxValue() const
    { return maxValue_; }

inline void VariableDesc::setMaxValue(qreal v)
    { maxValue_ = v; }

inline const QString& VariableDesc::conversionType() const
    { return conversionType_; }

inline void VariableDesc::setConversionType(const QString& t)
    { conversionType_ = t; }

inline const QString& VariableDesc::outputUnit() const
    { return outputUnit_; }

inline void VariableDesc::setOutputUnit(const QString& u)
    { outputUnit_ = u; }

inline qreal VariableDesc::aValue() const
    { return aValue_; }

inline void VariableDesc::setAValue(qreal v)
    { aValue_ = v; }

inline qreal VariableDesc::bValue() const
    { return bValue_; }

inline void VariableDesc::setBValue(qreal v)
    { bValue_ = v; }

inline qreal VariableDesc::errorValue() const
    { return errorValue_; }

inline void VariableDesc::setErrorValue(qreal v)
    { errorValue_ = v; }

inline qreal VariableDesc::spectroA() const
    { return spectroA_; }

inline void VariableDesc::setSpectroA(qreal v)
    { spectroA_ = v; }

inline qreal VariableDesc::spectroB() const
    { return spectroB_; }

inline void VariableDesc::setSpectroB(qreal v)
    { spectroB_ = v; }

inline qreal VariableDesc::nomTimelag() const
    { return nomTimelag_; }

inline void VariableDesc::setNomTimelag(qreal l)
    { nomTimelag_ = l; }

inline qreal VariableDesc::minTimelag() const
    { return minTimelag_; }

inline void VariableDesc::setMinTimelag(qreal l)
    { minTimelag_ = l; }

inline qreal VariableDesc::maxTimelag() const
    { return maxTimelag_; }

inline void VariableDesc::setMaxTimelag(qreal l)
    { maxTimelag_ = l; }

#endif // VARIABLE_DESC_H

