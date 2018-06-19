/// \brief Classes to construct herbivores.
/// \ingroup group_herbivory
/// \author Wolfgang Pappa, Senckenberg BiK-F
/// \date July 2017
///////////////////////////////////////////////////////////////////////////
#ifndef HERBIV_CREATEHERBIVORES_H
#define HERBIV_CREATEHERBIVORES_H

#include <cassert>        // for assert()
#include "herbiv_utils.h" // for Sex
#include <memory>         // for std::auto_ptr

namespace Fauna{
	// forward declarations
	class HerbivoreCohort;
	class HerbivoreIndividual;
	class Parameters;
	class Hft;

	/// Parent class for \ref CreateHerbivoreIndividual and \ref CreateHerbivoreCohort
	/** This parent class simply provides some common functionality
	 * common to both child classes.
	 */
	class CreateHerbivoreCommon{
		public:
			/// The herbivore functional type.
			const Hft& get_hft()const{
				assert(hft!=NULL);
				return *hft;
			}

		protected:
			/// Protected constructor.
			/** \throw std::invalid_argument If `hft==NULL` or
			 * `parameters==NULL`*/
			CreateHerbivoreCommon(
					const Hft* hft,
					const Parameters* parameters);

			/// Fat mass per maximum fat mass.
			double get_body_condition(const int age_days)const;

			/// Global simulation parameters.
			const Parameters& get_params()const{
				assert(parameters != NULL);
				return *parameters;
			}
		private:
			const Hft* hft;
			const Parameters* parameters;
	};

	/// Function class constructing \ref HerbivoreIndividual objects.
	class CreateHerbivoreIndividual: public CreateHerbivoreCommon{
		public:
			/// Constructor
			/** \copydoc CreateHerbivoreCommon::CreateHerbivoreCommon() */
			CreateHerbivoreIndividual(
					const Hft* hft,
					const Parameters* parameters):
				CreateHerbivoreCommon(hft, parameters){}

			/// Habitat area size [km²].
			double get_area_km2()const;

			/// Create a new object instance.
			/**
			 * \param age_days Age in days.
			 * \param sex Gender of the herbivore.
			 * \return New object instance.
			 * \throw std::invalid_argument If `age_days<0`
			 */
			virtual HerbivoreIndividual operator()(const int age_days,
					const Sex sex)const;
	};

	/// Function class constructing \ref HerbivoreCohort objects.
	class CreateHerbivoreCohort: public CreateHerbivoreCommon{
		public:
			/// Constructor
			/** \copydoc CreateHerbivoreCommon::CreateHerbivoreCommon() */
			CreateHerbivoreCohort(
					const Hft* hft,
					const Parameters* parameters):
				CreateHerbivoreCommon(hft, parameters){}

			/// Create a new object instance
			/**
			 * \param ind_per_km2 Individual density of the new cohort.
			 * \param age_years Age in years.
			 * \param sex Gender of the herbivore.
			 * \return New object instance.
			 * \throw std::invalid_argument if `ind_per_km2<=0.0` or
			 * `age_years<0`
			 */
			virtual HerbivoreCohort operator()(
					const double ind_per_km2, const int age_years, Sex sex)const;
	};

}
#endif // HERBIV_CREATEHERBIVORES_H
