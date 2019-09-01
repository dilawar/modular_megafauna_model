#include "herbivore_base.h"
#include "energetics.h"
#include "environment.h"
#include "net_energy_models.h"
#include "get_forage_demands.h"
#include "herbivore_data.h"
#include "hft.h"
#include "mortality.h"
#include "reproduction.h"

using namespace Fauna;

HerbivoreBase::HerbivoreBase(const int age_days, const double body_condition,
                             const Hft* hft,
                             const Sex sex)
    : hft(check_hft_pointer(hft)),  // can be NULL
      sex(sex),                     // always valid
      age_days(age_days),
      today(-1),  // not initialized yet; call simulate_day() first
      body_condition_gestation(get_hft().gestation_months * 30) {
  // Check validity of parameters
  if (age_days == 0)
    throw std::invalid_argument(
        "Fauna::HerbivoreBase::HerbivoreBase() "
        "Establishment constructor called with age_days==0. "
        "Use the birth constructor instead.");
  if (age_days < 0)
    throw std::invalid_argument(
        "Fauna::HerbivoreBase::HerbivoreBase() "
        "age_days < 0");
  if (get_hft().mortality_factors.count(MF_LIFESPAN) &&
      age_days > get_hft().lifespan * 365)
    throw std::invalid_argument(
        "Fauna::HerbivoreBase::HerbivoreBase() "
        "age_days is greater than maximum lifespan.");
  if (body_condition > 1.0)
    throw std::invalid_argument(
        "Fauna::HerbivoreBase::HerbivoreBase() "
        "body_condition > 1.0");
  if (body_condition < 0.0)
    throw std::invalid_argument(
        "Fauna::HerbivoreBase::HerbivoreBase() "
        "body_condition < 0.0");

  // Create energy budget (validity check inside that class)
  energy_budget = std::auto_ptr<FatmassEnergyBudget>(new FatmassEnergyBudget(
      body_condition * get_max_fatmass(),  // initial fat mass
      get_max_fatmass()));                 // maximum fat mass

  // Create other object instances for std::auto_ptr
  current_output =
      std::auto_ptr<Output::HerbivoreData>(new Output::HerbivoreData());
  get_forage_demands_per_ind =
      std::auto_ptr<GetForageDemands>(new GetForageDemands(hft, sex));
}

HerbivoreBase::HerbivoreBase(const Hft* hft, const Sex sex)
    : hft(check_hft_pointer(hft)),
      sex(sex),
      age_days(0),
      current_output(new Output::HerbivoreData),
      get_forage_demands_per_ind(new GetForageDemands(hft, sex)),
      body_condition_gestation(get_hft().gestation_months * 30) {
  // Create energy budget (validity check inside that class)
  energy_budget = std::auto_ptr<FatmassEnergyBudget>(new FatmassEnergyBudget(
      get_hft().bodyfat_birth * get_hft().bodymass_birth,  // initial fat mass
      get_max_fatmass()));                                 // maximum fat mass

  // Create other object instances for std::auto_ptr
  current_output =
      std::auto_ptr<Output::HerbivoreData>(new Output::HerbivoreData());
  get_forage_demands_per_ind =
      std::auto_ptr<GetForageDemands>(new GetForageDemands(hft, sex));
}

HerbivoreBase::HerbivoreBase(const HerbivoreBase& other)
    : age_days(other.age_days),
      hft(other.hft),
      sex(other.sex),
      body_condition_gestation(get_hft().gestation_months * 30),
      nitrogen(other.nitrogen),
      // Create new object instances for std::auto_ptr with copy construction:
      energy_budget(new FatmassEnergyBudget(other.get_energy_budget())),
      current_output(new Output::HerbivoreData(other.get_todays_output())),
      get_forage_demands_per_ind(new GetForageDemands(other.hft, other.sex)) {}

