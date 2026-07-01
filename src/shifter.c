/*
 * shifter.c - Atari ST Shifter video rendering engine (UC26)
 *
 * Decodes the Atari ST video frame buffer (bitplane planar format) to
 * a flat RGB32 pixel array.
 *
 * Frame buffer layout — all modes share 32,000 bytes:
 *   Low  (320x200): 200 rows x 20 groups x 8 bytes  (4 planes x 2 bytes)
 *   Med  (640x200): 200 rows x 40 groups x 4 bytes  (2 planes x 2 bytes)
 *   High (640x400): 400 rows x 40 words  x 2 bytes  (1 plane  x 2 bytes)
 *
 * Bitplane decoding (per 16-pixel group):
 *   For pixel p (0..15), bit position b = 15 - p.
 *   Low : color_index = (P0[b]) | (P1[b]<<1) | (P2[b]<<2) | (P3[b]<<3)
 *   Med : color_index = (P0[b]) | (P1[b]<<1)
 *   High: color_index = P0[b]
 *
 * Palette: ST colour word bits [10:8]=R, [6:4]=G, [2:0]=B (3 bits each).
 * Channel scale: (n * 255) / 7  maps 0-7 to 0-255.
 *
 * LOG_TRACE omitted: shifter_render is called at VBL rate (50 Hz) — R22.
 */

#include "shifter.h"
#include "trace.h"

/* Frame buffer size in bytes (same for all three resolutions) */
#define SHIFTER_FB_SIZE  32000u

/* ------------------------------------------------------------------
 * Internal: scale a 3-bit ST colour channel (0-7) to 8 bits (0-255)
 * ------------------------------------------------------------------ */

static st_u8_t st_channel_to8(st_u8_t ui3)
{
    return (st_u8_t)((ui3 * 255u) / 7u);
}

/* ------------------------------------------------------------------
 * Internal: render low resolution (320x200, 4 bitplanes, 16 colours)
 *
 * Row stride   : 160 bytes (20 groups x 8 bytes)
 * Pixels / row : 320 (20 groups x 16 pixels)
 * ------------------------------------------------------------------ */

static void render_low(const st_u8_t  *pFB,
                        st_u32_t       *auPixels,
                        const st_u32_t *auRGB)
{
    int            iRow;
    int            iGrp;
    int            iPix;
    int            iOut;
    int            iBit;
    const st_u8_t *pGrp;
    st_u16_t       uiP0;
    st_u16_t       uiP1;
    st_u16_t       uiP2;
    st_u16_t       uiP3;
    st_u8_t        uiCI;

    iOut = 0;
    for (iRow = 0; iRow < SHIFTER_HEIGHT_LOW; ++iRow)
    {
        for (iGrp = 0; iGrp < 20; ++iGrp)
        {
            pGrp = pFB + (iRow * 160) + (iGrp * 8);
            uiP0 = ((st_u16_t)pGrp[0] << 8) | (st_u16_t)pGrp[1];
            uiP1 = ((st_u16_t)pGrp[2] << 8) | (st_u16_t)pGrp[3];
            uiP2 = ((st_u16_t)pGrp[4] << 8) | (st_u16_t)pGrp[5];
            uiP3 = ((st_u16_t)pGrp[6] << 8) | (st_u16_t)pGrp[7];
            for (iPix = 0; iPix < 16; ++iPix)
            {
                iBit = 15 - iPix;
                uiCI = (st_u8_t)(
                          ((uiP0 >> iBit) & 1u)
                        | (((uiP1 >> iBit) & 1u) << 1u)
                        | (((uiP2 >> iBit) & 1u) << 2u)
                        | (((uiP3 >> iBit) & 1u) << 3u));
                auPixels[iOut++] = auRGB[uiCI];
            }
        }
    }
}

/* ------------------------------------------------------------------
 * Internal: render medium resolution (640x200, 2 bitplanes, 4 colours)
 *
 * Row stride   : 160 bytes (40 groups x 4 bytes)
 * Pixels / row : 640 (40 groups x 16 pixels)
 * ------------------------------------------------------------------ */

static void render_med(const st_u8_t  *pFB,
                        st_u32_t       *auPixels,
                        const st_u32_t *auRGB)
{
    int            iRow;
    int            iGrp;
    int            iPix;
    int            iOut;
    int            iBit;
    const st_u8_t *pGrp;
    st_u16_t       uiP0;
    st_u16_t       uiP1;
    st_u8_t        uiCI;

    iOut = 0;
    for (iRow = 0; iRow < SHIFTER_HEIGHT_MED; ++iRow)
    {
        for (iGrp = 0; iGrp < 40; ++iGrp)
        {
            pGrp = pFB + (iRow * 160) + (iGrp * 4);
            uiP0 = ((st_u16_t)pGrp[0] << 8) | (st_u16_t)pGrp[1];
            uiP1 = ((st_u16_t)pGrp[2] << 8) | (st_u16_t)pGrp[3];
            for (iPix = 0; iPix < 16; ++iPix)
            {
                iBit = 15 - iPix;
                uiCI = (st_u8_t)(
                          ((uiP0 >> iBit) & 1u)
                        | (((uiP1 >> iBit) & 1u) << 1u));
                auPixels[iOut++] = auRGB[uiCI & 0x03u];
            }
        }
    }
}

