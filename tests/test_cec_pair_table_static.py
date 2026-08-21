"""Which channels the partition pairs is the user's to state, and both of
them are required.

Conditional Eddy Covariance sorts air parcels into octants by the signs of the
vertical wind, the water and the carbon dioxide *together*. Two consequences
follow, and the interface got both wrong.

The first is that the method needs a water channel and a carbon channel
whichever of the two fluxes it is asked to report - "CO2 only" still reads the
water to know which octant a parcel is in. `updateCecAvailability` used to
accept *either*, and force the choice to whichever was present. The engine then
refused to build an octant at all, and the project got a full set of columns
containing nothing but the error code, with nothing anywhere saying why.

The second is that which carbon channel goes with which water channel is a real
choice at a site with more than one analyser, and it used to be made silently:
the first record of each species, wherever each happened to sit. A pairing
across two instruments builds its octants from series that reach the sensor at
different time lags and through different spectral responses. So there is a
table now, it defaults to same-analyser pairings, and it says so when a pairing
crosses.

The global three-way "flux partitioning" combo went with it. It said nothing
useful once a site could have a pairing per analyser, and it was the thing that
forced the impossible choice above. Each pairing carries its own.
"""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


def read(relative):
    return (ROOT / relative).read_text(encoding="utf-8", errors="replace")


class BothSpeciesAreRequired(unittest.TestCase):
    def setUp(self):
        self.page = read("src/advprocessingoptions.cpp")

    def test_availability_asks_for_both_not_either(self):
        self.assertIn("const bool hasBoth = hasCo2 && hasH2o;", self.page)
        self.assertIn("cecCheckBox->setEnabled(hasBoth);", self.page)
        self.assertNotIn("const bool hasAny = hasCo2 || hasH2o;", self.page)

    def test_nothing_forces_a_partition_the_engine_cannot_run(self):
        #> The forced index is gone with the combo it drove.
        self.assertNotIn("forcedIndex", self.page)
        self.assertNotIn("cecMethodCombo", self.page)
        self.assertNotIn("cecMethodCombo", read("src/advprocessingoptions.h"))

    def test_the_records_are_what_is_asked_not_the_retired_columns_alone(self):
        self.assertIn('gasRecordsFor(QStringLiteral("co2"))', self.page)
        self.assertIn('gasRecordsFor(QStringLiteral("h2o"))', self.page)

    def test_smartflux_still_wins_over_availability(self):
        #> A SmartFlux module runs LI-COR's EddyPro, which has never heard of
        #> this. Checked before the species are, so no arrangement of records
        #> can switch it back on.
        block = self.page[self.page.index("void AdvProcessingOptions::updateCecAvailability"):]
        block = block[:block.index("\n}\n")]
        self.assertLess(block.index("configState_->project.smartfluxMode"),
                        block.index("hasCo2"))

    def test_the_checkbox_is_now_a_master_switch(self):
        self.assertIn("ecProject_->setGeneralCecMeth(b ? 1 : 0);", self.page)
        self.assertNotIn("updateCecMeth_2", self.page)

    def test_the_checkbox_has_the_help_link_its_neighbours_have(self):
        block = self.page[self.page.index("cecCheckBox = new RichTextCheckBox;"):]
        block = block[:block.index("cecSettingsButton = new QPushButton")]
        self.assertIn("setQuestionMark", block)

    def test_the_tooltip_names_the_columns_the_engine_actually_writes(self):
        block = self.page[self.page.index("cecCheckBox = new RichTextCheckBox;"):]
        block = block[:block.index("cecSettingsButton = new QPushButton")]
        for name in ("E_cec", "Tr_cec", "Reco_cec", "P_cec"):
            self.assertIn(name, block)
        #> Respiration is Reco_cec. The tooltip used to promise "R".
        self.assertNotIn("<i>R</i>", block)


