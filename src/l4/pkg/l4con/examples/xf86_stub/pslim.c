/*!
 * \file	con/examples/xf86_stub/pslim.c
 * \brief	pSLIM interface to console
 *
 * \date	01/2002
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de> */

/*
 * Copyright (c) 2003 by Technische Universität Dresden, Germany
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * TECHNISCHE UNIVERSITÄT DRESDEN BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include "pslim.h"

#include <l4/env/errno.h>
#include <l4/sys/syscalls.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/types.h>
#include <l4/dm_phys/dm_phys.h>
#include <l4/names/libnames.h>
#include <l4/util/l4_macros.h>
#include <l4/l4con/l4con.h>
#include <l4/l4con/l4con-client.h>

/* All drivers initialising the SW cursor need this */
#include "mipointer.h"
#include "shadowfb.h"

/* Colormap handling */
#include "micmap.h"
#include "xf86cmap.h"
#include "xf86Priv.h"
//#include "xf86Bus.h"

#define L4IOTYPE	(0x34)

#undef ALLOW_OFFSCREEN_PIXMAPS

/* Mandatory functions */
static const OptionInfoRec *PSLIMAvailableOptions (int chipid, int busid);
static void PSLIMIdentify (int flags);
static Bool PSLIMProbe (DriverPtr drv, int flags);
static Bool PSLIMPreInit (ScrnInfoPtr pScrn, int flags);
static Bool PSLIMScreenInit (int Index, ScreenPtr pScreen, 
			     int argc, char **argv);
static Bool PSLIMEnterVT (int scrnIndex, int flags);
static void PSLIMLeaveVT (int scrnIndex, int flags);
static Bool PSLIMCloseScreen (int scrnIndex, ScreenPtr pScreen);
static Bool PSLIMSaveScreen (ScreenPtr pScreen, int mode);

static Bool PSLIMSwitchMode (int scrnIndex, DisplayModePtr pMode, int flags);
static void PSLIMAdjustFrame (int scrnIndex, int x, int y, int flags);
static void PSLIMFreeScreen (int scrnIndex, int flags);
static void PSLIMFreeRec (ScrnInfoPtr pScrn);

/* locally used functions */
static void PSLIMLoadPalette (ScrnInfoPtr pScrn, int numColors, int *indices,
			      LOCO *colors, VisualPtr pVisual);

static void PSLIMIpcUpdate(ScreenPtr pScreen, shadowBufPtr pBuf);
static void PSLIMMapShadowUpdate(ScreenPtr pScreen, shadowBufPtr pBuf);
static void PSLIMMapPhysUpdate(ScreenPtr pScreen, shadowBufPtr pBuf);
static void PSLIMPostDirtyUpdate(ScrnInfoPtr pScrn, int nboxes, BoxPtr boxPtr);
static void PSLIMInitAccel(ScreenPtr pScreen);
static Bool PSLIMInitSIGIOHandler(ScrnInfoPtr pScrn);

static Bool PSLIMMapFB (ScrnInfoPtr pScrn);
static void PSLIMUnmapFB (ScrnInfoPtr pScrn);

#ifdef L4CON_USE_DRIVER_FUNC
static Bool PSLIMDriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op,
                            pointer ptr);
#endif

/* 
 * This contains the functions needed by the server after loading the
 * driver module.  It must be supplied, and gets added the driver list by
 * the Module Setup funtion in the dynamic case.  In the static case a
 * reference to this is compiled in, and this requires that the name of
 * this DriverRec be an upper-case version of the driver name.
 */
DriverRec PSLIM = {
  PSLIM_VERSION,
  PSLIM_DRIVER_NAME,
  PSLIMIdentify,
  PSLIMProbe,
  PSLIMAvailableOptions,
  NULL,
  0,
#ifdef L4CON_USE_DRIVER_FUNC
  PSLIMDriverFunc
#endif
};

enum GenericTypes
{
  CHIP_PSLIM_GENERIC
};

/* Supported chipsets */
static SymTabRec PSLIMChipsets[] = {
  {CHIP_PSLIM_GENERIC, "pslim"},
  {-1,                 NULL}
};

typedef enum {
  OPTION_MODE,
} PSLIMOpts;

/*
 * Modes:
 *   0  Physical FB mapped from L4 console, use shadow FB. Fastest mode since
 *      screen updates (normally) does not involve any IPC and we read from
 *      from the fast shadow framebuffer (RAM). This is much like the VESA
 *      driver with shadowFB enabled.
 *   1  Physical FB mapped from L4 console, no  shadow FB. Slower because we
 *      read from the physical framebuffer which is usually very slow. HW
 *      acceleration possible. This is much like the VESA driver w/o shadowFB
 *      enabled but with some acceleration.
 *   2  Physical FB not mapped, use shadow FB, use long IPC to transfer
 *      dirty regions to L4 console. Screen updates involve many IPCs but
 *      speed is still acceptable on modern systems.
 *   3  Physical FB not mapped, shadow FB mapped into console, update dirty
 *      regions in L4 console. Slightly slower than mode 0 since each screen
 *      update involves an L4 IPC.
 * 
 * Modes 0 and 3 should have very similar performance.
 */

static const OptionInfoRec PSLIMOptions[] = {
    { OPTION_MODE,          "Mode",         OPTV_INTEGER, {0}, FALSE },
    { -1,                    NULL,          OPTV_NONE,    {0}, FALSE }
};

/*
 * List of symbols from other modules that this module references.  This
 * list is used to tell the loader that it is OK for symbols here to be
 * unresolved providing that it hasn't been told that they haven't been
 * told that they are essential via a call to xf86LoaderReqSymbols() or
 * xf86LoaderReqSymLists().  The purpose is this is to avoid warnings about
 * unresolved symbols that are not required.
 */
static const char *fbSymbols[] = {
  "fbScreenInit",
  "fbPictureInit",
#ifdef L4CON_USE_CFB24_32
  "cfb24_32ScreenInit",
#endif
  NULL
};

static const char *shadowfbSymbols[] = {
  "ShadowFBInit2",
  NULL
};

static const char *shadowSymbols[] = {
  "shadowAlloc",
  "shadowInit",
  NULL
};

static const char *xaaSymbols[] = {
  "XAAInit",
  "XAACreateInfoRec",
  NULL
};


#ifdef XFree86LOADER

/* Module loader interface */
static MODULESETUPPROTO (pslimSetup);

static XF86ModuleVersionInfo pslimVersionRec = {
  PSLIM_DRIVER_NAME,
  "TU Dresden, OS Group, Frank Mehnert",
  MODINFOSTRING1,
  MODINFOSTRING2,
#if 1
  (((7) * 10000000) + ((1) * 100000) + ((1) * 1000) + 0),
#else
  XF86_VERSION_CURRENT,
#endif
  PSLIM_MAJOR_VERSION, PSLIM_MINOR_VERSION, PSLIM_PATCHLEVEL,
  ABI_CLASS_VIDEODRV,		/* This is a video driver */
  ABI_VIDEODRV_VERSION,
  MOD_CLASS_VIDEODRV,
  {0, 0, 0, 0}
};

