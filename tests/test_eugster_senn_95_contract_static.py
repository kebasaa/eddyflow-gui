"""The Eugster and Senn (1995) correction agrees across interface and engine.

``hf_meth = 7`` selects it, and each gas record may state its damping time
constant L as ``gas_<i>_es_tau`` in ``[Project]``: RP applies the correction
itself, and FCC only reads ``[Project]``.

Its combo row is not its ini value. The row order follows publication year, so
Eugster and Senn (1995) is the first row, while the engine's codes follow the
order methods were added. Every translation goes through one table,
``kHfMethodForRow``, and this file pins that table against the engine's
decoder so the two cannot drift. Nothing else on the page may use a bare row
number to mean a method.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parents[1]
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"
ENGINE_DECODER = (ENGINE_ROOT / "src" / "src_common"
                  / "write_processing_project_variables.f90")
ENGINE_TAGS = ENGINE_ROOT / "src" / "src_common" / "m_common_global_var.f90"


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


PAGE = read(GUI_ROOT / "src" / "advspectraloptions.cpp")
HEADER = read(GUI_ROOT / "src" / "advspectraloptions.h")
PROJECT = read(GUI_ROOT / "src" / "ecproject.cpp")
RECORD = read(GUI_ROOT / "src" / "measurement_record.h")

#: Combo labels in row order, each with the engine name its row stands for.
ROWS = [
    ("Eugster and Senn (1995)", "eugster_senn_95"),
    ("Moncrieff et al. (1997)", "moncrieff_97"),
    ("Massmann (2000, 2001)", "massman_00"),
    ("Horst (1997)", "horst_97"),
    ("Ibrom et al. (2007)", "ibrom_07"),
    ("Fratini et al. (2012)", "fratini_12"),
]

engine_available = ENGINE_DECODER.is_file() and ENGINE_TAGS.is_file()


def body(src, signature):
    start = src.index(signature)
    return src[start:src.index("\n}\n", start)]


def row_table():
    block = PAGE[PAGE.index("constexpr int kHfMethodForRow[] = {"):]
    block = block[:block.index("};")]
    return [int(v) for v in re.findall(r"^\s*(\d+),", block, re.M)]


def engine_codes():
    """hf_meth digit -> engine name, from the decoder's select case."""
    src = read(ENGINE_DECODER)
    block = src[src.index("select case (EPPrjCTags(23)%value(1:1))"):]
    block = block[:block.index("end select")]
    return {int(d): name for d, name in re.findall(
        r"case \('(\d)'\)[\s\S]*?hf_meth = '(\w+)'", block)}


class TheCombo(unittest.TestCase):

    def test_the_rows_are_in_order_of_publication(self):
        labels = re.findall(r'hfMethCombo->addItem\(tr\("([^"]+?) - ', PAGE)
        self.assertEqual(labels, [label for label, _ in ROWS])

    @unittest.skipUnless(engine_available, "engine checkout not found")
    def test_each_row_maps_to_the_method_it_names(self):
        codes = engine_codes()
        table = row_table()
        self.assertEqual(len(table), len(ROWS))
        for (label, name), code in zip(ROWS, table):
            with self.subTest(label=label):
                self.assertEqual(codes.get(code), name)

    def test_tooltips_are_keyed_by_method_not_by_row(self):
        #> Four of them sat one row off once; keyed by method, they cannot.
        tips = re.findall(r"hfMethCombo->setItemData\((.+?), tr\(\"<b>", PAGE)
        self.assertEqual(len(tips), len(ROWS))
        for target in tips:
            self.assertRegex(target, r"^hfRowForMethod\(\d\)$")

    def test_no_bare_row_number_stands_for_a_method(self):
        self.assertNotRegex(PAGE, r"hfMethCombo->setItemData\(\d,")
        self.assertNotRegex(PAGE, r"hfMethCombo->setCurrentIndex\(\d\)")
        self.assertNotIn("emit updateOutputsRequest(hfMethCombo->currentIndex())", PAGE)
        self.assertNotIn("emit updateOutputsRequest(n);", PAGE)
        impl = body(PAGE, "void AdvSpectralOptions::updateHfMethod_2(int n)")
        self.assertNotRegex(impl, r"\bn [<>=]")

    def test_the_default_row_is_moncrieff(self):
        self.assertIn("WidgetUtils::resetComboToItem(hfMethCombo, hfRowForMethod(1));", PAGE)

    def test_smartflux_mode_closes_the_row(self):
        impl = body(PAGE, "void AdvSpectralOptions::setSmartfluxUI()")
        self.assertIn('hfMethCombo->setItemData(hfRowForMethod(7), QStringLiteral("disabled")', impl)


class TheTimeConstant(unittest.TestCase):

    def test_it_lives_on_the_gas_record(self):
        self.assertIn("qreal esTau = -1.0;", RECORD)

    def test_it_is_written_and_read_as_a_record_key(self):
        writer = body(PROJECT, "void EcProject::writeMeasurementRecords")
        reader = body(PROJECT, "bool EcProject::readMeasurementRecords")
        self.assertIn('p + QStringLiteral("es_tau")', writer)
        self.assertIn("rec.esTau >= 0.0 ? QString::number(rec.esTau, 'f', 3) : QString()",
                      writer)
        self.assertIn('p + QStringLiteral("es_tau")', reader)

    @unittest.skipUnless(engine_available, "engine checkout not found")
    def test_the_engine_reads_it(self):
        self.assertIn("'gas_1_es_tau'", read(ENGINE_TAGS))
        self.assertIn("EddyFlowProj%gas(i)%es_tau", read(ENGINE_DECODER))

    def test_the_list_is_shown_only_for_the_method(self):
        impl = body(PAGE, "void AdvSpectralOptions::refreshEsTauBox()")
        self.assertIn("hfMethodCheck->isChecked() && isEugsterSenn()", impl)
        self.assertIn("return (ecProject_->generalHfMethod() == 7);", PAGE)
        self.assertIn("QVector<QDoubleSpinBox*> esTauSpins_;", HEADER)

    def test_unset_is_stored_as_minus_one(self):
        impl = body(PAGE, "void AdvSpectralOptions::onEsTauChanged(int gasIndex, double value)")
        self.assertIn("const qreal stored = value < 0.0 ? -1.0 : value;", impl)


class TheProject(unittest.TestCase):

    def test_seven_is_kept_on_load_and_six_is_not(self):
        self.assertIn("if (m < 0 || m > 7 || m == 6)", PROJECT)

    def test_it_needs_no_second_engine_step(self):
        impl = body(PROJECT, "bool EcProject::isEngineStep2Needed()")
        self.assertRegex(impl, r"case 5:\s+case 7:\s+break;")

    def test_a_smartflux_package_refuses_it(self):
        impl = body(PROJECT, "QString EcProject::smartfluxBlockReason")
        self.assertIn("ec_project_state_.projectGeneral.hf_meth == 7", impl)

    def test_the_default_is_unchanged(self):
        state = read(GUI_ROOT / "src" / "ecprojectstate.h")
        self.assertIn("int hf_meth = 1;", state)


if __name__ == "__main__":
    unittest.main()
