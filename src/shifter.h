/*
 * shifter.h - ST4Ever Atari ST Shifter video rendering engine (UC26)
 *
 * Converts the Atari ST video frame buffer (bitplane planar format) into
 * a linear array of RGB32 pixels suitable for display by exec_screen (UC27).
 *
 * The Atari ST Shifter supports three resolutions:
 *   Low  (ST_RES_LOW,  0): 320x200, 4 bitplanes, 16 colours
 *   Med  (ST_RES_MED,  1): 640x200, 2 bitplanes,  4 colours
 *   High (ST_RES_HIGH, 2): 640x400, 1 bitplane,   2 colours (mono)
 *
 * All three modes share a 32,000-byte frame buffer.
 * Each 16-pixel group is stored as interleaved big-endian bitplane words:
 *   Low : 4 words (planes 0-3) x 2 bytes = 8 bytes / group
 *   Med : 2 words (planes 0-1) x 2 bytes = 4 bytes / group
 *   High: 1 word  (plane  0  ) x 2 bytes = 2 bytes / group
 *
 * Palette entries (auPalette[16] in ST.h) are 16-bit ST colour words:
 *   bits [10:8] = Red (0-7), bits [6:4] = Green (0-7), bits [2:0] = Blue (0-7)
 *
 * Resolution constants and st_machine_t are defined in ST.h.
 */

#ifndef SHIFTER_H
#define SHIFTER_H

#include "common.h"
#include "ST.h"

/* ------------------------------------------------------------------
 * Screen dimension constants
 * ------------------------------------------------------------------ */

#define SHIFTER_WIDTH_LOW    320
#define SHIFTER_HEIGHT_LOW   200
#define SHIFTER_WIDTH_MED    640
#define SHIFTER_HEIGHT_MED   200
#define SHIFTER_WIDTH_HIGH   640
#define SHIFTER_HEIGHT_HIGH  400
#define SHIFTER_MAX_PIXELS   (SHIFTER_WIDTH_HIGH * SHIFTER_HEIGHT_HIGH)

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/*
 * shifter_screen_size() - Return screen dimensions for a given resolution.
 *
 * Parameters:
 *   uiRes    [in]  : ST_RES_LOW, ST_RES_MED or ST_RES_HIGH (from ST.h).
 *   piWidth  [out] : Receives screen width in pixels.
 *   piHeight [out] : Receives screen height in pixels.
 *
 * Both outputs are set to 0 for an unknown resolution.
 * Both pointers must be non-NULL.
 */
void shifter_screen_size(st_u8_t uiRes, int *piWidth, int *piHeight);

/*
 * shifter_palette_to_rgb() - Convert a 16-bit ST palette entry to RGB32.
 *
 * The ST colour word packs three 3-bit channels:
 *   bits [10:8] = Red   (0-7)
 *   bits  [6:4] = Green (0-7)
 *   bits  [2:0] = Blue  (0-7)
 * Each channel is scaled to 8 bits using (n * 255) / 7.
 *
 * Parameters:
 *   uiColor [in] : 16-bit ST colour register value.
 *
 * Returns: 32-bit value 0x00RRGGBB.
 */
st_u32_t shifter_palette_to_rgb(st_u16_t uiColor);

/*
 * shifter_render() - Decode the ST video frame buffer into RGB32 pixels.
 *
 * Reads ptMachine->uiScreenBase, ->uiResolution and ->auPalette to
 * locate and decode the frame buffer within ->aRam.
 * Output is a flat row-major array: pixel (x,y) = auPixels[y*width + x].
 * Each pixel value is 0x00RRGGBB.
 *
 * Parameters:
 *   ptMachine    [in]  : Emulated ST machine state (must not be NULL).
 *   auPixels     [out] : Caller-allocated pixel buffer (must not be NULL).
 *   uiPixelCount [in]  : Capacity of auPixels in pixels.
 *   piWidth      [out] : Receives actual screen width in pixels.
 *   piHeight     [out] : Receives actual screen height in pixels.
 *
 * Returns:
 *   ST_NO_ERROR  Frame decoded OK; *piWidth and *piHeight set.
 *   ST_ERROR     NULL parameter, unknown resolution, buffer too small,
 *                or frame buffer extends beyond ST RAM bounds.
 */
st_error_t shifter_render(st_u32_t           *auPixels,
                           size_t              uiPixelCount,
                           int                *piWidth,
                           int                *piHeight);

#endif /* SHIFTER_H */