HerbivoreBase& HerbivoreBase::operator=(const HerbivoreBase& other) {
  if (this != &other) {
    hft = other.hft;
    age_days = other.age_days;
    body_condition_gestation = other.body_condition_gestation;
    nitrogen = other.nitrogen;
    sex = other.sex;

    // Create a new copies of auto_ptr objects.
    // The old ones will be automatically released.
    // For those objects where there is no copy constructor available,
    // we call operator=() to assign values from `other`.
    energy_budget = std::auto_ptr<FatmassEnergyBudget>(
        new FatmassEnergyBudget(other.get_energy_budget()));

    get_forage_demands_per_ind =
        std::auto_ptr<GetForageDemands>(new GetForageDemands(hft, sex));
    *get_forage_demands_per_ind = *other.get_forage_demands_per_ind;

    current_output =
        std::auto_ptr<Output::HerbivoreData>(new Output::HerbivoreData());
    *current_output = *other.current_output;
  }
  return *this;
}

void HerbivoreBase::apply_mortality_factors_today() {
  // Sum of death proportions today. Because different mortality
  // factors are thought to be mutually exclusive (i.e. each death
  // event has exactly one causing factor), we just add them up.
  double mortality_sum = 0.0;

  // iterate through all mortality factors.
  std::set<MortalityFactor>::const_iterator itr;
  for (itr = get_hft().mortality_factors.begin();
       itr != get_hft().mortality_factors.end(); itr++) {
    if (*itr == MF_BACKGROUND) {
      const GetBackgroundMortality background(get_hft().mortality_juvenile,
                                              get_hft().mortality);
      const double mortality = background(get_age_days());
      mortality_sum += mortality;
      // output:
      get_todays_output().mortality[MF_BACKGROUND] = mortality;
    }

    if (*itr == MF_LIFESPAN) {
      const GetSimpleLifespanMortality lifespan(get_hft().lifespan);
      const double mortality = lifespan(get_age_days());
      mortality_sum += mortality;
      // output:
      get_todays_output().mortality[MF_LIFESPAN] = mortality;
    }

    if (*itr == MF_STARVATION_ILLIUS_OCONNOR_2000) {
      double mortality = 0.0;
      const double body_condition = get_fatmass() / get_max_fatmass();
      double new_body_condition = body_condition;

      // Standard deviation of body fat in this cohort.
      // Juveniles (1st year of life) have no variation in body fat
      // so that there is no artificial mortality created if their
      // body fat at birth is very low.
      double bodyfat_deviation = 0;
      if (get_age_years() >= 1) bodyfat_deviation = get_hft().bodyfat_deviation;

      const GetStarvationIlliusOConnor2000 starv_illius(
          bodyfat_deviation, get_hft().shift_body_condition_for_starvation);

      // Call the function object and obtain mortality and new body
      // condition.
      mortality = starv_illius(body_condition, new_body_condition);

      // Apply the changes to the herbivore object
      mortality_sum += mortality;
      if (new_body_condition != body_condition)
        get_energy_budget().force_body_condition(new_body_condition);

      // output:
      get_todays_output().mortality[MF_STARVATION_ILLIUS_OCONNOR_2000] =
          mortality;
    }

    if (*itr == MF_STARVATION_THRESHOLD) {
      // This function object can be static because it is in no way
      // specific to this herbivore instance.
      static const GetStarvationMortalityThreshold starv_thresh;
      const double mortality = starv_thresh(get_bodyfat());
      mortality_sum += mortality;
      // output:
      get_todays_output().mortality[MF_STARVATION_THRESHOLD] = mortality;
    }
  }
  // make sure that mortality does not exceed 1.0
  mortality_sum = std::min(1.0, mortality_sum);

  // Call pure virtual function, which is implemented by derived
  // classes
  apply_mortality(mortality_sum);
}

