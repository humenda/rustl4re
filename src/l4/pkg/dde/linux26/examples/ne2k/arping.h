#pragma once

#include <l4/dde/ddekit/semaphore.h>

struct arping_elem
{
	struct arping_elem *next;
	struct sk_buff     *skb;
};

extern ddekit_sem_t *arping_semaphore;
extern struct arping_elem *arping_list;

#define mac_fmt       "%02X:%02X:%02X:%02X:%02X:%02X"
#define mac_str(mac)  (unsigned char)((mac)[0]), (unsigned char)((mac)[1]),(mac)[2],(mac)[3],(mac)[4],(mac)[5]
