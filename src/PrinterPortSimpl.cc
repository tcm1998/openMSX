// $Id$

#include <cassert>
#include "PrinterPortSimpl.hh"
#include "DACSound.hh"
#include "PluggingController.hh"
#include "sound/DACSound.hh"


PrinterPortSimpl::PrinterPortSimpl()
{
	dac = NULL;
	PluggingController::instance()->registerPluggable(this);
}

PrinterPortSimpl::~PrinterPortSimpl()
{
	PluggingController::instance()->unregisterPluggable(this);
	delete dac;	// unplug();
}

bool PrinterPortSimpl::getStatus(const EmuTime &time)
{
	return true;	// TODO check
}

void PrinterPortSimpl::setStrobe(bool strobe, const EmuTime &time)
{
	// ignore strobe	TODO check
}

void PrinterPortSimpl::writeData(byte data, const EmuTime &time)
{
	dac->writeDAC(data,time);
}

void PrinterPortSimpl::plug(const EmuTime &time)
{
	short volume = 12000;	// TODO read from config 
	dac = new DACSound(volume, time);
}

void PrinterPortSimpl::unplug(const EmuTime &time)
{
	delete dac;
	dac = NULL;
}

const std::string &PrinterPortSimpl::getName()
{
	static const std::string name("simpl");
	return name;
}
