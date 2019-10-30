#if !defined(METADATA_SECTION_ENTRY_ITERATOR_HPP_INCLUDED)
#define METADATA_SECTION_ENTRY_ITERATOR_HPP_INCLUDED

#include "OSCacheRegionBumpFocus.hpp"

struct SOMCacheMetadataItemHeader
{
    enum ItemDesc: uint8_t {
      object = 0x1, array, num_integer,
      block, clazz, num_double,
      eval_prim, frame, method,
      prim, vm_string, symbol
    };

    ItemDesc desc;
    class AbstractVMObject* obj_id;
    size_t size;

    inline bool operator <(const SOMCacheMetadataItemHeader& header) const {
      return obj_id < header.obj_id || (obj_id == header.obj_id && desc < header.desc);
    }
};

using ItemHeader = SOMCacheMetadataItemHeader;

struct SOMCacheMetadataEntryDescriptor {
  SOMCacheMetadataEntryDescriptor()
    : header(NULL)
    , metadataLocation(NULL)
  {}

  SOMCacheMetadataEntryDescriptor(SOMCacheMetadataItemHeader* header)
    : header(header)
    , metadataLocation(header + 1) // 1 is implicitly multiplied by sizeof(SOMCacheMetadataItemHeader).
  {}

  SOMCacheMetadataEntryDescriptor(const SOMCacheMetadataEntryDescriptor& desc)
    : header(desc.header)
    , metadataLocation(desc.metadataLocation)
  {}

  inline bool operator ==(const SOMCacheMetadataEntryDescriptor& rhs) const {
    return header == rhs.header && metadataLocation == rhs.metadataLocation;
  }

  operator bool() const;

  SOMCacheMetadataItemHeader* header;
  void* metadataLocation;
};

static const SOMCacheMetadataEntryDescriptor nullCacheMetadataEntryDescriptor;

class SOMCacheMetadataEntryIterator
{
public:
  SOMCacheMetadataEntryIterator(OSCacheContiguousRegion* region)
    : _focus(region, (ItemHeader*) region->regionStartAddress())
    , _limit((ItemHeader*) region->regionEnd())
    , _region(region)
  {}

  // this creates a delimited iterator, whose end point is demarcated by limit.
  SOMCacheMetadataEntryIterator(OSCacheContiguousRegion* region, ItemHeader* limit)
    : _focus(region, (ItemHeader*) region->regionStartAddress())
    , _region(region)
    , _limit(limit)
  {}

  void fastForward(ItemHeader* start) {    
    _focus = start;
  }

  SOMCacheMetadataEntryDescriptor next()
  {
    SOMCacheMetadataItemHeader* header = _focus;

    if (header == NULL || header->size == 0 || !(_focus < _limit)) {
       return nullCacheMetadataEntryDescriptor;
    }

    SOMCacheMetadataEntryDescriptor descriptor(_focus++);
    _focus += header->size;

    return descriptor;
  }

  ItemHeader* operator*() {
    return _focus;
  }

  ItemHeader& header() {
    return *_focus;
  }

  SOMCacheMetadataEntryIterator& operator+=(size_t bytes) {
    _focus += bytes;
    return *this;
  }

  bool operator ==(const SOMCacheMetadataEntryIterator& it) const {
    return _region == it._region && _focus == it._focus;
  }

  bool operator !=(const SOMCacheMetadataEntryIterator& it) const {
    return !(*this == it);
  }

protected:
  OSCacheRegionBumpFocus<ItemHeader> _focus;
  OSCacheContiguousRegion* _region;
  ItemHeader* _limit;
};

#endif
