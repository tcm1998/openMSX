// $Id$

#ifndef __MSXDISKROMPATCH_HH__
#define __MSXDISKROMPATCH_HH__

#include "MSXRomPatchInterface.hh"
#include "msxconfig.hh"
#include "config.h"
#include "CPU.hh"
#include <iostream>
#include <fstream>

#ifdef HAVE_FSTREAM_TEMPL
#define IOFILETYPE std::fstream<byte>
#else
#define IOFILETYPE std::fstream
#endif

class MSXDiskRomPatch: public MSXRomPatchInterface
{
	class NoSuchSectorException : public MSXException {};
	class DiskIOErrorException  : public MSXException {};
	class DiskImage
	{
		public:
			DiskImage(std::string filename);
			~DiskImage();

			void readSector(byte* to, int sector);
			void writeSector(const byte* from, int sector);

		private:
			int nbSectors;
			IOFILETYPE *file;
	};

	public:
		MSXDiskRomPatch();
		virtual ~MSXDiskRomPatch();

		virtual void patch() const;

	private:

		/**
		 * read/write sectors
		 */
		void PHYDIO(CPU::CPURegs& regs) const;
		static const int A_PHYDIO;
		/**
		 * check disk
		 */
		void DSKCHG(CPU::CPURegs& regs) const;
		static const int A_DSKCHG;

		/**
		 * get disk format
		 */
		void GETDPB(CPU::CPURegs& regs) const;
		static const int A_GETDPB;

		/**
		 * format a disk
		 */
		void DSKFMT(CPU::CPURegs& regs) const;
		static const int A_DSKFMT;

		/**
		 * stop drives
		 */
		void DRVOFF(CPU::CPURegs& regs) const;
		static const int A_DRVOFF;

		// drive letters
		static const int A = 0;
		static const int B = 1;
		static const int LAST_DRIVE = B+1;

		// files for drives
		MSXDiskRomPatch::DiskImage* disk[LAST_DRIVE];

		static const int SECTOR_SIZE = 512;
};

#endif // __MSXDISKROMPATCH_HH__