class ThePairingTableStatesTheChoice(unittest.TestCase):
    def setUp(self):
        self.model = read("src/cecpairmodel.cpp")
        self.header = read("src/cecpairmodel.h")
        self.dialog = read("src/cecsettingsdialog.cpp")

    def test_the_table_has_the_columns_the_choice_needs(self):
        for column in ("Use", "Carbon", "Water", "Partition", "Extra", "Warning"):
            self.assertIn(column, self.header)
        self.assertIn("class CecPairModel : public QAbstractTableModel", self.header)
        self.assertIn("class CecPairDelegate : public QStyledItemDelegate", self.header)

    def test_cells_carry_the_record_index_never_the_label(self):
        """Two channels of one species share a label; only the index is unique.

        The same discipline the moisture column in the Basic Settings variable
        table keeps, and for the same reason.
        """
        self.assertIn("case Carbon: return pair.carbonIndex;", self.model)
        self.assertIn("case Water: return pair.waterIndex;", self.model)
        self.assertIn("combo->addItem(choice.second, choice.first);", self.model)
        self.assertIn("model->setData(index, combo->currentData(), Qt::UserRole);",
                      self.model)

    def test_a_pairing_may_not_name_its_own_channel_as_an_extra(self):
        #> It is already targets one and two. Naming it again would partition
        #> the same series twice under two names.
        self.assertIn("if (i + 1 == pair.carbonIndex || i + 1 == pair.waterIndex) { continue; }",
                      self.model)

    def test_a_pairing_across_two_analysers_is_flagged(self):
        self.assertIn("bool CecPairModel::crossAnalyser", self.model)
        self.assertIn("instrumentId", self.model)
        self.assertIn("QStyle::SP_MessageBoxWarning", self.model)

    def test_the_default_is_one_pairing_per_carbon_channel_same_analyser(self):
        record = read("src/measurement_record.cpp")
        self.assertIn("QVector<CecPairRecord> defaultCecPairs", record)
        self.assertIn('gases.at(j).instrumentId != gases.at(i).instrumentId', record)
        #> And the table shows that default rather than an empty grid, so what
        #> the user sees is what an unstated project will run.
        self.assertIn("pairs_ = MeasurementRecords::defaultCecPairs", self.model)

    def test_the_table_follows_the_gas_records(self):
        #> A pairing names records, so a record removed elsewhere can invalidate
        #> one. refresh() is bound to EcProject::ecProjectChanged by the page.
        self.assertIn("pairModel->reload();", self.dialog)
        self.assertIn("void CecPairModel::reload()", self.model)


