#ifndef TEXT_TABLE_WRITER_H
#define TEXT_TABLE_WRITER_H

#include "output_writer_interface.h"
#include "parameters.h"

namespace Fauna {
namespace Output {

/// Writes output data to tabular plaintext files.
/**
 * \ref Parameters::TextTableWriterOptions contains boolean switches to enable
 * and disable the creation of different data tables in plaintext files. The
 * files have the same name as the corresponding boolean variable in
 * \ref Parameters::TextTableWriterOptions.
 * All files are created in a directory specified by
 * \ref Parameters::TextTableWriterOptions::output_directory.
 */
class TextTableWriter : public OutputWriterInterface {
 public:
  /// Constructor
  /**
   * Create all files (selected in options) with table headers.
   * \param interval Selector if output is daily/monthly/annual/...
   * \param options Specific user-defined options for this class.
   */
  TextTableWriter(const OutputInterval interval,
                  const Parameters::TextTableWriterOptions& options);

  /// Append spatially & temporally aggregated output data to table files.
  void write_datapoint(const DataPoint& datapoint);

  /// Character to separate columns.
  static const char FIELD_SEPARATOR = '\t';
 private:
  const OutputInterval interval;
  const Parameters::TextTableWriterOptions options;
};
}  // namespace Output
}  // namespace Fauna

#endif  // TEXT_TABLE_WRITER_H
