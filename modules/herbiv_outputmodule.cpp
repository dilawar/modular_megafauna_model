//////////////////////////////////////////////////////////////////////////
/// \file 
/// \brief Output module for the herbivory module.
/// \ingroup group_herbivory
/// \author Wolfgang Pappa, Senckenberg BiK-F
/// \date May 2017
//////////////////////////////////////////////////////////////////////////

#include "config.h"
#include "herbiv_outputmodule.h"
#include "guess.h"                  // for Date and Gridcell
#include "herbiv_simulation_unit.h" // for SimulationUnit
#include "herbiv_hft.h"             // for HftList
#include "herbiv_outputclasses.h"   // for HabitatData and HerbivoreData
#include "herbiv_patchhabitat.h"    // for Patch::get_habitat()
#include "herbiv_population.h"      // for HerbivoreVector
#include <algorithm>                // for std::max
#include <cassert>                  // for assert()
#include <cmath>                    // for NAN
#ifndef NO_GUESS_PARAMETERS
#include "parameters.h"     // for declare_parameter()
#endif // NO_GUESS_PARAMETERS

using namespace GuessOutput;
using namespace Fauna;

bool GuessOutput::is_first_day_of_month(int day){
	if (day < 0 || day > 364)
		throw std::invalid_argument("GuessOutput::is_first_day_of_month() "
				"Parameter \"day\" is out of range.");
	int m = 0;
	int month_length = date.ndaymonth[m];
	// Subtract successively the months (beginning with January).
	while (day > 0){
		assert(m<=11);
		month_length = date.ndaymonth[m];
		day -= month_length;
		m++;
	}
	assert( day <= 0);
	return day == 0;
}

REGISTER_OUTPUT_MODULE("herbivory", HerbivoryOutput)

// define and initialize static variable
HerbivoryOutput* HerbivoryOutput::global_instance = NULL;
const double HerbivoryOutput::NA_VALUE = NAN;

