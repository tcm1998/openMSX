// $Id$

#ifndef __VDPVRAM_HH__
#define __VDPVRAM_HH__

#include "openmsx.hh"
#include "Renderer.hh"
#include "VDP.hh"
#include "VDPCmdEngine.hh"
#include "SpriteChecker.hh"
#include <cassert>


/*
Note: The way VRAM is accessed depends a lot on who is doing the accessing.

For example, the ranges:
- Table access is done using masks.
- Command engine work areas are rectangles.
- CPU access always spans full memory.

Planar mode:
- CPU access can remap locally
- renderer can remap locally
- sprite checker is easier if VDPVRAM does the remapping
  workaround: split sprite mode 2 into "nonplanar mode 2" and "planar mode 2"
- command engine can remap locally
Remapping locally is an optimisation, it is never easier than having VDPVRAM
do the remapping.
Advantage of remapping locally is that VDPVRAM no longer has to know the
current display mode.

Maybe define an interface with multiple subclasses?
Or is that too much of a performance hit?
If accessed through the interface, a virtual method call is made.
But invoking the objects directly should be inlined.

Timing:

Each window reflects the state of the VRAM as a specified moment in time.

Because the CPU has full-range write access, it is incorrect for any window
to be ahead in time compared to the CPU. Because multi-cycle operations are
implemented as atomic, it is currently possible that a window which starts
an operation slightly before CPU time ends up slightly after CPU time.
Solutions:
- break up operations in 1-cycle suboperations
  (very hard to reverse engineer accurately)
- do not start an operation until its end time is after CPU time
  (requires minor rewrite of command engine)
- make the code that uses the timestamps resilient to after-CPU times
  (current implementation; investigate if this is correct)

Window ranges are not at fixed. But they can only be changed by the CPU, so
they are fixed until CPU time, which subsystems will never go beyond anyway.

Two only two subsystems with write access are CPU and command engine.
The command engine can only start executing a new command if instructed so
by the CPU. Therefore it is known which area the command engine can write
in until CPU time:
- empty, if the command engine is not executing a command
- the command's reach, if the command engine is executing a command
Currently the command's reach is not computed: full VRAM is used.
Taking the Y coordinate into account would speed things up a lot, because
usually commands execute on invisible pages, so the number of area overlaps
between renderer and command engine would be reduced significantly.
Also sprite tables are usually not written by commands.

Reading through a window is done as follows:
A subsystem reads the VRAM because it is updating itself to a certain moment
in time T.
1. the subsystems syncs the window to T
2. VDPVRAM checks overlap of the window with the command write area
   no overlap -> go to step 6
3. VDPVRAM syncs the command engine to T
4. the command engine calls VDPVRAM to write each byte it changes in VRAM,
   call the times this happens C1, C2, C3...
5. at the n-th write, VDPVRAM updates any subsystem with the written address
   in its window to Cn, this can include the original subsystem
6. the window has reached T
   now the subsystem can update itself to T
Using this approach instead of syncing on read makes sure there is no
re-entrance on the subsystem update methods.

Note: command engine reads through write window when doing logic-ops.

Interesting observation:
Each window is at the same moment in time as the command engine (C):
- if a window doesn't overlap with the command write window, it is stable
  from a moment before C until the CPU time T
- if a window overlaps with the command window, it cannot be before C
  (incorrect) or after C (uncertainty)
Since there is only one time for the entire VRAM, the VRAM itself can be said
to be at C. This is a justification for having the sync method in VDPVRAM
instead of in Window.

Writing through a window is done as follows:
- CPU write: sync with all non-CPU windows, including command engine write
- command engine write: sync with non-CPU and non-command engine windows
Syncing with a window is only necessary if the write falls into that window.

If all non-CPU windows are disjunct, then all subsystems function
independently (at least until CPU time), no need for syncs.
So what is interesting, is which windows overlap.
Since windows change position infrequently, it may be beneficial to
precalculate overlaps.
Not necessarily though, because even if two windows overlap, a single write
may not be inside the other window. So precalculated overlaps only speeds up
in the case there is no overlap.
Maybe it's not necessary to know exactly which windows overlap with cmdwrite,
only to know whether there are any. If not, sync can be skipped.

Is it possible to read multiple bytes at the same time?
In other words, get a pointer to an array instead of reading single bytes.
Yes, but only the first 64 bytes are guaranteed to be correct, because that
is the granularity of the colour table.
But since whatever is reading the VRAM knows what it is operating on, it
can decide for itself how many bytes to read.

*/

/** Manages VRAM contents and synchronises the various users of the VRAM.
  */
class VDPVRAM {
public:

	VDPVRAM(int size);
	~VDPVRAM();

