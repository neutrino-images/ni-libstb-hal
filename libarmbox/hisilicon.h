#ifndef _hisilicon_h
#define _hisilicon_h

#define HI_FORMAT_MAX_URL_LEN               (2048)
#define HI_FORMAT_MAX_FILE_NAME_LEN         (512)
#define HI_FORMAT_TITLE_MAX_LEN             (512)
#define HI_FORMAT_LANG_LEN                  (64)
#define HI_FORMAT_MAX_LANGUAGE_NUM          (4)
#define HI_FORMAT_SERVICE_DESCRIPT_LEN      (64)

typedef uint8_t           HI_U8;
typedef uint16_t          HI_U16;
typedef uint32_t            HI_U32;
typedef int8_t             HI_S8;
typedef int16_t                   HI_S16;
typedef int32_t                     HI_S32;
typedef uint64_t      HI_U64;
typedef int64_t               HI_S64;
typedef void HI_VOID;
typedef char HI_CHAR;

typedef enum
{
	HI_FALSE    = 0,
	HI_TRUE     = 1,
} HI_BOOL;

typedef enum hiFORMAT_SUBTITLE_TYPE_E
{
	HI_FORMAT_SUBTITLE_ASS = 0x0,    /**< ASS subtitle */
	HI_FORMAT_SUBTITLE_LRC,       /**< LRC subtitle */
	HI_FORMAT_SUBTITLE_SRT,       /**< SRT subtitle */
	HI_FORMAT_SUBTITLE_SMI,       /**< SMI subtitle */
	HI_FORMAT_SUBTITLE_SUB,       /**< SUB subtitle */
	HI_FORMAT_SUBTITLE_TXT,       /**< RAW UTF8 subtitle */
	HI_FORMAT_SUBTITLE_HDMV_PGS,      /**< pgs subtitle */
	HI_FORMAT_SUBTITLE_DVB_SUB,       /**< DVB subtitle */
	HI_FORMAT_SUBTITLE_DVD_SUB,       /**< DVD subtitle */
	HI_FORMAT_SUBTITLE_TTML,          /**< TTML subtitle */
	HI_FORMAT_SUBTITLE_WEBVTT,
	HI_FORMAT_SUBTITLE_BUTT
} HI_FORMAT_SUBTITLE_TYPE_E;

