class AMD64RelocationTarget : public TR::X86RelocationTarget
   {
   public:
      TR_ALLOC(TR_Memory::Relocation)
      void * operator new(size_t, TR::JitConfig *);
      AMD64RelocationTarget(RelocationRuntime *reloRuntime) : TR::X86RelocationTarget(reloRuntime) {}
      
      virtual void storeRelativeAddressSequence(uint8_t *address, uint8_t *reloLocation, uint32_t seqNumber) 
         {
         storeAddressSequence(address, reloLocation, seqNumber);
         }

      virtual bool useTrampoline(uint8_t * helperAddress, uint8_t *baseLocation);
   };