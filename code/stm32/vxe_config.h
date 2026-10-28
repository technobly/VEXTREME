/*
 * vxe_config.h - the one switch for VXE (loadable co-op game payloads). GPLv3.
 *
 * VXE_ENABLED is 1 only when the Makefile builds with VXE=1 (it passes -DVXE_ENABLED=1). Anything else,
 * including a build that does not know about VXE at all, gets 0, and every VXE hook in the firmware is
 * compiled out: the result is the regular VEXTREME firmware, unchanged.
 */
#ifndef VXE_CONFIG_H
#define VXE_CONFIG_H

#ifndef VXE_ENABLED
#define VXE_ENABLED 0
#endif

#endif /* VXE_CONFIG_H */
