"""The interface must write the two keys the per-instrument rate feature reads.

An instrument slower than the station's acquisition frequency cannot fill every
row of the raw file. The engine measures such a column against what that
instrument should have produced rather than against the rows, but only if it is
told: `instr_<K>_ac_freq` in the .metadata, `instr_<n>_max_lack` in the project.
Neither is derivable, so a key the interface does not write is a feature no user
can reach.

Two properties matter beyond the keys existing:

  1. **Unset means "follow", not "0" or "10".** The rate is stored as 0 and the
     allowance as no key at all, so an instrument nobody has touched uses the
     station's frequency and the project-wide allowance, and keeps following
     them when those change. Writing today's numbers onto every instrument
     would freeze them, which is the opposite of a default.
  2. **The allowance is keyed by position**, anemometers first and then
     analysers with one shared counter, because that is the order the .metadata
     is written in and the order the engine numbers instr_<K>_* in. The two
     files agree only as long as both walk the lists the same way.

The cross-repo half is skipped, not failed, when the engine checkout is not
beside this one - the same contract as test_species_constants_static.
"""

import re
import unittest
from pathlib import Path

GUI_ROOT = Path(__file__).resolve().parent.parent
ENGINE_ROOT = GUI_ROOT.parent / "eddyflow-engine"


def read(rel, root=GUI_ROOT):
    return (root / rel).read_text(encoding="utf-8", errors="replace")


