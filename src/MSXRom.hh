// $Id$

#ifndef __MSXROM_HH__
#define __MSXROM_HH__

#ifndef VERSION
#include "config.h"
#endif

#include "MSXRomDevice.hh"
#include "MSXMemDevice.hh"
#include "RomTypes.hh"

// forward declarations
#ifndef DONT_WANT_SCC
class SCC;
#endif
class DACSound;


class MSXRom : public MSXMemDevice, public MSXRomDevice
{
	public:
		MSXRom(MSXConfig::Device *config, const EmuTime &time);
		virtual ~MSXRom();

		virtual void reset(const EmuTime &time);
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);
		virtual byte* getReadCacheLine(word start);
		
	private:
		void retrieveMapperType();
		bool mappedOdd();
		
		inline void setBank4kB (int region, byte* adr);
		inline void setBank8kB (int region, byte* adr);
		inline void setBank16kB(int region, byte* adr);
		inline void setROM4kB  (int region, int block);
		inline void setROM8kB  (int region, int block);
		inline void setROM16kB (int region, int block);
	
		MapperType mapperType;
		byte *internalMemoryBank[16];	// 16 blocks of 4kB
		byte *unmapped;
		
		byte *memorySRAM;
		byte regioSRAM;	//bit n=1 => SRAM in [n*0x2000, (n+1)*0x2000)

#ifndef DONT_WANT_SCC
		SCC* cartridgeSCC;
		bool enabledSCC;
#endif
		DACSound* dac;
};

#endif
