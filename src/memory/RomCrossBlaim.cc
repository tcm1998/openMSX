// $Id$

// Thanks to hap (enen) for buying the real cartridge and
// investigating it in detail. See his results on:
//
//   http://www.msx.org/forumtopicl8629.html
//
// To summarize:
//   The whole 0x0000-0xffff region acts as a single switch region. Only
//   the lower 2 bits of the written value have any effect. The mapping
//   is like the table below. The initial state is 00.
//
//                    | 0x | 10 | 11
//      --------------+----+----+----
//      0x0000-0x3fff |  1 |  x |  x    (x means unmapped, reads as 0xff)
//      0x4000-0x7fff |  0 |  0 |  0
//      0x8000-0xbfff |  1 |  2 |  3
//      0xc000-0xffff |  1 |  x |  x

#include "RomCrossBlaim.hh"
#include "Rom.hh"
#include "serialize.hh"

namespace openmsx {

RomCrossBlaim::RomCrossBlaim(const DeviceConfig& config, std::auto_ptr<Rom> rom)
	: Rom16kBBlocks(config, rom)
{
	reset(EmuTime::dummy());
}

void RomCrossBlaim::reset(EmuTime::param dummy)
{
	writeMem(0, 0, dummy);
}

void RomCrossBlaim::writeMem(word /*address*/, byte value, EmuTime::param /*time*/)
{
	switch (value & 3) {
		case 0:
		case 1:
			setRom(0, 1);
			setRom(1, 0);
			setRom(2, 1);
			setRom(3, 1);
			break;
		case 2:
			setUnmapped(0);
			setRom(1, 0);
			setRom(2, 2);
			setUnmapped(3);
			break;
		case 3:
			setUnmapped(0);
			setRom(1, 0);
			setRom(2, 3);
			setUnmapped(3);
			break;
	}
}

byte* RomCrossBlaim::getWriteCacheLine(word /*address*/) const
{
	return NULL;
}

REGISTER_MSXDEVICE(RomCrossBlaim, "RomCrossBlaim");

} // namespace openmsx
