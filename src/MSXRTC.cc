// $Id$

#include "MSXRTC.hh"
#include "MSXCPUInterface.hh"
#include "RP5C01.hh"
#include "FileOpener.hh"


MSXRTC::MSXRTC(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	PRT_DEBUG("Creating an MSXRTC object");
	bool emuTimeBased = (deviceConfig->getParameter("mode") == "RealTime")
	                    ? false : true;
	try {
		if (deviceConfig->getParameterAsBool("load")) {
			std::string filename = deviceConfig->getParameter("filename");
			IFILETYPE* file = FileOpener::openFileRO(filename);
			unsigned char buffer[4 * 13];
			file->read(buffer, 4 * 13);
			rp5c01 = new RP5C01(emuTimeBased, buffer, time);	// use data from buffer
			file->close();
			delete file;
		} else {
			throw FileOpenerException("dummy");	// use default values
		}
	} catch (FileOpenerException &e) {
		rp5c01 = new RP5C01(emuTimeBased, time);	// use default values
	}
	MSXCPUInterface::instance()->register_IO_Out(0xB4, this);
	MSXCPUInterface::instance()->register_IO_Out(0xB5, this);
	MSXCPUInterface::instance()->register_IO_In (0xB5, this);
	reset(time);
}

MSXRTC::~MSXRTC()
{
	if (deviceConfig->getParameterAsBool("save")) {
		std::string filename = deviceConfig->getParameter("filename");
		IOFILETYPE* file = FileOpener::openFileTruncate(filename);
		file->write(rp5c01->getRegs(), 4 * 13);
		file->close();
		delete file;
	}
	delete rp5c01;
}

void MSXRTC::reset(const EmuTime &time)
{
	rp5c01->reset(time);
}

byte MSXRTC::readIO(byte port, const EmuTime &time)
{
	return rp5c01->readPort(registerLatch, time) | 0xf0;
}

void MSXRTC::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port) {
		case 0xB4:
			registerLatch = value & 0x0f;
			break;
		case 0xB5:
			rp5c01->writePort(registerLatch, value & 0x0f, time);
			break;
	}
}

