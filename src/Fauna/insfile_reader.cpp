/**
 * \file
 * \brief Read the instruction file with HFTs and global parameters.
 * \copyright ...
 * \date 2019
 */
#include "insfile_reader.h"
#include <algorithm>
#include <string>
#include "hft.h"

using namespace Fauna;

namespace {
/// Convert given string to lowercase.
std::string lowercase(const std::string& s) {
  std::string result;
  std::transform(s.begin(), s.end(), std::back_inserter(result), ::tolower);
  return result;
}

}  // namespace

//============================================================
// invalid_option
//============================================================

std::string invalid_option::construct_message(
    const std::string& key, const std::string& value,
    const std::set<std::string>& valid_options) {
  if (valid_options.empty())
    throw std::logic_error(
        "Fauna::invalid_option No set of valid options provided.");
  std::stringstream msg;
  msg << "Invalid option for parameter \"" << key << "\"\n";
  msg << "This was in the instruction file: \"" << value << "\"\n";
  msg << "These are the valid options:";
  for (const auto& s : valid_options) msg << " \"" << s << "\"";
  return msg.str();
}

//============================================================
// InsfileReader
//============================================================

InsfileReader::InsfileReader(const std::string filename)
    : ins(cpptoml::parse_file(filename)) {
  // Read global parameters
  read_table_output();
  if (params.output_format == OutputFormat::TextTables)
    read_table_output_text_tables();
  read_table_simulation();

  {
    std::string err_msg;
    if (!params.is_valid(err_msg))
      throw std::runtime_error("Parameters are not valid:\n" + err_msg);
  }

  auto hft_table_array = ins->get_table_array("hft");
  if (hft_table_array)
    for (const auto& hft_table : *hft_table_array) {
      const Hft hft = read_hft(hft_table);
      std::string err_msg;
      if (!hft.is_valid(params, err_msg))
        throw std::runtime_error("HFT \"" + hft.name + "\" is not valid:\n" +
                                 err_msg);
      hfts.insert(std::move(hft));
    }
}

std::shared_ptr<cpptoml::table> InsfileReader::get_group_table(
    const std::string& group_name) const {
  auto group_table_array = ins->get_table_array("group");
  if (group_table_array)
    for (const auto& group_table : *group_table_array) {
      const auto name = group_table->get_as<std::string>("name");
      if (!name) throw missing_parameter("group.name");
      if (*name == group_name) return group_table;
    }
  return std::shared_ptr<cpptoml::table>(NULL);  // nothing found
}

template <class T>
cpptoml::option<T> InsfileReader::find_hft_parameter(
    const std::shared_ptr<cpptoml::table>& hft_table, const std::string& key,
    const bool mandatory) const {
  assert(hft_table->get_as<std::string>("name"));  // Name must be defined.
  const std::string name = *hft_table->get_as<std::string>("name");

  {
    const auto value = hft_table->get_qualified_as<T>(key);
    if (value) return value;
  }

  const auto groups = hft_table->get_array_of<std::string>("groups");
  if (groups)
    for (const auto& g : *groups) {
      const auto group_table = get_group_table(g);
      if (!group_table) throw missing_group(name, g);
      {
        const auto value = group_table->get_qualified_as<T>(key);
        if (value) return value;
      }
    }

  // If we reach this point, nothing has been found.
  if (mandatory)
    throw missing_hft_parameter(name, key);
  else
    return cpptoml::option<T>();  // empty pointer
}

template <class T>
typename cpptoml::array_of_trait<T>::return_type
InsfileReader::find_hft_array_parameter(
    const std::shared_ptr<cpptoml::table>& hft_table, const std::string& key,
    const bool mandatory) const {
  assert(hft_table->get_as<std::string>("name"));  // Name must be defined.
  const std::string name = *hft_table->get_as<std::string>("name");

  {
    const auto value = hft_table->get_qualified_array_of<T>(key);
    if (value) return value;
  }

  const auto groups = hft_table->get_array_of<std::string>("groups");
  if (groups)
    for (const auto& g : *groups) {
      const auto group_table = get_group_table(g);
      if (!group_table) throw missing_group(name, g);
      {
        const auto value = group_table->get_qualified_array_of<T>(key);
        if (value) return value;
      }
    }

  // If we reach this point, nothing has been found.
  if (mandatory)
    throw missing_hft_parameter(name, key);
  else {
    typename cpptoml::array_of_trait<T>::return_type empty;  // empty pointer
    return empty;
  }
}