	/** Update VRAM state to specified moment in time.
	  * @param time Moment in emulated time to update VRAM to.
	  * TODO: Replace this method by Window::sync().
	  */
	inline void sync(const EmuTime &time) {
		cmdEngine->sync(time);
	}

	/** Write a byte from the command engine.
	  * Synchronisation with reads by the command engine is skipped.
	  * TODO: Replace by "cmdSync ; Window::write".
	  *       Note: "cmdSync", because it checks against read windows, unlike
	  *       the other sync which checks against the cmd write window.
	  */
	inline void cmdWrite(int address, byte value, const EmuTime &time) {
		// Rewriting history is not allowed.
		//assert(time >= currentTime);

		spriteChecker->updateVRAM(address, value, time);
		// Problem: infinite loop (renderer <-> cmdEngine)
		// Workaround: only dirty check, no render update
		// Solution: break the chain somehow
		if (true || nameTable.isInside(address)
		|| colourTable.isInside(address)
		|| patternTable.isInside(address)
		|| bitmapWindow.isInside(address)) {
			// TODO: Call separate routines for the different windows.
			//       Otherwise renderer has to do the range checks again.
			renderer->updateVRAM(address, value, time);
		}
		data[address] = value;
		currentTime = time;
	}

	/** Write a byte to VRAM through the CPU interface.
	  * Takes planar addressing into account if necessary.
	  * @param address The address to write.
	  * @param value The value to write.
	  * @param time The moment in emulated time this write occurs.
	  */
	inline void cpuWrite(int address, byte value, const EmuTime &time) {
		if (cmdReadWindow.isInside(address)
		|| cmdWriteWindow.isInside(address)) {
			cmdEngine->sync(time);
		}
		if (planar) address = ((address << 16) | (address >> 1)) & 0x1FFFF;
		cmdWrite(address, value, time);
	}

	/** Reads a byte from the VRAM in its current state.
	  * Planar address remapping is performed in planar display modes.
	  */
	inline byte read(int address) {
		if (planar) address = ((address << 16) | (address >> 1)) & 0x1FFFF;
		assert(0 <= address && address < size);
		return data[address];
	}

	/** Read a byte from VRAM though the CPU interface.
	  * Takes planar addressing into account if necessary.
	  * @param address The address to read.
	  * @return The VRAM contents at the specified address.
	  */
	inline byte cpuRead(int address, const EmuTime &time) {
		// VRAM should never get ahead of CPU.
		//assert(time >= currentTime);

		if (cmdWriteWindow.isInside(address)) {
			cmdEngine->sync(time);
		}
		return read(address);
	}

	/** Used by the VDP to signal display mode changes.
	  * VDPVRAM will inform the Renderer, command engine and the sprite
	  * checker of this change.
	  * @param mode The new display mode: M5..M1.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateDisplayMode(int mode, const EmuTime &time) {
		// Synchronise subsystems.
		renderer->updateDisplayMode(mode, time);
		cmdEngine->updateDisplayMode(mode, time);
		spriteChecker->updateDisplayMode(mode, time);

		// Commit change inside VDPVRAM.
		// TODO: Is the display mode check OK? Profile undefined modes.
		planar = (mode & 0x14) == 0x14;
	}

	/** Used by the VDP to signal display enabled changes.
	  * Both the regular border start/end and forced blanking by clearing
	  * the display enable bit are considered display enabled changes.
	  * @param enabled The new display enabled state.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateDisplayEnabled(bool enabled, const EmuTime &time) {
		// Synchronise subsystems.
		renderer->updateDisplayEnabled(enabled, time);
		cmdEngine->sync(time);
		spriteChecker->updateDisplayEnabled(enabled, time);
	}

	/** Used by the VDP to signal sprites enabled changes.
	  * @param enabled The new sprites enabled state.
	  * @param time The moment in emulated time this change occurs.
	  */
	inline void updateSpritesEnabled(bool enabled, const EmuTime &time) {
		// Synchronise subsystems.
		cmdEngine->sync(time);
		spriteChecker->updateSpritesEnabled(enabled, time);
	}

	inline void setRenderer(Renderer *renderer) {
		this->renderer = renderer;
	}

	/** Necessary becaus of circular dependencies.
	  */
	inline void setSpriteChecker(SpriteChecker *spriteChecker) {
		this->spriteChecker = spriteChecker;
	}

	/** Necessary becaus of circular dependencies.
	  */
	inline void setCmdEngine(VDPCmdEngine *cmdEngine) {
		this->cmdEngine = cmdEngine;
	}

