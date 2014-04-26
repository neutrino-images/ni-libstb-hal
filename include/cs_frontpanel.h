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
	FP_ICON_USB = 0x27,
	FP_ICON_HD = 0x27,
	FP_ICON_HDD = ICON_HDD,
	FP_ICON_LOCK = 0x27,
	FP_ICON_BT = 0x27,
	FP_ICON_MP3 = 0x27,
	FP_ICON_MUSIC = 0x27,
	FP_ICON_DD = 0x27,
	FP_ICON_MAIL = 0x27,
	FP_ICON_MUTE = 0x27,
	FP_ICON_PLAY = 0x27,
	FP_ICON_PAUSE = 0x27,
	FP_ICON_FF = 0x27,
	FP_ICON_FR = 0x27,
	FP_ICON_REC = 0x27,
	FP_ICON_CLOCK = 0x27,
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
	FP_ICON_PAUSE,
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
	FP_ICON_USB = ICON_USB,
	FP_ICON_REC = ICON_REC,
	FP_ICON_CLOCK = ICON_TIMER,
	FP_ICON_HD = ICON_HD,
	FP_ICON_LOCK = ICON_SCRAMBLED,
	FP_ICON_DD = ICON_DOLBY,
	FP_ICON_MUTE = ICON_MUTE,
	FP_ICON_MP3 = ICON_MP3,
	FP_ICON_PLAY = ICON_Play,
	FP_ICON_HDD,
	FP_ICON_MUSIC,
	FP_ICON_MAIL,
	FP_ICON_FF,
	FP_ICON_FR,
#elif defined(BOXMODEL_UFS910)
	FP_ICON_USB = 0x10,
	FP_ICON_HD,
	FP_ICON_HDD,
	FP_ICON_LOCK,
	FP_ICON_BT,
	FP_ICON_MP3,
	FP_ICON_MUSIC,
	FP_ICON_DD,
	FP_ICON_MAIL,
	FP_ICON_MUTE,
	FP_ICON_PLAY,
	FP_ICON_PAUSE,
	FP_ICON_FF,
	FP_ICON_FR,
	FP_ICON_REC,
	FP_ICON_CLOCK,
#else
	FP_ICON_USB = 0x10,
	FP_ICON_HD,
	FP_ICON_HDD,
	FP_ICON_LOCK,
	FP_ICON_BT,
	FP_ICON_MP3,
	FP_ICON_MUSIC,
	FP_ICON_DD,
	FP_ICON_MAIL,
	FP_ICON_MUTE,
	FP_ICON_PLAY,
	FP_ICON_PAUSE,
	FP_ICON_FF,
	FP_ICON_FR,
	FP_ICON_REC,
	FP_ICON_CLOCK,
#endif
	FP_ICON_RECORD,
	FP_ICON_DOWNLOAD,
	FP_ICON_TIMESHIFT,
	FP_ICON_MAX
} fp_icon;

typedef enum {
	FP_FLAG_NONE			= 0x00,
	FP_FLAG_SCROLL_ON		= 0x01,	/* switch scrolling on */
	FP_FLAG_SCROLL_LTR		= 0x02,	/* scroll from left to rightinstead of default right to left direction (i.e. for arabic text) */
	FP_FLAG_SCROLL_SIO		= 0x04,	/* start/stop scrolling with empty screen (scroll in/out) */
	FP_FLAG_SCROLL_DELAY		= 0x08,	/* delayed scroll start */
	FP_FLAG_ALIGN_LEFT		= 0x10,	/* align the text in display from the left (default) */
	FP_FLAG_ALIGN_RIGHT		= 0x20,	/* align the text in display from the right (arabic) */
	FP_FLAG_UPDATE_SCROLL_POS	= 0x40,	/* update the current position for scrolling */
} fp_flag;

typedef struct {
	unsigned char		brightness;
	unsigned char		flags;
	unsigned char		current_hour;
	unsigned char		current_minute;
	unsigned char		timer_minutes_hi;
	unsigned char		timer_minutes_lo;
} fp_standby_data_t;

typedef struct {
	unsigned char		source;
	unsigned char		time_minutes_hi;
	unsigned char		time_minutes_lo;
} fp_wakeup_data_t;

typedef enum {
	FP_LED_1_ON		= 0x81,
	FP_LED_2_ON		= 0x82,
	FP_LED_3_ON		= 0x83,
	FP_LED_1_OFF		= 0x01,
	FP_LED_2_OFF		= 0x02,
	FP_LED_3_OFF		= 0x03,
} fp_led_ctrl_t;

typedef enum {
	FP_WAKEUP_SOURCE_TIMER  = 0x01,
	FP_WAKEUP_SOURCE_BUTTON = 0x02,
	FP_WAKEUP_SOURCE_REMOTE = 0x04,
	FP_WAKEUP_SOURCE_PWLOST = 0x7F,
	FP_WAKEUP_SOURCE_POWER  = 0xFF
} fp_wakeup_source;

typedef struct {
	unsigned short		addr;
	unsigned short		cmd;
} fp_standby_cmd_data_t;

#endif /* __DUCKBOX_VFD__ */