class InstrumentSamplingStaticTests(unittest.TestCase):
    def test_the_acquisition_frequency_key_is_defined_for_both_categories(self):
        defs = read("src/dlinidefs.h")
        assert 'INI_ANEM_17          = QStringLiteral("ac_freq")' in defs
        assert 'INI_IRGA_17          = QStringLiteral("ac_freq")' in defs

    def test_the_acquisition_frequency_is_written_and_read_back(self):
        body = read("src/dlproject.cpp")
        for key, getter in (("INI_ANEM_17", "anem.acFreq()"),
                            ("INI_IRGA_17", "irga.acFreq()")):
            assert f"prefix + DlIni::{key}" in body, key
            assert f"QString::number({getter}, 'f', 3)" in body, getter
        # Read back with 0 as the default, which is what "follow the station"
        # is stored as - and what an absent key means for every metadata file
        # written before the key existed.
        assert "setAcFreq(project_ini.value(prefix + DlIni::INI_ANEM_17, 0.0).toReal())" in body
        assert "setAcFreq(project_ini.value(prefix + DlIni::INI_IRGA_17, 0.0).toReal())" in body

    def test_an_untouched_instrument_stores_zero_rather_than_a_number(self):
        """0 is what makes the instrument follow the station. A desc that
        defaulted to 10.0 would write 10.0 into the metadata of every site,
        and those instruments would stop following the station's own rate."""
        assert "acFreq_(0.0)" in read("src/anem_desc.cpp")
        assert "acFreq_(0.0)" in read("src/irga_desc.cpp")

    def test_typing_the_station_rate_keeps_the_instrument_following(self):
        for path in ("src/anem_model.cpp", "src/irga_model.cpp"):
            body = read(path)
            assert "qFuzzyCompare(entered, stationAcFreq())" in body, path
            # An explicit zero is the user asking for it outright, by winding
            # the spin down to its "Station frequency" entry or typing one.
            assert "entered <= 0.0" in body, path
            # qFuzzyCompare is undefined against exactly zero, which is the
            # value that means "follow the station".
            assert "+ 1.0)" in body, path

    def test_the_follow_the_station_state_can_be_seen_and_chosen(self):
        """0 is the stored sentinel for "follow the station". A spin that
        cannot reach 0, seeded with the RESOLVED rate rather than the stored
        one, left no way back to it and no way to tell it apart from an
        instrument pinned to the same number."""
        for delegate in ("src/anem_delegate.cpp", "src/irga_delegate.cpp"):
            body = read(delegate)
            assert "dspin->setRange(0.0, 100.0)" in body, delegate
            assert 'setSpecialValueText(tr("Station frequency"))' in body, delegate

        for model in ("src/anem_model.cpp", "src/irga_model.cpp"):
            body = read(model)
            # The EditRole hands back the stored value, not the resolved one.
            assert "displayedAcFreq" not in body.split("case ACFREQ:")[-1], model
            assert 'tr("Station frequency (%1 Hz)")' in body, model

    def test_the_row_is_shown_edited_and_is_the_last_one(self):
        for model, view, delegate, enum in (
                ("src/anem_model.h", "src/anem_tableview.cpp",
                 "src/anem_delegate.cpp", "ANEMNUMCOLS"),
                ("src/irga_model.h", "src/irga_tableview.cpp",
                 "src/irga_delegate.cpp", "IRGANUMCOLS")):
            head = read(model)
            assert re.search(r"ACFREQ,\s*\n", head), model
            assert 'addSection(tr("Acquisition frequency")' in read(view), view
            body = read(delegate)
            assert "::ACFREQ:" in body, delegate
            # Same validation as the station's own frequency spin.
            assert 'dspin->setSuffix(QStringLiteral(" [Hz]"))' in body, delegate
            #> 0 is reserved as the "follow the station" entry; the station's
            #> own spin (dlsitetab.cpp) keeps the 0.001 floor.
            assert "dspin->setRange(0.0, 100.0)" in body, delegate

    def test_the_whole_column_signal_reaches_the_last_row(self):
        """setData emits dataChanged over MANUFACTURER..<last row>, and the
        last row is whatever sits just before the NUMCOLS sentinel. Naming any
        earlier one leaves every row past it never repainting after an edit -
        which is exactly what the ACFREQ row walked into, and then SAMPLING.

        Derived rather than named, so the next row added to either table is
        caught here instead of looking fine.
        """
        for header, source, enum in (
                ("src/anem_model.h", "src/anem_model.cpp", "ANEMNUMCOLS"),
                ("src/irga_model.h", "src/irga_model.cpp", "IRGANUMCOLS"),
                #> The variable table walked into this too, when the error-value
                #> row landed after MAXTIMELAG.
                ("src/variable_model.h", "src/variable_model.cpp", "VARNUMCOLS")):
            rows = re.search(r"enum \w+\s*\{(.*?)" + enum, read(header), re.S).group(1)
            last = [r.strip().rstrip(",") for r in rows.strip().splitlines()
                    if r.strip() and not r.strip().startswith("//")][-1]
            body = read(source)
            assert f"index.sibling({last}, column)" in body, (
                f"{source} signals up to something other than {last}, the last "
                f"row of {header}"
            )

    def test_the_sampling_mode_is_offered_and_written(self):
        """instr_<K>_integrates decides how the vertical wind is paired with a
        slower instrument's samples. Read by the engine and written by nothing,
        the block-averaging branch would be unreachable."""
        defs = read("src/dlinidefs.h")
        assert 'INI_ANEM_18          = QStringLiteral("integrates")' in defs
        assert 'INI_IRGA_18          = QStringLiteral("integrates")' in defs

        body = read("src/dlproject.cpp")
        for key in ("INI_ANEM_18", "INI_IRGA_18"):
            assert f"prefix + DlIni::{key}" in body, key
        #> Absent means 0, which is the safe answer: pairing an averaged wind
        #> against a point-sampled gas biases the covariance.
        assert "DlIni::INI_ANEM_18, 0).toInt() == 1" in body
        assert "DlIni::INI_IRGA_18, 0).toInt() == 1" in body

        for model, delegate in (("src/anem_model.h", "src/anem_delegate.cpp"),
                                ("src/irga_model.h", "src/irga_delegate.cpp")):
            assert "SAMPLING," in read(model), model
            d = read(delegate)
            assert "::SAMPLING:" in d, delegate
            #> A combo, not a spin box: isComboRow is what the delegate keys on.
            assert "|| row == " in d and "::SAMPLING" in d, delegate

    def test_the_sampling_row_is_editable_on_every_instrument(self):
        """Sampling used to be greyed whenever the instrument had no rate of
        its own - which is also the state it lands in the moment it is set to
        the station's rate, so stating the sampling once put the cell out of
        reach. Whether the choice MATTERS is said by colour and tooltip now,
        not by withholding the cell."""
        for model in ("src/anem_model.cpp", "src/irga_model.cpp"):
            body = read(model)
            flags = body[body.index("Qt::ItemFlags"):]
            sampling = flags[flags.index("case SAMPLING:"):]
            sampling = sampling[:sampling.index("return currentFlags;")]
            assert "ItemIsEnabled" not in sampling, model
            assert "ItemIsEditable" not in sampling, model
            assert "ItemIsSelectable" not in sampling, model
            #> The relevance test the grey text and the tooltip both key on.
            assert "bool %s::samplingIsRelevant" % (
                "AnemModel" if "anem" in model else "IrgaModel") in body, model
            assert "acFreq() < stationAcFreq()" in body, model

    def test_the_sampling_default_is_instantaneous(self):
        """The first entry of the list is the default, and the descriptors must
        construct to it - an instrument that says nothing must not be treated
        as integrating."""
        for desc, getter in (("src/anem_desc.cpp", "getANEM_SAMPLING_STRING_0()"),
                             ("src/irga_desc.cpp", "getIRGA_SAMPLING_STRING_0()")):
            body = read(desc)
            assert f"sampling_({getter})" in body, desc
            #> Ordered, not sorted: "Averaged over the interval" would otherwise
            #> sort ahead of "Instantaneous" and silently become the default.
            block = body[body.index("samplingStringList"):]
            block = block[:block.index("}")]
            assert "sortedWithOtherLast" not in block, desc

    def test_the_allowance_key_is_written_only_where_it_is_set(self):
        defs = read("src/ecinidefs.h")
        assert 'QStringLiteral("instr_%1_max_lack").arg(slot)' in defs

        body = read("src/ecproject.cpp")
        assert "EcIni::iniScreenSettingsInstrMaxLack(it.key())" in body, (
            "the allowance is not written per instrument"
        )
        assert "if (!project_ini.contains(key)) { continue; }" in body, (
            "an absent key must stay absent; defaulting it would pin every "
            "instrument to whatever the global allowance was at load time"
        )
        assert "if (!lacks.remove(slot)) { return; }" in body, (
            "clearing an instrument's allowance must remove the key, not "
            "store a sentinel the engine would read as a percentage"
        )

    def test_the_allowance_rows_are_numbered_the_way_the_metadata_is(self):
        """Anemometers first, then analysers, one shared counter - the order
        DlProject::saveProject writes instr_<K>_* in."""
        body = read("src/basicsettingspage.cpp")
        fn = body[body.index("void BasicSettingsPage::refreshInstrMaxLackRows()"):]
        fn = fn[:fn.index("\nvoid BasicSettingsPage::")]
        assert fn.index("dlProject_->anems()") < fn.index("dlProject_->irgas()"), (
            "the analysers are walked before the anemometers, so every "
            "allowance lands on the wrong instrument"
        )
        assert "setScreenInstrMaxLack(thisSlot, value)" in fn
        assert "Defs::MAX_INSTRUMENTS" in fn, (
            "the rows are not capped at the engine's instrument limit, so a "
            "large site writes allowances nothing reads back"
        )
        assert 'setSpecialValueText(tr("Same as above"))' in fn
        assert "setRange(-1, 99)" in fn

        writer = read("src/dlproject.cpp")
        writer = writer[writer.index("// instruments section"):]
        assert (writer.index("project_state_.anemometerList")
                < writer.index("project_state_.irgaList")), (
            "the metadata is written analysers-first, which no longer matches "
            "the order the allowance rows are numbered in"
        )

    def test_the_rows_are_rebuilt_when_the_instruments_change(self):
        body = read("src/basicsettingspage.cpp")
        parse = body[body.index("void BasicSettingsPage::parseMetadataProject"):]
        parse = parse[:parse.index("\nvoid BasicSettingsPage::")]
        assert "refreshInstrMaxLackRows();" in parse, (
            "a reloaded metadata file can change the instrument set in any "
            "way, and a stale row writes to a slot that now belongs to "
            "another device"
        )

    def test_the_column_error_value_round_trips_with_9999_as_the_default(self):
        """The engine reads col_<N>_error_value already; unwritable from here,
        a logger with a fill other than -9999 has no way to say so."""
        assert 'INI_VARDESC_ERROR_VALUE  = QStringLiteral("error_value")' in             read("src/dlinidefs.h")

        body = read("src/dlproject.cpp")
        assert "DlIni::INI_VARDESC_ERROR_VALUE, -9999.0" in body, (
            "a file written before the key existed must read back as -9999, "
            "which is what the engine assumes for it anyway"
        )
        assert "QString::number(var.errorValue(), 'f', 4)" in body
        assert "var.setErrorValue(-9999.0)" in body, (
            "a fresh column must default to the conventional fill"
        )
        assert "errorValue_(-9999.0)" in read("src/variable_desc.cpp")

        assert "ERRORVALUE," in read("src/variable_model.h")
        assert 'addSection(tr("Error value")' in read("src/variable_tableview.cpp")
        d = read("src/variable_delegate.cpp")
        assert "VariableModel::ERRORVALUE:" in d
        #> Wide and unsuffixed: it is a raw-file value in the column's own unit,
        #> and loggers write -9999, 6999, -99999 and large positives alike.
        assert "dspin->setRange(-1.0e9, 1.0e9)" in d

    def test_the_engine_reads_the_keys_the_interface_writes(self):
        if not ENGINE_ROOT.exists():
            self.skipTest(f"engine checkout not found at {ENGINE_ROOT}")

        tags = read("src/src_common/m_common_global_var.f90", ENGINE_ROOT)
        assert "'instr_1_ac_freq'" in tags, (
            "the engine's metadata tag table has no ac_freq key, so the one "
            "the interface writes is read by nothing"
        )

        rp_tags = read("src/src_rp/m_rp_global_var.f90", ENGINE_ROOT)
        n_instr = int(re.search(
            r"integer, parameter :: MaxNumInstruments = (\d+)",
            read("src/src_common/m_typedef.f90", ENGINE_ROOT)).group(1))
        for k in range(1, n_instr + 1):
            assert f"'instr_{k}_max_lack'" in rp_tags, k

        #> The interface caps the allowance rows at the same number, or it
        #> would write keys the engine's table cannot match.
        assert f"MAX_INSTRUMENTS = {n_instr}" in read("src/defs.h"), (
            "the two repositories disagree on how many instruments there can "
            "be, so the interface writes allowances the engine drops"
        )


if __name__ == "__main__":
    unittest.main()