/*
 * This data is accessed by the loader.  The name must be the module name
 * followed by "ModuleData".
 */
XF86ModuleData pslimModuleData = { &pslimVersionRec, pslimSetup, NULL };

static pointer
pslimSetup (pointer Module, pointer Options, int *ErrorMajor, int *ErrorMinor)
{
  static Bool Initialised = FALSE;

  if (!Initialised)
    {
      int flags = 0;
#ifdef L4CON_USE_DRIVER_FUNC
      flags |= HaveDriverFuncs;
#endif
      Initialised = TRUE;
      xf86AddDriver (&PSLIM, Module, flags);
      LoaderRefSymLists(fbSymbols, shadowSymbols, 
			shadowfbSymbols, xaaSymbols, NULL);
      return (pointer) TRUE;
    }

  if (ErrorMajor)
    *ErrorMajor = LDR_ONCEONLY;
  return (NULL);
}

#endif

static const OptionInfoRec *
PSLIMAvailableOptions (int chipid, int busid)
{
  return (PSLIMOptions);
}

static void
PSLIMIdentify (int flags)
{
  xf86PrintChipsets (PSLIM_NAME, "driver for DROPS pSLIM protocol", 
		     PSLIMChipsets);
}

#ifdef L4CON_USE_DRIVER_FUNC
static Bool
PSLIMDriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op, pointer ptr)
{
  CARD32 *flag;

  xf86DrvMsg (0, X_CONFIG, "PSLIMDriverFunc %d\n", op);
  switch (op)
    {
    case GET_REQUIRED_HW_INTERFACES:
      flag = (CARD32*)ptr;
      (*flag) = 0;
      return TRUE;
    default:
      return FALSE;
    }
}
#endif

/*
 * This function is called once, at the start of the first server generation to
 * do a minimal probe for supported hardware.
 */
static Bool
PSLIMProbe (DriverPtr drv, int flags)
{
  Bool foundScreen = FALSE;
  int numDevSections;
  GDevPtr *devSections;
  int i, entity;
  ScrnInfoPtr pScrn = NULL;
  EntityInfoPtr pEnt;

  /*
   * Find the config file Device sections that match this
   * driver, and return if there are none.
   */
  if ((numDevSections = xf86MatchDevice (PSLIM_NAME, &devSections)) <= 0)
    return FALSE;

  for (i = 0; i < numDevSections; i++)
    {
      entity = xf86ClaimFbSlot(drv, 0, devSections[i], TRUE);

      pEnt = xf86GetEntityInfo(entity);
      pScrn = xf86AllocateScreen(pEnt->driver, 0);
      xf86AddEntityToScreen(pScrn, entity);
      
      pScrn->driverVersion = PSLIM_VERSION;
      pScrn->driverName    = PSLIM_DRIVER_NAME;
      pScrn->name          = PSLIM_NAME;
      pScrn->Probe         = PSLIMProbe;
      pScrn->PreInit       = PSLIMPreInit;
      pScrn->ScreenInit    = PSLIMScreenInit;
      pScrn->SwitchMode    = PSLIMSwitchMode;
      pScrn->AdjustFrame   = PSLIMAdjustFrame;
      pScrn->EnterVT       = PSLIMEnterVT;
      pScrn->LeaveVT       = PSLIMLeaveVT;
      pScrn->FreeScreen    = PSLIMFreeScreen;
      foundScreen = TRUE;
    }

  xfree (devSections);

  return (foundScreen);
}

static PSLIMPtr
PSLIMGetRec (ScrnInfoPtr pScrn)
{
  PSLIMPtr pSlim = (PSLIMPtr) pScrn->driverPrivate;
  
  if (!pScrn->driverPrivate)
    {
      pScrn->driverPrivate = xcalloc (sizeof (PSLIMRec), 1);
      pSlim = (PSLIMPtr) pScrn->driverPrivate;
      pSlim->mapShadowDs = L4DM_INVALID_DATASPACE;
    }

  return pSlim;
}

static void
PSLIMFreeRec (ScrnInfoPtr pScrn)
{
  xfree (pScrn->driverPrivate);
  pScrn->driverPrivate = NULL;
}

/*
 * This function is called once for each screen at the start of the first
 * server generation to initialise the screen for all server generations.
 */
