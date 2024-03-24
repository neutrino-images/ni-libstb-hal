#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>
#include <string.h>

#include "hal_debug.h"

#define hal_debug(args...) _hal_debug(HAL_DEBUG_PLAYER, NULL, args)
#define hal_info(args...) _hal_info(HAL_DEBUG_PLAYER, NULL, args)

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define log_error(fmt, x...) do { hal_info("[%s:%s] " fmt, __FILENAME__, __FUNCTION__, ## x); } while (0)
#define log_printf(maxlevel, level, fmt, x...) do { if (maxlevel >= level) hal_info("[%s:%s] " fmt, __FILENAME__, __FUNCTION__, ## x); } while (0)
#define log_debug(maxlevel, level, fmt, x...) do { if (maxlevel >= level) hal_debug("[%s:%s] " fmt, __FILENAME__, __FUNCTION__, ## x); } while (0)

#define FFMPEG_DEBUG_ALL 10

/*******************************************
 * ffmpeg
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define FFMPEG_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define FFMPEG_DEBUG_LEVEL 0
#endif
#define FFMPEG_SILENT

#if FFMPEG_DEBUG_LEVEL
#define ffmpeg_printf(...) log_debug(FFMPEG_DEBUG_LEVEL, __VA_ARGS__)
#else
#define ffmpeg_printf(...)
#endif

#ifndef FFMPEG_SILENT
#define ffmpeg_err(...) log_error(__VA_ARGS__)
#else
#define ffmpeg_err(...)
#endif

/*******************************************
 * container
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define CONTAINER_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define CONTAINER_DEBUG_LEVEL 0
#endif
#define CONTAINER_SILENT

#if CONTAINER_DEBUG_LEVEL
#define container_printf(...) log_debug(CONTAINER_DEBUG_LEVEL, __VA_ARGS__)
#else
#define container_printf(...)
#endif

#ifndef CONTAINER_SILENT
#define container_err(...) log_error(__VA_ARGS__)
#else
#define container_err(...)
#endif

/*******************************************
 * latmenc
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define LATMENC_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define LATMENC_DEBUG_LEVEL 0
#endif
#define LATMENC_SILENT

#if LATMENC_DEBUG_LEVEL
#define latmenc_printf(...) log_debug(LATMENC_DEBUG_LEVEL, __VA_ARGS__)
#else
#define latmenc_printf(...)
#endif

#ifndef LATMENC_SILENT
#define latmenc_err(...) log_error(__VA_ARGS__)
#else
#define latmenc_err(...)
#endif

/*******************************************
 * audio_mgr
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define AUDIO_MGR_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define AUDIO_MGR_DEBUG_LEVEL 0
#endif
#define AUDIO_MGR_SILENT

#if AUDIO_MGR_DEBUG_LEVEL
#define audio_mgr_printf(...) log_debug(AUDIO_MGR_DEBUG_LEVEL, __VA_ARGS__)
#else
#define audio_mgr_printf(...)
#endif

#ifndef AUDIO_MGR_SILENT
#define audio_mgr_err(...) log_error(__VA_ARGS__)
#else
#define audio_mgr_err(...)
#endif

/*******************************************
 * chapter_mgr
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define CHAPTER_MGR_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define CHAPTER_MGR_DEBUG_LEVEL 0
#endif
#define CHAPTER_MGR_SILENT

#if CHAPTER_MGR_DEBUG_LEVEL
#define chapter_mgr_printf(...) log_debug(CHAPTER_MGR_DEBUG_LEVEL, __VA_ARGS__)
#else
#define chapter_mgr_printf(...)
#endif

#ifndef CHAPTER_MGR_SILENT
#define chapter_mgr_err(...) log_error(__VA_ARGS__)
#else
#define chapter_mgr_err(...)
#endif

/*******************************************
 * subtitle_mgr
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define SUBTITLE_MGR_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define SUBTITLE_MGR_DEBUG_LEVEL 0
#endif
#define SUBTITLE_MGR_SILENT

#if SUBTITLE_MGR_DEBUG_LEVEL
#define subtitle_mgr_printf(...) log_debug(SUBTITLE_MGR_DEBUG_LEVEL, __VA_ARGS__)
#else
#define subtitle_mgr_printf(...)
#endif

#ifndef SUBTITLE_MGR_SILENT
#define subtitle_mgr_err(...) log_error(__VA_ARGS__)
#else
#define subtitle_mgr_err(...)
#endif

/*******************************************
 * video_mgr
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define VIDEO_MGR_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define VIDEO_MGR_DEBUG_LEVEL 0
#endif
#define VIDEO_MGR_SILENT

#if VIDEO_MGR_DEBUG_LEVEL
#define video_mgr_printf(...) log_debug(VIDEO_MGR_DEBUG_LEVEL, __VA_ARGS__)
#else
#define video_mgr_printf(...)
#endif

#ifndef VIDEO_MGR_SILENT
#define video_mgr_err(...) log_error(__VA_ARGS__)
#else
#define video_mgr_err(...)
#endif

/*******************************************
 * linuxdvb
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define LINUXDVB_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define LINUXDVB_DEBUG_LEVEL 0
#endif
#define LINUXDVB_SILENT

#if LINUXDVB_DEBUG_LEVEL
#define linuxdvb_printf(...) log_debug(LINUXDVB_DEBUG_LEVEL, __VA_ARGS__)
#else
#define linuxdvb_printf(...)
#endif

#ifndef LINUXDVB_SILENT
#define linuxdvb_err(...) log_error(__VA_ARGS__)
#else
#define linuxdvb_err(...)
#endif

/*******************************************
 * buff
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define BUFF_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define BUFF_DEBUG_LEVEL 0
#endif
#define BUFF_SILENT

#if BUFF_DEBUG_LEVEL
#define buff_printf(...) log_debug(BUFF_DEBUG_LEVEL, __VA_ARGS__)
#else
#define buff_printf(...)
#endif

#ifndef BUFF_SILENT
#define buff_err(...) log_error(__VA_ARGS__)
#else
#define buff_err(...)
#endif

/*******************************************
 * output
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define OUTPUT_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define OUTPUT_DEBUG_LEVEL 0
#endif
#define OUTPUT_SILENT

#if OUTPUT_DEBUG_LEVEL
#define output_printf(...) log_debug(OUTPUT_DEBUG_LEVEL, __VA_ARGS__)
#else
#define output_printf(...)
#endif

#ifndef OUTPUT_SILENT
#define output_err(...) log_error(__VA_ARGS__)
#else
#define output_err(...)
#endif

/*******************************************
 * subtitle
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define SUBTITLE_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define SUBTITLE_DEBUG_LEVEL 0
#endif
#define SUBTITLE_SILENT

#if SUBTITLE_DEBUG_LEVEL
#define subtitle_printf(...) log_debug(SUBTITLE_DEBUG_LEVEL, __VA_ARGS__)
#else
#define subtitle_printf(...)
#endif

#ifndef SUBTITLE_SILENT
#define subtitle_err(...) log_error(__VA_ARGS__)
#else
#define subtitle_err(...)
#endif

/*******************************************
 * writer
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define WRITER_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define WRITER_DEBUG_LEVEL 0
#endif
#define WRITER_SILENT

#if WRITER_DEBUG_LEVEL
#define writer_printf(...) log_debug(WRITER_DEBUG_LEVEL, __VA_ARGS__)
#else
#define writer_printf(...)
#endif

#ifndef WRITER_SILENT
#define writer_err(...) log_error(__VA_ARGS__)
#else
#define writer_err(...)
#endif

/*******************************************
 * playback
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define PLAYBACK_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define PLAYBACK_DEBUG_LEVEL 0
#endif
#define PLAYBACK_SILENT

#if PLAYBACK_DEBUG_LEVEL
#define playback_printf(...) log_debug(PLAYBACK_DEBUG_LEVEL, __VA_ARGS__)
#else
#define playback_printf(...)
#endif

#ifndef PLAYBACK_SILENT
#define playback_err(...) log_error(__VA_ARGS__)
#else
#define playback_err(...)
#endif

/*******************************************
 * aac
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define AAC_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define AAC_DEBUG_LEVEL 0
#endif
#define AAC_SILENT

#if AAC_DEBUG_LEVEL
#define aac_printf(...) log_debug(AAC_DEBUG_LEVEL, __VA_ARGS__)
#else
#define aac_printf(...)
#endif

#ifndef AAC_SILENT
#define aac_err(...) log_error(__VA_ARGS__)
#else
#define aac_err(...)
#endif

/*******************************************
 * ac3
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define AC3_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define AC3_DEBUG_LEVEL 0
#endif
#define AC3_SILENT

#if AC3_DEBUG_LEVEL
#define ac3_printf(...) log_debug(AC3_DEBUG_LEVEL, __VA_ARGS__)
#else
#define ac3_printf(...)
#endif

#ifndef AC3_SILENT
#define ac3_err(...) log_error(__VA_ARGS__)
#else
#define ac3_err(...)
#endif

/*******************************************
 * amr
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define AMR_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define AMR_DEBUG_LEVEL 0
#endif
#define AMR_SILENT

#if AMR_DEBUG_LEVEL
#define amr_printf(...) log_debug(AMR_DEBUG_LEVEL, __VA_ARGS__)
#else
#define amr_printf(...)
#endif

#ifndef AMR_SILENT
#define amr_err(...) log_error(__VA_ARGS__)
#else
#define amr_err(...)
#endif

/*******************************************
 * divx
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define DIVX_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define DIVX_DEBUG_LEVEL 0
#endif
#define DIVX_SILENT

#if DIVX_DEBUG_LEVEL
#define divx_printf(...) log_debug(DIVX_DEBUG_LEVEL, __VA_ARGS__)
#else
#define divx_printf(...)
#endif

#ifndef DIVX_SILENT
#define divx_err(...) log_error(__VA_ARGS__)
#else
#define divx_err(...)
#endif

/*******************************************
 * dts
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define DTS_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define DTS_DEBUG_LEVEL 0
#endif
#define DTS_SILENT

#if DTS_DEBUG_LEVEL
#define dts_printf(...) log_debug(DTS_DEBUG_LEVEL, __VA_ARGS__)
#else
#define dts_printf(...)
#endif

#ifndef DTS_SILENT
#define dts_err(...) log_error(__VA_ARGS__)
#else
#define dts_err(...)
#endif

/*******************************************
 * h263
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define H263_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define H263_DEBUG_LEVEL 0
#endif
#define H263_SILENT

#if H263_DEBUG_LEVEL
#define h263_printf(...) log_debug(H263_DEBUG_LEVEL, __VA_ARGS__)
#else
#define h263_printf(...)
#endif

#ifndef H263_SILENT
#define h263_err(...) log_error(__VA_ARGS__)
#else
#define h263_err(...)
#endif

/*******************************************
 * h264
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define H264_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define H264_DEBUG_LEVEL 0
#endif
#define H264_SILENT

#if H264_DEBUG_LEVEL
#define h264_printf(...) log_debug(H264_DEBUG_LEVEL, __VA_ARGS__)
#else
#define h264_printf(...)
#endif

#ifndef H264_SILENT
#define h264_err(...) log_error(__VA_ARGS__)
#else
#define h264_err(...)
#endif

/*******************************************
 * h265
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define H265_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define H265_DEBUG_LEVEL 0
#endif
#define H265_SILENT

#if H265_DEBUG_LEVEL
#define h265_printf(...) log_debug(H265_DEBUG_LEVEL, __VA_ARGS__)
#else
#define h265_printf(...)
#endif

#ifndef H265_SILENT
#define h265_err(...) log_error(__VA_ARGS__)
#else
#define h265_err(...)
#endif

/*******************************************
 * lpcm
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define LPCM_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define LPCM_DEBUG_LEVEL 0
#endif
#define LPCM_SILENT

#if LPCM_DEBUG_LEVEL
#define lpcm_printf(...) log_debug(LPCM_DEBUG_LEVEL, __VA_ARGS__)
#else
#define lpcm_printf(...)
#endif

#ifndef LPCM_SILENT
#define lpcm_err(...) log_error(__VA_ARGS__)
#else
#define lpcm_err(...)
#endif

/*******************************************
 * mp3
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define MP3_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define MP3_DEBUG_LEVEL 0
#endif
#define MP3_SILENT

#if MP3_DEBUG_LEVEL
#define mp3_printf(...) log_debug(MP3_DEBUG_LEVEL, __VA_ARGS__)
#else
#define mp3_printf(...)
#endif

#ifndef MP3_SILENT
#define mp3_err(...) log_error(__VA_ARGS__)
#else
#define mp3_err(...)
#endif

/*******************************************
 * mpeg2
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define MPEG2_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define MPEG2_DEBUG_LEVEL 0
#endif
#define MPEG2_SILENT

#if MPEG2_DEBUG_LEVEL
#define mpeg2_printf(...) log_debug(MPEG2_DEBUG_LEVEL, __VA_ARGS__)
#else
#define mpeg2_printf(...)
#endif

#ifndef MPEG2_SILENT
#define mpeg2_err(...) log_error(__VA_ARGS__)
#else
#define mpeg2_err(...)
#endif

/*******************************************
 * mpeg4
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define MPEG4_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define MPEG4_DEBUG_LEVEL 0
#endif
#define MPEG4_SILENT

#if MPEG4_DEBUG_LEVEL
#define mpeg4_printf(...) log_debug(MPEG4_DEBUG_LEVEL, __VA_ARGS__)
#else
#define mpeg4_printf(...)
#endif

#ifndef MPEG4_SILENT
#define mpeg4_err(...) log_error(__VA_ARGS__)
#else
#define mpeg4_err(...)
#endif

/*******************************************
 * pcm
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define PCM_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define PCM_DEBUG_LEVEL 0
#endif
#define PCM_SILENT

#if PCM_DEBUG_LEVEL
#define pcm_printf(...) log_debug(PCM_DEBUG_LEVEL, __VA_ARGS__)
#else
#define pcm_printf(...)
#endif

#ifndef PCM_SILENT
#define pcm_err(...) log_error(__VA_ARGS__)
#else
#define pcm_err(...)
#endif

/*******************************************
 * vc1
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define VC1_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define VC1_DEBUG_LEVEL 0
#endif
#define VC1_SILENT

#if VC1_DEBUG_LEVEL
#define vc1_printf(...) log_debug(VC1_DEBUG_LEVEL, __VA_ARGS__)
#else
#define vc1_printf(...)
#endif

#ifndef VC1_SILENT
#define vc1_err(...) log_error(__VA_ARGS__)
#else
#define vc1_err(...)
#endif

/*******************************************
 * vp
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define VP_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define VP_DEBUG_LEVEL 0
#endif
#define VP_SILENT

#if VP_DEBUG_LEVEL
#define vp_printf(...) log_debug(VP_DEBUG_LEVEL, __VA_ARGS__)
#else
#define vp_printf(...)
#endif

#ifndef VP_SILENT
#define vp_err(...) log_error(__VA_ARGS__)
#else
#define vp_err(...)
#endif

/*******************************************
 * wma
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define WMA_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define WMA_DEBUG_LEVEL 0
#endif
#define WMA_SILENT

#if WMA_DEBUG_LEVEL
#define wma_printf(...) log_debug(WMA_DEBUG_LEVEL, __VA_ARGS__)
#else
#define wma_printf(...)
#endif

#ifndef WMA_SILENT
#define wma_err(...) log_error(__VA_ARGS__)
#else
#define wma_err(...)
#endif

/*******************************************
 * wmv
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define WMV_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define WMV_DEBUG_LEVEL 0
#endif
#define WMV_SILENT

#if WMV_DEBUG_LEVEL
#define wmv_printf(...) log_debug(WMV_DEBUG_LEVEL, __VA_ARGS__)
#else
#define wmv_printf(...)
#endif

#ifndef WMV_SILENT
#define wmv_err(...) log_error(__VA_ARGS__)
#else
#define wmv_err(...)
#endif

/*******************************************
 * mjpeg
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define MJPEG_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define MJPEG_DEBUG_LEVEL 0
#endif
#define MJPEG_SILENT

#if MJPEG_DEBUG_LEVEL
#define mjpeg_printf(...) log_debug(MJPEG_DEBUG_LEVEL, __VA_ARGS__)
#else
#define mjpeg_printf(...)
#endif

#ifndef MJPEG_SILENT
#define mjpeg_err(...) log_error(__VA_ARGS__)
#else
#define mjpeg_err(...)
#endif

/*******************************************
 * bcma
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define BCMA_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define BCMA_DEBUG_LEVEL 0
#endif
#define BCMA_SILENT

#if BCMA_DEBUG_LEVEL
#define bcma_printf(...) log_debug(BCMA_DEBUG_LEVEL, __VA_ARGS__)
#else
#define bcma_printf(...)
#endif

#ifndef BCMA_SILENT
#define bcma_err(...) log_error(__VA_ARGS__)
#else
#define bcma_err(...)
#endif

/*******************************************
 * plugin
 *******************************************/
#if FFMPEG_DEBUG_ALL > 0
#define PLUGIN_DEBUG_LEVEL FFMPEG_DEBUG_ALL
#else
#define PLUGIN_DEBUG_LEVEL 0
#endif
#define PLUGIN_SILENT

#if PLUGIN_DEBUG_LEVEL
#define plugin_printf(...) log_debug(PLUGIN_DEBUG_LEVEL, __VA_ARGS__)
#else
#define plugin_printf(...)
#endif

#ifndef PLUGIN_SILENT
#define plugin_err(...) log_error(__VA_ARGS__)
#else
#define plugin_err(...)
#endif

#endif
