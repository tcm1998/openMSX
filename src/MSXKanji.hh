// $Id$

#ifndef __MSXKANJI_HH__
#define __MSXKANJI_HH__

#include "MSXIODevice.hh"
#include "MSXRomDevice.hh"


class MSXKanji : public MSXIODevice, public MSXRomDevice
{
	public:
		/**
		 * Constructor
		 */
		MSXKanji(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		virtual ~MSXKanji();
		
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);
		virtual void reset(const EmuTime &time);

	private:
		static const int ROM_SIZE = 256*1024;
		
		int adr1, adr2;
};

#endif //__MSXKANJI_HH__