static Bool
PSLIMPreInit (ScrnInfoPtr pScrn, int flags)
{
  PSLIMPtr pSlim;
  DisplayModePtr pMode;
  ModeInfoData *data = NULL;
  char *mod = NULL;
  const char *reqSym = NULL;
  Gamma gzeros = { 0.0, 0.0, 0.0 };
  rgb rzeros = { 0, 0, 0 };
  int error;
  CORBA_Environment env = dice_default_environment;
  l4_uint8_t gmode;
  unsigned bits_per_pixel, bytes_per_pixel, bytes_per_line;
  unsigned xres, yres, fn_x, fn_y;
  unsigned long sbuf1_size, sbuf2_size, sbuf3_size;

  struct __attribute__((packed)) {
      l4_threadid_t    x_id;
      l4_threadid_t    vc_id;
      l4dm_dataspace_t ds_id;
  } ctu;

  if (flags & PROBE_DETECT)
    return FALSE;

  pSlim = PSLIMGetRec (pScrn);
  pSlim->pEnt = xf86GetEntityInfo (pScrn->entityList[0]);
  pSlim->device = xf86GetDevFromEntity (pScrn->entityList[0],
					pScrn->entityInstanceList[0]);

  if ((pSlim->dropscon_dev = open("/proc/dropscon",       O_RDONLY)) < 0 &&
      (pSlim->dropscon_dev = open("/proc/l4/l4fb_xf86if", O_RDONLY)) < 0)
    {
      xf86DrvMsg (pScrn->scrnIndex, X_ERROR,
		  "Error opening dropscon device\n");
      return FALSE;
    }

  /* Pass our thread ID to the Linux server (for l4dm_share) and request the
   * L4 thread ID and the framebuffer dataspace ID of the Linux virtual console */
  ctu.x_id = l4_myself();

  if ((error = ioctl(pSlim->dropscon_dev, (L4IOTYPE << 8) | 1, &ctu)) < 0)
    {
      xf86DrvMsg (pScrn->scrnIndex, X_ERROR,
		  "Error %d ioctl 1 on dropscon device\n", error);
      return FALSE;
    }

  pSlim->vc_tid      = ctu.vc_id;
  pSlim->mapShadowDs = ctu.ds_id;

  xf86DrvMsg (pScrn->scrnIndex, X_CONFIG,
	      "virtual DROPS console at "l4util_idfmt"\n", 
	      l4util_idstr(pSlim->vc_tid));

  /* get maximum receive buffer size which was set in dropscon module */
  if (con_vc_gmode_call(&pSlim->vc_tid, 
		   &gmode, &sbuf1_size, &sbuf2_size, &sbuf3_size, &env)
      || DICE_HAS_EXCEPTION(&env))
    {
      xf86DrvMsg (pScrn->scrnIndex, X_ERROR,
		  "Error determining sbuf_size from console parameters "
		  "(exc=%d, %08x)\n", DICE_EXCEPTION_MAJOR(&env), 
		  DICE_IPC_ERROR(&env));
      return FALSE;
    }

  /* get graphics screen resolution, bpp and Bpp */
  if (con_vc_graph_gmode_call(&pSlim->vc_tid, &gmode, &xres, &yres,
			      &bits_per_pixel, &bytes_per_pixel,
                              &bytes_per_line, &pSlim->accel_flags,
                              &fn_x, &fn_y, &env)
      || DICE_HAS_EXCEPTION(&env))
    {
      xf86DrvMsg (pScrn->scrnIndex, X_ERROR,
		  "Error determining console parameters (exc=%d, %08x)\n", 
		  DICE_EXCEPTION_MAJOR(&env), DICE_IPC_ERROR(&env));
      return FALSE;
    }
  
  xf86DrvMsg (pScrn->scrnIndex, X_CONFIG,
	      "DROPS console is %dx%d@%d (%dBpp, sbuf_size=%ld)\n",
	      xres, yres, bits_per_pixel, bytes_per_pixel, sbuf1_size);

  if (sbuf1_size < xres * bytes_per_pixel)
    {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	      "sbuf1_size at console too small (should be %d but is %ld\n",
	      xres*bytes_per_pixel, sbuf1_size);
      return FALSE;
    }

  /* hack: override config entry */
  pScrn->bitsPerPixel = bits_per_pixel;
  pScrn->confScreen->defaultdepth = bits_per_pixel == 32 ? 24 : bits_per_pixel;
  pScrn->chipset = "pslim";
  pScrn->monitor = pScrn->confScreen->monitor;
  pScrn->progClock = TRUE;
  pScrn->rgbBits = 8;

  if (!xf86SetDepthBpp (pScrn, 16, 0, 0,
			bits_per_pixel == 32 ? Support32bppFb : Support24bppFb))
    return FALSE;
  xf86PrintDepthBpp (pScrn);

  /* Get the depth24 pixmap format */
  if (pScrn->depth == 24 && pSlim->pix24bpp == 0)
    pSlim->pix24bpp = xf86GetBppFromDepth (pScrn, 24);

  /* color weight */
  if (pScrn->depth > 8 && !xf86SetWeight (pScrn, rzeros, rzeros))
    return FALSE;

  /* visual init */
  if (!xf86SetDefaultVisual (pScrn, -1))
    return FALSE;

  xf86SetGamma (pScrn, gzeros);

  /* XXX */
  pScrn->videoRam = 8 << 20;

  /* Set display resolution */
  xf86SetDpi (pScrn, 0, 0);

  pScrn->monitor->DDC = NULL;

  /* allocate mode */
  pMode = xcalloc(sizeof(DisplayModeRec), 1);

  pMode->status       = MODE_OK;
  pMode->type         = M_T_BUILTIN;
  pMode->name         = "L4con mode";
  pMode->HDisplay     = xres;
  pMode->VDisplay     = yres;
  data                = xcalloc(sizeof(ModeInfoData), 1);
  data->mode          = 0; /* only one mode */
  data->memory_model  = 6; /* Packed Pixel, XXX get from console server */
  pMode->PrivSize     = sizeof(ModeInfoData);
  pMode->Private      = (INT32*)data;
  pScrn->modePool     = pMode;
  pMode->next         =
  pMode->prev         = pMode;
  pScrn->modes        = pScrn->modePool;
  pSlim->bytesPerScanline = bytes_per_line;
  pScrn->currentMode  = pScrn->modes;
  pScrn->displayWidth = bytes_per_line / bytes_per_pixel;
  pScrn->virtualX     = xres;
  pScrn->virtualY     = yres;

  xf86PrintModes (pScrn);
  
  /* options */
  xf86CollectOptions(pScrn, NULL);
  if (!(pSlim->Options = xalloc(sizeof(PSLIMOptions))))
    return FALSE;

  memcpy(pSlim->Options, PSLIMOptions, sizeof(PSLIMOptions));
  xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pSlim->Options);

  xf86GetOptValInteger(pSlim->Options, OPTION_MODE, &pSlim->Mode);
  
  switch (pSlim->Mode)
    {
      case 0:
        /* physical FB mapped from L4 console, use shadow FB */
        pSlim->shadowFB  = TRUE;
        pSlim->mapShadow = FALSE;
        pSlim->mapPhys   = TRUE;
        break;
      case 1:
        /* physical FB mapped from L4 console, no shadow FB */
        pSlim->shadowFB  = FALSE;
        pSlim->mapShadow = FALSE;
        pSlim->mapPhys   = TRUE;
        break;
      case 2:
        /* physical FB not mapped, use shadow FB, use long IPC to transfer
         * dirty regions to L4 console */
        pSlim->shadowFB  = TRUE;
        pSlim->mapShadow = FALSE;
        pSlim->mapPhys   = FALSE;
        break;
      case 3:
        /* physical FB not mapped, shadow FB mapped into console, update
         * dirty regions in L4 console */
        pSlim->shadowFB  = TRUE;
        pSlim->mapShadow = TRUE;
        pSlim->mapPhys   = FALSE;
        break;
      default:
        xf86DrvMsg (pScrn->scrnIndex, X_ERROR, "Unsupported mode %d\n",
                    pSlim->Mode);
        return FALSE;
    }

  xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "Using mode %d\n", pSlim->Mode);

  if (!pSlim->mapPhys)
    {
      /* we don't access the framebuffer directly -- con translates between
       * xres*bpp and bytes_per_scanline */
      pSlim->bytesPerScanline = xres * bytes_per_pixel;
      pScrn->displayWidth = xres;
    }

  mod = "fb";
  reqSym = "fbScreenInit";
  pScrn->bitmapBitOrder = BITMAP_BIT_ORDER;
  xf86LoaderReqSymbols ("fbPictureInit", NULL);

  switch (pScrn->bitsPerPixel)
    {
    case 16:
    case 32:
      break;
    case 24:
#ifdef L4CON_USE_CFB24_32
      /* Not necessary anymore with X.org */
      if (pSlim->pix24bpp == 32)
	{
	  mod = "xf24_32bpp";
	  reqSym = "cfb24_32ScreenInit";
	}
#endif
      break;
    default:
      xf86DrvMsg (pScrn->scrnIndex, X_ERROR,
	          "Unsupported bpp: %d\n", pScrn->bitsPerPixel);
      return FALSE;
    }

  if (pSlim->shadowFB)
    {
      /* load shadow  module */
      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "Using shadow framebuffer\n");
      if (!xf86LoadSubModule (pScrn, "shadow"))
	{
	  PSLIMFreeRec (pScrn);
	  return FALSE;
	}
      xf86LoaderReqSymLists (shadowSymbols, NULL);
    }
  else
    {
      /* load module for post dirty updates */
      if (!xf86LoadSubModule (pScrn, "shadowfb"))
	{
	  PSLIMFreeRec (pScrn);
	  return FALSE;
	}
      xf86LoaderReqSymLists(shadowfbSymbols, NULL);
    }

  if (pSlim->mapPhys && !pSlim->shadowFB)
    {
      /* load accel module */
      if (!xf86LoadSubModule (pScrn, "xaa"))
	{
	  PSLIMFreeRec (pScrn);
	  return FALSE;
	}
      xf86LoaderReqSymLists (xaaSymbols, NULL);
    }

  /* load fb module */
  if (mod && xf86LoadSubModule (pScrn, mod) == NULL)
    {
      PSLIMFreeRec (pScrn);
      return FALSE;
    }
  xf86LoaderReqSymbols (reqSym, NULL);

  if (!PSLIMInitSIGIOHandler (pScrn))
    return FALSE;

  return TRUE;
}

