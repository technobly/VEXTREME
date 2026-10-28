/*
 * vxt_music.h - VXT toolkit: 3-channel song player (STM32 side). GPLv3.
 *
 * STM32-side only, built on top of vxt_sound.h - same "reusable layer over
 * a lower primitive" relationship gamelib_beam.h has to vxt_smart.h, just
 * for sound instead of graphics.
 *
 * Data source: songs are not authored here. They're converted offline from
 * Vectrex Studio's MIDI-import .vpy export (tools/vpy_to_music.py) into a
 * generated {frameDelta, period, amp} event array per channel - see that
 * script's own header for the full rationale (why offline, why this
 * format, the AY period formula). This module only PLAYS that data; it has
 * no knowledge of ticks, MIDI notes, or JSON.
 *
 * Why one event list per channel, DELTA-ENCODED: matches the AY's own
 * shape (3 independent monophonic oscillators) and keeps playback O(1) per
 * channel per frame - each channel just holds "frames remaining until my
 * next event" and decrements it, no per-frame search. Checked directly
 * against real .vpy exports before choosing this: the tested songs are
 * already near-perfectly monophonic per channel (3 conflicting notes out
 * of 723 in the worst case), so one active note per channel is a real
 * match to the source data, not a lossy simplification forced on it.
 *
 * Must share one vxtSoundBegin()/vxtSoundEnd() span per frame with every
 * OTHER vxtSound* CALLER. vxtSoundBegin() resets the pending-pairs buffer;
 * calling it a second time in the same frame discards whatever
 * the first caller queued (confirmed against vxt_sound.c - vs_count is
 * just reset to 0, and vxtSoundEnd() writes into the same served-image
 * bytes each time, so only the LAST Begin/End pair in a frame survives).
 * vxtMusicUpdate() below does not call vxtSoundBegin()/vxtSoundEnd()
 * itself for exactly this reason - the caller must already be inside an
 * open span (see the game's own sound-update function, which wraps this
 * and its explosion-sound update together).
 */
#ifndef VXT_MUSIC_H
#define VXT_MUSIC_H

#include <stdint.h>
#include "vxt_sound.h"   /* Added - vxtMusicReassertChannel()'s
                          * own signature needs vxtSndChan visible even for
                          * a caller that only includes this header. */

typedef struct {
    uint16_t frameDelta;   /* frames since the PREVIOUS event in this same
                            * channel (first event: frames since song
                            * start). 0 is legal (simultaneous with the
                            * previous event - shouldn't occur post-
                            * conversion, but not asserted against here). */
    uint16_t period;       /* AY 12-bit tone period, already computed from
                            * the source MIDI note - see vxtSoundTone().
                            * Ignored when amp==0 (a rest/note-off). */
    uint8_t  amp;          /* 0-15; 0 = silence this channel until the next
                            * event (a MIDI note-off or an implicit one at
                            * song end). */
} VxtMusicEvent;

typedef struct {
    const VxtMusicEvent *events;
    uint16_t count;
} VxtMusicChannel;

typedef struct {
    VxtMusicChannel ch[3];   /* index = AY channel A/B/C, matches vxtSndChan */
    uint32_t totalFrames;    /* song length, for loop wraparound */
    uint8_t  loop;           /* 1 = restart from frame 0 at totalFrames */
} VxtMusicSong;

/* Starts (or restarts) playback of `song` from frame 0. Does not itself
 * touch the AY - the first vxtMusicUpdate() call after this emits frame
 * 0's events. */
void vxtMusicPlay(const VxtMusicSong *song);

/* Added, real bug found via direct feedback ("still a tone [not
 * noise] in Scene 2 and the Bonus round") - a scene's saucer-spawn/despawn
 * track switch (background <-> saucer track, in the game's own
 * spawn function) was calling vxtMusicPlay(), which sets
 * vm_need_start_init - the same "reclaim the whole mixer: all 3 tones on,
 * no noise" flag whose loop-restart case was fixed in a later round.
 * That fix only covered a song looping into ITSELF; switching to a
 * different song still went through vxtMusicPlay() and re-armed the flag,
 * so a saucer spawning/dying mid-explosion (very common - saucers die FROM
 * a hit, which is exactly when the explosion noise burst is active) still
 * stomped channel B's noise routing back to tone-only. Use this instead of
 * vxtMusicPlay() for a mid-gameplay switch to a different song while combat
 * SFX may be active: same per-channel timeline reset as a loop-restart,
 * but leaves vm_need_start_init (and therefore the mixer/silence reclaim)
 * alone, so an in-progress explosion/laser/coin channel-borrow survives
 * the switch untouched. Reserve plain vxtMusicPlay() for a song starting
 * from a unknown prior AY state (e.g. a scene's first track). */
