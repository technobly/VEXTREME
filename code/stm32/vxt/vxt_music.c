/*
 * vxt_music.c - see vxt_music.h for the full design rationale.
 */
#include "vxt_music.h"
#include "vxt_sound.h"
#include "../delay.h"   /* millis() - see vm_last_ms's own comment below */

static const VxtMusicSong *vm_song = 0;
static uint32_t vm_frame;
static uint32_t vm_loop_count;   /* Added - see vxtMusicLoopCount()'s
                                  * own header comment */
static uint16_t vm_idx[3];
static uint32_t vm_countdown[3];   /* frames until this channel's next
                                    * event; 0xFFFFFFFF = channel exhausted,
                                    * never fires again until vxtMusicPlay() */
#define VM_COUNTDOWN_DONE  0xFFFFFFFFUL

/* Added - the last real {period, amp} this channel's OWN sequencer wrote via
 * vxtSoundTone(), updated every note event in vm_tick() below regardless
 * of whether the channel was audible that frame (a borrowed/
 * muted channel's sequencing is never paused, only its OUTPUT is). Lets
 * vxtMusicReassertChannel() (see its own comment) snap a handed-back
 * channel straight back to wherever the timeline says it should currently
 * be, instead of leaving it silent until the next scheduled event. */
static uint16_t vm_last_period[3];
static uint8_t  vm_last_amp[3];

/* Added, real bug found via direct feedback ("sounds very
 * staticy and noisy, not clear sounds") - NOTHING in this project ever
 * wrote the AY's mixer register (reg 7, which selects tone vs. noise per
 * channel) to enable the 3 TONE channels. vxtSoundTone() only ever touches
 * the period/amp registers, never the mixer - it assumed the mixer was
 * already in a sane state. The only mixer write anywhere in the game's own
 * code is the explosion effect's `vxtSoundMixer(0, VXT_MIX_NOISE_A)`, which disables
 * All three tone channels and routes noise onto channel A, and is never
 * restored afterward - so any explosion firing earlier in the same
 * power-on session (or just the AY's own post-reset register state) could
 * leave tone output disabled/noise-routed indefinitely, which reads as
 * exactly "staticky/noisy" instead of clean tones. Fixed by having
 * playback claim the correct mixer state itself on start, rather than
 * assuming a caller elsewhre already left it right - deferred to the
 * first vxtMusicUpdate() call (not done here in vxtMusicPlay() itself)
 * because THIS function is called from outside the shared vxtSoundBegin/
 * End span (see vxt_music.h's own header) and must not touch the AY
 * directly.
 *
 * Widened after a real bug found via direct feedback ("when i switch
 * from one song to another, there was a drone that persisted that didn't belong -
 * i think it's from channel C of the prior track"): this flag used to
 * only queue the mixer write. A channel switch never independently
 * SILENCED anything - if the outgoing song left a channel's amp non-zero
 * and the incoming song's FIRST event on that same channel isn't at frame
 * 0 (i.e. its initial countdown is > 0), that channel keeps sounding the
 * OLD song's last note, unchanged, until the new song's own first event
 * for it eventually fires. Now this same one-time hook also queues
 * vxtSoundSilence() (all 3 amps -> 0) BEFORE the normal per-channel loop
 * runs its frame-0 events, in the same vxtMusicUpdate() call - so a
 * channel with a real frame-0 event gets silenced then immediately
 * overwritten with its real note (net effect: no change), while a channel
 * with no frame-0 event just stays silenced, instead of inheriting
 * whatever the last song left there. */
static int vm_need_start_init;

