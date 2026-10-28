# Pitfalls and conventions

Mistakes that are easy to make when writing a co-op game. Each one came from a bug on hardware. Source comments cite
these rules by number, so the numbers stay fixed even where they are out of order below.
[SDK_GUIDE.md](SDK_GUIDE.md) section 9 lists the ones to read first.

## 6809 side

1. App layout. Put `jmp main` right after the cartridge header rather than falling through to it, and put the
   RAM-resident RPC stub last in the file, after every branch target. Otherwise the cart can freeze at power-on.
2. The RPC trigger stub runs from RAM, not ROM. Normal ROM bytes are not served while the STM32 is computing, so
   6809 code still fetching from ROM at that moment can crash.

## STM32 side: memory and types

3. Match `extern` types. If a payload declares shared STM32 data as a pointer where the firmware defines an
   array, or the other way round, it compiles and links but writes to the wrong place. Copy the declaration from the
   header.
4. Write runtime changes to `cartData` into both 32K banks. `romemu.S` inverts PB6/A15, so write at `pos` and at
   `pos ^ 0x8000`. Don't do this for `menuData`, which is only 20K and would overflow.
5. Prefix limit macros with the game or feature name (`MYGAME_MAX_BOXES`, not `MAX_BOXES`). A macro applies from its
   definition to the end of the file, so a shared name can change another feature's limit without warning.
6. Pass the capacity of the sub-region to the draw engine (`vxtSmartBegin` and similar), not the size of the whole
   image. If the bound is too large, one region can overwrite the next.
16. Growth in bss comes out of the stack. The stack is whatever is left between `_ebss` and `_stack`. After any bss
    change, check the margin with `grep -E '_ebss|_stack' VEXTREME.map` and the stack frame sizes with
    `arm-none-eabi-objdump -d game/yourgame.o | grep 'sub.*sp, #'`. Use the narrowest type that fits. A VXE payload
    has its own fixed 16 KB of RAM, and the same applies there.

## STM32 side: timing and hardware

7. No interrupts in the ROM-emulation path. `romemu.S` runs from RAM with interrupts off and times itself with the
   DWT cycle counter, not SysTick. Code on this path must poll.
8. `Read_Btns` changes `VIA_DDR_a`, which must be an output for the DAC. `vxt_input` sets it back after every button
   read; that code is needed, so don't remove it.
9. `HW_VER` is chosen at compile time. Build for the board you have. With the wrong revision the firmware hangs
   waiting for a signal that is not asserted.
