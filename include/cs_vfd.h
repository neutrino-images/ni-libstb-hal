#ifndef __DUCKBOX_VFD__
#define __DUCKBOX_VFD__

#define VFDDISPLAYCHARS      0xc0425a00
#define VFDWRITECGRAM        0x40425a01
#define VFDBRIGHTNESS        0xc0425a03
#define VFDPWRLED            0xc0425a04
#define VFDDRIVERINIT        0xc0425a08
#define VFDICONDISPLAYONOFF  0xc0425a0a
#define VFDDISPLAYWRITEONOFF 0xc0425a05

#define VFDCLEARICONS        0xc0425af6
#define VFDSETRF             0xc0425af7
#define VFDSETFAN            0xc0425af8
#define VFDGETWAKEUPMODE     0xc0425af9
#define VFDGETTIME           0xc0425afa
#define VFDSETTIME           0xc0425afb
#define VFDSTANDBY           0xc0425afc
#define VFDREBOOT            0xc0425afd
#define VFDSETLED            0xc0425afe
#define VFDSETMODE           0xc0425aff

#define VFDGETWAKEUPTIME     0xc0425b00
#define VFDGETVERSION        0xc0425b01
#define VFDSETDISPLAYTIME    0xc0425b02
#define VFDSETTIMEMODE       0xc0425b03
#define VFDDISPLAYCLR        0xc0425b00

typedef enum {
#if defined(BOXMODEL_OCTAGON1008)
	ICON_DTS,
	ICON_VIDEO,
	ICON_AUDIO,
	ICON_LINK,
	ICON_HDD,
	ICON_DISC,
	ICON_DVB,
	VFD_ICON_USB = 0x27,
	VFD_ICON_HD = 0x27,
	VFD_ICON_HDD = ICON_HDD,
	VFD_ICON_LOCK = 0x27,
	VFD_ICON_BT = 0x27,
	VFD_ICON_MP3 = 0x27,
	VFD_ICON_MUSIC = 0x27,
	VFD_ICON_DD = 0x27,
	VFD_ICON_MAIL = 0x27,
	VFD_ICON_MUTE = 0x27,
	VFD_ICON_PLAY = 0x27,
	VFD_ICON_PAUSE = 0x27,
	VFD_ICON_FF = 0x27,
	VFD_ICON_FR = 0x27,
	VFD_ICON_REC = 0x27,
	VFD_ICON_CLOCK = 0x27,
#elif defined(BOXMODEL_FORTIS_HDBOX)
	ICON_STANDBY = 0x10,
	ICON_SAT,
	ICON_REC,
	ICON_TIMESHIFT,
	ICON_TIMER,
	ICON_HD,
	ICON_USB,
	ICON_SCRAMBLED,
	ICON_DOLBY,
	ICON_MUTE,
	ICON_TUNER1,
	ICON_TUNER2,
	ICON_MP3,
	ICON_REPEAT,
	ICON_Play,
	VFD_ICON_PAUSE,
	ICON_TER,
	ICON_FILE,
	ICON_480i,
	ICON_480p,
	ICON_576i,
	ICON_576p,
	ICON_720p,
	ICON_1080i,
	ICON_1080p,
	ICON_Play_1,
	VFD_ICON_USB = ICON_USB,
	VFD_ICON_REC = ICON_REC,
	VFD_ICON_CLOCK = ICON_TIMER,
	VFD_ICON_HD = ICON_HD,
	VFD_ICON_LOCK = ICON_SCRAMBLED,
	VFD_ICON_DD = ICON_DOLBY,
	VFD_ICON_MUTE = ICON_MUTE,
	VFD_ICON_MP3 = ICON_MP3,
	VFD_ICON_PLAY = ICON_Play,
	VFD_ICON_HDD,
	VFD_ICON_MUSIC,
	VFD_ICON_MAIL,
	VFD_ICON_FF,
	VFD_ICON_FR,
#elif defined(BOXMODEL_UFS910)
	VFD_ICON_USB = 0x10,
	VFD_ICON_HD,
	VFD_ICON_HDD,
	VFD_ICON_LOCK,
	VFD_ICON_BT,
	VFD_ICON_MP3,
	VFD_ICON_MUSIC,
	VFD_ICON_DD,
	VFD_ICON_MAIL,
	VFD_ICON_MUTE,
	VFD_ICON_PLAY,
	VFD_ICON_PAUSE,
	VFD_ICON_FF,
	VFD_ICON_FR,
	VFD_ICON_REC,
	VFD_ICON_CLOCK,
#else
	VFD_ICON_USB = 0x10,
	VFD_ICON_HD,
	VFD_ICON_HDD,
	VFD_ICON_LOCK,
	VFD_ICON_BT,
	VFD_ICON_MP3,
	VFD_ICON_MUSIC,
	VFD_ICON_DD,
	VFD_ICON_MAIL,
	VFD_ICON_MUTE,
	VFD_ICON_PLAY,
	VFD_ICON_PAUSE,
	VFD_ICON_FF,
	VFD_ICON_FR,
	VFD_ICON_REC,
	VFD_ICON_CLOCK,
#endif
	VFD_ICON_MAX
} vfd_icon;

typedef enum
{
	VFD_FLAG_NONE			= 0x00,
	VFD_FLAG_SCROLL_ON		= 0x01,	/* switch scrolling on */
	VFD_FLAG_SCROLL_LTR		= 0x02,	/* scroll from left to rightinstead of default right to left direction (i.e. for arabic text) */
	VFD_FLAG_SCROLL_SIO		= 0x04,	/* start/stop scrolling with empty screen (scroll in/out) */
	VFD_FLAG_SCROLL_DELAY		= 0x08,	/* delayed scroll start */
	VFD_FLAG_ALIGN_LEFT		= 0x10,	/* align the text in display from the left (default) */
	VFD_FLAG_ALIGN_RIGHT		= 0x20,	/* align the text in display from the right (arabic) */
	VFD_FLAG_UPDATE_SCROLL_POS	= 0x40,	/* update the current position for scrolling */
} vfd_flag;

typedef struct {
	unsigned char		brightness;
	unsigned char		flags;
	unsigned char		current_hour;
	unsigned char		current_minute;
	unsigned char		timer_minutes_hi;
	unsigned char		timer_minutes_lo;
} standby_data_t;

typedef struct {
	unsigned char		source;
	unsigned char		time_minutes_hi;
	unsigned char		time_minutes_lo;
} wakeup_data_t;

typedef enum {
	VFD_LED_1_ON		= 0x81,
	VFD_LED_2_ON		= 0x82,
	VFD_LED_3_ON		= 0x83,
	VFD_LED_1_OFF		= 0x01,
	VFD_LED_2_OFF		= 0x02,
	VFD_LED_3_OFF		= 0x03,
} vfd_led_ctrl_t;

typedef enum
{
	WAKEUP_SOURCE_TIMER  = 0x01,
	WAKEUP_SOURCE_BUTTON = 0x02,
	WAKEUP_SOURCE_REMOTE = 0x04,
	WAKEUP_SOURCE_PWLOST = 0x7F,
	WAKEUP_SOURCE_POWER  = 0xFF
} wakeup_source;

#endif /* __DUCKBOX_VFD__ */
