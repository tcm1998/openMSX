// $Id$

#ifndef __NATIONALFDC_HH__
#define __NATIONALFDC_HH__

#include "WD2793BasedFDC.hh"


class NationalFDC : public WD2793BasedFDC
{
	public:
		NationalFDC(MSXConfig::Device *config, const EmuTime &time);
		virtual ~NationalFDC();
		
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);  
		virtual byte* getReadCacheLine(word start);

	private:
		byte* emptyRom;
		bool brokenFDCread;
		byte driveReg;
};
#endif
