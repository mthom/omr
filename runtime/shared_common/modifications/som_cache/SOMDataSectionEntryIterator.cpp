#include "SOMDataSectionEntryIterator.hpp"

SOMCacheEntryDescriptor::operator bool() const {
  return !(*this == nullCacheEntryDescriptor);
}