Hft InsfileReader::read_hft(const std::shared_ptr<cpptoml::table>& table) {
  Hft hft;
  {
    const auto key = "name";
    auto value = table->get_as<std::string>("name");
    if (value)
      hft.name = *value;
    else
      throw missing_parameter("hft.name");
  }

  // ======= MANDATORY PARAMETERS =======
  {
    const auto value =
        find_hft_parameter<double>(table, "body_fat.maximum", true);
    assert(value);
    hft.body_fat_maximum = *value;
  }
  {
    const auto value =
        find_hft_parameter<double>(table, "body_fat.maximum_daily_gain", true);
    assert(value);
    hft.body_fat_maximum_daily_gain = *value;
  }
  {
    const auto value = find_hft_parameter<int>(table, "body_mass.female", true);
    assert(value);
    hft.body_mass_female = *value;
  }
  {
    const auto value = find_hft_parameter<int>(table, "body_mass.male", true);
    assert(value);
    hft.body_mass_male = *value;
  }
  {
    const auto value =
        find_hft_parameter<std::string>(table, "digestion.limit", true);
    assert(value);
    if (lowercase(*value) == lowercase("None"))
      hft.digestion_limit = DigestiveLimit::None;
    else if (lowercase(*value) == lowercase("Allometric"))
      hft.digestion_limit = DigestiveLimit::Allometric;
    else if (lowercase(*value) == lowercase("FixedFraction"))
      hft.digestion_limit = DigestiveLimit::FixedFraction;
    else if (lowercase(*value) == lowercase("IlliusGordon1992"))
      hft.digestion_limit = DigestiveLimit::IlliusGordon1992;
    else
      throw invalid_option(
          hft, "digestion.limit", *value,
          {"None", "Allometric", "FixedFraction", "IlliusGordon1992"});
  }
  {
    const auto value =
        find_hft_parameter<std::string>(table, "digestion.type", true);
    assert(value);
    if (lowercase(*value) == lowercase("Hindgut"))
      hft.digestion_type = DigestionType::Hindgut;
    else if (lowercase(*value) == lowercase("Ruminant"))
      hft.digestion_type = DigestionType::Ruminant;
    else
      throw invalid_option(hft, "digestion.type", *value,
                           {"Hindgut", "Ruminant"});
  }
  {
    const auto value =
        find_hft_parameter<int>(table, "establishment.age_range.first", true);
    assert(value);
    hft.establishment_age_range.first = *value;
  }
  {
    const auto value =
        find_hft_parameter<int>(table, "establishment.age_range.last", true);
    assert(value);
    hft.establishment_age_range.second = *value;
  }
  {
    const auto value =
        find_hft_parameter<double>(table, "establishment.density", true);
    assert(value);
    hft.establishment_density = *value;
  }
  {
    const auto array = find_hft_array_parameter<std::string>(
        table, "expenditure.components", true);
    assert(array);
    hft.expenditure_components = {};
    for (const auto& i : *array)
      if (lowercase(i) == lowercase("Allometric"))
        hft.expenditure_components.insert(ExpenditureComponent::Allometric);
      else if (lowercase(i) == lowercase("Taylor1981"))
        hft.expenditure_components.insert(ExpenditureComponent::Taylor1981);
      else if (lowercase(i) == lowercase("Thermoregulation"))
        hft.expenditure_components.insert(
            ExpenditureComponent::Thermoregulation);
      else if (lowercase(i) == lowercase("Zhu2018"))
        hft.expenditure_components.insert(ExpenditureComponent::Zhu2018);
      else
        throw invalid_option(
            hft, "expenditure.components", i,
            {"Allometric", "Taylor1981", "Thermoregulation", "Zhu2018"});
  }
  {
    const auto value =
        find_hft_parameter<std::string>(table, "foraging.diet_composer", true);
    assert(value);
    if (lowercase(*value) == lowercase("PureGrazer"))
      hft.foraging_diet_composer = DietComposer::PureGrazer;
    else
      throw invalid_option(hft, "foraging.diet_composer", *value,
                           {"PureGrazer"});
  }
  {
    const auto value = find_hft_parameter<std::string>(
        table, "foraging.net_energy_model", true);
    assert(value);
    if (lowercase(*value) == lowercase("Default"))
      hft.foraging_net_energy_model = NetEnergyModel::Default;
    else
      throw invalid_option(hft, "foraging.net_energy_model", *value,
                           {"Default"});
  }
  {
    const auto array = find_hft_array_parameter<std::string>(
        table, "hft.mortality.factors", false);
    hft.mortality_factors = {};
    if (array) {
      for (const auto& i : *array)
        if (lowercase(i) == lowercase("Background"))
          hft.mortality_factors.insert(MortalityFactor::Background);
        else if (lowercase(i) == lowercase("Lifespan"))
          hft.mortality_factors.insert(MortalityFactor::Lifespan);
        else if (lowercase(i) == lowercase("StarvationIlliusOConnor2000"))
          hft.mortality_factors.insert(
              MortalityFactor::StarvationIlliusOConnor2000);
        else if (lowercase(i) == lowercase("StarvationThreshold"))
          hft.mortality_factors.insert(MortalityFactor::StarvationThreshold);
        else
          throw invalid_option(
              hft, "hft.mortality.factors", i,
              {"Background", "Lifespan", "StarvationIlliusOConnor2000",
               "StarvationThreshold"});
    }
  }
  {
    const auto value =
        find_hft_parameter<std::string>(table, "reproduction.model", true);
    assert(value);
    if (lowercase(*value) == lowercase("None")) {
      hft.reproduction_model = ReproductionModel::None;
      hft.life_history_physical_maturity_female = 1;
      hft.life_history_physical_maturity_male = 1;
    } else if (lowercase(*value) == lowercase("ConstantMaximum"))
      hft.reproduction_model = ReproductionModel::ConstantMaximum;
    else if (lowercase(*value) == lowercase("IlliusOConnor2000"))
      hft.reproduction_model = ReproductionModel::IlliusOConnor2000;
    else if (lowercase(*value) == lowercase("Linear"))
      hft.reproduction_model = ReproductionModel::Linear;
    else
      throw invalid_option(
          hft, "reproduction.model", *value,
          {"None", "ConstantMaximum", "IlliusOConnor2000", "Linear"});
  }

  // ======== NON-MANDATORY PARAMETERS =======
  {
    const auto array =
        find_hft_array_parameter<std::string>(table, "foraging.limits", false);
    hft.foraging_limits = {};
    if (array) {
      for (const auto& i : *array)
        if (lowercase(i) == lowercase("GeneralFunctionalResponse"))
          hft.foraging_limits.insert(ForagingLimit::GeneralFunctionalResponse);
        else if (lowercase(i) == lowercase("IlliusOConnor2000"))
          hft.foraging_limits.insert(ForagingLimit::IlliusOConnor2000);
        else
          throw invalid_option(
              hft, "foraging.limits", i,
              {"GeneralFunctionalResponse", "IlliusOConnor2000"});
    }
  }

  // ======== DEPENDENT PARAMETERS =======

  if (hft.reproduction_model != ReproductionModel::None) {
    const auto value =
        find_hft_parameter<double>(table, "body_fat.birth", true);
    assert(value);
    hft.body_fat_birth = *value;
  }

  if (hft.mortality_factors.count(
          MortalityFactor::StarvationIlliusOConnor2000)) {
    const auto value =
        find_hft_parameter<double>(table, "body_fat.deviation", true);
    assert(value);
    hft.body_fat_deviation = *value;
  }

  if (hft.reproduction_model != ReproductionModel::None) {
    const auto value = find_hft_parameter<int>(table, "body_mass.birth", true);
    assert(value);
    hft.body_mass_birth = *value;
  }

  if (hft.reproduction_model != ReproductionModel::None) {
    const auto value =
        find_hft_parameter<int>(table, "breeding_season.length", true);
    assert(value);
    hft.breeding_season_length = *value;
  }

  if (hft.reproduction_model != ReproductionModel::None) {
    const auto value =
        find_hft_parameter<int>(table, "breeding_season.start", true);
    assert(value);
    hft.breeding_season_start = *value;
  }

  if (hft.digestion_limit == DigestiveLimit::FixedFraction) {
    const auto value =
        find_hft_parameter<double>(table, "digestion.fixed_fraction", true);
    assert(value);
    hft.digestion_fixed_fraction = *value;
  }

  if (hft.digestion_limit == DigestiveLimit::Allometric) {
    const auto value = find_hft_parameter<double>(
        table, "digestion.allometric.coefficient", true);
    assert(value);
    hft.digestion_allometric.coefficient = *value;
  }

  if (hft.digestion_limit == DigestiveLimit::Allometric) {
    const auto value = find_hft_parameter<double>(
        table, "digestion.allometric.exponent", true);
    assert(value);
    hft.digestion_allometric.exponent = *value;
  }

  if (hft.expenditure_components.count(ExpenditureComponent::Allometric)) {
    const auto value = find_hft_parameter<double>(
        table, "expenditure.allometric.coefficient", true);
    assert(value);
    hft.expenditure_allometric.coefficient = *value;
  }

  if (hft.expenditure_components.count(ExpenditureComponent::Allometric)) {
    const auto value = find_hft_parameter<double>(
        table, "expenditure.allometric.exponent", true);
    assert(value);
    hft.expenditure_allometric.exponent = *value;
  }

  if (hft.foraging_limits.count(ForagingLimit::GeneralFunctionalResponse) ||
      hft.foraging_limits.count(ForagingLimit::IlliusOConnor2000)) {
    const auto value = find_hft_parameter<double>(
        table, "foraging.half_max_intake_density", true);
    assert(value);
    hft.foraging_half_max_intake_density = *value;
  }

  if (hft.mortality_factors.count(MortalityFactor::Lifespan)) {
    const auto value =
        find_hft_parameter<int>(table, "life_history.lifespan", true);
    assert(value);
    hft.life_history_lifespan = *value;
  }

  if (hft.reproduction_model != ReproductionModel::None) {
    const auto value = find_hft_parameter<int>(
        table, "life_history.physical_maturity_female", true);
    assert(value);
    hft.life_history_physical_maturity_female = *value;
  }

  if (hft.reproduction_model != ReproductionModel::None) {
    const auto value = find_hft_parameter<int>(
        table, "life_history.physical_maturity_male", true);
    assert(value);
    hft.life_history_physical_maturity_male = *value;
  }

  if (hft.reproduction_model != ReproductionModel::None) {
    const auto value =
        find_hft_parameter<int>(table, "life_history.sexual_maturity", true);
    assert(value);
    hft.life_history_sexual_maturity = *value;
  }

  if (hft.mortality_factors.count(MortalityFactor::Background)) {
    const auto value =
        find_hft_parameter<double>(table, "mortality.adult_rate", true);
    assert(value);
    hft.mortality_adult_rate = *value;
  }

  if (hft.mortality_factors.count(MortalityFactor::Background)) {
    const auto value =
        find_hft_parameter<double>(table, "mortality.juvenile_rate", true);
    assert(value);
    hft.mortality_juvenile_rate = *value;
  }

  if (hft.reproduction_model != ReproductionModel::None) {
    const auto value = find_hft_parameter<double>(
        table, "mortality.minimum_density_threshold", true);
    assert(value);
    hft.mortality_minimum_density_threshold = *value;
  }

  if (hft.mortality_factors.count(MortalityFactor::StarvationThreshold) ||
      hft.mortality_factors.count(
          MortalityFactor::StarvationIlliusOConnor2000)) {
    const auto value = find_hft_parameter<bool>(
        table, "mortality.shift_body_condition_for_starvation", true);
    assert(value);
    hft.mortality_shift_body_condition_for_starvation = *value;
  }

  if (hft.reproduction_model == ReproductionModel::ConstantMaximum ||
      hft.reproduction_model == ReproductionModel::IlliusOConnor2000 ||
      hft.reproduction_model == ReproductionModel::Linear) {
    const auto value =
        find_hft_parameter<double>(table, "reproduction.annual_maximum", true);
    assert(value);
    hft.reproduction_annual_maximum = *value;
  }

  if (hft.reproduction_model != ReproductionModel::None) {
    const auto value =
        find_hft_parameter<int>(table, "reproduction.gestation_length", true);
    assert(value);
    hft.reproduction_gestation_length = *value;
  }

  if (hft.expenditure_components.count(
          ExpenditureComponent::Thermoregulation)) {
    const auto value = find_hft_parameter<std::string>(
        table, "thermoregulation.conductance", true);
    assert(value);
    if (lowercase(*value) == lowercase("BradleyDeavers1980"))
      hft.thermoregulation_conductance = ConductanceModel::BradleyDeavers1980;
    else if (lowercase(*value) == lowercase("CuylerOeritsland2004"))
      hft.thermoregulation_conductance = ConductanceModel::CuylerOeritsland2004;
    else
      throw invalid_option(hft, "thermoregulation.conductance", *value,
                           {"BradleyDeavers1980", "CuylerOeritsland2004"});
  }

  if (hft.expenditure_components.count(
          ExpenditureComponent::Thermoregulation)) {
    const auto value = find_hft_parameter<double>(
        table, "thermoregulation.core_temperature", true);
    assert(value);
    hft.thermoregulation_core_temperature = *value;
  }

  return hft;
}

