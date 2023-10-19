#ifndef _SST_TDFX_H_
#define _SST_TDFX_H_

#ifndef _CONFIG_H_
# include <config.h>
#endif

#if DYNAMIC_VIS_PIX
int VIS_H_PIX, VIS_V_PIX, AN_VIS_ROW, AN_VIS_COL, SST_RESOLUTION;
#else
# if !defined(SST_RESOLUTION) && GLIDE_VERSION <= 203
#  if VIS_V_PIX == 256
#   define SST_RESOLUTION	GR_RESOLUTION_512x256
#  else
#   define SST_RESOLUTION	GR_RESOLUTION_512x384
#  endif
# endif
#endif

#if !(WHICH_IOASIC&1)
# define IO_DIPSW_INVERT (0)
#else
# define IO_DIPSW_INVERT (0xFFFFFFFF)
#endif

#if DYNAMIC_PIXEL_CLOCK
# define SELECT_PIXEL_CLOCK() \
    do { \
	int sw, ans=0; \
	extern int grPixelClockFreq; \
	sw = (IO_DIPSW_T^IO_DIPSW_INVERT)&(IO_DIPSW11|IO_DIPSW12); \
	if (sw == (IO_DIPSW11|IO_DIPSW12)) ans = 45; \
	else if (sw == IO_DIPSW12) ans = 47; \
	else if (sw == IO_DIPSW11) ans = 49; \
	else ans = 51; \
	grPixelClockFreq = ans;\
    } while (0)
#else
# define SELECT_PIXEL_CLOCK() \
    do { ; } while (0)
#endif

#if GLIDE_VERSION > 203
# undef SST_RESOLUTION
# define SST_RESOLUTION GR_RESOLUTION_640x480
#endif

#ifndef SST_COLOR_FORMAT
# define SST_COLOR_FORMAT	GR_COLORFORMAT_ARGB
#endif

#ifndef SST_REFRESH_RATE
# define SST_REFRESH_RATE	GR_REFRESH_60Hz
#endif

#ifndef SST_ORIGIN
# define SST_ORIGIN		GR_ORIGIN_LOWER_LEFT
#endif

#ifndef SST_SMOOTHING
# define SST_SMOOTHING		GR_SMOOTHING_ENABLE
#endif

#if GLIDE_VERSION > 203
# if !SSTVIDEOTIMINGSTRUCT
typedef struct {
    FxU32 hSyncOn;
    FxU32 hSyncOff;
    FxU32 vSyncOn;
    FxU32 vSyncOff;
    FxU32 hBackPorch;
    FxU32 vBackPorch;
    FxU32 xDimension;
    FxU32 yDimension;
    FxU32 memOffset;
    FxU32 memFifoEntries_1MB;
    FxU32 memFifoEntries_2MB;
    FxU32 memFifoEntries_4MB;
    FxU32 tilesInX_Over2;
    FxU32 vFifoThreshold;
    FxBool video16BPPIsOK;
    FxBool video24BPPIsOK;
    float clkFreq16bpp;
    float clkFreq24bpp;
} sst1VideoTimingStruct;
# endif
extern void grSstVidMode(U32 whichsst, const sst1VideoTimingStruct *timing);
extern const sst1VideoTimingStruct sst_t512x256, sst_t512x384, sst_t640x480;
#endif

#if DYNAMIC_VIS_PIX
# if !defined(DEFAULT_RES_METHOD)
#  if GLIDE_VERSION <= 203
#   define DEFAULT_RES_METHOD() do { \
    SELECT_PIXEL_CLOCK(); \
    VIS_H_PIX = 512; \
    if (!(READ_RAW_SWITCHES(1)&SW_OPT8)) { \
	VIS_V_PIX = 256; \
	SST_RESOLUTION = GR_RESOLUTION_512x256; \
    } else { \
	VIS_V_PIX = 384; \
	SST_RESOLUTION = GR_RESOLUTION_512x384; \
    } \
    AN_VIS_COL = VIS_H_PIX/8; \
    AN_VIS_ROW = VIS_V_PIX/8; \
    } while (0)
#  else
#   define DEFAULT_RES_METHOD() do { \
    SELECT_PIXEL_CLOCK(); \
    VIS_H_PIX = 512; \
    if (!(READ_RAW_SWITCHES(1)&SW_OPT8)) { \
	VIS_V_PIX = 256; \
	grSstVidMode(0, &sst_t512x256); \
    } else { \
	VIS_V_PIX = 384; \
	grSstVidMode(0, &sst_t512x384); \
    } \
    AN_VIS_COL = VIS_H_PIX/8; \
    AN_VIS_ROW = VIS_V_PIX/8; \
    } while (0)
#  endif
# endif
#else
# if !defined(DEFAULT_RES_METHOD)
#  if GLIDE_VERSION > 203
#   if VIS_V_PIX == 256
#    define DEFAULT_RES_METHOD() do { SELECT_PIXEL_CLOCK(); grSstVidMode(0, &sst_t512x256); } while (0)
#   endif
#   if VIS_V_PIX == 384
#    define DEFAULT_RES_METHOD() do { SELECT_PIXEL_CLOCK(); grSstVidMode(0, &sst_t512x384); } while (0)
#   endif
#  endif
#  if !defined(DEFAULT_RES_METHOD)
#   define DEFAULT_RES_METHOD() do { SELECT_PIXEL_CLOCK(); } while(0)
#  endif
# endif
#endif

#endif
