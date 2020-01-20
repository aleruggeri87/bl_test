#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "data.h"


int main(void) {
	ty_bl my_bl;
	float pct=0;
	uint32_t address;
	uint8_t *data = NULL;
	uint32_t k;

	bl_construct(&my_bl, 172, 0x400, 0x040800);
	if(bl_read_hex_file(my_bl, "in.hex")) {
		while(pct != 1.0) {
			bl_get_nextpage(my_bl, &address, &data, &pct);
			printf("pct: %.1f, addr: %06X, data: ", pct, address);
			for(k = 0; k < 0x400 * 3; k++) {
				printf("%02X,", *data++);
			}
			printf("\n");
			fflush(stdout);
		}
	}

	bl_destruct(my_bl);


	exit(EXIT_SUCCESS);
} // END PROGRAM
