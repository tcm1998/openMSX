// $Id$

#include "MSXE6Timer.hh"
#include "MSXCPUInterface.hh"
#include <cassert>


MSXE6Timer::MSXE6Timer(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	MSXCPUInterface::instance()->register_IO_In (0xE6,this);
	MSXCPUInterface::instance()->register_IO_In (0xE7,this);
	MSXCPUInterface::instance()->register_IO_Out(0xE6,this);
	reset(time);
}

MSXE6Timer::~MSXE6Timer()
{
}
 
void MSXE6Timer::reset(const EmuTime &time)
{
	reference = time;
}

byte MSXE6Timer::readIO(byte port, const EmuTime &time)
{
	int counter = reference.getTicksTill(time);
	switch (port) {
		case 0xE6:
			return counter & 0xff;
		case 0xE7:
			return (counter >> 8) & 0xff;
		default:
			assert(false);
			return 255;	// avoid warning
	}
}

void MSXE6Timer::writeIO(byte port, byte value, const EmuTime &time)
{
	reference = time;
}