void HerbivoreBase::eat(const ForageMass& kg_per_km2,
                        const Digestibility& digestibility,
                        const ForageMass& N_kg_per_km2) {
  if (get_ind_per_km2() == 0.0 && kg_per_km2 == 0.0)
    throw std::logic_error(
        "Fauna::HerbivoreBase::eat() "
        "This herbivore has no individuals and cannot be fed.");

  // convert forage from *per km²* to *per individual*
  assert(get_ind_per_km2() != 0.0);
  const ForageMass kg_per_ind = kg_per_km2 / get_ind_per_km2();
  const ForageMass N_kg_per_ind = N_kg_per_km2 / get_ind_per_km2();

  // net energy in the forage [MJ/ind]
  // Divide mass by energy content and set any forage with zero
  // energy content to zero mass.
  const ForageEnergy mj_per_ind =
      get_net_energy_content(digestibility) * kg_per_ind;

  try {
    assert(get_forage_demands_per_ind.get() != NULL);
    // Deduct the eaten forage from today’s maximum intake.
    // This function also checks whether we are violating ingestion constraints.
    get_forage_demands_per_ind->add_eaten(kg_per_ind);
  } catch (const std::logic_error e) {
    throw std::logic_error(std::string(e.what()) +
                           " (Passed on by Fauna::HerbivoreBase::eat().)");
  }

  // Send energy to energy model.
  get_energy_budget().metabolize_energy(mj_per_ind.sum());

  // Add to output
  get_todays_output().eaten_forage_per_ind += kg_per_ind;
  get_todays_output().eaten_forage_per_mass += kg_per_ind / get_bodymass();
  get_todays_output().energy_intake_per_ind += mj_per_ind;
  get_todays_output().energy_intake_per_mass += mj_per_ind / get_bodymass();
  get_todays_output().eaten_nitrogen_per_ind += N_kg_per_ind.sum();

  // Ingest the nitrogen
  nitrogen.ingest(N_kg_per_ind.sum() * get_ind_per_km2());
}

Hft const* HerbivoreBase::check_hft_pointer(const Hft* _hft) {
  // Exception error message is like from a constructor because that’s
  // where this function gets called.
  if (_hft == NULL)
    throw std::invalid_argument(
        "Fauna::HerbivoreBase::HerbivoreBase() "
        "Parameter `hft` is NULL.");
  return _hft;
}

double HerbivoreBase::get_bodyfat() const {
  return get_energy_budget().get_fatmass() / get_bodymass();
}

double HerbivoreBase::get_bodymass() const {
  return get_energy_budget().get_fatmass() + get_lean_bodymass();
}

double HerbivoreBase::get_bodymass_adult() const {
  if (get_sex() == SEX_MALE)
    return get_hft().bodymass_male;
  else
    return get_hft().bodymass_female;
}

double HerbivoreBase::get_conductance() const {
  if (get_hft().conductance == CM_BRADLEY_DEAVERS_1980) {
    return get_conductance_bradley_deavers_1980(get_bodymass());
  } else if (get_hft().conductance == CM_CUYLER_OERITSLAND_2004) {
    // Currently, we only choose winter fur.
    return get_conductance_cuyler_oeritsland_2004(get_bodymass(), FS_WINTER);
  } else
    throw std::logic_error(
        "Fauna::HerbivoreBase::get_conductance() "
        "Conductance model is not implemented.");
}

const HabitatEnvironment& HerbivoreBase::get_environment() const {
  if (environment.get() == NULL)
    throw std::logic_error(
        "Fauna::HerbivoreBase::get_environment() "
        "Member variable `environment` is NULL. `simulate_day()` "
        "must be called first.");
  return *environment;
}

double HerbivoreBase::get_fatmass() const {
  return get_energy_budget().get_fatmass();
}

double HerbivoreBase::get_lean_bodymass() const {
  return get_potential_bodymass() * (1.0 - get_hft().bodyfat_max);
}

ForageMass HerbivoreBase::get_forage_demands(
    const HabitatForage& available_forage) {
  if (is_dead()) return ForageMass(0.0);

  assert(get_forage_demands_per_ind.get() != NULL);

  // Prepare GetForageDemands helper object if not yet done today.
  if (!get_forage_demands_per_ind->is_day_initialized(this->get_today())) {
    // Net energy content [MJ/kgDM]
    const ForageEnergyContent net_energy_content =
        get_net_energy_content(available_forage.get_digestibility());

    get_forage_demands_per_ind->init_today(get_today(), available_forage,
                                           net_energy_content, get_bodymass());

    // Update output
    get_todays_output().energy_content.operator=(net_energy_content);
  }

  // energy demands [MJ/ind] for expenditure plus fat anabolism
  const double total_energy_demands =
      get_energy_budget().get_energy_needs() +
      get_energy_budget().get_max_anabolism_per_day();

  // Use helper object GetForageDemands to calculate per individual.
  const ForageMass demand_ind =
      (*get_forage_demands_per_ind)(total_energy_demands);

  // Convert the demand per individual [kgDM/ind]
  // to demand per area [kgDM/km²]
  return demand_ind * get_ind_per_km2();
}

