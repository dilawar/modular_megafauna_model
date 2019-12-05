/**
 * \file
 * \brief Different models how to restrict daily forage intake of herbivores.
 * \copyright ...
 * \date 2019
 */
#ifndef FAUNA_FORAGING_LIMITS_H
#define FAUNA_FORAGING_LIMITS_H

#include "forage_values.h"
#include "hft.h"

namespace Fauna {

/// Get digestion-limited daily net energy intake after Illius & Gordon (1992)
/**
 * The model of digestive passage rates by Illius & Gordon
 * (1992)\cite illius1992modelling constrains maximum daily energy intake
 * \f$I_{dig[MJ/day]}\f$ by gut size and retention time.
 *
 * \f[
 * I_{dig[MJ/day]} = i * e^{jd} * M_{ad_{[kg]}}^{(ke^d+0.73)} * u_g
 * \f]
 *
 * - e: Euler number
 * - d: proportional digestibility
 * - \f$M_{ad}\f$: Adult body mass in kg
 * - \f$u_g = (M/M_{ad})^{0.75}\f$ is a scaling factor for
 *   gut capacity, introduced by Illius & Gordon (1999)
 *   \cite illius1999scaling
 * - Parameters i, j, and k are derived from regression analysis
 *   with 12 mammalian herbivores (0.05--547 kg) and are specific
 *   to hindguts and ruminants (Shipley et al. 1999)
 *   \cite shipley1999predicting.
 *
 * Grass forage:
 * |     | Hindgut | Ruminant |
 * |-----|---------|----------|
 * | i   | 0.108   | 0.034    |
 * | j   | 3.284   | 3.565    |
 * | k   | 0.080   | 0.077    |
 *
 * \note This function currently only works for pure grass diet.
 *
 * \param bodymass_adult Body mass [kg] at physical maturity.
 * \param digestion_type The herbivore’s digestion type.
 * \param bodymass current body mass [kg/ind]
 * \param digestibility proportional digestibility of the forage
 * \return maximum energy intake [MJ/day/ind]
 *
 * \throw std::invalid_argument If `bodymass_adult<=0.0`.
 * \throw std::invalid_argument If `bodymass<=0.0`
 * \throw std::logic_error If `bodymass > bodymass_adult`.
 * \throw std::logic_error If `digestion_type` not implemented.
 * \throw std::logic_error If not all forage types are implemented.
 *
 * \see \ref DigestiveLimit::IlliusGordon1992
 */
ForageEnergy get_digestive_limit_illius_gordon_1992(
    const double bodymass_adult, const DigestionType digestion_type,
    const double bodymass, const Digestibility& digestibility);

/// Ingestion rate as Holling’s Type II functional response.
/**
 * The functional response of grazers (feeding rate depending on
 * grass biomass) is usually described as a “Type II” of the types
 * formulated by Crawford Stanley Holling (1959)
 * \cite holling1959components.
 *
 * Intake rate \f$I\f$ is expressed as a hyperbolically saturating
 * (‘Michalig-Menten’) function:
 * \f[
 * I = I_{max} * \frac{V}{V_{1/2} + V}
 * \f]
 * \f$I_{max}\f$ is the maximum intake rate: the asymptote of the
 * function curve. \f$V\f$ (gDM/m²) is the dry-matter forage (grass)
 * density and \f$V_{1/2}\f$ (gDM/m²) is a species-specific
 * half-saturation constant at which the herbivore reaches half of its
 * maximum ingestion rate.
 * This model is primarily empirical and does not represent any
 * underlying mechanisms. The parameter \f$V_{1/2}\f$ does not generally
 * scale with body mass and usually needs to be derived from field
 * observations of the particular species.
 * \note Illius & O’Connor (2000) and Pachzelt et al. (2013) also call
 * \f$V_{1/2}\f$ “beta” (β).
 *
 * \see \ref ForagingLimit::IlliusOConnor2000
 * \see \ref ForagingLimit::GeneralFunctionalResponse
 */
class HalfMaxIntake {
 public:
  /// Constructor
  /**
   * \param half_max_density The forage density at which
   * the intake rate of a herbivore is half of its maximum.
   * The unit can be freely chosen, but must correspond to the
   * parameter `density` in \ref get_intake_rate().
   * \param max_intake Maximum intake rate; the asymptote of the
   * functional response curve. The unit can be freely chosen as
   * mass or energy per day or per minute etc.
   * \throw std::invalid_argument If either `half_max_density` or
   * `max_intake` is not a positive number.
   */
  HalfMaxIntake(const double half_max_density, const double max_intake);

  /// Get maximum daily net energy intake [unit as `max_intake`].
  /**
   * \param density Grass sward density [unit as `half_max_density`].
   * \return Realized feeding rate [unit as `max_intake`].
   * \throw std::invalid_argument If `density` is negative.
   */
  double get_intake_rate(const double density) const;

 private:
  const double half_max_density;
  const double max_intake;
};

}  // namespace Fauna

#endif  // FAUNA_FORAGING_LIMITS_H