static int
PSLIMMapFB (ScrnInfoPtr pScrn)
{
  PSLIMPtr pSlim = PSLIMGetRec (pScrn);
  /* reserve 20MB here in order to find a 4MB-aligned area of 16MB */
  static char fb_buf[20 << 20];
  l4_addr_t map_addr, fb_offs, dummy;
  l4_snd_fpage_t snd_fpage;
  int rc;
  DICE_DECLARE_ENV(env);

  map_addr = (unsigned)(fb_buf+L4_SUPERPAGESIZE-1) & L4_SUPERPAGEMASK;

  pSlim->fbOffs = 0;
 
  for (fb_offs=0; pSlim->fbPhysSize==0 || fb_offs<pSlim->fbPhysSize;
       fb_offs+=L4_SUPERPAGESIZE)
    {
        if (fb_offs >= (16 << 20))
          goto done;

      /* unmap dangling L4 pages */
      l4_fpage_unmap(l4_fpage(map_addr+fb_offs, L4_LOG2_SUPERPAGESIZE,
	                      L4_FPAGE_RW, L4_MAP_ITEM_MAP),
                     L4_FP_FLUSH_PAGE|L4_FP_ALL_SPACES);

      /* map framebuffer from con server */
      env.rcv_fpage = l4_fpage(map_addr+fb_offs, L4_LOG2_SUPERPAGESIZE, 0, 0);
      rc = con_vc_graph_mapfb_call(&pSlim->vc_tid, fb_offs, &snd_fpage,
                                   fb_offs == 0 ? &pSlim->fbOffs : &dummy,
                                   &env);
      if (DICE_HAS_EXCEPTION(&env))
        {
          xf86DrvMsg (pScrn->scrnIndex, X_INFO,
                      "Error mapping offset %08lx of framebuffer\n",
                      fb_offs);
          return -1;
        }

      if (rc == -L4_EPERM)
        {
          xf86DrvMsg (pScrn->scrnIndex, X_INFO,
                      "Mapping of framebuffer currently not allowed\n");
          return 0;
        }

      if (rc == -L4_EINVAL_OFFS)
        {
done:
          if (!pSlim->fbPhysSize)
            {
              pSlim->fbPhysSize = fb_offs;
              break;
            }
        }
    }

  /* even if we failed to map the framebuffer we got the offset */
  pSlim->fbPhysBase = (CARD8*)map_addr;
  pSlim->fbPhysPtr  = (CARD8*)map_addr + pSlim->fbOffs;
  pSlim->fbMapped   = TRUE;

  xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, 
	      "Framebuffer mapped to %08lx-%08lx\n",
              (l4_addr_t)pSlim->fbPhysPtr, (l4_addr_t)map_addr+pSlim->fbPhysSize);

  return 1;
}

static void
PSLIMUnmapFB (ScrnInfoPtr pScrn)
{
  PSLIMPtr pSlim = PSLIMGetRec (pScrn);
  l4_addr_t fb_offs;

  xf86DrvMsg (pScrn->scrnIndex, X_INFO, "Unmapping framebuffer\n");
  pSlim->fbMapped = FALSE;

  /* unmap framebuffer */
  for (fb_offs = 0; fb_offs<pSlim->fbPhysSize; fb_offs += L4_SUPERPAGESIZE)
    {
      l4_fpage_unmap(l4_fpage((l4_addr_t)pSlim->fbPhysBase,
                              L4_LOG2_SUPERPAGESIZE, 0, 0),
                     L4_FP_FLUSH_PAGE | L4_FP_ALL_SPACES);
    }
}

static Bool
PSLIMCreateScreenResources(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  PSLIMPtr pSlim = PSLIMGetRec(pScrn);
  Bool ret;

  pScreen->CreateScreenResources = pSlim->CreateScreenResources;
  ret = pScreen->CreateScreenResources(pScreen);
  pScreen->CreateScreenResources = PSLIMCreateScreenResources;
  shadowAdd(pScreen, pScreen->GetScreenPixmap(pScreen), pSlim->update, 0, 0, 0);

  return ret;
}

