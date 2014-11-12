#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "cpu.h"

// Sound Overview
// 
// There are two sound channels connected to the output terminals SO1 and SO2. There is also
// a input terminal Vin connected to the cartridge. It can be routed to either of both output
// terminals. GameBoy circuitry allows producing sound in four different ways:
// 
//    Quadrangular wave patterns with sweep and envelope functions.
//    Quadrangular wave patterns with envelope functions.
//    Voluntary wave patterns from wave RAM.
//    White noise with an envelope function.
// 
// These four sounds can be controlled independantly and then mixed separately for each of the
// output terminals.
// 
// Sound registers may be set at all times while producing sound.
// 
// (Sounds will have a 2.4% higher frequency on Super GB.)
// 
// 
//  Sound Channel 1 - Tone & Sweep
// 
// FF10 - NR10 - Channel 1 Sweep register (R/W)
//   Bit 6-4 - Sweep Time
//   Bit 3   - Sweep Increase/Decrease
//              0: Addition    (frequency increases)
//              1: Subtraction (frequency decreases)
//   Bit 2-0 - Number of sweep shift (n: 0-7)
// Sweep Time:
//   000: sweep off - no freq change
//   001: 7.8 ms  (1/128Hz)
//   010: 15.6 ms (2/128Hz)
//   011: 23.4 ms (3/128Hz)
//   100: 31.3 ms (4/128Hz)
//   101: 39.1 ms (5/128Hz)
//   110: 46.9 ms (6/128Hz)
//   111: 54.7 ms (7/128Hz)
// 
// The change of frequency (NR13,NR14) at each shift is calculated by the following formula
// where X(0) is initial freq & X(t-1) is last freq:
//   X(t) = X(t-1) +/- X(t-1)/2^n
// 
// FF11 - NR11 - Channel 1 Sound length/Wave pattern duty (R/W)
//   Bit 7-6 - Wave Pattern Duty (Read/Write)
//   Bit 5-0 - Sound length data (Write Only) (t1: 0-63)
// Wave Duty:
//   00: 12.5% ( _-------_-------_------- )
//   01: 25%   ( __------__------__------ )
//   10: 50%   ( ____----____----____---- ) (normal)
//   11: 75%   ( ______--______--______-- )
// Sound Length = (64-t1)*(1/256) seconds
// The Length value is used only if Bit 6 in NR14 is set.
// 
// FF12 - NR12 - Channel 1 Volume Envelope (R/W)
//   Bit 7-4 - Initial Volume of envelope (0-0Fh) (0=No Sound)
//   Bit 3   - Envelope Direction (0=Decrease, 1=Increase)
//   Bit 2-0 - Number of envelope sweep (n: 0-7)
//             (If zero, stop envelope operation.)
// Length of 1 step = n*(1/64) seconds
// 
// FF13 - NR13 - Channel 1 Frequency lo (Write Only)
// 
// Lower 8 bits of 11 bit frequency (x).
// Next 3 bit are in NR14 ($FF14)
// 
// FF14 - NR14 - Channel 1 Frequency hi (R/W)
//   Bit 7   - Initial (1=Restart Sound)     (Write Only)
//   Bit 6   - Counter/consecutive selection (Read/Write)
//             (1=Stop output when length in NR11 expires)
//   Bit 2-0 - Frequency's higher 3 bits (x) (Write Only)
// Frequency = 131072/(2048-x) Hz
// 
// 
//  Sound Channel 2 - Tone
// 
// This sound channel works exactly as channel 1, except that it doesn't have a Tone Envelope/
// Sweep Register.
// 
// FF16 - NR21 - Channel 2 Sound Length/Wave Pattern Duty (R/W)
//   Bit 7-6 - Wave Pattern Duty (Read/Write)
//   Bit 5-0 - Sound length data (Write Only) (t1: 0-63)
// Wave Duty:
//   00: 12.5% ( _-------_-------_------- )
//   01: 25%   ( __------__------__------ )
//   10: 50%   ( ____----____----____---- ) (normal)
//   11: 75%   ( ______--______--______-- )
// Sound Length = (64-t1)*(1/256) seconds
// The Length value is used only if Bit 6 in NR24 is set.
// 
// FF17 - NR22 - Channel 2 Volume Envelope (R/W)
//   Bit 7-4 - Initial Volume of envelope (0-0Fh) (0=No Sound)
//   Bit 3   - Envelope Direction (0=Decrease, 1=Increase)
//   Bit 2-0 - Number of envelope sweep (n: 0-7)
//             (If zero, stop envelope operation.)
// Length of 1 step = n*(1/64) seconds
// 
// FF18 - NR23 - Channel 2 Frequency lo data (W)
// Frequency's lower 8 bits of 11 bit data (x).
// Next 3 bits are in NR24 ($FF19).
// 
// FF19 - NR24 - Channel 2 Frequency hi data (R/W)
//   Bit 7   - Initial (1=Restart Sound)     (Write Only)
//   Bit 6   - Counter/consecutive selection (Read/Write)
//             (1=Stop output when length in NR21 expires)
//   Bit 2-0 - Frequency's higher 3 bits (x) (Write Only)
// Frequency = 131072/(2048-x) Hz
// 
// 
//  Sound Channel 3 - Wave Output
// 
// This channel can be used to output digital sound, the length of the sample buffer (Wave RAM)
// is limited to 32 digits. This sound channel can be also used to output normal tones when
// initializing the Wave RAM by a square wave. This channel doesn't have a volume envelope
// register.
// 
// FF1A - NR30 - Channel 3 Sound on/off (R/W)
//   Bit 7 - Sound Channel 3 Off  (0=Stop, 1=Playback)  (Read/Write)
// 
// FF1B - NR31 - Channel 3 Sound Length
//   Bit 7-0 - Sound length (t1: 0 - 255)
// Sound Length = (256-t1)*(1/256) seconds
// This value is used only if Bit 6 in NR34 is set.
// 
// FF1C - NR32 - Channel 3 Select output level (R/W)
//   Bit 6-5 - Select output level (Read/Write)
// Possible Output levels are:
//   0: Mute (No sound)
//   1: 100% Volume (Produce Wave Pattern RAM Data as it is)
//   2:  50% Volume (Produce Wave Pattern RAM data shifted once to the right)
//   3:  25% Volume (Produce Wave Pattern RAM data shifted twice to the right)
// 
// FF1D - NR33 - Channel 3 Frequency's lower data (W)
// Lower 8 bits of an 11 bit frequency (x).
// 
// FF1E - NR34 - Channel 3 Frequency's higher data (R/W)
//   Bit 7   - Initial (1=Restart Sound)     (Write Only)
//   Bit 6   - Counter/consecutive selection (Read/Write)
//             (1=Stop output when length in NR31 expires)
//   Bit 2-0 - Frequency's higher 3 bits (x) (Write Only)
// Frequency = 4194304/(64*(2048-x)) Hz = 65536/(2048-x) Hz
// 
// FF30-FF3F - Wave Pattern RAM
// Contents - Waveform storage for arbitrary sound data
// 
// This storage area holds 32 4-bit samples that are played back upper 4 bits first.
// 
// 
//  Sound Channel 4 - Noise
// 
// This channel is used to output white noise. This is done by randomly switching the amplitude
// between high and low at a given frequency. Depending on the frequency the noise will appear
// 'harder' or 'softer'.
// 
// It is also possible to influence the function of the random generator, so the that the output
// becomes more regular, resulting in a limited ability to output Tone instead of Noise.
// 
// FF20 - NR41 - Channel 4 Sound Length (R/W)
//   Bit 5-0 - Sound length data (t1: 0-63)
// Sound Length = (64-t1)*(1/256) seconds
// The Length value is used only if Bit 6 in NR44 is set.
// 
// FF21 - NR42 - Channel 4 Volume Envelope (R/W)
//   Bit 7-4 - Initial Volume of envelope (0-0Fh) (0=No Sound)
//   Bit 3   - Envelope Direction (0=Decrease, 1=Increase)
//   Bit 2-0 - Number of envelope sweep (n: 0-7)
//             (If zero, stop envelope operation.)
// Length of 1 step = n*(1/64) seconds
// 
// FF22 - NR43 - Channel 4 Polynomial Counter (R/W)
// The amplitude is randomly switched between high and low at the given frequency. A higher
// frequency will make the noise to appear 'softer'.
// When Bit 3 is set, the output will become more regular, and some frequencies will sound more
// like Tone than Noise.
//   Bit 7-4 - Shift Clock Frequency (s)
//   Bit 3   - Counter Step/Width (0=15 bits, 1=7 bits)
//   Bit 2-0 - Dividing Ratio of Frequencies (r)
// Frequency = 524288 Hz / r / 2^(s+1) ;For r=0 assume r=0.5 instead
// 
// FF23 - NR44 - Channel 4 Counter/consecutive; Inital (R/W)
//   Bit 7   - Initial (1=Restart Sound)     (Write Only)
//   Bit 6   - Counter/consecutive selection (Read/Write)
//             (1=Stop output when length in NR41 expires)
// 
// 
//  Sound Control Registers
// 
// FF24 - NR50 - Channel control / ON-OFF / Volume (R/W)
// The volume bits specify the "Master Volume" for Left/Right sound output.
//   Bit 7   - Output Vin to SO2 terminal (1=Enable)
//   Bit 6-4 - SO2 output level (volume)  (0-7)
//   Bit 3   - Output Vin to SO1 terminal (1=Enable)
//   Bit 2-0 - SO1 output level (volume)  (0-7)
// The Vin signal is received from the game cartridge bus, allowing external hardware in the
// cartridge to supply a fifth sound channel, additionally to the gameboys internal four
// channels. As far as I know this feature isn't used by any existing games.
// 
// FF25 - NR51 - Selection of Sound output terminal (R/W)
//   Bit 7 - Output sound 4 to SO2 terminal
//   Bit 6 - Output sound 3 to SO2 terminal
//   Bit 5 - Output sound 2 to SO2 terminal
//   Bit 4 - Output sound 1 to SO2 terminal
//   Bit 3 - Output sound 4 to SO1 terminal
//   Bit 2 - Output sound 3 to SO1 terminal
//   Bit 1 - Output sound 2 to SO1 terminal
//   Bit 0 - Output sound 1 to SO1 terminal
// 
// FF26 - NR52 - Sound on/off
// If your GB programs don't use sound then write 00h to this register to save 16% or more on
// GB power consumption. Disabeling the sound controller by clearing Bit 7 destroys the contents
// of all sound registers. Also, it is not possible to access any sound registers (execpt FF26)
// while the sound controller is disabled.
//   Bit 7 - All sound on/off  (0: stop all sound circuits) (Read/Write)
//   Bit 3 - Sound 4 ON flag (Read Only)
//   Bit 2 - Sound 3 ON flag (Read Only)
//   Bit 1 - Sound 2 ON flag (Read Only)
//   Bit 0 - Sound 1 ON flag (Read Only)
// Bits 0-3 of this register are read only status bits, writing to these bits does NOT enable/
// disable sound. The flags get set when sound output is restarted by setting the Initial flag
// (Bit 7 in NR14-NR44), the flag remains set until the sound length has expired (if enabled).
// A volume envelopes which has decreased to zero volume will NOT cause the sound flag to go off.

void audio_init(struct audio* a, struct regs* cpu) {
  memset(a, 0, sizeof(*a));
  a->cpu = cpu;
}

void audio_update(struct audio* a, int cycles) { } // TODO

uint8_t read_audio_register(struct audio* a, uint8_t addr) {
  return ((uint8_t*)a)[addr - 0x10];
}

void write_audio_register(struct audio* a, uint8_t addr, uint8_t value) {
  ((uint8_t*)a)[addr - 0x10] = value;
}