HerbivoryOutput::HerbivoryOutput():
	interval(ANNUAL),
	isactive(true),
	precision(4),
	// Output variables in the order of declaration in the header:
	TBL_HABITAT(
			"file_herbiv_habitat",
			"Various whole-habitat variables.",
			"",
			CS_HABITAT),
	TBL_AVAILABLE_FORAGE(
			"file_herbiv_available_forage",
			"Available forage in the habitats.",
			"kgDM/m²", // In kg/m² like cmass from LPJ-GUESS.
			CS_FORAGE),
	TBL_DIGESTIBILITY(
			"file_herbiv_digestibility",
			"Forage digestibility.",
			"frac",
			CS_FORAGE),
	TBL_EATEN_FORAGE(
			"file_herbiv_eaten_forage",
			"Forage eaten by herbivores per day.",
			"kgDM/km²/day",
			CS_FORAGE),
	TBL_BODYFAT(
			"file_herbiv_bodyfat",
			"Herbivore body fat.",
			"kg/kg",
			CS_HFT),
	TBL_BOUND_NITROGEN(
			"file_herbiv_bound_nitrogen",
			"Nitrogen bound in herbivores.",
			"kgN/km²",
			CS_HFT),
	TBL_EXPENDITURE(
			"file_herbiv_expenditure",
			"Herbivore daily energy expenditure.",
			"MJ/ind/day",
			CS_HFT),
	TBL_INDDENS(
			"file_herbiv_inddens",
			"Individual herbivore density.",
			"ind/km²",
			CS_HFT),
	TBL_MASSDENS(
			"file_herbiv_massdens",
			"Herbivore mass density.",
			"kg/km²",
			CS_HFT),
	TBL_MORT_BACKGROUND(
			"file_herbiv_mort_background",
			"Daily herbivore background mortality.",
			"ind/ind/day",
			CS_HFT),
	TBL_MORT_LIFESPAN(
			"file_herbiv_mort_lifespan",
			"Daily herbivore mortality through old age.",
			"ind/ind/day",
			CS_HFT),
	TBL_MORT_STARVATION(
			"file_herbiv_mort_starvation",
			"Daily herbivore mortality through starvation.",
			"ind/ind/day",
			CS_HFT),
	TBL_OFFSPRING(
			"file_herbiv_offspring",
			"Offspring produced per day.",
			"ind/km²/day",
			CS_HFT),
	TBL_EATEN_PER_IND(
			"file_herbiv_eaten_ind",
			"Forage eaten by herbivore individuals per day.",
			"kgDM/ind/day",
			CS_HFT_FORAGE),
	TBL_EATEN_PER_MASS(
			"file_herbiv_eaten_per_mass",
			"Forage eaten per day per herbivore mass.",
			"kgDM/kg/day",
			CS_HFT_FORAGE),
	TBL_ENERGY_CONTENT(
			"file_herbiv_energy_content",
			"Net energy content of available forage.",
			"MJ/kgDM",
			CS_HFT_FORAGE),
	TBL_ENERGY_INTAKE_PER_IND(
			"file_herbiv_energy_intake_per_ind",
			"Herbivore net energy intake from forage per individual.",
			"MJ/ind/day",
			CS_HFT_FORAGE),
	TBL_ENERGY_INTAKE_PER_MASS(
			"file_herbiv_energy_intake_per_mass",
			"Herbivore net energy intake from forage per herbivore mass.",
			"MJ/kg/day",
			CS_HFT_FORAGE),
	TABLEFILES(init_tablefiles())
{
	// Check if someone is trying to create another instance
	if (global_instance == NULL)
		global_instance = this; 
	else
		throw std::logic_error("GuessOutput::HerbivoryOutput::HerbivoryOutput(): "
				"Constructor called a second time. There should be only "
				"one global instance of an output module.");

	declare_parameter("herbiv_output_interval", 
			&interval_xtring, 128,
			"Interval for herbivory output: \"annual\", \"monthly\"\n");

	declare_parameter("herbiv_output_precision",
			&precision,
			0, 20, // min,max
			"Decimal precision of values in the herbivory output tables. "
			"Defaults to 4 if not defined.");

	// Declare parameters for file names
	assert( !TABLEFILES.empty() );
	for (std::vector<TableFile*>::const_iterator itr = TABLEFILES.begin();
			itr != TABLEFILES.end(); itr++) {
		TableFile& tf = **itr;
		// Check if parameter name is valid
		const std::string param_check_str(tf.paramname);
		assert( param_check_str.find(' ') == std::string::npos );
		assert( param_check_str.find('.') == std::string::npos );

		const std::string help = tf.description + " ["+tf.unit+"]";
		declare_parameter(
				tf.paramname, // This must be const char
				&tf.filename,
				300, // max string length
				help.c_str());
	}
}

HerbivoryOutput& HerbivoryOutput::get_instance(){
	if (global_instance == NULL)
		throw std::logic_error("GuessOutput::HerbivoryOutput::get_instance(): "
				"No instance for this class created yet.");
	return *global_instance;
}

const std::vector<HerbivoryOutput::TableFile*> HerbivoryOutput::init_tablefiles(){
	std::vector<HerbivoryOutput::TableFile*> list;
	// initialize the vector on first call
	if (list.empty()){
		list.push_back(&TBL_HABITAT);
		list.push_back(&TBL_AVAILABLE_FORAGE);
		list.push_back(&TBL_DIGESTIBILITY);
		list.push_back(&TBL_EATEN_FORAGE);
		list.push_back(&TBL_ENERGY_CONTENT);
		list.push_back(&TBL_ENERGY_INTAKE_PER_IND);
		list.push_back(&TBL_ENERGY_INTAKE_PER_MASS);
		list.push_back(&TBL_BODYFAT);
		list.push_back(&TBL_BOUND_NITROGEN);
		list.push_back(&TBL_EXPENDITURE);
		list.push_back(&TBL_INDDENS);
		list.push_back(&TBL_MASSDENS);
		list.push_back(&TBL_MORT_BACKGROUND);
		list.push_back(&TBL_MORT_LIFESPAN);
		list.push_back(&TBL_MORT_STARVATION);
		list.push_back(&TBL_OFFSPRING);
		list.push_back(&TBL_EATEN_PER_IND);
		list.push_back(&TBL_EATEN_PER_MASS);
	}
	return list;
}