	/** Specifies an address range in the VRAM.
	  * A VDP subsystem can use this to put a claim on a certain area.
	  * For example, the owner of a read window will be notified before
	  * writes to the corresponding area are commited.
	  * The address range is specified by a mask and is not necessarily
	  * continuous. See "doc/vram-addressing.txt" for details.
	  * TODO: Rename to "Table"? That's the term the VDP data book uses.
	  */
	class Window {
	private:
		inline bool isEnabled() {
			return baseAddr != -1;
		}
	public:
		/** Create a new window.
		  * Initially, the window is disabled; use setRange to enable it.
		  */
		Window();

		/** Gets the mask for this window.
		  * Should only be called if the window is enabled.
		  * TODO: Only used by dirty checking. Maybe a new dirty checking
		  *       approach can obsolete this method?
		  */
		inline int getMask() {
			assert(isEnabled());
			return baseMask;
		}

		/** Sets the mask and enables this window.
		  * @param baseMask VDP table base register type mask.
		  * @param indexBits Width of the table index in bits.
		  */
		inline void setMask(int baseMask, int indexBits) {
			this->baseMask = baseMask;
			int indexMask = (-1 << indexBits);
			baseAddr = baseMask & indexMask;
			combiMask = ~baseMask | indexMask;
		}

		/** Disable this window: no address will be considered inside.
		  */
		inline void disable() {
			baseAddr = -1;
		}

		/** Gets a pointer to part of the VRAM in its current state.
		  * The byte pointer to corresponds to the index given,
		  * how many subsequent bytes correspond to subsequent indices
		  * depends on the mask. It is the responsibility of the caller
		  * to take this into account and to make sure no reads outside
		  * the VRAM will occur.
		  * Non-planar addressing is used no matter the display mode.
		  * This can speed up VRAM access when the caller knows if the current
		  * display mode is non-planar or when the caller has already performed
		  * the planar address remapping.
		  * @param index Index in table, with unused bits set to 1.
		  */
		inline const byte *readArea(int index) {
			// Reads are only allowed if window is enabled.
			assert(isEnabled());
			int addr = baseMask & index;
			assert(0 <= addr && addr < (1 << 17));
			return &data[addr];
		}

		/** Reads a byte from VRAM in its current state.
		  * Non-planar addressing is used no matter the display mode.
		  * This can speed up VRAM access when the caller knows if the current
		  * display mode is non-planar or when the caller has already performed
		  * the planar address remapping.
		  * @param index Index in table, with unused bits set to 1.
		  * TODO: getData is more powerful; keep this method as a convenience
		  *       or get rid of it altogether?
		  */
		inline byte readNP(int index) {
			return *readArea(index);
		}

		/** Test whether an address is inside this window.
		  * @param address The address to test.
		  * @return true iff the address is inside this window.
		  */
		inline bool isInside(int address) {
			return (address & combiMask) == baseAddr;
		}

		/** Does this window overlap the specified range?
		  * @param start Start address (inclusive) of range.
		  * @param end End address (exclusive) of range.
		  * @return truee iff range and window overlap.
		  * TODO: Use a Window as parameter instead?
		  * TODO: Which routine wants to use this method?
		  */
		/*
		inline bool overlaps(int start, int end) {
			return start < this->end && this->start < end;
		}
		*/

	private:
		/** For access to setData.
		  */
		friend class VDPVRAM;

		/** Used by VDPVRAM to pass a pointer to the VRAM data.
		  */
		void setData(byte *data);

		/** Pointer to the entire VRAM data.
		  */
		byte *data;

		/** Mask of this window.
		  */
		int baseMask;

		/** Lowest address in this window.
		  */
		int baseAddr;

		/** Combination of baseMask and index mask used for "inside" checks.
		  */
		int combiMask;

	};

	// TODO: Move windows to user classes?
	// Advantage: limited scope.
	// Disadvantage: maybe VDPVRAM needs access for deciding whether
	//   or not to send updates.
	Window cmdReadWindow;
	Window cmdWriteWindow;
	Window nameTable;
	Window colourTable;
	Window patternTable;
	Window bitmapWindow;
	Window spriteAttribTable;
	Window spritePatternTable;

private:

	/** Pointer to VRAM data block.
	  */
	byte *data;

	/** Size of VRAM in bytes.
	  */
	int size;

	bool planar;

	Renderer *renderer;
	VDPCmdEngine *cmdEngine;
	SpriteChecker *spriteChecker;

	/** Current time: the moment up until when the VRAM is updated.
	  * TODO: Is this just for debugging or is it functional?
	  *       Maybe it should stay in either case, possibly between IFDEFs.
	  */
	EmuTimeFreq<VDP::TICKS_PER_SECOND> currentTime;

};

#endif //__VDPVRAM_HH__

