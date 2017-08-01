///////////////////////////////////////////////////////////////////////////////////////
/// \file 
/// \brief Output module for the herbivory module.
/// \ingroup group_herbivory
/// \author Wolfgang Pappa, Senckenberg BiK-F
/// \date May 2017
///////////////////////////////////////////////////////////////////////////////////////

#ifndef HERBIV_OUTPUT_MODULE_H
#define HERBIV_OUTPUT_MODULE_H

#include "outputmodule.h"
#include "gutil.h"           // for xtring
#include "herbiv_foraging.h" // for Fauna::ForageType
#include <cassert>
#include <memory>            // for std::auto_ptr
#include <stdexcept>         // for std::invalid_argument

// forward declarations of referenced classes
class Date;
namespace Fauna { 
	class Habitat;
	class HabitatOutputData;
	class HftList; //TODO
}

namespace GuessOutput {

	/// Abstract functor strategy class to limit herbivory output to a specific time.
	/**
	 * This base class includes everything.
	 * \see \ref sec_herbiv_limit_output 
	 * \see \ref sec_strategy
	 */
	struct IncludeDate{
		/// Check whether the date shall be included in the output.
		/** 
		 * \param day_of_year Day of year (0=Jan 1st).
		 * \param year Simulation year (0=first year).
		 * \return True if the given year/date shall be included.
		 * \note This is equivalent to the check in \ref outlimit() in 
		 * \ref commonoutput.cpp, but implemented with the strategy
		 * pattern.
		 */
		virtual bool operator()(
				const int year, 
				const int day_of_year) const {return true;}
	};

	/// Limits output to the time after \ref nyear_spinup.
	struct IncludeNoSpinup: public IncludeDate{
		virtual bool operator()(const int year, 
				const int day_of_year) const;
	};

	/// Output module for the herbivory module.
	/** 
	 * If \ref deactivate() is called, all public methods will not
	 * do anything anymore.
	 * \see \ref sec_herbiv_output
	 * \see \ref sec_herbiv_new_output
	 * \see \ref sec_herbiv_limit_output
	 */
	class HerbivoryOutput : public OutputModule {
		public:

			/// Constructor, declaring parameters, setting global instance
			/**
			 * There is only one single instance of this class in the
			 * program.
			 * \throw std::logic_error If the constructor is called 
			 * a second time.
			 */
			HerbivoryOutput();

			/// Returns the one global instance of this class
			/**
			 * \throw std::logic_error If no global instance has been
			 * created yet.
			 */
			static HerbivoryOutput& get_instance(); 

			/// Initialize, defines output tables.
			void init();

			/// Write output of one year for a gridcell.
			/** \see \ref OutputModule::outannual() 
			 * \see \ref outannual() */
			void outannual(Gridcell& gridcell);

			/// Write output of one year for a number of habitats.
			/** 
			 * Depending on \ref interval, for each day, each month,
			 * or the one year one output row is written.
			 * \note This function is independent of any global LPJ-GUESS
			 * variables (except for \ref Date::ndaymonth).
			 * \param longitude Value for the longitude column.
			 * \param latitude  Value for the latitude column.
			 * \param year Simulation year (starting with 0).
			 * \param habitats A vector of pointers to habitats whose
			 * output will be merged.
			 */
			void outannual( const double longitude, const double latitude,
					const int year,
					const std::vector<const Fauna::Habitat*> habitats) const;

			/// Write daily output for gridcell (not used).
			/** Also daily output is done in \ref outannual().
			 * \see \ref OutputModule::outdaily() */
			void outdaily(Gridcell& gridcell){}

			/// Disable any activity all together.
			void deactivate(){ isactive = false; }

			// TODO
			/// Set the list of HFTs for the output tables
			void set_hftlist(const Fauna::HftList * _hftlist){hftlist = _hftlist;}

			/// Set the strategy object that limits the output.
			/** \throw std::invalid_argument if `ptr==NULL`*/
			void set_include_date(std::auto_ptr<IncludeDate> ptr){ 
				if (ptr.get()==NULL)
					throw std::invalid_argument("GuessOutput::HerbivoryOutput::set_include_date() "
							"Received NULL as parameter");
				include_date = ptr; 
			}
		protected:
			/// Temporal aggregation interval (monthly, yearly, ...)
			enum Interval{
				/// Daily output
				DAILY,
				/// Monthly output
				MONTHLY,
				/// Annual output
				ANNUAL
			};

			/// Create a column descriptor for each forage type.
			const ColumnDescriptors get_forage_columns() const;

			// TODO
			/// Create a column descriptor for each \ref Fauna::Hft
			/** \throw std::logic_error if \ref hftlist not defined. */
			const ColumnDescriptors get_hft_columns() const;

			/// Add one line to each output table
			/**
			 * \param out The Outputrows object manages the next row of output for 
			 * each output table. Create a new one for each function call with
			 * the right lon/lat and time info.
			 * \param data The aggregated output data for the whole gridcell.
			 */
			void add_output_object(OutputRows out, const Fauna::HabitatOutputData& data) const;

			/// Width of one column in the output table.
			static const int column_width = 8;

			/// List of herbivore functional types.
			const Fauna::HftList& get_hftlist()const{
				assert (hftlist != NULL);
				return *hftlist;
			}

			/// Temporal aggregation interval
			Interval get_interval()const{return interval;}

			/// Decimal precision for the values in the columns
			double get_precision()const{return precision;}

			/// The object limiting output. This is never NULL.
			std::auto_ptr<IncludeDate> include_date;
		private: 
			Fauna::HftList const* hftlist; // TODO
			static HerbivoryOutput* global_instance;

			Interval interval;
			/// Interval parameter string as read from instruction file.
			xtring interval_xtring;

			bool isactive;
			int precision; 

			/// Defines all output tables and their formats.
			/** This function specifies all columns in all output tables, their names,
			 *  column widths and precision.
			 *
			 *  For each table a TableDescriptor object is created which is then sent to
			 *  the output channel to create the table.
			 */
			void define_output_tables();

			/**@{ \name Output file names */
			/// forage output files.
			std::string file_forage_avail, file_forage_eaten, 
				file_digestibility;
			/// Herbivore output files.
			std::string file_hft_dens_ind, file_hft_dens_mass;
			/**@} */ //Output file names

			/**@{ \name Output tables */
			/// forage output tables.
			Table out_forage_avail, out_forage_eaten, out_digestibility;
			/// Herbivore output tables.
			Table out_hft_dens_ind, out_hft_dens_mass;
			/**@} */ // Output tables
	};
}

#endif // HERBIV_OUTPUT_MODULE_H
