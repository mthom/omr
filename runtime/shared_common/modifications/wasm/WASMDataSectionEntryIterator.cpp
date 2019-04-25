#include "WASMDataSectionEntryIterator.hpp"

WASMCacheEntryDescriptor::operator bool() const {
  return *this == nullCacheEntryDescriptor;
}