double HerbivoreBase::get_kg_per_km2() const {
  return get_bodymass() * get_ind_per_km2();
}

double HerbivoreBase::get_max_fatmass() const {
  return get_potential_bodymass() * get_hft().bodyfat_max;
}

ForageEnergyContent HerbivoreBase::get_net_energy_content(
    const Digestibility& digestibility) const {
  if (get_hft().net_energy_model == NE_DEFAULT) {
    const GetNetEnergyContentDefault get_default(get_hft().digestion_type);
    return get_default(digestibility);

    // ADD NEW NET ENERGY MODELS HERE
    // in new if statements
  } else
    throw std::logic_error(
        "Fauna::HerbivoreBase::get_net_energy_content() "
        "Selected net energy model not implemented.");
}

double HerbivoreBase::get_potential_bodymass() const {
  // age of physical maturity in years
  const double maturity_age =
      ((get_sex() == SEX_MALE) ? get_hft().maturity_age_phys_male
                               : get_hft().maturity_age_phys_female);

  if (get_age_years() >= maturity_age)
    return get_bodymass_adult();
  else {
    // CALCULATE BODY MASS FOR PRE-ADULTS

    // lean weight at birth
    const double birth_leanmass =
        get_hft().bodymass_birth * (1.0 - get_hft().bodyfat_birth);

    // potential full mass at birth
    assert(1.0 - get_hft().bodyfat_max > 0.0);
    const double birth_potmass = birth_leanmass / (1.0 - get_hft().bodyfat_max);

    // age fraction from birth to physical maturity
    assert(maturity_age > 0.0);
    const double fraction = (double)get_age_days() / (maturity_age * 365.0);

    // difference from birth to adult
    const double difference = get_bodymass_adult() - birth_potmass;

    return birth_potmass + fraction * difference;
  }
}

int HerbivoreBase::get_today() const {
  if (today == -1)  // initial value from constructor
    throw std::logic_error(
        "Fauna::HerbivoreBase::get_today() "
        "Current day not yet initialized. Has `simulate_day()` "
        "been called first?");
  assert(today >= 0 && today < 365);
  return today;
}

double HerbivoreBase::get_todays_expenditure() const {
  // Sum of all expenditure components [MJ/ind/day]
  double result = 0.0;

  // Iterate through all selected expenditure components and sum up the
  // results of their algorithsms.

  bool add_thermoregulation = false;

  for (std::set<ExpenditureComponent>::const_iterator itr =
           get_hft().expenditure_components.begin();
       itr != get_hft().expenditure_components.end(); itr++) {
    if (*itr == EC_ALLOMETRIC) {
      assert(get_hft().expenditure_allometry.coefficient > 0.0);
      result += get_hft().expenditure_allometry.calc(get_bodymass());
    } else if (*itr == EC_TAYLOR_1981) {
      result +=
          get_expenditure_taylor_1981(get_bodymass(), get_bodymass_adult());
    } else if (*itr == EC_ZHU_2018) {
      result += get_expenditure_zhu_et_al_2018(
          get_bodymass(), get_environment().air_temperature);
    } else if (*itr == EC_THERMOREGULATION) {
      add_thermoregulation = true;
    }
    // ** Add new expenditure components in else-if statements here. **
    else
      throw std::logic_error(
          "Fauna::HerbivoreBase::get_todays_expenditure() "
          "Expenditure component not implemented.");
  }

  // Thermoregulation needs to be “added” to the other energy expenses
  // because any other burning of energy is already heating the body
  // passively.
  if (add_thermoregulation) {
    result += get_thermoregulatory_expenditure(
        result,  // thermoneutral_rate
        get_conductance(), get_hft().core_temperature,
        get_environment().air_temperature);
  }

  assert(result >= 0.0);
  return result;
}

