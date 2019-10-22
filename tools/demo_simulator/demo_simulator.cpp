/**
 * \file
 * \brief A minimal program to show how to use the megafauna library.
 * \copyright ...
 * \date 2019
 */
#include "demo_simulator.h"
#include <cassert>
#include <cfloat>
#include <climits>
#include <iostream>
#include "cpptoml.h"
#include "megafauna.h"
#include "simple_habitat.h"

using namespace Fauna;
using namespace Fauna::Demo;

namespace {
/// Convert g/m² to kg/km².
double g_m2_to_kg_km2(const double g_m2) { return g_m2 * 1000; }
}  // namespace

/// Run the demo simulation with parameters read from instruction file
/** \todo Print version, print help */
int main(int argc, char* argv[]) {
  std::string insfile_fauna;
  std::string insfile_demo;
  try {
    std::cerr << "This is the demo simulator for the Modular Megafauna Model."
              << std::endl;

    // The singleton instance for managing the whole simulation.
    Framework& framework = Framework::get_instance();

    // Read ins file from command line parameters.
    // We expect two arguments: the two instruction files.
    if (argc == 3) {
      if (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-help")
        framework.print_help();
      else {
        insfile_fauna = argv[1];
        insfile_demo = argv[2];
      }
    } else {
      framework.print_usage();
      return EXIT_FAILURE;
    }

    // Run the simulation with the global parameters
    const bool success = framework.run(insfile_fauna, insfile_demo);
    if (!success) {
      std::cerr << "Exiting simulation." << std::endl;
      return EXIT_FAILURE;
    }

  } catch (const std::exception& e) {
    std::cerr << "Unhandled exception:\n" << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  std::cerr << "Successfully finished." << std::endl;
  return EXIT_SUCCESS;
}

////////////////////////////////////////////////////////

void Framework::print_help() {
  // We use C++11 raw string literals like a Bash Here Document.
  // TODO: complete help message
  std::cout << R"EOF(
This is a stub help message.
)EOF";
}

void Framework::print_usage() {
  // We use C++11 raw string literals like a Bash Here Document.
  std::cerr << R"EOF(
Usage:
  megafauna_demo_simulator <fauna_instruction_file> <simulation_instruction_file>
  megafauna_demo_simulator -help
)EOF";
}

void Framework::read_instruction_file(const std::string filename) {
  const auto ins = cpptoml::parse_file(filename);

  {
    const std::string key = "general.years";
    const auto value = ins->get_qualified_as<int>(key);
    if (value) {
      params.nyears = *value;
      if (params.nyears < 1)
        throw std::runtime_error(key + " must be greater than 1.");
    } else
      throw missing_parameter(key);
  }
  {
    const std::string key = "general.habitat_groups";
    const auto value = ins->get_qualified_as<int>(key);
    if (value) {
      params.ngroups = *value;
      if (params.ngroups < 1)
        throw std::runtime_error(key + " must be greater than 1.");
    } else
      throw missing_parameter(key);
  }
  {
    const std::string key = "general.habitats_per_group";
    const auto value = ins->get_qualified_as<int>(key);
    if (value) {
      params.nhabitats_per_group = *value;
      if (params.nhabitats_per_group < 1)
        throw std::runtime_error(key + " must be greater than 1.");
    } else
      throw missing_parameter(key);
  }
  {
    const std::string key = "environment.snow_depth";
    const auto value = ins->get_qualified_array_of<double>(key);
    if (value) {
      params.habitat.snow_depth_monthly = *value;
      for (const auto& i : params.habitat.snow_depth_monthly)
        if (i < 0) throw std::runtime_error(key + " must be greater than 0.");
    } else
      throw missing_parameter(key);
  }
  {
    const std::string key = "grass.daily_decay_rate";
    const auto value = ins->get_qualified_array_of<double>(key);
    if (value) {
      params.habitat.grass.decay_monthly = *value;
      for (const auto& i : params.habitat.grass.decay_monthly) {
        if (i < 0) throw std::runtime_error(key + " must be greater than 0.");
        if (i > 1) throw std::runtime_error(key + " must be smaller than 1.");
      }
    } else
      throw missing_parameter(key);
  }
  {
    const std::string key = "grass.daily_growth_rate";
    const auto value = ins->get_qualified_array_of<double>(key);
    if (value) {
      params.habitat.grass.growth_monthly = *value;
      for (const auto& i : params.habitat.grass.growth_monthly) {
        if (i < 0) throw std::runtime_error(key + " must be greater than 0.");
        if (i > 1) throw std::runtime_error(key + " must smaller than 1.");
      }
    } else
      throw missing_parameter(key);
  }
  {
    const std::string key = "grass.digestibility";
    const auto value = ins->get_qualified_array_of<double>(key);
    if (value) {
      params.habitat.grass.digestibility = *value;
      for (const auto& i : params.habitat.grass.digestibility) {
        if (i < 0) throw std::runtime_error(key + " must be greater than 0.");
        if (i > 1) throw std::runtime_error(key + " must smaller than 1.");
      }
    } else
      throw missing_parameter(key);
  }
  {
    const std::string key = "grass.fpc";
    const auto value = ins->get_qualified_as<double>(key);
    if (value) {
      params.habitat.grass.fpc = *value;
      if (params.habitat.grass.fpc <= 0.0 || params.habitat.grass.fpc > 1.0)
        throw std::runtime_error(key + " must be between 0 and 1.");
    } else
      throw missing_parameter(key);
  }
  {
    const std::string key = "grass.initial_mass";
    const auto value = ins->get_qualified_as<int>(key);
    if (value) {
      params.habitat.grass.init_mass = g_m2_to_kg_km2(*value);
      if (params.habitat.grass.init_mass <= 0)
        throw std::runtime_error(key + " must be greater than 0.");
    } else
      throw missing_parameter(key);
  }
  {
    const std::string key = "grass.saturation_mass";
    const auto value = ins->get_qualified_as<int>(key);
    if (value) {
      params.habitat.grass.saturation = g_m2_to_kg_km2(*value);
      if (params.habitat.grass.saturation < params.habitat.grass.init_mass)
        throw std::runtime_error(key +
                                 " must be greater than grass.initial_mass.");
    } else
      throw missing_parameter(key);
  }
  {
    const std::string key = "grass.ungrazeable_reserve";
    const auto value = ins->get_qualified_as<int>(key);
    if (value) {
      params.habitat.grass.reserve = g_m2_to_kg_km2(*value);
      if (params.habitat.grass.reserve >= params.habitat.grass.saturation)
        throw std::runtime_error(
            key + " must be smaller than grass.saturation_mass.");
    } else
      throw missing_parameter(key);
  }
}

