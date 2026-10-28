/*
 * vxt_sound.h - VXT toolkit: AY-3-8912 sound emitter (STM32 side). GPLv3.
 *
 * STM32 computes, 6809 services, via the
 * BIOS primitive Sound_Byte (WRREG, $F256) - not raw VIA writes.
 *
 * Mirrors code/app6809/vxt/vxt_sound.asm's block layout. Keep the two in step.
 *
 * BLOCK (STM32 -> 6809, in the served image - not parmRam, which is
 * 6809-write-only):
 *
 *   $1000:  seq                 sequence number; changes only when the command
 *                               list changes. The 6809 skips servicing entirely
 *                               when it matches its shadow (~12 cycles).
 *   $1001+: reg, val            reg = $00..$0D
 *           ...
 *           $FF                 terminator in the register-number slot
 *
 * Register numbering is decimal here. The AY-3-8910/8912 datasheet numbers its
 * registers in OCTAL: its "R10/R11/R12" (amplitude) are decimal 8/9/10, and its
 * "R13/R14/R15" (envelope) are decimal 11/12/13. Reading the datasheet's effect
 * charts literally will write amplitude values into envelope registers.
 *
 * Why register commands and not samples: badapple.asm's `doaudio`
 * is not a PSG example - it writes VIA_port_b = $06, which leaves BC1 (bit 3)
 * and BDIR (bit 4) both LOW, i.e. the PSG bus inactive (confirmed against
 * vecx.c's snd_update(): via_orb & 0x18 == 0 is "sound chip is disabled").
 * $06 selects mux channel 3, the analog audio line - it is streaming PCM
 * through the DAC. That is why it needs servicing after nearly every drawn line
 * and why it sounds rough: each sample's timing is hostage to the draw loop.
 * The AY's registers, by contrast, are LATCHED - "once programmed, generate and
 * sustain the sounds, thus freeing the system processor for other tasks"
 * (AY-3-8910/8912 datasheet, sec.2). One write, then nothing.
 */

#ifndef VXT_SOUND_H
#define VXT_SOUND_H

#include <stdint.h>

/* MOVED from $1000 -> $2000. The vector list starts at $0800;
 * $1000 left only 2048 bytes = 512 records before the list overwrote this
 * block. TEST 6a at ~85+ boxes did exactly that: the 6809 then fed garbage
 * reg/val pairs to Sound_Byte and the PSG went permanently silent. $2000
 * gives 6144 bytes = 1536 records, and vxt_frame now hard-bounds the list.
 * must stay in step with vxt_sound.asm's VXT_SND_BLOCK. */
#define VXT_SND_OFFSET      0x2000  /* must match vxt_sound.asm              */
#define VXT_SND_END         0xFF    /* project-defined terminator. not the    */
                                    /* Sound_Bytes string terminator, whose   */
                                    /* value is undocumented (E.5.2).         */
#define VXT_SND_MAX_PAIRS   14      /* regs 0..13; more is pointless          */

/* --- AY register map (DECIMAL - see header note) -------------------------- */
#define VXT_AY_A_FINE       0
#define VXT_AY_A_COARSE     1   /* low 4 bits only  */
#define VXT_AY_B_FINE       2
#define VXT_AY_B_COARSE     3
#define VXT_AY_C_FINE       4
#define VXT_AY_C_COARSE     5
#define VXT_AY_NOISE        6   /* low 5 bits only  */
#define VXT_AY_MIXER        7
#define VXT_AY_AMP_A        8
#define VXT_AY_AMP_B        9
#define VXT_AY_AMP_C        10
#define VXT_AY_ENV_FINE     11
#define VXT_AY_ENV_COARSE   12
#define VXT_AY_ENV_SHAPE    13
/* Register 14 = the AY's I/O Port A = the buttons. Never written. Sound_Byte's
 * documented range is $00-$0D (Vol.2, WRREG) and vecx.c refuses reg 14 too.
 * vxtSoundReg() DROPS anything above 13 rather than clamping - clamping would
 * write into reg 13 (envelope shape). */

/* --- Mixer bits. AY logic is INVERTED: 0 = enabled. The API below takes
 *     positive-logic ENABLE masks and inverts for you. ---------------------- */