static Bool
PSLIMScreenInit (int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
  ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
  PSLIMPtr pSlim = PSLIMGetRec (pScrn);
  VisualPtr visual;
  int memory_model;
  int init_picture = 0;

  /* XXX */
  pScrn->videoRam = 8 << 20;

  if (pSlim->shadowFB)
    {
      /* use shadow framebuffer */
      if (pSlim->mapShadow)
	{
	  /* we want to map the shadow frame buffer into console therefore
      	   * allocate a dataspace */
	  unsigned bpp  = (pScrn->bitsPerPixel+1) / 8;
      	  pSlim->fbVirtSize = l4_round_page(pScrn->virtualX *
					    bpp * pScrn->virtualY);
	  CORBA_Environment env = dice_default_environment;
	  int error;

	  if (l4dm_is_invalid_ds(pSlim->mapShadowDs))
	    {
	      l4_threadid_t dm_id;
	      l4dm_dataspace_t ds;

	      /* where is our default memory dataspace manager */
	      if (!names_waitfor_name(L4DM_MEMPHYS_NAME, &dm_id, 2000))
		{
		  xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			     L4DM_MEMPHYS_NAME " not found\n");
		  return FALSE;
		}
	  
	      /* allocate dataspace */
	      if ((error = l4dm_mem_open(dm_id, pSlim->fbVirtSize,
                                         0, 0, "xf86 vfb", &ds)))
		{
		  xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			     "Can't allocate dataspace of %ldkB\n",
			     pSlim->fbVirtSize / 1024);
		  return FALSE;
		}

	      /* con_tid has to be a client of the dataspace */
	      if ((error = l4dm_share(&ds, pSlim->vc_tid, L4DM_RO)))
		{
		  xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			     "Can't share data_ds with console\n");
		  return FALSE;
		}

	      if (con_vc_direct_setfb_call(&pSlim->vc_tid, &ds, &env)
		  || DICE_HAS_EXCEPTION(&env))
		{
		  xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Can't set framebuffer at console (exc=%d, %08x)\n",
			     DICE_EXCEPTION_MAJOR(&env), DICE_IPC_ERROR(&env));
		  return FALSE;
		}
    	      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			 "Using own dataspace as framebuffer\n");
	      pSlim->mapShadowDs = ds;
	    }
	  else
	    {
	      /* framebuffer already mapped to console */
    	      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			 "Using dataspace from Linux server as framebuffer\n");
              pSlim->shareDS = TRUE;
	    }

	  if ((error = l4rm_attach(&pSlim->mapShadowDs, pSlim->fbVirtSize, 0, 0,
				   (void**)&pSlim->fbVirtPtr)))
    	    {
	      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			 "Can't attach framebuffer ds\n");
	      return FALSE;
	    }
	}
      else /* !mapShadow */
	{
	  /* default way like XFree86 does it */
	  if ((pSlim->fbVirtPtr = shadowAlloc (pScrn->virtualX, pScrn->virtualY,
					       pScrn->bitsPerPixel)) == NULL)
	    {
	      xf86DrvMsg (pScrn->scrnIndex, X_ERROR,
			 "Error allocating shadow buffer of size %d\n",
			 pScrn->virtualX*pScrn->virtualY*pScrn->bitsPerPixel/8);
	      return FALSE;
	    }

          pSlim->fbVirtSize = pScrn->virtualX*pScrn->virtualY*pScrn->bitsPerPixel/8;

          if (!pSlim->mapPhys)
            xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Using pSLIM region update\n");
	}

      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                 "Using shadow framebuffer %08lx-%08lx\n",
                 (l4_addr_t)pSlim->fbVirtPtr,
                 (l4_addr_t)pSlim->fbVirtPtr+pSlim->fbVirtSize);

      /* we are active */
      if (!pSlim->mapPhys)
        pScrn->vtSema = TRUE;
    }

  if (pSlim->mapPhys)
    {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                 "Mapping physical framebuffer from L4 console\n");

      /* use direct mapped framebuffer */
      switch (PSLIMMapFB(pScrn))
        {
          case -1: return FALSE;          /* FATAL error */
          case  0: pScrn->vtSema = FALSE; /* mapping denied */
          case  1: pScrn->vtSema = TRUE;  /* mapping permitted */
        }
    }

  /* mi layer */
  miClearVisualTypes ();
  if (!xf86SetDefaultVisual (pScrn, -1))
    return FALSE;

  if (pScrn->bitsPerPixel > 8)
    {
      if (!miSetVisualTypes (pScrn->depth, TrueColorMask,
			     pScrn->rgbBits, TrueColor))
	return FALSE;
    }
  else
    {
      if (!miSetVisualTypes (pScrn->depth,
			     miGetDefaultVisualMask (pScrn->depth),
			     pScrn->rgbBits, pScrn->defaultVisual))
	return FALSE;
    }
  if (!miSetPixmapDepths ())
    return FALSE;

  memory_model = ((ModeInfoData*)pScrn->modes->Private)->memory_model;
  
  switch (memory_model)
    {
    case 0x0:			/* Text mode */
    case 0x1:			/* CGA graphics */
    case 0x2:			/* Hercules graphics */
    case 0x3:			/* Planar */
    case 0x5:			/* Non-chain 4, 256 color */
    case 0x7:			/* YUV */
      xf86DrvMsg (pScrn->scrnIndex, X_ERROR,
		  "Unsupported Memory Model: %d\n", memory_model);
      return FALSE;
    case 0x4:			/* Packed pixel */
    case 0x6:			/* Direct Color */
      switch (pScrn->bitsPerPixel)
	{
	case 24:
#ifdef L4CON_USE_CFB24_32
	  /* Not necessary with X.org */
	  if (pSlim->pix24bpp == 32)
	    {
	      if (!cfb24_32ScreenInit (pScreen,
				       pSlim->shadowFB
                                          ? pSlim->fbVirtPtr : pSlim->fbPhysPtr,
				       pScrn->virtualX, pScrn->virtualY,
				       pScrn->xDpi, pScrn->yDpi,
				       pScrn->displayWidth))
		return FALSE;
              init_picture = 1;
	      break;
	    }
#endif
	case 8:
	case 16:
	case 32:
	  if (!fbScreenInit (pScreen,
			     pSlim->shadowFB
                               ? pSlim->fbVirtPtr : pSlim->fbPhysPtr,
			     pScrn->virtualX, pScrn->virtualY,
			     pScrn->xDpi, pScrn->yDpi,
			     pScrn->displayWidth, pScrn->bitsPerPixel))
	    return FALSE;
          init_picture = 1;
	  break;
	default:
	  xf86DrvMsg (pScrn->scrnIndex, X_ERROR,
		      "Unsupported bpp: %d\n", pScrn->bitsPerPixel);
	  return FALSE;
	}
      break;
    }

  if (pScrn->bitsPerPixel > 8)
    {
      /* Fixup RGB ordering */
      visual = pScreen->visuals + pScreen->numVisuals;
      while (--visual >= pScreen->visuals)
	{
	  if ((visual->class | DynamicClass) == DirectColor)
	    {
	      visual->offsetRed   = pScrn->offset.red;
	      visual->offsetGreen = pScrn->offset.green;
	      visual->offsetBlue  = pScrn->offset.blue;
	      visual->redMask     = pScrn->mask.red;
	      visual->greenMask   = pScrn->mask.green;
	      visual->blueMask    = pScrn->mask.blue;
	    }
	}
    }

  /* must be done after RGB ordering fixed */
  if (init_picture)
      fbPictureInit (pScreen, 0, 0);

  pSlim->update = 0;
  switch (pSlim->Mode)
    {
      case 0:
        /* physical FB mapped from L4 console, use shadow FB */
        pSlim->update = PSLIMMapPhysUpdate;
        break;
      case 1:
        /* physical FB mapped from L4 console, no shadow FB */
        if (!ShadowFBInit2(pScreen, NULL, PSLIMPostDirtyUpdate))
	  {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
        	       "ShadowFB initialization failed\n");
            return FALSE;
          }
        /* accleration possible */
        PSLIMInitAccel(pScreen);
        break;
      case 2:
        /* physical FB not mapped, use shadow FB, use long IPC to transfer
         * dirty regions to L4 console */
        pSlim->update = PSLIMIpcUpdate;
        break;
      case 3:
        /* physical FB not mapped, shadow FB mapped into console, update
         * dirty regions in L4 console */
	pSlim->update = PSLIMMapShadowUpdate;
        break;
    }

  if (pSlim->update)
    {
      if (!shadowSetup (pScreen))
        return FALSE;
      pSlim->CreateScreenResources   = pScreen->CreateScreenResources;
      pScreen->CreateScreenResources = PSLIMCreateScreenResources;
    } 

  xf86SetBlackWhitePixels (pScreen);
  miInitializeBackingStore (pScreen);
  xf86SetBackingStore (pScreen);

  /* software cursor */
  miDCInitialize (pScreen, xf86GetPointerScreenFuncs ());

  /* colormap */
  if (!miCreateDefColormap (pScreen))
    return FALSE;

  if (!xf86HandleColormaps (pScreen, 256, 6,
			    PSLIMLoadPalette, NULL, 0))
    return FALSE;

  pSlim->CloseScreen = pScreen->CloseScreen;
  pScreen->CloseScreen = PSLIMCloseScreen;
  pScreen->SaveScreen = PSLIMSaveScreen;

  /* tell the L4Linux stub that we have entered the X mode */
  ioctl(pSlim->dropscon_dev, (L4IOTYPE << 8) | 4, NULL);
  
  return TRUE;
}