19. No file or flash access in the per-frame handler. The 6809 waits in its RAM stub while the STM32 builds the
    frame, so anything that blocks the STM32 blanks the screen. An `f_open`, `f_write` or `flashDoWriteback()` takes
    about 100 ms, which at 50 Hz shows as flicker.
    - This covers the per-frame RPC handler and everything it calls (`hello_frame_handler()` in the template, and
      the calibration rig's `vxt_cal_handler()`).
    - Do file access once, in the init RPC (`hello_init_handler()`), which runs once per 6809 boot. That is where
      `vxtCalLoadTextComp()`, `vxtCalLoadClosureComp()` and `vxtCalLoadDrawGain()` go. Keep the result in a static
      and apply it each frame. Keep calling the cheap setter `gamelibBeamSetDrawGain()` every frame: the calibration
      rig resets the gain while it runs, and the game has to set it again.
    - To check, grep the handler for `f_open|f_gets|f_read|f_write|vxtCalLoad|calSaveLog`. There should be no hits.
20. Don't pass an uninitialised `FILINFO` to `f_stat` or `f_readdir`. This FatFs (R0.11, `_USE_LFN 1`) writes
    through `fno->lfname` whenever it is non-NULL, so a `FILINFO` on the stack with a garbage `lfname` corrupts
    memory. It only happens when the file exists (`f_stat` returns early for a missing file), so code that works on
    a fresh card hangs on the next boot. To test whether a file exists, use `f_open` (see `game/hello.c`): treat
    `FR_NO_FILE` as absent and any other error as unknown. Don't create a file just because an open failed. If you
    need a `FILINFO`, use a zero-initialised static or set `lfname` yourself, as `menu.c` does.

## Input

10. Handle one-shot and held actions differently. For one-shot actions (menu navigation, switching scenes, a
    toggle), use the per-button edge byte (`parm[VXT_IN_BTN1_1..4]`), which is set for one frame per press. For held
    actions (changing a value while a button is down), record the button's bit on its first edge and test that bit
    in `VXT_IN_BTNS`. Using the held test for a one-shot action fires on every frame of a normal tap, so one press
    can skip through several scenes.

## Drawing budget

11. A frame has a fixed budget of about 1536 SmartList records. A game region's capacity is
    `(0x2000 - VXT_SMART_OFFSET) / 4`, because the sound region starts at `0x2000` (`VXT_SND_OFFSET`). A chained
    edge (`gamelibChainBigGrouped`) costs about 3 records. An independent edge (`gamelibDrawBigLine`) costs 9 to 14,
    because it closes, recenters, sets the scale, moves and sets the scale again. `vxtSmartMoveBig` and `DrawBig`
    emit one record per 100 units of scaled travel, so far or off-centre targets cost more. Going over the budget
    drops geometry, and getting close to it means the 6809 cannot finish the frame, which shows as flicker. Count
    records with `vxtSmartRecordCount()` rather than estimating.
12. Draw edges independently only where needed. Independent edges fix drift and doubled lines, but cost three to
    four times as many records. Chain by default, with a recenter every N lines (N is a per-game constant), and
    draw independently only the edges that must land exactly on a vertex that another path also draws.
13. An object that straddles the window does not mean every edge crosses it. `gamelibClassifyCircle()` tests a
    bounding circle, so an object can be flagged as straddling with no edge actually crossing. Test each edge's
    endpoints with `gamelibPointInWindow()`, and clip only the edges that cross.
18. Don't add records or recenters to gain precision. One record per big line is cheaper for the 6809 than the same
    line chained, so splitting it to improve accuracy makes things worse.
    - One record draws `(dy, dx)` as `(sy*s, sx*s)` with `|sy|, |sx| <= 100`, so the ramp scale `s` only has a lower
      bound, `s >= ceil(maxAbs / 100)`. Any larger `s` still takes one record, and the endpoint error is `+-s/2`.
      `gb_dispatch_huge()` therefore searches for the `s` that lands closest. That costs STM32 time and no 6809
      records.
    - A reposition can only land on a multiple of its scale. Point both the chained and the independent paths at
      that rounded coordinate, not the ideal one, or they end at different points.
    - Split a long chained move proportionally across both axes. Clamping each axis to +-100 separately turns
      `(200, 100)` into `(100, 100)` then `(100, 0)`, a bent line instead of a straight one. `sm_chain_steps()`
      splits it correctly.

## 3D

14. Clip against the near plane. `gamelibProject3DDeep()` computes `scale = focalLength / (focalLength + z)`, so the
    eye is at `z = -focalLength`. A vertex at or past that point projects mirrored and greatly magnified, and shows as
    long straight lines across the screen. A model that is long in Z can reach it. Check every new model, and any
    change to the depth range or focal length, against your near-plane constant. Call `gamelibClipEdgeNear()` before
    any window test, because the window tests give meaningless results for those coordinates.
15. Change a model's scale and its `hitRadius` together. `hitRadius` sets both the level of detail and the window
    classify and clip, so scaling only the model draws a bigger object but tests the old, smaller circle. Also check
    that every level of detail can be reached: a very large radius can keep a model at full detail across the whole
    depth range.
17. Backface culling is correct only for a closed convex mesh. The code is `gamelibCullFaces()` in
    `gamelib/gamelib_cull.h`, and `tools/obj_cull_tool.py` generates its data. The tool orients the faces
    consistently, sets the overall orientation from the signed volume, checks the result by ray casting from the eye,
    and prints the C arrays. It does not compute a convex hull; use any hull routine for that, for example
    `scipy.spatial.ConvexHull`.
    - Don't approximate a face normal as the direction from the centroid. That is only right for a convex mesh
      centred on its origin.
    - The convex hull of the vertices gives the faces and outward normals, so a convex mesh needs no face export. It
      also checks supplied face data: where the hull and an exported face list disagree about an edge, suspect the
      export. A quad split along the wrong diagonal looks like a crease.
    - The facing test uses perspective, not an orthographic view. With the eye at `z = -focalLength`, a face is
      front-facing when `dot(n_rot, objectPos) + modelScale*d + focalLength*n_rot.z < 0`, where
      `d = dot(n, faceCentre)`. Store `d` itself, not a threshold with the scale built in, or it goes wrong when the
      scale changes.
    - Test the result by ray casting from the eye position. Checks based on centroid direction or winding can
      pass on a mesh that is still wrong.
    - In a non-convex model (wings on struts, fins on stalks), a front-facing face can still be hidden behind
      another part, and a flat single-sided panel is visible from both sides. Face normals cannot handle this.
    - Hiding other geometry behind a convex solid is a separate problem. The shadow volume of a convex solid is a
      convex polyhedron, so the hidden lines can be removed with an ordinary Liang-Barsky segment clip. A face's
      orientation is not fixed: the near cap flips once the eye passes it, so classify every face every frame.
    - Culling removes edges, so the next edge no longer starts where the last one ended and some chained lines must
      be repositioned. About half the edges of a convex mesh are culled, so the record count still drops.

## Other

- `vxt_draw` and `vxt_smart` use different formats and cannot share a region of the served image. A frame that uses
  both needs two regions, each handled by its own engine call on the 6809 side.
- Copy bulk vertex and edge data into the source with a script rather than by hand, and count the entries in the
  data instead of trusting a count in a comment. In one mesh, a comment said 43 edges and the list had 45.

## Testing

There is no simulator for the STM32's ROM-emulation bus timing, so a clean build does not show that a change works.
Test toolkit changes on a board. The host tests in `tools/` cover the loader, the file-path mapping, save durability
and the template game's frame. They do not model the cartridge bus, USB or the LEDs.

Where a check can be automated, automate it:

- After rewriting a block that contains conditionals, diff its `if`/`else`/`for` lines against the old version.
- Grep the frame path for file I/O (convention 19).
- Run a new parser or fit over logged data on the host before relying on it.
- To show that `VXE=0` changes nothing, run `tools/check_vxe_off.sh`. It builds the upstream firmware and the `VXE=0`
  build and compares the binaries.
