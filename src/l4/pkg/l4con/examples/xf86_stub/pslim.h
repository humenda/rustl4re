/*!
 * \file	con/examples/xf86_stub/pslim.h
 * \brief
 *
 * \date	01/2002
 * \author	Frank Mehnert <fm3@os.inf.tu-dresden.de>
 */

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

#ifndef __CON_EXAMPLES_XF86_STUB_PSLIM_H_
#define __CON_EXAMPLES_XF86_STUB_PSLIM_H_

#include <l4/sys/types.h>
#include <l4/dm_generic/types.h>

/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"

#ifndef XORG73
/* All drivers need this */
#include "xf86_ansic.h"
#else
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#endif

#include "compiler.h"

/* ShadowFB support */
#include "shadow.h"

#include "fb.h"
#include "afb.h"
#include "mfb.h"
#ifdef L4CON_USE_CFB24_32
#include "cfb24_32.h"
#endif

#include "xaa.h"

#define PSLIM_VERSION		4000
#define PSLIM_NAME		"PSLIM"
#define PSLIM_DRIVER_NAME	"pslim"
#define PSLIM_MAJOR_VERSION	1
#define PSLIM_MINOR_VERSION	1
#define PSLIM_PATCHLEVEL	0

typedef struct _PSLIMRec
{
  EntityInfoPtr pEnt;
  GDevPtr device;
  CARD16 bytesPerScanline;
  int pix24bpp;
  CARD8 *fbVirtPtr;
  CARD32 fbVirtSize;
  CARD8 *fbPhysPtr;
  CARD8 *fbPhysBase;
  CARD32 fbPhysSize;
  CARD32 fbOffs;
  CloseScreenProcPtr CloseScreen;
  CreateScreenResourcesProcPtr CreateScreenResources;
  XAAInfoRecPtr AccelInfoRec;
  l4_threadid_t vc_tid;
  int dropscon_dev;
  l4_uint32_t accel_flags;
  int SavedFgColor;
  Bool shadowFB;
  Bool mapShadow;
  Bool mapPhys;
  int  Mode;
  Bool fbMapped;
  Bool shareDS;
  OptionInfoPtr Options;
  l4dm_dataspace_t mapShadowDs;
  ShadowUpdateProc update;
} PSLIMRec, *PSLIMPtr;

typedef struct _ModeInfoData 
{
  int mode;
  int memory_model;
} ModeInfoData;


#endif /* _PSLIM_H_ */