/* Added - two of the songs still played too slowly even with
 * the correct tempo VALUES read straight from the .vpy (100 BPM for both -
 * confirmed against the source file, not a misread), which means the bug
 * is in tools/vpy_to_music.py's TICK duration assumption (specifically,
 * what "ticksPerBeat" means as a note-length unit - e.g. whether the
 * tool's "beat" is a quarter note as assumed, or a finer subdivision),
 * not the BPM number itself. Rather than keep guessing at Vectrex Studio's
 * internal semantics, this is a live-adjustable RATE multiplier so the
 * right value can be found by ear on hardware - whatever value fixes
 * it can then be folded into the converter as a fixed scale (or the
 * ticksPerBeat interpretation corrected directly) once known. 1.0 = the
 * generated data's own nominal speed (matches the OLD unconditional
 * per-frame-tick behavior exactly when left at 1.0 - not a behavior change
 * for any existing caller). Accumulator-based so any rate (not just whole
 * multiples) is exact over time, not just approximated frame-to-frame. */
/* Changed, per direct feedback after the real-time decoupling
 * fix (see vm_last_ms's own comment below) - "the decoupling helped with
 * the speed of the midi playback... reduce by about 10%". This is now the
 * FIRST real-tempo feedback given against a stable, frame-rate-independent
 * baseline (every earlier tempo comment predates that fix and was
 * confounded by it), so it's baked in as the new default rather than left
 * for the user to dial in by hand via the live RATE control every session -
 * that control (`vxtMusicSetRate()`) still works identically on top of
 * this new baseline for any further by-ear tuning.
 *
 * Changed again - 0.9 * 0.85 = 0.765. Same reasoning as above:
 * this is feedback against the already-fixed real-time-based baseline, so
 * it's compounded into the default rather than left as a manual RATE
 * nudge.
 *
 * Changed again, per direct feedback
 * ("slow down another 5%") - 0.765 * 0.95 = 0.72675, rounded to 0.727.
 *
 * Changed, per direct feedback after the vm_tick() off-by-one
 * fix (see that function's own comment) - "tempo is a tad too fast now,
 * slow down about 5% for all songs": 0.727 was hand-tuned against the OLD
 * buggy vm_tick(), which fired every event one tick late (a built-in
 * per-event delay that read as extra slowness). With that bug fixed,
 * playback runs faster at the same vm_rate, hence the report. This IS a
 * GLOBAL, SHARED variable (Changed: a per-scene override now
 * exists in the game's own init code for one scene's music - the old
 * live RATE control was removed and nothing else called
 * vxtMusicSetRate() before that addition), so THIS value - the baseline
 * every fresh vxtMusicPlay() call resets to, see that function's own
 * comment - retunes every song in every OTHER state identically (Scene
 * 1/2/4 backgrounds included), not just the Game Info jukebox where this
 * was originally noticed. Does not reintroduce cross-channel
 * misalignment: vm_rate only scales how fast vm_accum fills up, i.e. how
 * often vm_tick() itself gets called - every call still ticks all 3
 * channels together in the same lockstep loop, so this is orthogonal to
 * the per-channel countdown logic the off-by-one fix touched.
 * 0.727 * 0.95 = 0.69065, rounded to 0.691.
 *
 * Changed - was a bare 0.691f here; now VXT_MUSIC_RATE_DEFAULT
 * (vxt_music.h), the same constant vxtMusicPlay() resets to on every
 * fresh song start and the same one the game's own scene-init code
 * derives its first per-scene override from - one canonical number
 * instead of this value being duplicated by hand wherever "the normal
 * rate" is needed. No behavior change here, this
 * static's own initial value is identical either way; the actual
 * per-scene-override mechanism is new, see vxtMusicPlay()'s own comment. */
static float vm_rate = VXT_MUSIC_RATE_DEFAULT;
static float vm_accum = 0.0f;

/* Added, real bug found via direct feedback (playing a song
 * alongside the crawl-text screen: tempo ran slow with lots of text on
 * screen, fast once it thinned out) - vxtMusicUpdate() used to assume it
 * is called at a fixed 50Hz (`vm_accum += vm_rate` once per call, no wall-
 * clock reference at all). It's called once per frame handler
 * RPC, i.e. once per real 6809 frame - and that cadence is not fixed, it's
 * however fast the 6809 finishes drawing+servicing the previous frame. More
 * on-screen SmartList records (more crawl text, more combat) means a slower
 * real frame rate, which under the old "1 call = 1 tick" assumption meant
 * literally slower music, and vice versa - the exact symptom reported. Not
 * a tempo-constant bug (the already-suspect 2x TEMPO_CORRECTION in
 * tools/vpy_to_music.py is a separate, still-open question), it was a unit
 * mismatch: the whole system quietly measured time in "RPC calls," not
 * seconds. Fixed by driving the accumulator from millis() (DWT-cycle-
 * counter-backed, see delay.c) instead of the call itself - vm_accum now
 * advances by real elapsed 50Hz-tick-equivalents, so playback speed no
 * longer depends on how fast the 6809 happens to be drawing this frame. */
