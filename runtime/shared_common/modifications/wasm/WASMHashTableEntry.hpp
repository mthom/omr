#if !defined(WASM_HASHTABLE_ENTRY_HPP_INCLUDED)
#define WASM_HASHTABLE_ENTRY_HPP_INCLUDED

#include "WASMCacheEntry.hpp"

#include "omr.h"

class WASMHashTableEntry
{
public:
  WASMHashTableEntry(UDATA key, const WASMCacheEntry* item);  
  
  /* Methods for getting properties of the object */
  UDATA key() const { return _key; }
  const WASMCacheEntry* item() const { return _item; }

  void setItem(const WASMCacheEntry* item) { _item = item; }

private:
  /* Placement operator new (<new> is not included) */
  void* operator new(size_t size, void* memoryPtr) { return memoryPtr; }

  /* Declared data */
  UDATA _key;
  const WASMCacheEntry* _item;
};

#endif