static Bool
PSLIMEnterVT (int scrnIndex, int flags)
{
  ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
  PSLIMPtr pSlim = PSLIMGetRec (pScrn);

//  xf86DrvMsg (scrnIndex, X_CONFIG, "%s\n", __FUNCTION__);

  /* tell the L4Linux stub that we are entering the X mode */
  ioctl(pSlim->dropscon_dev, (L4IOTYPE << 8) | 4, NULL);
  
  return TRUE;
}

static void
PSLIMLeaveVT (int scrnIndex, int flags)
{
  ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
  PSLIMPtr pSlim = PSLIMGetRec (pScrn);
  
//  xf86DrvMsg (scrnIndex, X_CONFIG, "%s\n", __FUNCTION__);

  /* tell the L4Linux stub that we are leaving the X mode */
  ioctl(pSlim->dropscon_dev, (L4IOTYPE << 8) | 3, NULL);
}

/* A close screen function is called from the DIX layer for each
 * screen at the end of each server generation. */                         
static Bool
PSLIMCloseScreen (int scrnIndex, ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
  PSLIMPtr pSlim = PSLIMGetRec (pScrn);

  pScrn->vtSema = FALSE;

  if (pSlim->AccelInfoRec)
    {
      xfree (pSlim->AccelInfoRec);
      pSlim->AccelInfoRec = NULL;
    }
  if (pSlim->mapShadow && !l4dm_is_invalid_ds(pSlim->mapShadowDs))
    {
      if (pSlim->shareDS)
        /* dataspace used together with L4Linux, only unmap */
        l4rm_detach(pSlim->fbVirtPtr);
      else
        /* dataspace created by ourself, destroy (and unmap) */
        l4dm_close(&pSlim->mapShadowDs);
      pSlim->mapShadowDs = L4DM_INVALID_DATASPACE;
      pSlim->fbVirtPtr = NULL;
    }
  if (pSlim->fbMapped)
    PSLIMUnmapFB (pScrn);
  if (pSlim->shadowFB && pSlim->fbVirtPtr)
    {
      xfree (pSlim->fbVirtPtr);
      pSlim->fbVirtPtr = NULL;
    }
  
  pScreen->CreateScreenResources = pSlim->CreateScreenResources;
  pScreen->CloseScreen = pSlim->CloseScreen;
  return pScreen->CloseScreen (scrnIndex, pScreen);
}

static Bool
PSLIMSwitchMode (int scrnIndex, DisplayModePtr pMode, int flags)
{
  xf86DrvMsg (scrnIndex, X_CONFIG, "%s\n", __FUNCTION__);
  return TRUE;
}

/* set beginning of frame buffer */
static void
PSLIMAdjustFrame (int scrnIndex, int x, int y, int flags)
{
  xf86DrvMsg (scrnIndex, X_CONFIG, "%s\n", __FUNCTION__);
}

static void
PSLIMFreeScreen (int scrnIndex, int flags)
{
  ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
  PSLIMPtr pSlim = PSLIMGetRec (pScrn);
  
  xf86DrvMsg (scrnIndex, X_CONFIG, "%s\n", __FUNCTION__);
  PSLIMFreeRec (xf86Screens[scrnIndex]);
  close(pSlim->dropscon_dev);
  
  if (pSlim->shadowFB && pSlim->fbVirtPtr)
    {
      xfree (pSlim->fbVirtPtr);
      pSlim->fbVirtPtr = NULL;
    }
  if (pSlim->mapShadow && !l4dm_is_invalid_ds(pSlim->mapShadowDs))
    {
      l4dm_close(&pSlim->mapShadowDs);
      pSlim->mapShadowDs = L4DM_INVALID_DATASPACE;
    }
  if (pSlim->fbMapped)
    PSLIMUnmapFB (pScrn);
}

/* X wrote to the shadow framebuffer. We need to send the updated region
 * to the L4 console using L4 long IPC. */
static void
PSLIMIpcUpdate(ScreenPtr pScreen, shadowBufPtr pBuf)
{
  FbBits *shaBase;
  FbStride shaStride;
  int shaBpp;
#ifdef XORG71
  RegionPtr pRegion = DamageRegion(pBuf->pDamage);
#else
  RegionPtr pRegion = &pBuf->damage;
#endif
  int nboxes = REGION_NUM_RECTS (pRegion);
  BoxPtr pbox = REGION_RECTS (pRegion);
  int shaXoff, shaYoff;
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  PSLIMPtr pSlim = PSLIMGetRec (pScrn);
  int bytes_per_pixel;

  fbGetDrawable (&pBuf->pPixmap->drawable, shaBase, shaStride, shaBpp,
		 shaXoff, shaYoff);

  bytes_per_pixel = shaBpp / 8;
  
  while (nboxes--)
    {
      l4con_pslim_rect_t rect;
      unsigned stride_width;
      unsigned h;
      l4_uint8_t *vfbofs;

      rect.x = pbox->x1;
      rect.y = pbox->y1;
      rect.w = pbox->x2 - pbox->x1;
      rect.h = 1;
      h      = pbox->y2 - pbox->y1;
      vfbofs = (l4_uint8_t*)shaBase 
             + rect.y * pSlim->bytesPerScanline
             + rect.x * bytes_per_pixel;

      stride_width = rect.w * bytes_per_pixel;

      while (h--)
	{
	  CORBA_Environment env = dice_default_environment;
	 
	  if (con_vc_pslim_set_call(&pSlim->vc_tid, &rect,
                                     vfbofs, stride_width, &env)
	      || DICE_HAS_EXCEPTION(&env))
	    {
	      xf86DrvMsg (pScrn->scrnIndex, X_ERROR,
			 "Error updating region at console (exc=%d, %08x)\n",
			 DICE_EXCEPTION_MAJOR(&env), DICE_IPC_ERROR(&env));
	      return;
	    }
          vfbofs += pSlim->bytesPerScanline;
	  rect.y++;
	}
      pbox++;
    }
}