void InsfileReader::read_table_output() {
  {
    const auto key = "output.format";
    auto value = ins->get_qualified_as<std::string>(key);
    if (value) {
      if (lowercase(*value) == lowercase("TextTables"))
        params.output_format = OutputFormat::TextTables;
      // -> Add new output formats here.
      else
        throw invalid_option(key, *value, {"TextTables"});
    } else
      throw missing_parameter(key);
  }
  {
    const auto key = "output.interval";
    auto value = ins->get_qualified_as<std::string>(key);
    if (value) {
      if (lowercase(*value) == lowercase("Daily"))
        params.output_interval = OutputInterval::Daily;
      else if (lowercase(*value) == lowercase("Monthly"))
        params.output_interval = OutputInterval::Monthly;
      else if (lowercase(*value) == lowercase("Annual"))
        params.output_interval = OutputInterval::Annual;
      else if (lowercase(*value) == lowercase("Decadal"))
        params.output_interval = OutputInterval::Decadal;
      else
        throw invalid_option(key, *value,
                             {"Daily", "Monthly", "Decadal", "Annual"});
    } else
      throw missing_parameter(key);
  }
}

void InsfileReader::read_table_output_text_tables() {
  {
    const auto key = "output.text_tables.directory";
    auto value = ins->get_qualified_as<std::string>(key);
    if (value) {
      params.output_text_tables.directory = *value;
    } else
      throw missing_parameter(key);
  }
  {
    const auto key = "output.text_tables.precision";
    auto value = ins->get_qualified_as<int>(key);
    if (value) {
      params.output_text_tables.precision = *value;
    }
  }
  {
    const auto key = "output.text_tables.tables";
    auto value = ins->get_qualified_array_of<std::string>(key);
    if (value) {
      for (const auto& s : *value)
        if (lowercase(s) == "digestibility")
          params.output_text_tables.digestibility = true;
        else if (lowercase(s) == "mass_density_per_hft")
          params.output_text_tables.mass_density_per_hft = true;
        // -> Add new output tables here (alphabetical order).
        else
          throw invalid_option(key, s,
                               {"digestibility", "mass_density_per_hft"});
    }
  }
}