class TheProjectFileCarriesIt(unittest.TestCase):
    def setUp(self):
        self.project = read("src/ecproject.cpp")

    def test_the_pairings_round_trip(self):
        self.assertIn("void EcProject::writeCecPairs", self.project)
        self.assertIn("void EcProject::readCecPairs", self.project)
        self.assertIn('QStringLiteral("cec_%1_").arg(i + 1)', self.project)
        for field in ("meth", "co2", "h2o", "extra"):
            self.assertIn('QStringLiteral("%s")' % field, self.project)

    def test_an_unstated_list_writes_nothing_rather_than_zero(self):
        """Absent and zero say different things to the engine.

        Absent is "you decide", and it derives one pairing per carbon channel
        from the analyser layout. Zero is "none", and it runs no partition.
        """
        block = self.project[self.project.index("void EcProject::writeCecPairs"):]
        block = block[:block.index("\n}\n")]
        self.assertIn("if (pairs.isEmpty()) { return; }", block)
        self.assertIn("project_ini.remove(EcIni::INI_PROJECT_CEC_NUM);", block)

    def test_stale_pairing_keys_are_cleared_first(self):
        #> QSettings keeps whatever it is not asked to overwrite, so shortening
        #> the list would otherwise leave the dropped pairings in the file.
        self.assertIn(r'QRegularExpression(QStringLiteral("^cec_\\d+_"))', self.project)

    def test_the_singularity_band_is_a_setting_with_the_papers_default(self):
        self.assertIn("cec_singular_band", read("src/ecinidefs.h"))
        self.assertIn("qreal cec_singular_band = 0.20;", read("src/ecprojectstate.h"))
        self.assertIn("void EcProject::setGeneralCecSingularBand", self.project)

    def test_a_rerun_is_forced_when_a_pairing_changes(self):
        self.assertIn("cecPairs == previousProject.ec_project_state_.projectGeneral.cecPairs",
                      self.project)

    def test_the_eddypro_export_still_strips_every_cec_key(self):
        #> One regex covers cec_meth, cec_singular_band, cec_num and cec_<i>_*.
        self.assertIn(r'removeMatchingKeys(ini, QStringLiteral("^cec_"))', self.project)

    def test_the_stationarity_mode_round_trips_and_defaults_to_the_paper(self):
        """A toggle between two criteria, defaulting to the published one.

        Foken's statistic is relative to the whole-period covariance, so it
        explodes when the flux is near zero - at night, on periods whose
        octants are perfectly well sampled. Checked, the same construction is
        applied to the partition ratio instead. Unchecked reproduces the
        paper, and that is what an untouched project gets.
        """
        self.assertIn('INI_PROJECT_81   = QStringLiteral("cec_stationarity_mode")',
                      read("src/ecinidefs.h"))
        self.assertIn("int cec_stationarity_mode = 0;", read("src/ecprojectstate.h"))
        proj = read("src/ecproject.cpp")
        self.assertIn("project_ini.setValue(EcIni::INI_PROJECT_81, "
                      "ec_project_state_.projectGeneral.cec_stationarity_mode)", proj)
        self.assertIn("EcIni::INI_PROJECT_81", proj)

    def test_an_unrecognised_mode_falls_back_to_the_paper(self):
        #> A project written by some later version must not silently select a
        #> criterion this build does not implement. Same rule as the engine's.
        proj = read("src/ecproject.cpp")
        start = proj.index("= (project_ini.value(EcIni::INI_PROJECT_81,")
        block = proj[start:proj.index("readCecPairs", start)]
        self.assertIn(".toInt() == 1) ? 1 : 0;", block)

    def test_the_checkbox_is_wired_to_it(self):
        dialog = read("src/cecsettingsdialog.cpp")
        self.assertIn("setGeneralCecStationarityMode(on ? 1 : 0)", dialog)
        self.assertIn("ratioStationarityBox->setChecked("
                      "ecProject_->generalCecStationarityMode() == 1)", dialog)
        #> Blocked during refresh like every other control here, or loading a
        #> project marks it modified.
        self.assertIn("const QSignalBlocker ratioStationarityBlocker(ratioStationarityBox);",
                      dialog)
        #> And Restore Defaults puts it back to the paper.
        self.assertIn("ratioStationarityBox->setChecked(false);", dialog)

    def test_the_tooltip_says_which_is_the_published_method(self):
        dialog = read("src/cecsettingsdialog.cpp")
        block = dialog[dialog.index("ratioStationarityBox->setToolTip"):]
        block = block[:block.index("));") + 3]
        #> Both states explained, the published one named, and the column
        #> pointed at - so a user can tell which is which and go and look at
        #> the number. Not pinned to any one phrase describing the mechanism;
        #> that wording has already changed once and the check should not
        #> break when it is improved again.
        self.assertIn("Zahn et al. (2022)", block)
        self.assertIn("<b>Unchecked</b>", block)
        self.assertIn("<b>Checked</b>", block)
        self.assertIn("cec_ns_", block)

    def test_the_format_version_was_bumped_with_the_new_keys(self):
        #> So an older engine refuses the file rather than ignoring cec_num and
        #> silently running one pairing.
        self.assertIn('PROJECT_FILE_VERSION_STR = QStringLiteral("5.1.0")',
                      read("src/defs.h"))


if __name__ == "__main__":
    unittest.main()