void HerbivoryOutput::init() {
	if (!isactive) return;

	if (interval_xtring == "annual")
		interval = ANNUAL;
	else if (interval_xtring == "monthly")
		interval = MONTHLY;
	else if (interval_xtring == "daily")
		interval = DAILY;
	else if (interval_xtring == "decadal")
		interval = DECADAL;
	else if (interval_xtring == ""){
		dprintf("HerbivoryOutput: parameter herbiv_output_interval "
				"is missing in the instruction file.");
		fail();
	} else {
		dprintf("HerbivoryOutput: parameter herbiv_output_interval "
				"has invalid value: %s", (char*) interval_xtring);
		fail();
	}

	if (hftlist.get() == NULL)
		throw std::logic_error("GuessOutput::HerbivoryOutput::init() "
				"HFT list has not been set. Call HerbivoryOutput::set_hftlist() "
				"before initializing the output module.");

	// CREATE TABLES
	for (std::vector<TableFile*>::const_iterator itr = TABLEFILES.begin();
			itr != TABLEFILES.end(); itr++)
	{
		TableFile& tf = **itr;

		// Only create HFT tables if there are any HFTs included.

		assert(hftlist.get() != NULL);
		if (hftlist->size() == 0 &&
				(tf.column_selector == CS_HFT || tf.column_selector == CS_HFT_FORAGE))
		{
			// Create empty table if there are no HFTs
			create_output_table(
					tf.table,
					"",
					ColumnDescriptors());
		} else {
			// Create a table as usual
			create_output_table(
					tf.table, 
					tf.filename.c_str(),
					get_columns(tf.column_selector));
		}
	}
}

ColumnDescriptors HerbivoryOutput::get_columns(
		const HerbivoryOutput::ColumnSelector selector){
	// The width of each column must be one greater than the longest
	// column caption. The column captions need to be defined first.
	
	std::vector<std::string> captions;

	switch (selector){
		case CS_FORAGE:
			// names of forage types
			for (std::set<ForageType>::const_iterator ft = FORAGE_TYPES.begin();
					ft != FORAGE_TYPES.end(); ft++)
			{
				captions.push_back(get_forage_type_name(*ft));
			}
			break;
		case CS_HFT:
			// names of HFTs
			assert(hftlist.get() != NULL);
			for (HftList::const_iterator hft = hftlist->begin();
					hft != hftlist->end(); hft++) {
				captions.push_back(hft->name);
			}
			break;
		case CS_HFT_FORAGE:
			// names of HFTs + names of forage types
			assert(hftlist.get() != NULL);
			// It is important to have the inner and outer loop here the
			// same as in write_datapoint()! Otherwise the values are
			// not congruent with the table header.
			for (HftList::const_iterator hft = hftlist->begin();
					hft != hftlist->end(); hft++) 
				for (std::set<ForageType>::const_iterator ft = FORAGE_TYPES.begin();
						ft != FORAGE_TYPES.end(); ft++)
				{
					const std::string combined = 
						hft->name + CAPTION_SEPARATOR + get_forage_type_name(*ft);
					captions.push_back(combined);
				}
			break;
		case CS_HABITAT:
			// In this special case, there is no fixed/independent
			// variable that would define the captions.
			// Each observational variable is simply one column.
			// Take care to add the column captions in the same order as you add
			// values!
			captions.push_back("snow_depth");
			// ** Add new habitat variables here **
			break;
	}

	// Get the maximum caption length
	assert( !captions.empty() );
	int max_length = 0;
	for (std::vector<std::string>::const_iterator itr=captions.begin();
			itr != captions.end(); itr++)
	{
		max_length = std::max(max_length, (int) itr->length());
	}

	// The column width should reserve a minimum space for the integer
	// part of the value.
	const int MIN_INT_SPACE = 6; // this is just a hopeful guess
	const int min_col_width = 
		MIN_INT_SPACE + 1 + precision + 1; // +1 for comma, +1 for space
	const int col_width = std::max(max_length+1, min_col_width);

	// Now that we know the maximum column width, we can create
	// the descriptor object.
	ColumnDescriptors result;
	for (std::vector<std::string>::const_iterator itr=captions.begin();
			itr != captions.end(); itr++)
	{
		result += ColumnDescriptor(
				itr->c_str(), // title
				col_width,    // column width
				precision);   // precision
	}
	return result;
}