void InsfileReader::read_table_simulation() {
  {
    const auto key = "simulation.forage_distribution";
    auto value = ins->get_qualified_as<std::string>(key);
    if (value) {
      if (lowercase(*value) == lowercase("Equally"))
        params.forage_distribution = ForageDistributionAlgorithm::Equally;
      // -> Add new forage distribution algorithm here.
      else
        throw invalid_option(key, *value, {"Equally"});
    }
  }
  {
    const auto key = "simulation.habitat_area_km2";
    auto value = ins->get_qualified_as<double>(key);
    if (value) {
      params.habitat_area_km2 = *value;
    }
  }
  {
    const auto key = "simulation.establishment_interval";
    auto value = ins->get_qualified_as<int>(key);
    if (value) {
      params.herbivore_establish_interval = *value;
    }
  }
  {
    const auto key = "simulation.herbivore_type";
    auto value = ins->get_qualified_as<std::string>(key);
    if (value) {
      if (lowercase(*value) == lowercase("Cohort"))
        params.herbivore_type = HerbivoreType::Cohort;
      else if (lowercase(*value) == lowercase("Individual"))
        params.herbivore_type = HerbivoreType::Individual;
      else
        throw invalid_option(key, *value, {"Cohort", "Individual"});
    } else
      throw missing_parameter(key);
  }
  {
    const auto key = "simulation.one_hft_per_habitat";
    auto value = ins->get_qualified_as<bool>(key);
    if (value) {
      params.one_hft_per_habitat = *value;
    }
  }
}