bool Framework::run(const std::string insfile_fauna,
                    const std::string insfile_demo) {
  Fauna::Parameters global_params;
  std::unique_ptr<Fauna::World> fauna_world;

  try {
    fauna_world.reset(new Fauna::World(insfile_fauna));
  } catch (const std::runtime_error& e) {
    std::cerr << "An exception ocurred while creating Fauna::World.\n"
              << e.what() << std::endl;
    return false;
  }

  try {
    read_instruction_file(insfile_demo);
  } catch (const std::runtime_error& e) {
    std::cerr << "Bad instruction file: \"" << insfile_demo << "\"\n"
              << e.what() << std::endl;
    return false;
  }

  // Container for all the groups, each being a vector of
  // simulation units.
  for (int g = 0; g < params.ngroups; g++) {
    const std::string aggregation_unit = std::to_string(g);
    // Fill one group with habitats and populations
    for (int h = 0; h < params.nhabitats_per_group; h++) {
      try {
        // We only pass the pointer to the new habitat to the megafauna
        // library, so special care is needed that it will stay valid.
        fauna_world->create_simulation_unit(
            new SimpleHabitat(params.habitat, aggregation_unit));
      } catch (const std::exception& e) {
        std::cerr << "Exception during habitat creation:" << std::endl
                  << "group number " << g << " of " << params.ngroups << '\n'
                  << "habitat number " << h << " of "
                  << params.nhabitats_per_group << '\n'
                  << "Exception message:\n"
                  << std::endl
                  << e.what();
        return false;
      }
    }
  }

  std::cerr << "Starting simulation." << std::endl;

  for (int year = 0; year < params.nyears; year++) {
    for (int day_of_year = 0; day_of_year < 365; day_of_year++) {
      // VEGATATION AND HERBIVORE SIMULATION
      const bool do_herbivores = true;
      const Date date(day_of_year, year);
      try {
        // The Fauna::World class will take care to iterate over all habitat
        // groups.
        fauna_world->simulate_day(date, do_herbivores);
      } catch (const std::exception& e) {
        std::cerr << "Exception during herbivore simulation:\n"
                  << e.what() << std::endl;
        return false;
      }
    }  // day loop: end of year

    // PRINT PROGRESS
    const int progress_interval = params.nyears / 10;  // every 10%
    if (year % progress_interval == 0 || year == params.nyears - 1)
      std::cerr << "Progress: " << (100 * year) / (params.nyears - 1)
                << std::endl;
  }  // year loop

  return true;  // success!
}