typedef enum hiFORMAT_AUDIO_TYPE_E
{
	HI_FORMAT_AUDIO_MP2 = 0x000,  /**< MPEG audio layer 1, 2.*/
	HI_FORMAT_AUDIO_MP3,          /**< MPEG audio layer 1, 2, 3.*/
	HI_FORMAT_AUDIO_AAC,
	HI_FORMAT_AUDIO_AC3,
	HI_FORMAT_AUDIO_DTS,
	HI_FORMAT_AUDIO_VORBIS,
	HI_FORMAT_AUDIO_DVAUDIO,
	HI_FORMAT_AUDIO_WMAV1,
	HI_FORMAT_AUDIO_WMAV2,
	HI_FORMAT_AUDIO_MACE3,
	HI_FORMAT_AUDIO_MACE6,
	HI_FORMAT_AUDIO_VMDAUDIO,
	HI_FORMAT_AUDIO_SONIC,
	HI_FORMAT_AUDIO_SONIC_LS,
	HI_FORMAT_AUDIO_FLAC,
	HI_FORMAT_AUDIO_MP3ADU,
	HI_FORMAT_AUDIO_MP3ON4,
	HI_FORMAT_AUDIO_SHORTEN,
	HI_FORMAT_AUDIO_ALAC,
	HI_FORMAT_AUDIO_WESTWOOD_SND1,
	HI_FORMAT_AUDIO_GSM,
	HI_FORMAT_AUDIO_QDM2,
	HI_FORMAT_AUDIO_COOK,
	HI_FORMAT_AUDIO_TRUESPEECH,
	HI_FORMAT_AUDIO_TTA,
	HI_FORMAT_AUDIO_SMACKAUDIO,
	HI_FORMAT_AUDIO_QCELP,
	HI_FORMAT_AUDIO_WAVPACK,
	HI_FORMAT_AUDIO_DSICINAUDIO,
	HI_FORMAT_AUDIO_IMC,
	HI_FORMAT_AUDIO_MUSEPACK7,
	HI_FORMAT_AUDIO_MLP,
	HI_FORMAT_AUDIO_GSM_MS, /**< as found in WAV.*/
	HI_FORMAT_AUDIO_ATRAC3,
	HI_FORMAT_AUDIO_VOXWARE,
	HI_FORMAT_AUDIO_APE,
	HI_FORMAT_AUDIO_NELLYMOSER,
	HI_FORMAT_AUDIO_MUSEPACK8,
	HI_FORMAT_AUDIO_SPEEX,
	HI_FORMAT_AUDIO_WMAVOICE,
	HI_FORMAT_AUDIO_WMAPRO,
	HI_FORMAT_AUDIO_WMALOSSLESS,
	HI_FORMAT_AUDIO_ATRAC3P,
	HI_FORMAT_AUDIO_EAC3,
	HI_FORMAT_AUDIO_SIPR,
	HI_FORMAT_AUDIO_MP1,
	HI_FORMAT_AUDIO_TWINVQ,
	HI_FORMAT_AUDIO_TRUEHD,
	HI_FORMAT_AUDIO_MP4ALS,
	HI_FORMAT_AUDIO_ATRAC1,
	HI_FORMAT_AUDIO_BINKAUDIO_RDFT,
	HI_FORMAT_AUDIO_BINKAUDIO_DCT,
	HI_FORMAT_AUDIO_DRA,
	HI_FORMAT_AUDIO_DTS_EXPRESS,

	HI_FORMAT_AUDIO_PCM = 0x100,   /**< various PCM codecs. */
	HI_FORMAT_AUDIO_PCM_BLURAY = 0x121,

	HI_FORMAT_AUDIO_ADPCM = 0x130, /**< various ADPCM codecs. */

	HI_FORMAT_AUDIO_AMR_NB = 0x160,/**< various AMR codecs. */
	HI_FORMAT_AUDIO_AMR_WB,
	HI_FORMAT_AUDIO_AMR_AWB,

	HI_FORMAT_AUDIO_RA_144 = 0x170, /**< RealAudio codecs. */
	HI_FORMAT_AUDIO_RA_288,

	HI_FORMAT_AUDIO_DPCM = 0x180, /**< various DPCM codecs. */

	HI_FORMAT_AUDIO_G711 = 0x190, /**< various G.7xx codecs. */
	HI_FORMAT_AUDIO_G722,
	HI_FORMAT_AUDIO_G7231,
	HI_FORMAT_AUDIO_G726,
	HI_FORMAT_AUDIO_G728,
	HI_FORMAT_AUDIO_G729AB,

	HI_FORMAT_AUDIO_MULTI = 0x1f0, /**< support multi codecs. */

	HI_FORMAT_AUDIO_BUTT = 0x1ff,
} HI_FORMAT_AUDIO_TYPE_E;

