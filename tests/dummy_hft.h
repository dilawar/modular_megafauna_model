/**
 * \file
 * \brief Hft implementation for unit tests.
 * \copyright ...
 * \date 2019
 */
#ifndef TESTS_DUMMY_HFT_H
#define TESTS_DUMMY_HFT_H

#include <sstream>
#include "hft.h"

namespace Fauna {
/// Create a simple, valid HftList
inline HftList create_hfts(const int count, const Parameters& params) {
  HftList hftlist;
  for (int i = 0; i < count; i++) {
    std::shared_ptr<Hft> hft(new Hft());
    // construct name for HFT
    std::ostringstream stream;
    stream << "hft" << i;
    hft->name = stream.str();

    std::string msg;
    if (!hft->is_valid(params, msg))
      FAIL("create_hfts(): HFT is not valid:\n" << msg);

    hftlist.push_back(hft);
  }
  REQUIRE(hftlist.size() == count);
  return hftlist;
}
}  // namespace Fauna

#endif  // TESTS_DUMMY_HFT_H