static uint32_t vm_last_ms;
#define VM_TICK_MS        20.0f   /* 1000/50 - the generated data's nominal
                                   * per-tick duration, unchanged meaning */
#define VM_MAX_CATCHUP_MS 100.0f  /* Added - caps a single
                                   * Update() call to at most 5 ticks' worth
                                   * of real elapsed time, so a genuine stall
                                   * (debugger pause, very first call after
                                   * vxtMusicPlay(), etc.) can't dump a burst
                                   * of queued notes all at once - same
                                   * "avoid a runaway" reasoning as vm_rate's
                                   * own [0.05, 8.0] clamp just above. */

void vxtMusicSetRate(float rate)
{
    if (rate < 0.05f) rate = 0.05f;   /* avoid a runaway-slow accumulator */
    if (rate > 8.0f)  rate = 8.0f;    /* avoid firing an unbounded number of
                                       * ticks in one real Update() call */
    vm_rate = rate;
}

float vxtMusicGetRate(void)
{
    return vm_rate;
}

void vxtMusicPlay(const VxtMusicSong *song)
{
    int c;

    /* Added - every FRESH song start resets to the tuned
     * default rate, unconditionally. Without this, vm_rate is shared,
     * global, mutable state with no natural reset point - a caller that
     * wants ONE scene's music slower (a slower scene) would otherwise leak that rate into whatever plays next,
     * since nothing else ever touches vm_rate. This makes "start a new
     * song" the natural reset point instead: vxtMusicSwitchSong() (used
     * for Scene 3's OWN background<->saucer transitions) deliberately
     * does not do this, so a per-scene override survives switching
     * between that scene's own tracks - only a NEW vxtMusicPlay()
     * clears it. A caller wanting a non-default rate calls
     * vxtMusicSetRate() AFTER this, same as before - this only changes
     * what happens when nobody does. */
    vm_rate = VXT_MUSIC_RATE_DEFAULT;

    vm_song = song;
    vm_frame = 0;
    vm_loop_count = 0;
    vm_accum = 0.0f;
    vm_last_ms = millis();   /* Added - see vm_last_ms's own
                              * comment; avoids the first vxtMusicUpdate()
                              * call after Play() computing a huge bogus
                              * elapsed time against a stale/zero timestamp */
    vm_need_start_init = 1;
    for (c = 0; c < 3; c++) {
        vm_idx[c] = 0;
        vm_countdown[c] = (song->ch[c].count > 0)
                         ? song->ch[c].events[0].frameDelta
                         : VM_COUNTDOWN_DONE;
        /* Fixed, real bug found via direct feedback ("bad
         * sound... buzzing tone... when it's supposed to be an
         * explosion") - vm_last_period[]/vm_last_amp[] used to carry
         * over UNRESET across a Play() call. A channel with zero events
         * (VM_COUNTDOWN_DONE right away, e.g. a channel-A-only background
         * track) never gets a real vm_tick() write for THIS song, so
         * vm_last_period/amp[c] still held whatever the PREVIOUS song
         * last left there. The instant a one-shot SFX (e.g. an explosion
         * sound effect) finishes borrowing that channel and calls
         * vxtMusicReassertChannel(),
         * it reasserted that stale, unrelated leftover tone instead of
         * silence - and since the channel never fires again, that wrong
         * tone kept sounding. Reset to (0,0) here so an unfired channel's
         * "last known" state is silent, matching what it
         * is for the song that just started. */
        vm_last_period[c] = 0;
        vm_last_amp[c] = 0;
    }
}