bool HerbivoryOutput::is_today_included()const{
	return date.year >= nyear_spinup;
}

void HerbivoryOutput::outdaily(Gridcell& gridcell){
	if (!isactive) return;

	// References to all simulation units in the gridcell.
	std::vector<SimulationUnit*> simulation_units;

	// reserve space in array: number of stands (gridcell.size())
	// times number of patches in first stand object.
	if (gridcell.size() > 0)
		simulation_units.reserve(
				gridcell.size() * (*gridcell.begin()).nobj);

	//Loop through Patches to gather all habitats
	Gridcell::iterator gc_itr = gridcell.begin();
	while (gc_itr != gridcell.end()) {
		Stand& stand = *gc_itr;
		stand.firstobj(); 
		while (stand.isobj) {
			Patch& patch = stand.getobj();
			// add pointer of the unit to the list
			simulation_units.push_back(&patch.get_herbivory_unit());
			stand.nextobj(); // next patch
		} // patch loop
		++gc_itr; // next stand
	} // stand loop

	// Use the more general function to do the rest
	outdaily(gridcell.get_lon(),
			gridcell.get_lat(), 
			date.day, 
			date.year,                // simulation_year
			date.get_calendar_year(), // calendar_year
			simulation_units);
}	

void HerbivoryOutput::outdaily(
		const double longitude, const double latitude,
		int day, const int simulation_year, int calendar_year,
		const std::vector<Fauna::SimulationUnit*>& simulation_units)
{
	if (day<0 || day>=365)
		throw std::invalid_argument("GuessOutput::HerbivoryOutput::outdaily() "
				"Parameter \"day\" is out of range.");
	if (simulation_year < 0)
		throw std::invalid_argument("GuessOutput::HerbivoryOutput::outdaily() "
				"Parameter \"simulation_year\" is below zero.");

	// Check if this day is included. If not, we skip all output cal
	if (!is_today_included())
		return;


	if ((interval == DAILY) ||
			(interval == MONTHLY && is_first_day_of_month(day)) ||
			(interval == ANNUAL && day==0) ||
			(interval == DECADAL && simulation_year%10==0 && day==0))
	{
		FaunaOut::CombinedData datapoint;

		// Loop through all habitats and aggregate their output
		// to one data point.
		for (std::vector<Fauna::SimulationUnit*>::const_iterator 
				itr  = simulation_units.begin();
				itr != simulation_units.end(); itr++)
		{
			Fauna::SimulationUnit& sim_unit= **itr;

			// AGGREGATE DATA
			datapoint.merge(sim_unit.get_output().reset());
		}

		// ADJUST OUTPUT DATE TO THE MIDDLE OF THE AVERAGE
		set_date_to_period_center(day, calendar_year);

		// WRITE OUTPUT
		// Use the calendar year here.
		write_datapoint( longitude, latitude, day, calendar_year, datapoint );
	}
}

void HerbivoryOutput::set_date_to_period_center(int& day, int& year)const{
	switch (interval){
		case DAILY: break;
		case MONTHLY: day -= 30/2; break;
		case ANNUAL:  day -= 365/2; break;
		case DECADAL: day -= 10 * 365/2; break;
	};
	// If we stepped back into one of the previous years, the `year` variable
	// is decremented and the `day` variable set to a positive number.
	if (day < 0){
		year += day / 365 - 1;
		day  = 365 + (day % 365);
	}
}

