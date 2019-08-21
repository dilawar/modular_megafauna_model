//////////////////////////////////////////////////////////////////////////
/// \file
/// \brief   \ref Fauna::Habitat implementations for testing purpose.
/// \author  Wolfgang Pappa, Senckenberg BiK-F
/// \date    June 2017
//////////////////////////////////////////////////////////////////////////

#ifndef TESTHABITATS_H
#define TESTHABITATS_H

#include <vector>
#include "megafauna.h"

using namespace Fauna;

// forward declarations
namespace Fauna {
class SimulationUnit;
}

namespace FaunaSim {

/// Helper class for performing simple grass growth to test herbivore
/// functionality
class LogisticGrass {
 public:
  /// Settings for grass growth
  struct Parameters {
    /// Constructor with arbitrary simple, *valid* values, but no growth.
    Parameters() : fpc(0.1), init_mass(0.0), reserve(0.1), saturation(1.0) {
      digestibility.push_back(.5);
      decay_monthly.push_back(0.0);
      growth_monthly.push_back(0.0);
    }

    /// Proportional daily rates of grass decay [day^-1]
    /**
     * This is a vector of *daily* decay rates for each month. When
     * the end of the vector is reached, the values are recycled.
     * A vector of length 12 creates the same behaviour every year.
     */
    std::vector<double> decay_monthly;

    /// Proportional digestibility of the grass [frac].
    /**
     * This is a vector of digestibility values for each month. When
     * the end of the vector is reached, the values are recycled.
     * A vector of length 12 creates the same behaviour every year.
     */
    std::vector<double> digestibility;

    /// \brief Percentage of habitat covered with grass
    ///        (Foliar Percentage Cover) [frac]
    double fpc;

    /// Proportional daily grass growth rates [day^-1]
    /**
     * This is a vector of *daily* growth rates for each month. When
     * the end of the vector is reached, the values are recycled.
     * A vector of length 12 creates the same behaviour every year.
     */
    std::vector<double> growth_monthly;

    /// Initial available forage [kgDM/km²]
    /** This should be smaller than \ref saturation */
    double init_mass;

    /// \brief Ungrazable grass biomass reserve, inaccessable
    ///        to herbivores [kgDM/km²]
    /** Owen-Smith (2002) gives value of 20 g/m²*/
    double reserve;

    /// Saturation grass biomass [kgDM/m²]
    /** Owen-Smith (2002): 200 g/m²*/
    double saturation;

    /// Check if parameters are valid
    /**
     * \param[out] msg Possible warnings and error messages.
     * \return true if values are valid, false if not.
     */
    bool is_valid(std::string& msg) const;
  };

  /// Constructor
  /**
   * \throw std::invalid_argument If `settings` are not valid.
   */
  LogisticGrass(const LogisticGrass::Parameters& settings);

  /// Perform grass growth and decay for one day.
  /**
   * \param day_of_year January 1st = 0
   * \throw std::invalid_argument if not `0<=day_of_year<=364`
   */
  void grow_daily(const int day_of_year);

  /// Get current grass forage
  const GrassForage& get_forage() const { return forage; }

  /// Set the grass forage
  void set_forage(const GrassForage& f) { forage = f; }

 private:
  /// Current forage
  /** Excluding the reserve
   * \ref LogisticGrass::Parameters::reserve. */
  GrassForage forage;

  LogisticGrass::Parameters settings;

  /// The current simulation month, starting with zero.
  /** We need this to address the current value in
   * \ref Parameters::growth_monthly and
   * \ref Parameters::decay_monthly.
   */
  int simulation_month;
};

/// A herbivore habitat independent of the LPJ-GUESS framework for testing.
class SimpleHabitat : public Habitat {
 public:
  /// Simulation parameters for a \ref SimpleHabitat object.
  struct Parameters {
    /// Parameters for logistic grass growth.
    LogisticGrass::Parameters grass;

    /// Snow depth [cm] for each month.
    /** When the end of the vector is reached, the values are recycled.
     * A vector of length 12 creates the same behaviour every year. */
    std::vector<double> snow_depth_monthly;
  };

  /// Constructor with simulation settings.
  /**
   * \param settings Simulation settings for the vegetation
   * model.
   */
  SimpleHabitat(const SimpleHabitat::Parameters settings)
      : settings(settings),
        grass(settings.grass),
        simulation_month(0),
        snow_depth(0.0) {}

 public:  // ------ Fauna::Habitat implementations ----
  virtual void add_excreted_nitrogen(const double) {}  // disabled
  virtual HabitatForage get_available_forage() const {
    HabitatForage result;
    result.grass = grass.get_forage();
    return result;
  }
  virtual HabitatEnvironment get_environment() const;
  virtual void init_day(const int today);
  virtual void remove_eaten_forage(const ForageMass& eaten_forage);

 protected:
  /// Perform daily growth.
  /** \param day_of_year January 1st = 0 */
  virtual void grow_daily(const int day_of_year) {
    grass.grow_daily(day_of_year);
  }

 private:
  SimpleHabitat::Parameters settings;

  /// Snow depth in cm, as read from \ref Parameters::snow_depth_monthly.
  double snow_depth;

  /// Grass in the habitat
  LogisticGrass grass;

  /// The current simulation month, starting with zero.
  /** We need this to address the current value in
   * \ref Parameters::snow_depth_monthly. */
  int simulation_month;
};

}  // namespace FaunaSim

#endif  // TESTHABITATS_H
