#include "tst_advspectraloptions.h"

#include "advoutputoptions.h"
#include "advspectraloptions.h"
#include "irga_desc.h"

#include <QTest>

void Test_AdvSpectralOptions_Class::initTestCase()
{
}

void Test_AdvSpectralOptions_Class::requiredOutputMapping()
{
    QVERIFY(!AdvOutputOptions::requiresBinnedSpectraOutput(0, true, false));
    QVERIFY(!AdvOutputOptions::requiresBinnedSpectraOutput(1, true, false));
    QVERIFY(!AdvOutputOptions::requiresBinnedSpectraOutput(2, false, false));
    QVERIFY(!AdvOutputOptions::requiresBinnedSpectraOutput(2, true, true));
    QVERIFY(AdvOutputOptions::requiresBinnedSpectraOutput(2, true, false));
    QVERIFY(AdvOutputOptions::requiresBinnedSpectraOutput(3, true, false));
    QVERIFY(AdvOutputOptions::requiresBinnedSpectraOutput(4, true, false));

    QVERIFY(!AdvOutputOptions::requiresFullTsCospectraOutput(3, false));
    QVERIFY(!AdvOutputOptions::requiresFullTsCospectraOutput(4, true));
    QVERIFY(AdvOutputOptions::requiresFullTsCospectraOutput(4, false));
}

void Test_AdvSpectralOptions_Class::massmanLi7500FamilyDetection()
{
    QVERIFY(AdvSpectralOptions::isLi7500FamilyIrgaModel(
                IrgaDesc::getIRGA_MODEL_STRING_2()));
    QVERIFY(AdvSpectralOptions::isLi7500FamilyIrgaModel(
                IrgaDesc::getIRGA_MODEL_STRING_3()));
    QVERIFY(AdvSpectralOptions::isLi7500FamilyIrgaModel(
                IrgaDesc::getIRGA_MODEL_STRING_12()));
    QVERIFY(AdvSpectralOptions::isLi7500FamilyIrgaModel(
                IrgaDesc::getIRGA_MODEL_STRING_14()));
    QVERIFY(!AdvSpectralOptions::isLi7500FamilyIrgaModel(
                IrgaDesc::getIRGA_MODEL_STRING_4()));
    QVERIFY(!AdvSpectralOptions::isLi7500FamilyIrgaModel(
                IrgaDesc::getIRGA_MODEL_STRING_15()));
}

void Test_AdvSpectralOptions_Class::cleanupTestCase()
{
}

QTTESTUTIL_REGISTER_TEST(Test_AdvSpectralOptions_Class);
