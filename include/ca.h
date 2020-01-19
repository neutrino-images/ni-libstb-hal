#ifndef __CA_H__
#define __CA_H__

#include <stdint.h>
#include "cs_types.h"
#include <vector>
#include <set>
typedef std::vector<u16>			CaIdVector;
typedef std::vector<u16>::iterator		CaIdVectorIterator;
typedef std::vector<u16>::const_iterator	CaIdVectorConstIterator;

enum CA_INIT_MASK {
	CA_INIT_SC = 1,
	CA_INIT_CI,
	CA_INIT_BOTH
};

enum CA_SLOT_TYPE {
	CA_SLOT_TYPE_SMARTCARD,
	CA_SLOT_TYPE_CI,
	CA_SLOT_TYPE_ALL
};

enum CA_MESSAGE_FLAGS {
	CA_MESSAGE_EMPTY		= (1 << 0),
	CA_MESSAGE_HAS_PARAM1_DATA	= (1 << 1), // Free after use!
	CA_MESSAGE_HAS_PARAM1_INT	= (1 << 2),
	CA_MESSAGE_HAS_PARAM1_PTR	= (1 << 3),
	CA_MESSAGE_HAS_PARAM2_INT	= (1 << 4),
	CA_MESSAGE_HAS_PARAM2_PTR	= (1 << 5),
	CA_MESSAGE_HAS_PARAM2_DATA	= (1 << 6),
	CA_MESSAGE_HAS_PARAM3_DATA	= (1 << 7), // Free after use!
	CA_MESSAGE_HAS_PARAM3_INT	= (1 << 8),
	CA_MESSAGE_HAS_PARAM3_PTR	= (1 << 9),
	CA_MESSAGE_HAS_PARAM4_INT	= (1 << 10),
	CA_MESSAGE_HAS_PARAM4_PTR	= (1 << 11),
	CA_MESSAGE_HAS_PARAM4_DATA	= (1 << 12),
	CA_MESSAGE_HAS_PARAM5_INT	= (1 << 13),
	CA_MESSAGE_HAS_PARAM5_PTR	= (1 << 14),
	CA_MESSAGE_HAS_PARAM5_DATA	= (1 << 15),
	CA_MESSAGE_HAS_PARAM6_INT	= (1 << 16),
	CA_MESSAGE_HAS_PARAM6_PTR	= (1 << 17),
	CA_MESSAGE_HAS_PARAM6_DATA	= (1 << 18),
	CA_MESSAGE_HAS_PARAM1_LONG	= (1 << 19),
	CA_MESSAGE_HAS_PARAM2_LONG	= (1 << 20),
	CA_MESSAGE_HAS_PARAM3_LONG	= (1 << 21),
	CA_MESSAGE_HAS_PARAM4_LONG	= (1 << 22)
};

enum CA_MESSAGE_MSGID {
	CA_MESSAGE_MSG_INSERTED,
	CA_MESSAGE_MSG_REMOVED,
	CA_MESSAGE_MSG_INIT_OK,
	CA_MESSAGE_MSG_INIT_FAILED,
	CA_MESSAGE_MSG_MMI_MENU,
	CA_MESSAGE_MSG_MMI_MENU_ENTER,
	CA_MESSAGE_MSG_MMI_MENU_ANSWER,
	CA_MESSAGE_MSG_MMI_LIST,
	CA_MESSAGE_MSG_MMI_TEXT,
	CA_MESSAGE_MSG_MMI_REQ_INPUT,
	CA_MESSAGE_MSG_MMI_CLOSE,
	CA_MESSAGE_MSG_INTERNAL,
	CA_MESSAGE_MSG_PMT_ARRIVED,
	CA_MESSAGE_MSG_CAPMT_ARRIVED,
	CA_MESSAGE_MSG_CAT_ARRIVED,
	CA_MESSAGE_MSG_ECM_ARRIVED,
	CA_MESSAGE_MSG_EMM_ARRIVED,
	CA_MESSAGE_MSG_CHANNEL_CHANGE,
	CA_MESSAGE_MSG_GUI_READY,
	CA_MESSAGE_MSG_EXIT
};

typedef std::set<int> ca_map_t;
typedef ca_map_t::iterator ca_map_iterator_t;

typedef struct CA_MESSAGE {
	uint32_t MsgId;
	enum CA_SLOT_TYPE SlotType;
	int Slot;
	uint32_t Flags;
	union {
		uint8_t *Data[4];
		uint32_t Param[4];
		void *Ptr[4];
		uint64_t ParamLong[4];
	} Msg;
} CA_MESSAGE;

class cCA {
private:
	cCA(void);
public:
	uint32_t GetNumberCISlots(void);
	uint32_t GetNumberSmartCardSlots(void);
	static cCA *GetInstance(void);
	bool SendPMT(int Unit, unsigned char *Data, int Len, CA_SLOT_TYPE SlotType = CA_SLOT_TYPE_ALL);
//	bool SendCAPMT(u64 /*Source*/, u8 /*DemuxSource*/, u8 /*DemuxMask*/, const unsigned char * /*CAPMT*/, u32 /*CAPMTLen*/, const unsigned char * /*RawPMT*/, u32 /*RawPMTLen*/) { return true; };
	bool SendCAPMT(u64 /*Source*/, u8 /*DemuxSource*/, u8 /*DemuxMask*/, const unsigned char * /*CAPMT*/, u32 /*CAPMTLen*/, const unsigned char * /*RawPMT*/, u32 /*RawPMTLen*/, enum CA_SLOT_TYPE
	/*SlotType*/, unsigned char /*scrambled = 0*/, ca_map_t /*camap = std::set<int>()*/, int /*mode = 0*/, bool /*enabled = false*/) { return true; };
	bool SendMessage(const CA_MESSAGE *Msg);
	void SetInitMask(enum CA_INIT_MASK InitMask);
	int GetCAIDS(CaIdVector & /*Caids*/) { return 0; };
	bool Start(void);
	void Stop(void);
	void Ready(bool Set);
	void ModuleReset(enum CA_SLOT_TYPE, uint32_t Slot);
	bool ModulePresent(enum CA_SLOT_TYPE, uint32_t Slot);
	void ModuleName(enum CA_SLOT_TYPE, uint32_t Slot, char *Name);
	void MenuEnter(enum CA_SLOT_TYPE, uint32_t Slot);
	void MenuAnswer(enum CA_SLOT_TYPE, uint32_t Slot, uint32_t choice);
	void InputAnswer(enum CA_SLOT_TYPE, uint32_t Slot, uint8_t * Data, int Len);
	void MenuClose(enum CA_SLOT_TYPE, uint32_t Slot);
	void SetTSClock(u32 /*Speed*/, int /*slot*/) { return; };
	bool checkChannelID(u64 /*chanID*/) { return false; };
	void setCheckLiveSlot(int /*check*/) { return; };
	virtual ~cCA();
};

#endif // __CA_H__
