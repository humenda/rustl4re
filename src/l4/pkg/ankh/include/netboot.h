#pragma once

extern void netboot_init(char *shm_name, int bufsize, l4_cap_idx_t rx_cap,
			 l4_cap_idx_t tx_cap);
extern void print_network_configuration(void);