/* ------------------------------------------------------------------
 * Internal: render high resolution (640x400, 1 bitplane, 2 colours)
 *
 * Row stride   : 80 bytes (40 words x 2 bytes)
 * Pixels / row : 640 (40 words x 16 pixels)
 * ------------------------------------------------------------------ */

static void render_high(const st_u8_t  *pFB,
                         st_u32_t       *auPixels,
                         const st_u32_t *auRGB)
{
    int            iRow;
    int            iWord;
    int            iBit;
    int            iOut;
    const st_u8_t *pWord;
    st_u16_t       uiW;
    st_u8_t        uiCI;

    iOut = 0;
    for (iRow = 0; iRow < SHIFTER_HEIGHT_HIGH; ++iRow)
    {
        for (iWord = 0; iWord < 40; ++iWord)
        {
            pWord = pFB + (iRow * 80) + (iWord * 2);
            uiW   = ((st_u16_t)pWord[0] << 8) | (st_u16_t)pWord[1];
            for (iBit = 15; iBit >= 0; --iBit)
            {
                uiCI = (st_u8_t)((uiW >> iBit) & 1u);
                auPixels[iOut++] = auRGB[uiCI & 0x01u];
            }
        }
    }
}

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

void shifter_screen_size(st_u8_t uiRes, int *piWidth, int *piHeight)
{
    switch (uiRes)
    {
        case ST_RES_LOW:
            *piWidth  = SHIFTER_WIDTH_LOW;
            *piHeight = SHIFTER_HEIGHT_LOW;
            break;
        case ST_RES_MED:
            *piWidth  = SHIFTER_WIDTH_MED;
            *piHeight = SHIFTER_HEIGHT_MED;
            break;
        case ST_RES_HIGH:
            *piWidth  = SHIFTER_WIDTH_HIGH;
            *piHeight = SHIFTER_HEIGHT_HIGH;
            break;
        default:
            *piWidth  = 0;
            *piHeight = 0;
            break;
    }
}

st_u32_t shifter_palette_to_rgb(st_u16_t uiColor)
{
    st_u8_t uiR;
    st_u8_t uiG;
    st_u8_t uiB;

    uiR = st_channel_to8((st_u8_t)((uiColor >> 8u) & 0x07u));
    uiG = st_channel_to8((st_u8_t)((uiColor >> 4u) & 0x07u));
    uiB = st_channel_to8((st_u8_t)( uiColor         & 0x07u));
    return ((st_u32_t)uiR << 16u)
         | ((st_u32_t)uiG <<  8u)
         |  (st_u32_t)uiB;
}

st_error_t shifter_render( st_u32_t           *auPixels,
                           size_t              uiPixelCount,
                           int                *piWidth,
                           int                *piHeight)
{
    st_u32_t       auRGB[ST_PALETTE_COLORS];
    const st_u8_t *pFB;
    int            iW;
    int            iH;
    int            i;

    if (auPixels  == NULL
        || piWidth   == NULL
        || piHeight  == NULL)
    {
        LOG_ERROR("NULL parameter: auPixels=%p "
                  "piWidth=%p piHeight=%p",
                  (void *)auPixels,
                  (void *)piWidth,   (void *)piHeight);
        return ST_ERROR;
    }

    shifter_screen_size(st_get_resolution(), &iW, &iH);
    if (iW == 0)
    {
        LOG_ERROR("unknown resolution %u",
                  st_get_resolution());
        return ST_ERROR;
    }
    if (uiPixelCount < (size_t)(iW * iH))
    {
        LOG_ERROR("pixel buffer too small: need %d have %zu",
                  iW * iH, uiPixelCount);
        return ST_ERROR;
    }
    if (st_get_screen_base() + SHIFTER_FB_SIZE
            > (st_u32_t)ST_RAM_SIZE)
    {
        LOG_ERROR("screen base 0x%06X out of RAM",
                  st_get_screen_base());
        return ST_ERROR;
    }

    pFB = st_get_ram_pointer(0) + st_get_screen_base();

    for (i = 0; i < ST_PALETTE_COLORS; ++i)
    {
        auRGB[i] = shifter_palette_to_rgb(st_get_palette(i));
    }

    switch (st_get_resolution())
    {
        case ST_RES_LOW:  render_low( pFB, auPixels, auRGB); break;
        case ST_RES_MED:  render_med( pFB, auPixels, auRGB); break;
        case ST_RES_HIGH: render_high(pFB, auPixels, auRGB); break;
        default:          return ST_ERROR;
    }

    *piWidth  = iW;
    *piHeight = iH;
    LOG_INFO("rendered %dx%d res=%u screen_base=0x%06X",
             iW, iH,
             st_get_resolution(),
             st_get_screen_base());
    return ST_NO_ERROR;
}