typedef enum hiFORMAT_VIDEO_TYPE_E
{
	HI_FORMAT_VIDEO_MPEG2 = 0x0, /**< MPEG2*/
	HI_FORMAT_VIDEO_MPEG4,       /**< MPEG4 DIVX4 DIVX5*/
	HI_FORMAT_VIDEO_AVS,         /**< AVS*/
	HI_FORMAT_VIDEO_H263,        /**< H263*/
	HI_FORMAT_VIDEO_H264,        /**< H264*/
	HI_FORMAT_VIDEO_REAL8,       /**< REAL*/
	HI_FORMAT_VIDEO_REAL9,       /**< REAL*/
	HI_FORMAT_VIDEO_VC1,         /**< VC-1*/
	HI_FORMAT_VIDEO_VP6,         /**< VP6*/
	HI_FORMAT_VIDEO_VP6F,        /**< VP6F*/
	HI_FORMAT_VIDEO_VP6A,        /**< VP6A*/
	HI_FORMAT_VIDEO_MJPEG,       /**< MJPEG*/
	HI_FORMAT_VIDEO_SORENSON,    /**< SORENSON SPARK*/
	HI_FORMAT_VIDEO_DIVX3,       /**< DIVX3, not supported*/
	HI_FORMAT_VIDEO_RAW,         /**< RAW*/
	HI_FORMAT_VIDEO_JPEG,        /**< JPEG added for VENC*/
	HI_FORMAT_VIDEO_VP8,         /**<VP8*/
	HI_FORMAT_VIDEO_MSMPEG4V1,   /**< MS private MPEG4 */
	HI_FORMAT_VIDEO_MSMPEG4V2,
	HI_FORMAT_VIDEO_MSVIDEO1,    /**< MS video */
	HI_FORMAT_VIDEO_WMV1,
	HI_FORMAT_VIDEO_WMV2,
	HI_FORMAT_VIDEO_RV10,
	HI_FORMAT_VIDEO_RV20,
	HI_FORMAT_VIDEO_SVQ1,        /**< Apple video */
	HI_FORMAT_VIDEO_SVQ3,        /**< Apple video */
	HI_FORMAT_VIDEO_H261,
	HI_FORMAT_VIDEO_VP3,
	HI_FORMAT_VIDEO_VP5,
	HI_FORMAT_VIDEO_CINEPAK,
	HI_FORMAT_VIDEO_INDEO2,
	HI_FORMAT_VIDEO_INDEO3,
	HI_FORMAT_VIDEO_INDEO4,
	HI_FORMAT_VIDEO_INDEO5,
	HI_FORMAT_VIDEO_MJPEGB,
	HI_FORMAT_VIDEO_MVC,
	HI_FORMAT_VIDEO_HEVC,        /**< HEVC(H265)*/
	HI_FORMAT_VIDEO_DV,
	HI_FORMAT_VIDEO_HUFFYUV,
	HI_FORMAT_VIDEO_DIVX,           /**< DIVX,not supported*/
	HI_FORMAT_VIDEO_REALMAGICMPEG4, /**< REALMAGIC MPEG4,not supported*/
	HI_FORMAT_VIDEO_VP9,         /**<VP9*/
	HI_FORMAT_VIDEO_WMV3,
	HI_FORMAT_VIDEO_AVS2,
	HI_FORMAT_VIDEO_BUTT
} HI_FORMAT_VIDEO_TYPE_E;

typedef enum hiFORMAT_SOURCE_TYPE_E
{
	HI_FORMAT_SOURCE_LOCAL = 0x0,   /**< Local file */
	HI_FORMAT_SOURCE_NET_VOD,       /**< Net VOD file */
	HI_FORMAT_SOURCE_NET_LIVE,      /**< Net Live stream */
	HI_FORMAT_SOURCE_BUTT
} HI_FORMAT_SOURCE_TYPE_E;

typedef enum hiFORMAT_STREAM_TYPE_E
{
	HI_FORMAT_STREAM_ES = 0x0,    /**< Element stream (ES) file */
	HI_FORMAT_STREAM_TS,          /**< TS file */
	HI_FORMAT_STREAM_BUTT
} HI_FORMAT_STREAM_TYPE_E;