double HerbivoreBase::get_todays_offspring_proportion() const {
  if (get_sex() == SEX_MALE || get_age_years() < get_hft().maturity_age_sex)
    return 0.0;

  // Several models use a BreedingSeason object, so we create one right
  // away.
  const BreedingSeason breeding_season(get_hft().breeding_season_start,
                                       get_hft().breeding_season_length);

  // Use the average body condition (fat mass/maximum fat mass) over the
  // last months of pregnancy.
  const double body_condition = body_condition_gestation.get_average();

  // choose the model
  if (get_hft().reproduction_model == RM_ILLIUS_OCONNOR_2000) {
    // create our model object
    const ReprIlliusOconnor2000 illius_2000(breeding_season,
                                            get_hft().reproduction_max);
    // get today’s value
    return illius_2000.get_offspring_density(get_today(), body_condition);
  } else if (get_hft().reproduction_model == RM_CONST_MAX) {
    const ReproductionConstMax const_max(breeding_season,
                                         get_hft().reproduction_max);
    return const_max.get_offspring_density(get_today());
  } else if (get_hft().reproduction_model == RM_LINEAR) {
    const ReproductionLinear linear(breeding_season,
                                    get_hft().reproduction_max);
    return linear.get_offspring_density(get_today(), body_condition);
  } else if (get_hft().reproduction_model == RM_NONE) {
    return 0.0;
  }
  // ADD NEW MODELS HERE
  // in new if statements
  else
    throw std::logic_error(
        "Fauna::HerbivoreBase::get_todays_offspring_proportion() "
        "Reproduction model not implemented.");
}

const Output::HerbivoreData& HerbivoreBase::get_todays_output() const {
  assert(current_output.get() != NULL);
  return *current_output;
}

Output::HerbivoreData& HerbivoreBase::get_todays_output() {
  assert(current_output.get() != NULL);
  return *current_output;
}

void HerbivoreBase::simulate_day(const int day,
                                 const HabitatEnvironment& _environment,
                                 double& offspring) {
  if (day < 0 || day >= 365)
    throw std::invalid_argument(
        "Fauna::HerbivoreBase::simulate_day() "
        "Argument \"day\" out of range.");
  if (is_dead())
    throw std::invalid_argument(
        "Fauna::HerbivoreBase::simulate_day() "
        "This herbivore is dead. `simulate_day()` must not be called "
        "on a dead herbivore object.");

  // Create a new HabitatEnvironment object in the auto_ptr object by
  // copy-construction from the function parameter.
  environment =
      std::auto_ptr<HabitatEnvironment>(new HabitatEnvironment(_environment));

  // In the following, we wrote doxygen comments in the function body.
  /// - Digest last day’s nitrogen (\ref NitrogenInHerbivore::digest_today())
  nitrogen.digest_today(get_retention_time(get_bodymass()), get_kg_per_km2());

  /// - Set current day.
  today = day;

  /// - Increase age.
  age_days++;

  /// - Update records.
  if (get_sex() == SEX_FEMALE)  // (males don’t need this for reproduction)
    body_condition_gestation.add_value(get_fatmass() / get_max_fatmass());

  /// - Update maximum fat mass and gain in \ref Fauna::FatmassEnergyBudget.
  get_energy_budget().set_max_fatmass(
      get_max_fatmass(),  // max. absolute fat mass
      get_hft().bodyfat_max_daily_gain *
          get_bodymass());  // max. possible gain today

  /// - Add new output.
  get_todays_output().reset();
  get_todays_output().age_years = get_age_years();
  get_todays_output().bodyfat = get_bodyfat();
  get_todays_output().bound_nitrogen = nitrogen.get_unavailable();
  get_todays_output().inddens = get_ind_per_km2();
  get_todays_output().massdens = get_kg_per_km2();

  /// - Catabolize fat to compensate unmet energy needs.
  get_energy_budget().catabolize_fat();

  /// - Add energy needs for today.
  const double todays_expenditure = get_todays_expenditure();
  get_energy_budget().add_energy_needs(todays_expenditure);
  get_todays_output().expenditure = todays_expenditure;

  /// - Calculate offspring.
  offspring = get_todays_offspring_proportion() * get_ind_per_km2();
  get_todays_output().offspring = offspring;

  /// - Apply mortality factor.
  apply_mortality_factors_today();
}

double HerbivoreBase::take_nitrogen_excreta() {
  if (!is_dead())
    return nitrogen.reset_excreta();
  else
    return nitrogen.reset_total();
}