/* X wrote to the shadow framebuffer. We need to tell the L4 console which
 * region to copy from the mapped shadow framebuffer to the physical frame-
 * buffer. */
static void 
PSLIMMapShadowUpdate(ScreenPtr pScreen, shadowBufPtr pBuf)
{
#ifdef XORG71
  RegionPtr pRegion = DamageRegion(pBuf->pDamage);
#else
  RegionPtr pRegion = &pBuf->damage;
#endif
  int nboxes = REGION_NUM_RECTS (pRegion);
  BoxPtr pbox = REGION_RECTS (pRegion);
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  PSLIMPtr pSlim = PSLIMGetRec (pScrn);

  while (nboxes--)
    {
      l4con_pslim_rect_t rect;
      CORBA_Environment env = dice_default_environment;

      rect.x = pbox->x1;
      rect.y = pbox->y1;
      rect.w = pbox->x2 - pbox->x1;
      rect.h = pbox->y2 - pbox->y1;
      
      /* try to send more than one line */
      if (con_vc_direct_update_call(&pSlim->vc_tid, &rect, &env)
	  || DICE_HAS_EXCEPTION(&env))
	{
	  xf86DrvMsg (pScrn->scrnIndex, X_ERROR,
	 	     "Error updating region at console (exc=%d, %08x)\n", 
		     DICE_EXCEPTION_MAJOR(&env), DICE_IPC_ERROR(&env));
	  return;
	}
      pbox++;
    }
}

/* X wrote to the shadow framebuffer. Therefore we need to update the mapped
 * physical framebuffer. If the framebuffer is not ``really'' physical (e.g.
 * VMware framebuffer), we need to tell the graphics device where the content
 * has changed (post dirty notification) */
static void
PSLIMMapPhysUpdate(ScreenPtr pScreen, shadowBufPtr pBuf)
{
  FbBits *shaBase;
  FbStride shaStride;
#ifdef XORG71
  RegionPtr pRegion = DamageRegion(pBuf->pDamage);
#else
  RegionPtr pRegion = &pBuf->damage;
#endif
  int shaBpp, nboxes = REGION_NUM_RECTS (pRegion);
  BoxPtr pbox = REGION_RECTS (pRegion);
  int shaXoff, shaYoff;
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  PSLIMPtr pSlim = PSLIMGetRec (pScrn);
  int bytes_per_pixel;

  if (!pSlim->fbMapped)
      return;

  fbGetDrawable (&pBuf->pPixmap->drawable, shaBase, shaStride, shaBpp,
		 shaXoff, shaYoff);

  bytes_per_pixel = shaBpp / 8;

  while (nboxes--)
    {
      l4con_pslim_rect_t rect;
      unsigned h, stride_width;
      l4_uint8_t *vfbofs, *pfbofs;
      CORBA_Environment env = dice_default_environment;

      rect.x = pbox->x1;
      rect.y = pbox->y1;
      rect.w = pbox->x2 - pbox->x1;
      rect.h = pbox->y2 - pbox->y1;
      h      = pbox->y2 - pbox->y1;
      vfbofs = (l4_uint8_t*)shaBase
             +              rect.y * pSlim->bytesPerScanline
             +              rect.x * bytes_per_pixel;
      pfbofs = (l4_uint8_t*)pSlim->fbPhysPtr
             +              rect.y * pSlim->bytesPerScanline
             +              rect.x * bytes_per_pixel;

      stride_width = rect.w * bytes_per_pixel;

      while (h--)
        {
          if (pfbofs              < pSlim->fbPhysPtr ||
              pfbofs+stride_width > pSlim->fbPhysBase+pSlim->fbPhysSize)
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                    "pfbofs = %08x, stride = %08x\n", (unsigned)pfbofs, stride_width);
          if (vfbofs              <  pSlim->fbVirtPtr || 
              vfbofs+stride_width > pSlim->fbVirtPtr+pSlim->fbVirtSize)
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                    "vfbofs = %08x, stride = %08x\n", (unsigned)vfbofs, stride_width);
          memcpy(pfbofs, vfbofs, stride_width);
          vfbofs += pSlim->bytesPerScanline;
          pfbofs += pSlim->bytesPerScanline;
        }

      if ((pSlim->accel_flags & L4CON_POST_DIRTY))
        {
          /* post dirty necessary (e.g. VMware) */
          if (con_vc_direct_update_call(&pSlim->vc_tid, &rect, &env)
              || DICE_HAS_EXCEPTION(&env))
            {
              xf86DrvMsg (pScrn->scrnIndex, X_ERROR,
	 	          "Error updating region at console (exc=%d, %08x)\n", 
                          DICE_EXCEPTION_MAJOR(&env), DICE_IPC_ERROR(&env));
              return;
            }
        }
      pbox++;
    }
}

/* X wrote directly to the framebuffer. If the framebuffer is not ``really''
 * physical (e.g. VMware framebuffer), we need to tell the graphics device
 * where the content has changed */
static void
PSLIMPostDirtyUpdate(ScrnInfoPtr pScrn, int nboxes, BoxPtr boxPtr)
{
  PSLIMPtr pSlim = PSLIMGetRec (pScrn);

  if (!(pSlim->accel_flags & L4CON_POST_DIRTY))
    /* post dirty not necessary since the fb uses graphics memory */
    return;

  while (nboxes--)
    {
      l4con_pslim_rect_t rect;
      CORBA_Environment env = dice_default_environment;

      rect.x = boxPtr->x1;
      rect.y = boxPtr->y1;
      rect.w = boxPtr->x2 - boxPtr->x1;
      rect.h = boxPtr->y2 - boxPtr->y1;

      if (con_vc_direct_update_call(&pSlim->vc_tid, &rect, &env)
	  || DICE_HAS_EXCEPTION(&env))
	{
	  xf86DrvMsg (pScrn->scrnIndex, X_ERROR,
	 	     "Error updating region at console (exc=%d, %08x)\n", 
		     DICE_EXCEPTION_MAJOR(&env), DICE_IPC_ERROR(&env));
	  return;
	}
      boxPtr++;
    }
}

