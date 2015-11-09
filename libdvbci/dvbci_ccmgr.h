#ifndef __dvbci_dvbci_ccmgr_h
#define __dvbci_dvbci_ccmgr_h

#include "dvbci_session.h"

class eDVBCIContentControlManagerSession: public eDVBCISession
{
	bool data_initialize(tSlot *tslot);
	bool ci_ccmgr_cc_data_req(tSlot *tslot, const uint8_t *data, unsigned int len);
	bool ci_ccmgr_cc_sac_data_req(tSlot *tslot, const uint8_t *data, unsigned int len);
	bool ci_ccmgr_cc_sac_send(tSlot *tslot, const uint8_t *tag, uint8_t *data, unsigned int pos);
	void ci_ccmgr_cc_sac_sync_req(tSlot *tslot, const uint8_t *data, unsigned int len);
	void ci_ccmgr_cc_sync_req();
	void ci_ccmgr_cc_open_cnf(tSlot *slot);

	enum {
		stateFinal=statePrivate
	};
	int receivedAPDU(const unsigned char *tag, const void *data, int len);
	int doAction();
public:
	eDVBCIContentControlManagerSession(tSlot *tslot);
	~eDVBCIContentControlManagerSession();
	void ci_ccmgr_doClose(tSlot *tslot);
};
#endif
