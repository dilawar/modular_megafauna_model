///////////////////////////////////////////////////////////////////////////////////////
/// \file 
/// \brief Central management of the herbivory simulation.
/// \ingroup group_herbivory
/// \author Wolfgang Pappa, Senckenberg BiK-F
/// \date 2017-05-10
///////////////////////////////////////////////////////////////////////////////////////

#include "config.h"
#include "herbiv_framework.h"
#include "guess.h"                // for Date
#include "herbiv_digestibility.h" // for DigestibilityModel
#include "herbiv_habitat.h"       // for Habitat and Population
#include "herbiv_herbivore.h"     // for HerbivoreInterface
#include "herbiv_hft.h"           // for Hft and HftList
#include "herbiv_parameters.h"    // for Fauna::Parameters
#include "assert.h"
#include <stdexcept> // for std::logic_error, std::invalid_argument

using namespace Fauna;

Simulator::Simulator(const Parameters& params):
	params(params)
{
	// DIGESTIBILITY MODEL
	// The global object instance is held in a static variable.

	switch (params.dig_model) {
		case DM_PFT_FIXED:
			static PftDigestibility pft_dig; // global instance
			DigestibilityModel::init_global(pft_dig);
			break;
			// add other models here ...
		default:
			// TODO: Use exceptions or assertions?
			throw std::invalid_argument(
					"DigestibilityModel::init_model(): "
					"unknown enum type."); 
			break;
	}

}

HftPopulationsMap Simulator::create_populations(){
	HftPopulationsMap map;
	const HftList& hftlist = HftList::get_global();
	for (int i=0; i<=hftlist.size(); i++){
		const Hft& hft = hftlist[i];
		switch (params.herbivore_type){
			case HT_COHORT:
				static CohortFactory cohort_factory;
				map.add(new CohortPopulation(
							hft, 
							cohort_factory,
							params.dead_herbivore_threshold));
				break;
			case HT_INDIVIDUAL:
				static IndividualFactory individual_factory;
				map.add(new IndividualPopulation(
							hft, 
							individual_factory));
				break;
			default:
				throw std::logic_error("Simulator::create_populations(): "
						"unknown herbivore type");
		}
	}
	return map;
}


void Simulator::simulate_day(const int day_of_year, Habitat& habitat,
		const bool do_herbivores){
	if (day_of_year < 0 || day_of_year >= 365)
		throw std::invalid_argument("Simulator::simulate_day(): "
				"argument day_of_year out of range");

	// pass the current date into the herbivore module
	habitat.init_todays_output(day_of_year);

	const HftList& hftlist = HftList::get_global();
	if (do_herbivores && hftlist.size()>0) {

		// all populations in the habitat (one for each HFT)
		HftPopulationsMap& populations = habitat.get_populations();

		// iterate through HFT populations
		HftPopulationsMap::iterator itr_pop = populations.begin();
		while (itr_pop != populations.end()) {
			PopulationInterface& pop = **itr_pop; // one population
			const Hft& hft = pop.get_hft();

			// ESTABLISHMENT
			if (pop.get_list().empty()){
				pop.create(hft.establishment_density,
						hft.maturity*365); // age
			}

			// mass density [kg/km²] of total offspring for this HFT today
			double total_offspring = 0.0; 

			// iterate through herbivores in the population
			{ // variable `list` is only valid in this scope
				const std::vector<HerbivoreInterface*> list = pop.get_list();
				std::vector<HerbivoreInterface*>::iterator itr_herbi;
				itr_herbi = pop.get_list().begin();
				while (itr_herbi != list.end()) {

					HerbivoreInterface& herbivore = **itr_herbi;

					// HERBIVORE SIMULATION

					// offspring [kg/km²] by this one herbivore today
					double offspring = 0.0;

					herbivore.simulate_day(day_of_year, offspring);

					total_offspring += offspring;
				}
			} // now the pointers in `list` are changed

			// REPRODUCTION
			if (total_offspring > 0.0)
				pop.create(total_offspring);

			// CLEANUP
			pop.remove_dead(); 

			itr_pop++;
		} 

		// FORAGING
		// get the forage distribution
		ForageDistribution forage_dist; // [kgDM/m²]
		ForageMass forage_sum; // [kgDM/m²]
		distribute_forage()(
				habitat.get_available_forage(),
				habitat.get_populations(),
				forage_dist,
				forage_sum);

		// let the herbivores eat
		for (ForageDistribution::iterator iter=forage_dist.begin();
				iter != forage_dist.end(); iter++)
		{
			iter->first->eat(iter->second);
		}
		habitat.remove_eaten_forage(forage_sum);

	}
}

DistributeForage& Simulator::distribute_forage(){
	// returns a function object
	static DistributeForageEqually equal;
	switch (params.forage_distribution) {
		case FD_EQUALLY:
			return equal;
		default:
			throw std::logic_error("Fauna::Simulator::distribute_forage(): "
					"chosen forage distribution algorithm not implemented");
	};
}

//============================================================
// DistributeForageEqually
//============================================================

void DistributeForageEqually::operator()(
		const HabitatForage& available,
		const HftPopulationsMap& populations,
		ForageDistribution& forage_distribution,
		ForageMass forage_sum) const {
	//
	// iterate through all populations and then all herbivores in them
	HftPopulationsMap::const_iterator itr_p = populations.begin();
	while (itr_p != populations.end()){
		const PopulationInterface& pop = **itr_p;
		const std::vector<const HerbivoreInterface*> herbivores = pop.get_list();

		std::vector<const HerbivoreInterface*>::const_iterator itr_h;
		itr_h = herbivores.begin();
		while (itr_h != herbivores.end()) {
			const HerbivoreInterface& herbiv = **itr_h;
			// TODO
			// herbiv.get_forage_demands();
			itr_h++; // herbivore iterator
		}
		itr_p++; // population iterator
	}
}

