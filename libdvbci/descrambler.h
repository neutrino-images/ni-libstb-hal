#ifndef __DESCR_H_
#define __DESCR_H_

int descrambler_init(void);
void descrambler_deinit(void);
int descrambler_set_key(int index, int parity, unsigned char *data);
int descrambler_set_pid(int index, int enable, int pid);

#endif