typedef struct hiFORMAT_AUD_INFO_S
{
	HI_S32 s32StreamIndex;                   /**< Stream index. The invalid value is ::HI_FORMAT_INVALID_STREAM_ID. */
	HI_U32 u32Format;                        /**< Audio encoding format. For details about the value definition, see ::HI_FORMAT_AUDIO_TYPE_E. */
	HI_U32 u32Profile;                       /**< Audio encoding version, such as 0x160(WMAV1) and 0x161 (WMAV2). It is valid only for WMA encoding. */
	HI_U32 u32SampleRate;                    /**< 8000,11025,441000,... */
	HI_U16 u16BitPerSample;                  /**< Number of bits occupied by each audio sampling point such as 8 bits or 16 bits. */
	HI_U16 u16Channels;                      /**< Number of channels, 1 or 2. *//**< CNcomment:ÉùµÀÊý, 1 or 2 */
	HI_S32 s32SubStreamID;                   /**< Sub audio stream ID */
	HI_U32 u32BlockAlign;                    /**< Number of bytes contained in a packet */
	HI_U32 u32Bitrate;                       /**< Audio bit rate, in the unit of bit/s. */
	HI_BOOL bBigEndian;                      /**< Big endian or little endian. It is valid only for the PCM format */
	HI_CHAR aszLanguage[HI_FORMAT_LANG_LEN]; /**< Audio stream language */
	HI_U32 u32ExtradataSize;                 /**< Length of the extended data */
	HI_U8*  pu8Extradata;                    /**< Extended data */
	HI_VOID*  pCodecContext;                 /**< Audio decode context */
	HI_U32 u32Role;                          /**< Role descriptor value of mpeg dash. the most 8 bits is scheme value(refer to HI_FORMAT_ROLE_SCHEME_E), the left 24 bits is descriptor value, the descriptor value may be the bitwise '|' result of value define in HI_FORMAT_ROLE_VALUE_E*/
	HI_U32 u32Accessibility;                 /**<  Accessbilitydescriptor value of mpeg dash. the most 8 bits is scheme value(refer to HI_FORMAT_ROLE_SCHEME_E), the left 24 bits is descriptor value, the descriptor value type is number*/
	HI_S64 s64Duration;                      /**< Duration of audio stream, in the unit of ms. */
	HI_U32 u32CodecTag; /**< Codec tag of audio stream format, fourcc (LSB first, so "ABCD" -> ('D'<<24) + ('C'<<16) + ('B'<<8) + 'A'). */
} HI_FORMAT_AUD_INFO_S;

typedef struct hiFORMAT_VID_INFO_S
{
	HI_S32 s32StreamIndex;                  /**< Stream index. The invalid value is ::HI_FORMAT_INVALID_STREAM_ID. */
	HI_U32 u32Format;                       /**< Video encoding format. For details about the value definition, see ::HI_FORMAT_VIDEO_TYPE_E. */
	HI_U16 u16RefFrameNum;                  /**< Number of reference frames. */
	HI_U16 u16Profile;                      /**< Profile level. */
	HI_U16 u16Width;                        /**< Width, in the unit of pixel. */
	HI_U16 u16Height;                       /**< Height, in the unit of pixel. */
	HI_U16 u16FpsInteger;                   /**< Integer part of the frame rate */
	HI_U16 u16FpsDecimal;                   /**< Decimal part of the frame rate */
	HI_U32 u32Bitrate;                      /**< Video bit rate, in the unit of bit/s. */
	HI_U32 u32CodecVersion;                 /**< Version of codec. */
	HI_U32 u32Rotate;                       /**< Video rotation angle, value is 90/180/270, default value is 0 */
	HI_U32 u32Reversed;
	HI_BOOL bEnableTVP;
	HI_U32 u32ExtradataSize;                /**< Length of the extended data */
	HI_U8*  pu8Extradata;                   /**< Extended data */
	HI_VOID*  pCodecContext;                /**< video decode context */
	HI_U32 u32Role;                         /**< Role descriptor value of mpeg dash. the most 8 bits is scheme value(refer to HI_FORMAT_ROLE_SCHEME_E), the left 24 bits is descriptor value, the descriptor value may be the bitwise '|' result of value define in HI_FORMAT_ROLE_VALUE_E*/
	HI_U32 u32Accessibility;                /**<  Accessbilitydescriptor value of mpeg dash. the most 8 bits is scheme value(refer to HI_FORMAT_ROLE_SCHEME_E), the left 24 bits is descriptor value, the descriptor value type is number*/
	HI_S64 s64Duration;                     /**< Duration of video stream, in the unit of ms. */
	HI_BOOL bNoPts;                         /**<this stream has no pts info or pts is invalid> */
} HI_FORMAT_VID_INFO_S;