void vxtMusicSwitchSong(const VxtMusicSong *song);

/* Stops playback and silences all 3 tone channels (queues amp=0 on each -
 * caller must be inside an open vxtSoundBegin()/vxtSoundEnd() span, same
 * requirement as vxtMusicUpdate()). Safe to call when nothing is playing
 * (no-op beyond the silence queue). */
void vxtMusicStop(void);

/* Call once per frame, INSIDE an already-open vxtSoundBegin()/
 * vxtSoundEnd() span (see this header's own top comment). No-op if
 * nothing is playing. Advances the song's frame position by one and
 * queues vxtSoundTone() for exactly the channels whose next event fires
 * on this frame - same "only queue what changed" convention every other
 * vxtSound* caller in this project follows, so a frame with no note
 * boundary costs nothing extra. */
void vxtMusicUpdate(void);

/* 1 if a song is currently assigned (playing or, for a non-looping song,
 * finished but not yet explicitly stopped - the caller decides what
 * "finished" should trigger). */
int vxtMusicIsPlaying(void);

/* Added - how many times the CURRENT song has looped back to
 * its own start since vxtMusicPlay() was last called for it (0 during its
 * first pass, 1 once it has completed one full pass and is on its second,
 * etc.) - i.e. the number of COMPLETED passes so far. Exists because
 * vxtMusicUpdate() advances the song by real elapsed wall-clock time
 * (millis()-based, see vxt_music.c's own header comment), not by the
 * caller's own per-frame counter - a caller comparing its own frame
 * counter against VxtMusicSong.totalFrames to detect "has this looping
 * song played N times" will drift from the song's real playback position
 * whenever the caller's own frame cadence isn't a perfectly stable 50Hz
 * (hardware isn't - confirmed by a real bug this exact mismatch
 * caused: a caller expecting exactly 2 passes measured by its own frame
 * count got closer to 2.5 real passes). Use this instead of
 * re-deriving pass count from elapsed frames. 0 for a non-looping song
 * (it can only ever be on its one and only pass). */
uint32_t vxtMusicLoopCount(void);

/* Added - for a caller (e.g. an explosion/hit sound effect)
 * that temporarily borrows one of the 3 AY channels away from music
 * playback: call this the instant the channel is handed back (AFTER
 * restoring its mixer bit to tone-only) to snap it straight back to
 * wherever this channel's own sequencer currently says it should be,
 * instead of leaving it at the effect's own last register values until
 * the channel's next scheduled note event. No-op if nothing is playing.
 * Must be called inside an open vxtSoundBegin()/vxtSoundEnd() span, same
 * requirement as vxtMusicUpdate(). */
void vxtMusicReassertChannel(vxtSndChan ch);

/* Added - live playback-speed multiplier, see vxt_music.c's own
 * comment (tools/vpy_to_music.py's tick->frame conversion is suspected
 * wrong - two of the songs played too slowly even with the correct source BPM -
 * this exists to find the right correction empirically on hardware rather
 * than guess again). 1.0 = the generated data's own nominal 50Hz speed
 * (default, and exactly the old unconditional behavior). Clamped to
 * [0.05, 8.0] internally - out-of-range calls are clamped, not
 * rejected. Does not touch the AY itself - safe to call from anywhere,
 * same as vxtMusicPlay(). */
void vxtMusicSetRate(float rate);
float vxtMusicGetRate(void);

/* Added - the tuned, hardware-confirmed baseline every song
 * plays at unless a caller explicitly requests otherwise (see
 * vxtMusicPlay()'s own header comment - it resets to exactly this value
 * on every fresh song start, so a per-scene override made via
 * vxtMusicSetRate() right after vxtMusicPlay() can never leak into
 * whatever scene/song starts next). Exported so a caller computing a
 * scene-specific rate (e.g. "5% slower than normal") derives it from this
 * one constant instead of duplicating the magic number. */
#define VXT_MUSIC_RATE_DEFAULT  0.691f

#endif /* VXT_MUSIC_H */
