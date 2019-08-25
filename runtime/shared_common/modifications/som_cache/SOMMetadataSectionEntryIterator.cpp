#include "SOMMetadataSectionEntryIterator.hpp"

SOMCacheMetadataEntryDescriptor::operator bool() const {
   return !(*this == nullCacheMetadataEntryDescriptor);
}