#define VXT_MIX_TONE_A      0x01
#define VXT_MIX_TONE_B      0x02
#define VXT_MIX_TONE_C      0x04
#define VXT_MIX_NOISE_A     0x08
#define VXT_MIX_NOISE_B     0x10
#define VXT_MIX_NOISE_C     0x20
/* bit 6 = I/O port A direction. must remain 0 (input) or the AY starts driving
 * the button lines and controller input reporting dies. vxtSoundMixer() forces it
 * clear; vxtSoundReg(7, x) called raw does not - use the wrapper. */

/* --- Amplitude (regs 8/9/10) --------------------------------------------- */
#define VXT_AMP_ENV         0x10  /* bit 4 = datasheet's "M": envelope-driven  */
                                  /* 0..15 = fixed level when M is clear       */

/* --- Envelope shape (reg 13) --------------------------------------------- */
#define VXT_ENV_DECAY       0x00  /* single decay to silence (gunshot/explosion)*/
#define VXT_ENV_ATTACK      0x04  /* single attack                              */
#define VXT_ENV_SAW         0x08  /* repeating saw                              */
#define VXT_ENV_TRIANGLE    0x0A  /* repeating triangle                         */
#define VXT_ENV_ATK_HOLD    0x0D  /* attack, then hold                          */

typedef enum { VXT_CH_A = 0, VXT_CH_B = 1, VXT_CH_C = 2 } vxtSndChan;

/* --- API -----------------------------------------------------------------
 * Once per frame, inside the frame RPC handler:
 *
 *     vxtSoundBegin();
 *     ... queue only what CHANGED this frame ...
 *     vxtSoundEnd();
 *
 * vxtSoundEnd() is a NO-OP if nothing was queued: the sequence byte is left
 * alone, so the 6809's seq-gate skips servicing entirely. A frame with no sound
 * change costs the 6809 ~12 cycles. Exploiting that is the whole point - do not
 * re-emit an unchanged tone every frame "to be safe". The AY sustains it.
 */
void vxtSoundBegin(void);
void vxtSoundReg(uint8_t reg, uint8_t val);            /* raw; reg>13 dropped */
void vxtSoundTone(vxtSndChan ch, uint16_t period12, uint8_t amp);
void vxtSoundNoise(uint8_t period5);
void vxtSoundMixer(uint8_t toneEnableMask, uint8_t noiseEnableMask);
void vxtSoundEnvelope(uint16_t period16, uint8_t shape);
void vxtSoundSilence(void);                            /* all three amps -> 0 */

/* TAKE OWNERSHIP OF THE AY FROM AN UNKNOWN PRIOR STATE.
 *
 * When to call it: once, on the first frame an application starts producing
 * sound, before starting any music or effect. The AY is a latching device and
 * nothing resets it between programs, so on entry its registers hold whatever
 * the previously running code left there - a tone period and a live amplitude
 * from a title tune, a noise period, an envelope shape, an arbitrary mixer
 * routing. A music player only writes a channel when that channel has an event
 * to fire, so a stale amplitude on a channel whose first event is some frames
 * away stays audible until then. That is heard as a scratch, a buzz or a held
 * tone over the opening of the first piece of audio, and it disappears on a
 * second attempt because by then the registers have been overwritten.
 *
 * What it does: disables every tone and noise routing, parks the noise
 * generator, and writes a known period and a zero amplitude to all three
 * channels, leaving the device silent and fully specified.
 *
 * The one constraint, and it is easy to miss: this call costs 11 register pairs
 * of the VXT_SND_MAX_PAIRS (14) available in a frame. It must therefore be the
 * Only sound work in its frame. Starting a three-channel song in the same frame
 * exceeds the budget and the surplus writes are discarded without an error. Reset on one
 * frame and start the audio on the next.
 *
 * Do not call it per frame, and do not call it to stop sound mid-programme -
 * vxtSoundSilence() is the cheaper way to go quiet while keeping the routing
 * intact. This is initialisation, not a volume control. */
void vxtSoundHardReset(void);
int  vxtSoundEnd(void);                                /* returns pairs written */

#endif /* VXT_SOUND_H */
