/*
 * image_msa.h - ST4Ever MSA disk image codec
 *
 * Adds MSA (Magic Shadow Archiver) support as a per-track RLE codec
 * layered on top of image_st_t.  A .msa file stores the same 737 280-
 * byte Atari ST disk content as a .st file, but with each track
 * independently compressed (or stored raw when that is shorter).
 *
 * MSA header (10 bytes, all fields big-endian):
 *   [0..1]  0x0E0F   magic
 *   [2..3]  sectors per track (9 for standard DD)
 *   [4..5]  sides - 1  (0=single-sided, 1=double-sided)
 *   [6..7]  first track (0 for a full image)
 *   [8..9]  last track  (79 for a standard 80-track DD image)
 *
 * Per track (t in [first..last]), per side (s in [0..sides]):
 *   2 bytes  data_length  (big-endian)
 *   N bytes  track data   (raw if data_length == SPT*512, else RLE)
 *
 * MSA RLE encoding (0xE5 is the escape byte):
 *   byte != 0xE5 → copy as-is
 *   0xE5 <run_byte> <count_hi> <count_lo> → emit run_byte × count
 *   Literal 0xE5 must always be escaped: 0xE5 0xE5 0x00 0x01.
 *   Runs of ≥ 5 identical non-escape bytes are also worth encoding.
 */

#ifndef IMAGE_MSA_H
#define IMAGE_MSA_H

#include "common.h"
#include "image_st.h"

/* ------------------------------------------------------------------ */
/* MSA format constants                                                 */
/* ------------------------------------------------------------------ */

#define IMSA_HDR_SIZE       10u               /* header length in bytes  */
#define IMSA_MAGIC          0x0E0Fu           /* magic word BE            */
#define IMSA_ESCAPE         0xE5u             /* RLE escape byte          */

/* Standard DD geometry values written in the MSA header              */
#define IMSA_SPT            IST_SPT           /* 9  sectors per track     */
#define IMSA_SIDES_HDR      1u                /* sides field: 1 = double  */
#define IMSA_TRACK_FIRST    0u                /* first track              */
#define IMSA_TRACK_LAST     (IST_TRACKS - 1u) /* last track (79)          */

/* Uncompressed bytes per track: sectors * bytes/sector = 4608         */
#define IMSA_TRACK_BYTES    (IST_SPT * IST_SECTOR_SIZE)

/* ------------------------------------------------------------------ */
/* API                                                                  */
/* ------------------------------------------------------------------ */

/*
 * image_msa_load() - Decompress a .msa file into a new image_st_t.
 *
 * The resulting image_st_t contains the same disk content as a .st
 * image loaded with image_st_load() from the same disk.
 *
 * Parameters:
 *   szPath [in]  : Path to a .msa file.
 *   pptImg [out] : Receives the allocated image_st_t handle on success.
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any parameter is NULL, the file cannot be opened,
 *               the MSA magic is wrong, the geometry is invalid, or
 *               any track fails to decompress.
 */
st_error_t image_msa_load(const char *szPath, image_st_t **pptImg);

/*
 * image_msa_save() - Compress an image_st_t to a .msa file.
 *
 * Each track is compressed independently; if the compressed form is
 * not shorter than the raw 4608-byte track it is stored raw.  The
 * header always describes a standard DD image (80 tracks, double-
 * sided, 9 sectors/track, first=0, last=79).
 *
 * Parameters:
 *   ptImg  [in] : Source image handle.
 *   szPath [in] : Destination .msa path (created or overwritten).
 *
 * Returns:
 *   ST_NO_ERROR on success.
 *   ST_ERROR    if any parameter is NULL or a write fails.
 */
st_error_t image_msa_save(image_st_t *ptImg, const char *szPath);

#endif /* IMAGE_MSA_H */