typedef struct hiFORMAT_SUB_INFO_S
{
	HI_S32  s32StreamIndex;                            /**< Stream index. The invalid value is ::HI_FORMAT_INVALID_STREAM_ID. */
	HI_U32  u32Format;                                 /**< Subtitle format, For details about the value definition, see::HI_FORMAT_SUBTITLE_TYPE_E */
	HI_U32  u32CharSet;                                /**< Encoding type of the subtitle, the value range is as follows:
															1. The default value is 0.
															2. The value of the u32CharSet is the identified byte encoding value if the IdentStream byte encoding function (for details about the definition, see hi_charset_common.h) is set.
															3. If the ConvStream function (for details about the definition, see hi_charset_common.h) is set and the invoke interface is called to set the encoding type to be converted by implementing HI_FORMAT_INVOKE_SET_SOURCE_CODETYPE, the value of the u32CharSet is the configured encoding type */
	HI_BOOL bExtSub;                                  /**< Whether subtitles are external subtitles. When bExtSub is HI_TRUE, the subtitles are external. When bExtSub is HI_FALSE, the subtitles are internal. */
	HI_U32  u32StreamNum;                             /**< contains stream number */
	HI_CHAR paszLanguage[HI_FORMAT_MAX_LANGUAGE_NUM][HI_FORMAT_LANG_LEN]; /**< Subtitle language */
	HI_U16  u16OriginalFrameWidth;                     /**< Width of the original image */
	HI_U16  u16OriginalFrameHeight;                    /**< Height of the original image */
	HI_U32  u32ExtradataSize;                          /**< Length of the extended data */
	HI_U8*   pu8Extradata;                             /**< Extended data */
	HI_VOID*  pCodecContext;                           /**< Audio decode context */
	HI_U32 u32Role;                                    /**< Role descriptor value of mpeg dash. the most 8 bits is scheme value(refer to HI_FORMAT_ROLE_SCHEME_E), the left 24 bits is descriptor value, the descriptor value may be the bitwise '|' result of value define in HI_FORMAT_ROLE_VALUE_E*/
	HI_U32 u32Accessibility;                           /**<  Accessibility descriptor value of mpeg dash. the most 8 bits is scheme value(refer to HI_FORMAT_ROLE_SCHEME_E), the left 24 bits is descriptor value, value type is number*/
	HI_CHAR paszFileName[HI_FORMAT_MAX_URL_LEN];       /**< File name of external subtitle. */
} HI_FORMAT_SUB_INFO_S;

typedef struct hiFORMAT_PROGRAM_INFO_S
{
	HI_U32 u32VidStreamNum;                /**< Number of video streams */
	HI_FORMAT_VID_INFO_S* pastVidStream;   /**< Video stream information */
	HI_U32 u32AudStreamNum;                /**< Number of audio streams */
	HI_FORMAT_AUD_INFO_S* pastAudStream;   /**< Audio stream information */
	HI_U32 u32SubStreamNum;                /**< Number of subtitles */
	HI_FORMAT_SUB_INFO_S* pastSubStream;   /**< Subtitle information */
	HI_CHAR aszServiceName[HI_FORMAT_SERVICE_DESCRIPT_LEN];       /**< Program service name info */
	HI_CHAR aszServiceProvider[HI_FORMAT_SERVICE_DESCRIPT_LEN];   /**<  Program service provider info */
	HI_S64  s64ProgramDuration;
	HI_S64  s64ProgramStartTime;
} HI_FORMAT_PROGRAM_INFO_S;

typedef struct hiFORMAT_FILE_INFO_S
{
	HI_FORMAT_SOURCE_TYPE_E eSourceType;    /**< File source type */
	HI_FORMAT_STREAM_TYPE_E eStreamType;    /**< File stream type */
	HI_S64  s64FileSize;                    /**< File size, in the unit of byte. */
	HI_S64  s64StartTime;                   /**< Start time of playing a file, in the unit is ms. */
	HI_S64  s64Duration;                    /**< Total duration of a file, in the unit of ms. */
	HI_U32  u32Bitrate;                     /**< File bit rate, in the unit of bit/s. */
	HI_CHAR aszFileFormat[HI_FORMAT_TITLE_MAX_LEN];   /**< File demuxer info .Not used now*/
	HI_U32  u32ProgramNum;                  /**< Actual number of programs */
	HI_FORMAT_PROGRAM_INFO_S* pastProgramInfo; /**< Program information */
	HI_BOOL bIsDivx;                        /**< If the stream is DIVX restricted stream,HI_TRUE yes,HI_FALSE no */
	HI_BOOL bIsDrmFile;
} HI_FORMAT_FILE_INFO_S;

#endif
