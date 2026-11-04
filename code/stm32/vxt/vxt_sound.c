/*
 * vxt_sound.c - VXT toolkit: AY-3-8912 sound emitter (STM32 side). GPLv3.
 * See vxt_sound.h for the block format and grounding notes.
 */

#include "vxt_sound.h"
#include "vxt_frame.h"   /* for vxtImgPut() */

/* Staged this frame; flushed by vxtSoundEnd(). */
static uint8_t vs_pairs[VXT_SND_MAX_PAIRS * 2];
static int     vs_count;
static uint8_t vs_seq;

void vxtSoundBegin(void)
{
    vs_count = 0;
}

void vxtSoundReg(uint8_t reg, uint8_t val)
{
    if (reg > VXT_AY_ENV_SHAPE) return;   /* never reg 14 - see vxt_sound.h */
    if (vs_count >= VXT_SND_MAX_PAIRS) return;
    vs_pairs[vs_count * 2 + 0] = reg;
    vs_pairs[vs_count * 2 + 1] = val;
    vs_count++;
}

void vxtSoundTone(vxtSndChan ch, uint16_t period12, uint8_t amp)
{
    uint8_t base = (uint8_t)(VXT_AY_A_FINE + 2 * (uint8_t)ch);

    period12 &= 0x0FFF;
    if (period12 == 0) period12 = 1;   /* lowest legal period is 1 (divide-by-1);
                                        * 0 is not a legal divisor (datasheet 3.1) */

    vxtSoundReg(base,     (uint8_t)(period12 & 0xFF));
    vxtSoundReg((uint8_t)(base + 1), (uint8_t)(period12 >> 8));   /* 4 bits used */
    vxtSoundReg((uint8_t)(VXT_AY_AMP_A + (uint8_t)ch), (uint8_t)(amp & 0x1F));
}

void vxtSoundNoise(uint8_t period5)
{
    vxtSoundReg(VXT_AY_NOISE, (uint8_t)(period5 & 0x1F));
}

void vxtSoundMixer(uint8_t toneEnableMask, uint8_t noiseEnableMask)
{
    uint8_t v = 0x3F;   /* all disabled; bits 6/7 clear */

    v &= (uint8_t)~(toneEnableMask  & (VXT_MIX_TONE_A | VXT_MIX_TONE_B | VXT_MIX_TONE_C));
    v &= (uint8_t)~(noiseEnableMask & (VXT_MIX_NOISE_A | VXT_MIX_NOISE_B | VXT_MIX_NOISE_C));
    v &= 0x3F;          /* bit 6 (I/O port A dir) forced INPUT - see vxt_sound.h */

    vxtSoundReg(VXT_AY_MIXER, v);
}

void vxtSoundEnvelope(uint16_t period16, uint8_t shape)
{
    vxtSoundReg(VXT_AY_ENV_FINE,   (uint8_t)(period16 & 0xFF));
    vxtSoundReg(VXT_AY_ENV_COARSE, (uint8_t)(period16 >> 8));
    /* Writing reg 13 RESTARTS the envelope - that is how a one-shot effect is
     * retriggered. Emit it only on the frame the effect fires, or a decaying
     * explosion will be re-struck every frame and never decay. */
    vxtSoundReg(VXT_AY_ENV_SHAPE, (uint8_t)(shape & 0x0F));
}

void vxtSoundSilence(void)
{
    vxtSoundReg(VXT_AY_AMP_A, 0);
    vxtSoundReg(VXT_AY_AMP_B, 0);
    vxtSoundReg(VXT_AY_AMP_C, 0);
}

/* See vxt_sound.h for when to call this and for the single-frame pair-budget
 * constraint. Cost: 11 pairs.
 *
 * The amplitude is set by the per-channel vxtSoundTone() calls below, so
 * vxtSoundSilence() is deliberately not called here as well - it would write the
 * same three amplitude registers a second time for no change in end state, and
 * three pairs is a fifth of the frame's budget.
 *
 * The envelope shape register is deliberately left alone. It only affects a
 * channel whose amplitude has bit 4 set, and every amplitude written here has it
 * clear, so the envelope generator drives nothing regardless of what it holds. */
void vxtSoundHardReset(void)
{
    int c;

    vxtSoundMixer(0, 0);   /* every tone and noise routing disabled */
    vxtSoundNoise(0);      /* park the shared noise generator, in case its
                            * period was left at something loud */
    for (c = 0; c < 3; c++) {
        /* A known period and a silent amplitude on every channel. Period 1 is
         * the lowest legal divisor; the value is immaterial while the amplitude
         * is zero, but leaving it unspecified is what allows a stale period to
         * sound the moment anything raises that amplitude. */
        vxtSoundTone((vxtSndChan)c, 1, 0);
    }
}

int vxtSoundEnd(void)
{
    uint16_t pos;
    int i;

    if (vs_count == 0) {
        /* Nothing changed. Leave the sequence byte alone so the 6809's
         * seq-gate short-circuits. This is the common case. */
        return 0;
    }

    vs_seq++;
    if (vs_seq == 0xFF) vs_seq = 0;   /* $FF is VXT_SOUND_INIT's "never match"
                                       * shadow value - keep seq out of it */

    /* Payload first, sequence byte LAST. The 6809 is parked in its RAM stub for
     * the whole of doHandleEvent() (vxt_rpc.h's timing note), so a torn block
     * isn't reachable - this just makes it structurally impossible. */
    pos = VXT_SND_OFFSET + 1;
    for (i = 0; i < vs_count; i++) {
        vxtImgPut(pos++, vs_pairs[i * 2 + 0]);
        vxtImgPut(pos++, vs_pairs[i * 2 + 1]);
    }
    vxtImgPut(pos, VXT_SND_END);
    vxtImgPut(VXT_SND_OFFSET, vs_seq);

    return vs_count;
}