void HerbivoryOutput::set_hftlist(const HftList& _hftlist){
	if (hftlist.get() != NULL)
		throw std::logic_error("GuessOutput::HerbivoryOutput::set_hftlist() "
				"HFT list has already been set. "
				"It cannot be set a second time.");
	// copy-construct a new HftList object.
	hftlist = std::auto_ptr<HftList>(new HftList(_hftlist));
}

void HerbivoryOutput::write_datapoint( 
		const double longitude, const double latitude,
		const int day, const int year,
		const FaunaOut::CombinedData& datapoint){
	// The OutputRows object manages the next row of output for each
	// output table
	OutputRows output_rows(
			output_channel, longitude, latitude, 
			year, day);

	// HABITAT TABLE
	// Add the values in the same order as in get_columns()!
	output_rows.add_value(TBL_HABITAT.table, 
			datapoint.habitat_data.environment.snow_depth);

	// FORAGE TABLES
	const ForageMass available_mass   = 
		datapoint.habitat_data.available_forage.get_mass() * 1e-6; // kg/km²⇒kg/m²
	const Digestibility digestibility = 
		datapoint.habitat_data.available_forage.get_digestibility();
	for (std::set<ForageType>::const_iterator ft=FORAGE_TYPES.begin();
			ft!=FORAGE_TYPES.end(); ft++)
	{
		output_rows.add_value(TBL_AVAILABLE_FORAGE.table, 
				available_mass[*ft]);

		if (available_mass[*ft] > 0.0)
			output_rows.add_value(TBL_DIGESTIBILITY.table, digestibility[*ft]);
		else
			output_rows.add_value(TBL_DIGESTIBILITY.table, NA_VALUE);

		output_rows.add_value(TBL_EATEN_FORAGE.table,
				datapoint.habitat_data.eaten_forage[*ft]);
	}

	// ALL HFT-SPECIFIC TABLES
	assert(hftlist.get() != NULL);
	for (HftList::const_iterator itr_hft = hftlist->begin();
			itr_hft != hftlist->end();
			itr_hft++)
	{
		const Hft& hft = *itr_hft;

		// See if we find the HFT in the datapoint.
		//
		// Here it is important to remember that HerbivoryOutput
		// has its own *copy* of an HftList. Therefore, we need to
		// compare Hft objects not by pointer but by their
		// operator==() function.

		const FaunaOut::HerbivoreData* pherbidata = NULL;

		// iterate through the data set and find the HFT we want to look at.
		for (std::map<const Hft*, FaunaOut::HerbivoreData>::const_iterator 
				itr2 = datapoint.hft_data.begin();
				itr2 != datapoint.hft_data.end();
				itr2++)
		{
			const Hft& hft2 = *itr2->first;
			if (hft2 == hft){ // call Hft::operator==()
				pherbidata = &itr2->second;
				break;
			}
		}

		if (pherbidata != NULL && pherbidata->massdens > 0.0) {
			// Okay, there is a data object for the HFT, it contains valid
			// data. We add it to the output

			// Now we can safely dereference the pointer.
			const FaunaOut::HerbivoreData& herbidata = *pherbidata;

			// HFT TABLES
			output_rows.add_value(TBL_BODYFAT.table, 
					herbidata.bodyfat);
			output_rows.add_value(TBL_BOUND_NITROGEN.table, 
					herbidata.bound_nitrogen);
			output_rows.add_value(TBL_EXPENDITURE.table, 
					herbidata.expenditure);
			output_rows.add_value(TBL_INDDENS.table, 
					herbidata.inddens);
			output_rows.add_value(TBL_MASSDENS.table, 
					herbidata.massdens);
			output_rows.add_value(TBL_OFFSPRING.table, 
					herbidata.offspring);
			// ** add new HFT variables here **

			{ // BACKGROUND MORTALITY
				// try to find the map entry
				std::map<MortalityFactor, double>::const_iterator 
					itr_background = herbidata.mortality.find(MF_BACKGROUND);
				// add value if it was found, otherwise add zero
				if (itr_background != herbidata.mortality.end())
					output_rows.add_value(TBL_MORT_BACKGROUND.table,
							itr_background->second);
				else
					output_rows.add_value(TBL_MORT_BACKGROUND.table, 0.0);
			}

			{ // LIFESPAN MORTALITY
				// try to find the map entry
				std::map<MortalityFactor, double>::const_iterator 
					itr_lifespan = herbidata.mortality.find(MF_LIFESPAN);
				// add value if it was found, otherwise add zero
				if (itr_lifespan != herbidata.mortality.end())
					output_rows.add_value(TBL_MORT_LIFESPAN.table,
							itr_lifespan->second);
				else
					output_rows.add_value(TBL_MORT_LIFESPAN.table, 0.0);
			}

			{ // STARVATION MORTALITY
				double starvation = 0.0;
				// try to find the starvation mortality factors
				std::map<MortalityFactor, double>::const_iterator 
					itr_starv1 = herbidata.mortality.find(MF_STARVATION_ILLIUS_OCONNOR_2000);
				std::map<MortalityFactor, double>::const_iterator 
					itr_starv2 = herbidata.mortality.find(MF_STARVATION_THRESHOLD);

				// Sum up the starvation mortality.
				if (itr_starv1 != herbidata.mortality.end())
					starvation += itr_starv1->second;
				if (itr_starv2 != herbidata.mortality.end())
					starvation += itr_starv2->second;

				// If *all* starvation mortality factors were not found,
				// insert zero.
				if (itr_starv1 == herbidata.mortality.end() &&
						itr_starv2 == herbidata.mortality.end())
					starvation = 0.0;

				output_rows.add_value(TBL_MORT_STARVATION.table, starvation);
			}

			// HFT-FORAGE TABLES
			// It is important to have the inner and outer loop here the
			// same as in get_columns()! Otherwise the values are
			// not congruent with the table header.
			for (std::set<ForageType>::const_iterator ft=FORAGE_TYPES.begin();
					ft!=FORAGE_TYPES.end(); ft++)
			{
				output_rows.add_value(TBL_EATEN_PER_IND.table,
						herbidata.eaten_forage_per_ind[*ft]);
				output_rows.add_value(TBL_EATEN_PER_MASS.table,
						herbidata.eaten_forage_per_mass[*ft]);
				output_rows.add_value(TBL_ENERGY_CONTENT.table,
						herbidata.energy_content[*ft] != 0.0 ?
						herbidata.energy_content[*ft] :
						NA_VALUE);
				output_rows.add_value(TBL_ENERGY_INTAKE_PER_IND.table,
						herbidata.energy_intake_per_ind[*ft]);
				output_rows.add_value(TBL_ENERGY_INTAKE_PER_MASS.table,
						herbidata.energy_intake_per_mass[*ft]);
				// ** add new HFT-Forage variables here
			}

		} else {
			// Oops, no data for this HFT. We need to insert NA values.
			// BUT: Only for those variables where it makes sense:
			//      Population density is *zero*, not a missing value.
			for (std::vector<TableFile*>::const_iterator itr_tab = TABLEFILES.begin();
					itr_tab != TABLEFILES.end(); itr_tab++) {
				TableFile& tablefile = **itr_tab;

				// Insert “missing value” if this table is HFT specific.
				if (tablefile.column_selector == CS_HFT ||
						tablefile.column_selector == CS_HFT_FORAGE) 
				{
					// Only individual density and mass density are never missing
					// values.
					if (&tablefile == &TBL_INDDENS || &tablefile == &TBL_MASSDENS)
						output_rows.add_value(tablefile.table, 0.0);
					else if (tablefile.column_selector == CS_HFT) {
						// For a simple HFT table we need to add only one missing
						// value.
						output_rows.add_value(tablefile.table, NA_VALUE);
					} else if (tablefile.column_selector == CS_HFT_FORAGE) {
						// For a HFT-Forage table, we need to add as many missing
						// values as there are forage types
						for (int i=0; i<FORAGE_TYPES.size(); i++)
							output_rows.add_value(tablefile.table, NA_VALUE);
					}
				}
			}
		}
	}
}
