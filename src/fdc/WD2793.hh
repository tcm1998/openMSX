// $Id$

#ifndef __WD2793_HH__
#define __WD2793_HH__

#include "EmuTime.hh"
#include "Schedulable.hh"

class DiskDrive;


class WD2793 : private Schedulable
{
	public: 
		WD2793(DiskDrive *drive, const EmuTime &time);
		virtual ~WD2793();

		void reset(const EmuTime &time);
		
		byte getStatusReg(const EmuTime &time);
		byte getTrackReg (const EmuTime &time);
		byte getSectorReg(const EmuTime &time);
		byte getDataReg  (const EmuTime &time);
		
		void setCommandReg(byte value, const EmuTime &time);
		void setTrackReg  (byte value, const EmuTime &time);
		void setSectorReg (byte value, const EmuTime &time);
		void setDataReg   (byte value, const EmuTime &time);
		
		bool getIRQ(const EmuTime &time);
		bool getDTRQ(const EmuTime &time);

	private:
		static const int BUSY = 0x01;
		static const int CRC  = 0x08;
		static const int SEEK = 0x10;
		static const int STEP_SPEED = 0x03;
		static const int V_FLAG = 0x04;
		static const int H_FLAG = 0x08;
		static const int T_FLAG = 0x10;
		static const int M_FLAG = 0x10;
		enum FSMState {
			FSM_SEEK
		};
		virtual void executeUntilEmuTime(const EmuTime &time, int state);

		void startType1Cmd(const EmuTime &time);
		void startType2Cmd(const EmuTime &time);
		void startType3Cmd(const EmuTime &time);
		void startType4Cmd(const EmuTime &time);

		void seek(const EmuTime &time);
		void step(const EmuTime &time);
		void seekNext(const EmuTime &time);
		void endType1Cmd(const EmuTime &time);

		void tryToReadSector(void);

		DiskDrive* drive;
		
		EmuTime commandStart;
		EmuTimeFreq<1000000> DRQTime;	// ms

		byte statusReg;
		byte commandReg;
		byte sectorReg;
		byte trackReg;
		byte dataReg;

		bool directionIn;
		bool INTRQ;
		bool DRQ;

		byte dataBuffer[1024];	// max sector size possible
		int dataCurrent;	// which byte in dataBuffer is next to be read/write
		int dataAvailable;	// how many bytes left in sector
};

#endif
