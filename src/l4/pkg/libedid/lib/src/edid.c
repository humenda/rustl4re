#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <l4/libedid/edid.h>

/// Defines various EDID length
enum
{
  Libedid_header_size     = 8,   ///< Size of the EDID header in bytes
  Libedid_num_st_timings  = 8,   ///< Up to 8 optional standard timings of 16byte each are defined in EDID 1.4
};

/// Defines addresses of EDID data structures
enum
{
  Libedid_pnp_id       = 0x8,    ///< EDID vendor and product identification
  Libedid_version      = 0x12,   ///< EDID version
  Libedid_revision     = 0x13,   ///< EDID revision
  Libedid_st           = 0x26,   ///< EDID standard timings
  Libedid_descriptors  = 0x36,   ///< EDID 18byte descriptors
  Libedid_ext_blocks   = 0x126,  ///< Number of EDID extension blocks
};

static unsigned char edid_v1_header[Libedid_header_size]
   = { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };

int libedid_check_header(const unsigned char *edid)
{
  if (memcmp(edid, edid_v1_header, Libedid_header_size))
    return -EINVAL;

  return 0;
}

int libedid_checksum(const unsigned char *edid)
{
  unsigned char sum = 0;

  for (unsigned i = 0; i < Libedid_block_size; ++i)
    sum += edid[i];

  if (sum)
    return -EINVAL;

  return 0;
}

unsigned libedid_version(const unsigned char *edid)
{
  return edid[Libedid_version];
}

unsigned libedid_revision(const unsigned char *edid)
{
  return edid[Libedid_revision];
}

void libedid_pnp_id(const unsigned char *edid, unsigned char *id)
{
  const unsigned char *m_id = edid + Libedid_pnp_id;
  id[0] = ((m_id[0] & 0x7c) >> 2) + '@';
  id[1] = (((m_id[0] & 0x2) << 3) | ((m_id[1] & 0xe0) >> 5)) + '@';
  id[2] = (m_id[1] & 0x1f) + '@';
  id[3] = 0;
}

void libedid_prefered_resolution(const unsigned char *edid,
                                 unsigned *w, unsigned *h)
{
  const unsigned char *block = edid + Libedid_descriptors;

  *w = (block[2] & 0xff) | ((block[4] & 0xf0) << 4);
  *h = (block[5] & 0xff) | ((block[7] & 0xf0) << 4);
}

unsigned libedid_num_ext_blocks(const unsigned char *edid)
{
  return edid[Libedid_ext_blocks];
}

unsigned libedid_dump_standard_timings(const unsigned char *edid)
{
  const unsigned char *st = edid + Libedid_st;
  unsigned hpx = 0, vpx = 0;
  unsigned i = 0;

  for (i = 0; i < Libedid_num_st_timings; ++i)
    {
      if (st[0] == 0)
        break;

      hpx = ((st[0] & 0xff) + 31) * 8;
      switch (((st[1] & 0xc0) >> 6))
        {
      case 0: // 16:10
        vpx = hpx * 10 / 16;
        break;
      case 1: // 4:3
        vpx = hpx * 3 / 4;
        break;
      case 2: // 5:4
        vpx = hpx * 4 / 5;
        break;
      case 3: // 16:9
        vpx = hpx * 9 / 16;
        break;
        }
      printf("%dx%d (AR=%d)\n", hpx, vpx, (st[1] & 0xc0) >> 6);
      st += 2;
    }

  return i;
}

void libedid_dump(const unsigned char *edid)
{
  for (unsigned i = 0; i < Libedid_block_size; ++i)
    {
      printf("0x%02x ", edid[i]);
      if ((i % 16) == 15)
        printf("\n");
    }
  printf("\n");
}