/* Added - see this function's own declaration comment in
 * vxt_music.h for the bug this fixes. Mirrors vm_tick()'s loop-restart
 * branch exactly (same per-channel cursor/timeline reset), just against
 * `song` instead of `vm_song`, and deliberately does not touch
 * vm_need_start_init - that's the entire point, leaving whatever mixer
 * state a borrowed channel (explosion/laser/coin) currently has alone. */
void vxtMusicSwitchSong(const VxtMusicSong *song)
{
    int c;

    vm_song = song;
    vm_frame = 0;
    for (c = 0; c < 3; c++) {
        vm_idx[c] = 0;
        vm_countdown[c] = (song->ch[c].count > 0)
                         ? song->ch[c].events[0].frameDelta
                         : VM_COUNTDOWN_DONE;
        vm_last_period[c] = 0;
        vm_last_amp[c] = 0;
    }
}

void vxtMusicStop(void)
{
    vm_song = 0;
    vxtSoundSilence();
}

int vxtMusicIsPlaying(void)
{
    return vm_song != 0;
}

uint32_t vxtMusicLoopCount(void)
{
    return vm_loop_count;
}

/* Added - a caller that borrowed a channel for a
 * sound effect (e.g. an explosion's noise burst) calls
 * this the instant it hands the channel back, AFTER restoring that
 * channel's mixer bit to tone-only. Snaps the channel straight to
 * vm_last_period/amp[ch] - whatever this channel's OWN sequencer currently
 * says should be sounding (including a legitimate silence, if the
 * channel's note happened to end while it was borrowed) - instead of
 * leaving it at the effect's own leftover register values until the
 * channel's next scheduled note event fires, which is what previously
 * read as the resumed channel being "out of sync" with the other two. A
 * no-op if no song is playing (nothing to reassert). */
void vxtMusicReassertChannel(vxtSndChan ch)
{
    if (!vm_song) return;
    vxtSoundTone(ch, vm_last_period[(int)ch], vm_last_amp[(int)ch]);
}

/* One nominal 50Hz tick's worth of playback advance - exactly the old
 * vxtMusicUpdate() body, unchanged, just extracted so vxtMusicUpdate()
 * below can call it a variable number of times per real frame (vm_rate). */