static void
PSLIMLoadPalette (ScrnInfoPtr pScrn, int numColors, int *indices,
		 LOCO * colors, VisualPtr pVisual)
{
  /* nothing to do */
}

static Bool
PSLIMSaveScreen(ScreenPtr pScreen, int mode)
{
  Bool on = xf86IsUnblank(mode);

  if (on)
    SetTimeSinceLastInputEvent();

  return TRUE;
}

static void
PSLIMXaaSetupCopy(ScrnInfoPtr pScrn, int xdir, int ydir, int rop,  
	   unsigned planemask, int transparency_color)
{
}

static void
PSLIMXaaCopy(ScrnInfoPtr pScrn,
	     int x1, int y1, int x2, int y2, int w, int h)
{
  PSLIMPtr pSlim = PSLIMGetRec (pScrn);
  l4con_pslim_rect_t rect = { x1, y1, w, h };
  CORBA_Environment env = dice_default_environment;

  if (!w || !h)
    return;

  if (con_vc_pslim_copy_call(&pSlim->vc_tid, &rect, x2, y2, &env)
      || DICE_HAS_EXCEPTION(&env))
    xf86DrvMsg (pScrn->scrnIndex, X_ERROR,
		"Error copying region at console (exc=%d, %08x)\n", 
		DICE_EXCEPTION_MAJOR(&env), DICE_IPC_ERROR(&env));
}

static void
PSLIMXaaSetupFill(ScrnInfoPtr pScrn, int color, int rop, unsigned planemask)
{
  PSLIMPtr pSlim = PSLIMGetRec (pScrn);

  /* set highest bit of color to inhibit color converting in console */
  pSlim->SavedFgColor = color | 0x80000000;
}

static void
PSLIMXaaFill(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
  PSLIMPtr pSlim = PSLIMGetRec (pScrn);
  l4con_pslim_rect_t rect = { x, y, w, h };
  CORBA_Environment env = dice_default_environment;
  
  if (!w || !h)
    return;

  if (con_vc_pslim_fill_call(&pSlim->vc_tid, &rect,
			(l4con_pslim_color_t)pSlim->SavedFgColor, &env)
      || DICE_HAS_EXCEPTION(&env))
    xf86DrvMsg (pScrn->scrnIndex, X_ERROR,
		"Error filling region at console (exc=%d, %08x)\n", 
		DICE_EXCEPTION_MAJOR(&env), DICE_IPC_ERROR(&env));
}

static void
PSLIMXaaSync(ScrnInfoPtr pScrn)
{
  /* sync is not needed since it is done by the console */
}

static void
PSLIMInitAccel(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  PSLIMPtr pSlim = PSLIMGetRec (pScrn);
  XAAInfoRecPtr xaaptr;
#ifdef ALLOW_OFFSCREEN_PIXEMAP
  BoxRec AvailFBArea;
#endif

  if (pSlim->accel_flags & L4CON_FAST_COPY)
    {
      if (!(xaaptr = pSlim->AccelInfoRec = XAACreateInfoRec()))
	return;

      /* general flags */
      xaaptr->Flags                        = LINEAR_FRAMEBUFFER
#ifdef ALLOW_OFFSCREEN_PIXMAPS
					   | OFFSCREEN_PIXMAPS
					   | PIXMAP_CACHE
#endif
					   ;

      /* fast copy function */
      xaaptr->SetupForScreenToScreenCopy   = PSLIMXaaSetupCopy;
      xaaptr->SubsequentScreenToScreenCopy = PSLIMXaaCopy;
      xaaptr->ScreenToScreenCopyFlags      = NO_TRANSPARENCY
					   | NO_PLANEMASK
					   | GXCOPY_ONLY
					   ;

      /* fast fill function */
      xaaptr->SetupForSolidFill            = PSLIMXaaSetupFill;
      xaaptr->SubsequentSolidFillRect      = PSLIMXaaFill;
      xaaptr->SolidFillFlags               = NO_PLANEMASK ;
  
      /* wait till graphics engine is idle before direct access to fb */
      xaaptr->Sync                         = PSLIMXaaSync;

#ifdef ALLOW_OFFSCREEN_PIXMAPS
      /* Finally, we set up the video memory space available to the pixmap
       * cache. In this case, all memory from the end of the virtual screen
       * to the end of the command overflow buffer can be used. If you haven't
       * enabled the PIXMAP_CACHE flag, then these lines can be omitted. */
      AvailFBArea.x1 = 0;
      AvailFBArea.y1 = 0;
      AvailFBArea.x2 = pScrn->virtualX; /* XXX */
      AvailFBArea.y2 = (4 << 20) / pSlim->bytesPerScanline;
      xf86InitFBManager(pScreen, &AvailFBArea);
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "Using %d lines for offscreen memory.\n",
		 AvailFBArea.y2 - pScrn->virtualY);
#endif
      
      XAAInit(pScreen, xaaptr);
    }
}

/* install SIGIO handler */
static void
PSLIMSIGIOHandler (int fd, void *closure)
{
  int error;
  int arg;
  ScrnInfoPtr pScrn = (ScrnInfoPtr) closure;
  PSLIMPtr pSlim = PSLIMGetRec (pScrn);

  /* get additional parameter from Linux stub (XXX possible race) */
  if ((error = ioctl(pSlim->dropscon_dev, (L4IOTYPE << 8) | 5, &arg)) < 0)
    {
      xf86DrvMsg (0, X_ERROR, "Error %d ioctl 5 on dropscon device\n", error);
      return;
    }

//  xf86DrvMsg (0, X_INFO, "PSLIMSIGIOHandler => %d\n", arg);

  switch (arg)
    {
    case 1:
      if (pSlim->mapPhys)
        {
          switch (PSLIMMapFB (pScrn))
            {
              case -1: /* fatal */
              case  0: pScrn->vtSema = FALSE; break;
              case  1: pScrn->vtSema = TRUE;  break;
            }
        }
      else
        pScrn->vtSema = TRUE;
      break;
    case 2:
      pScrn->vtSema = FALSE;
      if (pSlim->mapPhys)
	PSLIMUnmapFB (pScrn);
      break;
    }
}

static Bool
PSLIMInitSIGIOHandler(ScrnInfoPtr pScrn)
{
  PSLIMPtr pSlim = PSLIMGetRec (pScrn);

  if (!xf86InstallSIGIOHandler (pSlim->dropscon_dev,
				PSLIMSIGIOHandler, (void*)pScrn))
    {
      xf86DrvMsg(0, X_CONFIG, "Error connecting SIGIO\n");
      return FALSE;
    }

  return TRUE;
}
