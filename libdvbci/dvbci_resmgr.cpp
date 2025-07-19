/* DVB CI Resource Manager */

#include <stdio.h>
#include "dvbci_resmgr.h"

int eDVBCIResourceManagerSession::receivedAPDU(const unsigned char *tag, const void *data, int len)
{
	printf("[CI RM] SESSION(%d)RES %02x %02x %02x (len = %d): \n", session_nb, tag[0], tag[1], tag[2], len);
	if (len)
	{
		for (int i = 0; i < len; i++)
			printf("%02x ", ((const unsigned char *)data)[i]);
		printf("\n");
	}
	if ((tag[0] == 0x9f) && (tag[1] == 0x80))
	{
		switch (tag[2])
		{
			case 0x10:  // profile enquiry
				printf("[CI RM] cam profile inquiry\n");
				state = stateProfileEnquiry;
				return 1;
				break;
			case 0x11: // Tprofile
				printf("[CI RM] -> my cam can do: ");
				if (!len)
					printf("nothing");
				else
					for (int i = 0; i < len; i++)
						printf("%02x ", ((const unsigned char *)data)[i]);
				printf("\n");

				if (state == stateFirstProfileEnquiry)
				{
					// profile change
					return 1;
				}
				state = stateFinal;
				break;
			default:
				printf("[CI RM] unknown APDU tag 9F 80 %02x\n", tag[2]);
		}
	}

	return 0;
}

int eDVBCIResourceManagerSession::doAction()
{
	switch (state)
	{
		case stateStarted:
		{
			const unsigned char tag[3] = {0x9F, 0x80, 0x10}; // profile enquiry
			cCA::GetInstance()->CheckCerts();
			sendAPDU(tag);
			state = stateFirstProfileEnquiry;
			return 0;
		}
		case stateFirstProfileEnquiry:
		{
			const unsigned char tag[3] = {0x9F, 0x80, 0x12}; // profile change
			sendAPDU(tag);
			state = stateProfileChange;
			return 0;
		}
		case stateProfileChange:
		{
			printf("[CI RM] cannot deal with statProfileChange\n");
			break;
		}
		case stateProfileEnquiry:
		{
			const unsigned char tag[3] = {0x9F, 0x80, 0x11};
			if (cCA::GetInstance()->CheckCerts())
			{
				if (cCA::GetInstance()->op[slot->slot])
				{
					const unsigned char data[][4] =
					{
						{0x00, 0x01, 0x00, 0x41},	// res mgr 1
//						{0x00, 0x01, 0x00, 0x42},	// res mgr 2
						{0x00, 0x02, 0x00, 0x41},	// app mgr 1
						{0x00, 0x02, 0x00, 0x42},	// app mgr 2
						{0x00, 0x02, 0x00, 0x43},	// app mgr 3
						{0x00, 0x02, 0x00, 0x45},	// app mgr 5
						{0x00, 0x03, 0x00, 0x41},	// ca mgr
						{0x00, 0x20, 0x00, 0x41},	// host ctrl 1
						{0x00, 0x20, 0x00, 0x42},	// host ctrl 2
						{0x00, 0x20, 0x00, 0x43},	// host ctrl 3
						{0x00, 0x24, 0x00, 0x41},	// datetime
						{0x00, 0x40, 0x00, 0x41},	// mmi
//						{0x00, 0x10, 0x00, 0x41},	// auth.
						{0x00, 0x41, 0x00, 0x41},	// app mmi 1
						{0x00, 0x41, 0x00, 0x42},	// app mmi 2
						{0x00, 0x8c, 0x10, 0x01},	// content ctrl 1
						{0x00, 0x8c, 0x10, 0x02},	// content ctrl 2
						{0x00, 0x8c, 0x10, 0x04},	// content ctrl 4
						{0x00, 0x8d, 0x10, 0x01},	// Host lang ctrl
						{0x00, 0x8e, 0x10, 0x01},	// Cam upgrade
						{0x00, 0x8f, 0x10, 0x01},	// operator profile 1
						{0x00, 0x8f, 0x10, 0x02}	// operator profile 2
//						{0x00, 0x97, 0x10, 0x01},
//						{0x00, 0x60, 0x60, 0x03},
//						{0x00, 0x04, 0x10, 0x01}
					};
//					printf("<<<<< CI Modul Slot %i - operator profile >>>>>\n", slot->slot);
					sendAPDU(tag, data, sizeof(data));
				}
				else
				{
					const unsigned char data[][4] =
					{
						{0x00, 0x01, 0x00, 0x41},	// res mgr 1
//						{0x00, 0x01, 0x00, 0x42},	// res mgr 2
						{0x00, 0x02, 0x00, 0x41},	// app mgr 1
						{0x00, 0x02, 0x00, 0x42},	// app mgr 2
						{0x00, 0x02, 0x00, 0x43},	// app mgr 3
						{0x00, 0x02, 0x00, 0x45},	// app mgr 5
						{0x00, 0x03, 0x00, 0x41},	// ca mgr
						{0x00, 0x20, 0x00, 0x41},	// host ctrl 1
						{0x00, 0x20, 0x00, 0x42},	// host ctrl 2
						{0x00, 0x20, 0x00, 0x43},	// host ctrl 3
						{0x00, 0x24, 0x00, 0x41},	// datetime
						{0x00, 0x40, 0x00, 0x41},	// mmi
//						{0x00, 0x10, 0x00, 0x41},	// auth.
						{0x00, 0x41, 0x00, 0x41},	// app mmi 1
						{0x00, 0x41, 0x00, 0x42},	// app mmi 2
						{0x00, 0x8c, 0x10, 0x01},	// content ctrl 1
						{0x00, 0x8c, 0x10, 0x02},	// content ctrl 2
						{0x00, 0x8c, 0x10, 0x04},	// content ctrl 4
						{0x00, 0x8d, 0x10, 0x01},	// Host lang ctrl
						{0x00, 0x8e, 0x10, 0x01}	// Cam upgrade
//						{0x00, 0x8f, 0x10, 0x01},	// operator profile 1
//						{0x00, 0x8f, 0x10, 0x02},	// operator profile 2
//						{0x00, 0x97, 0x10, 0x01},
//						{0x00, 0x60, 0x60, 0x03},
//						{0x00, 0x04, 0x10, 0x01}
					};
//					printf("<<<<< CI Modul Slot %i - NO operator profile >>>>>\n", slot->slot);
					sendAPDU(tag, data, sizeof(data));
				}
			}
			else
			{
				const unsigned char data[][4] =
				{
					{0x00, 0x01, 0x00, 0x41},	// res mgr 1
					{0x00, 0x02, 0x00, 0x41},	// app mgr 1
					{0x00, 0x02, 0x00, 0x43},	// app mgr 3
					{0x00, 0x03, 0x00, 0x41},	// ca mgr
//					{0x00, 0x20, 0x00, 0x41},	// host ctrl 1
					{0x00, 0x24, 0x00, 0x41},	// datetime
					{0x00, 0x40, 0x00, 0x41}	// mmi
//					{0x00, 0x10, 0x00, 0x41}	// auth.
				};
				sendAPDU(tag, data, sizeof(data));
			}
			//sendAPDU(tag, data, sizeof(data));
			state = stateFinal;
			return 0;
		}
		case stateFinal:
			printf("[CI RM] Should not happen: action on stateFinal\n");
		default:
			break;
	}
	return 0;
}