static void vm_tick(void)
{
    int c;

    /* Fixed, real bug root-caused via direct feedback ("in the
     * MIDI they play correctly but on the Vectrex, notes in different
     * channels don't align"): the tick a channel's event FIRED on used to
     * also be the tick its NEW countdown started counting from - i.e. the
     * firing tick was never consumed, so every event fired one
     * tick later than its authored frameDelta says it should, and that
     * lateness accumulates once per fired event. A channel's total
     * accumulated delay therefore equals its own event COUNT, so dense
     * channels (the largest song files) drift far more than
     * sparse ones over the same musical span, reading as exactly "channels
     * misaligned" rather than "whole song slow." Fixed by decrementing
     * FIRST, then firing every event whose delta has now elapsed in a
     * while (not if) - the while handles back-to-back zero-delta events
     * (chords) landing on the same tick instead of one tick apart, exactly
     * like the source MIDI. Each channel's cumulative tick position now
     * exactly matches the sum of its own authored frameDeltas, same as a
     * MIDI player already does - both channels share the same vm_frame
     * clock, so this restores a common timeline, not just a per-channel
     * fix. NOTE: TEMPO_CORRECTION (tools/vpy_to_music.py) and vm_rate
     * (this file) were both empirically tuned to compensate for THIS bug -
     * they scale uniformly so they never caused the misalignment, but they
     * need re-deriving now or the song plays at the wrong overall tempo
     * (not misaligned, just off-speed). */
    for (c = 0; c < 3; c++) {
        const VxtMusicChannel *mc = &vm_song->ch[c];

        if (vm_countdown[c] == VM_COUNTDOWN_DONE) continue;

        if (vm_countdown[c] > 0) vm_countdown[c]--;

        while (vm_countdown[c] == 0 && vm_idx[c] < mc->count) {
            const VxtMusicEvent *e = &mc->events[vm_idx[c]];
            vxtSoundTone((vxtSndChan)c, e->period, e->amp);
            vm_last_period[c] = e->period;   /* Added - see this
                                              * file's own vm_last_period[]
                                              * comment */
            vm_last_amp[c] = e->amp;
            vm_idx[c]++;
            vm_countdown[c] = (vm_idx[c] < mc->count)
                             ? mc->events[vm_idx[c]].frameDelta
                             : VM_COUNTDOWN_DONE;
        }
    }

    vm_frame++;
    if (vm_song->loop && vm_frame >= vm_song->totalFrames) {
        /* Fixed (round 18), real bug found via direct feedback
         * ("explosion sound... I get a tone instead of explosion noise" -
         * Scene 2 and Bonus Scene, both short looping tracks). Root cause:
         * this used to call the FULL vxtMusicPlay(vm_song), which also sets
         * vm_need_start_init = 1 - and vxtMusicUpdate() (below) treats that
         * flag as "claim the whole mixer register: all 3 tones on, all
         * noise off" on its next call. That claim is correct the first time
         * a song STARTS (recovering from whatever the PREVIOUS,
         * different song/effect left the AY in), but firing it again on
         * every ordinary loop-restart of the same song blindly stomps
         * channel B's mixer bits back to tone-only mid-gameplay - exactly
         * cancelling an explosion effect's own noise-routing
         * write if an explosion happened to be active the moment the
         * background track looped. Fixed by only resetting this song's own
         * per-channel playback cursors on a loop-restart (below), leaving
         * vm_need_start_init alone - the mixer state doesn't need
         * reclaiming for a song looping back into itself. */
        vm_frame = 0;
        vm_loop_count++;   /* Added - one more COMPLETED pass;
                            * see vxtMusicLoopCount()'s own header comment */
        for (c = 0; c < 3; c++) {
            vm_idx[c] = 0;
            vm_countdown[c] = (vm_song->ch[c].count > 0)
                             ? vm_song->ch[c].events[0].frameDelta
                             : VM_COUNTDOWN_DONE;
            vm_last_period[c] = 0;
            vm_last_amp[c] = 0;
        }
    }
}

void vxtMusicUpdate(void)
{
    uint32_t now, elapsedMs;

    if (!vm_song) return;

    if (vm_need_start_init) {
        /* All 3 tones enabled, all noise disabled, all 3 amps silenced -
         * see this file's own header note above vm_need_start_init for
         * why both parts are needed. Queued once, on the first update
         * after vxtMusicPlay(), not every frame - matches this project's
         * "only queue what changed" vxtSound* convention. The per-channel
         * loop below runs in this same vxtSoundUpdate() call right after
         * this, so a channel with a real frame-0 event overwrites this
         * silence with its actual note before anything is ever serviced -
         * no audible click. */
        vxtSoundMixer(VXT_MIX_TONE_A | VXT_MIX_TONE_B | VXT_MIX_TONE_C, 0);
        vxtSoundSilence();
        vm_need_start_init = 0;
    }

    now = millis();
    elapsedMs = now - vm_last_ms;   /* unsigned wraparound-safe even across
                                     * millis()'s own eventual rollover */
    vm_last_ms = now;
    if ((float)elapsedMs > VM_MAX_CATCHUP_MS) elapsedMs = (uint32_t)VM_MAX_CATCHUP_MS;

    vm_accum += ((float)elapsedMs / VM_TICK_MS) * vm_rate;
    while (vm_accum >= 1.0f && vm_song) {   /* vm_song re-checked - a tick
                                             * can loop-restart it, and a
                                             * NON-looping song can run out
                                             * mid-loop (vm_song stays set,
                                             * see vxtMusicIsPlaying()'s own
                                             * comment, so this guard is
                                             * only for the loop-restart
                                             * case in practice) */
        vm_accum -= 1.0f;
        vm_tick();
    }
}
