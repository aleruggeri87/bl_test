#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "data.h"

uint8_t Ascii2Hex(uint8_t c) {
	if(c >= '0' && c <= '9') {
		return (uint8_t)(c - '0');
	}
	if(c >= 'A' && c <= 'F') {
		return (uint8_t)(c - 'A' + 10);
	}
	if(c >= 'a' && c <= 'f') {
		return (uint8_t)(c - 'a' + 10);
	}

	return 0;  // this "return" will never be reached, but some compilers give a warning if it is not present
}

void clear_special_char(FILE * file, uint8_t * charToPut, uint32_t * totalCharsRead) {
	//Removes CR, LF, ':'  --Bdk6's
	while(*charToPut == '\n' || *charToPut == '\r' || *charToPut == ':') {
		(*charToPut = fgetc(file));
		*totalCharsRead++;
	}
}

uint32_t hex_file_line_count(FILE * file_to_count) {
	uint32_t line_count = 0;
	char got_char = '\0';

	while(got_char != EOF) {
		got_char = fgetc(file_to_count);
		if(got_char == ':') { line_count++; }
	}
	rewind(file_to_count);
	return line_count;
}

uint8_t read_byte_from_file(FILE * file, uint8_t * char_to_put, uint32_t * total_chars_read) {
	//Holds combined nibbles.
	uint8_t hexValue;
	//Get first nibble.
	*char_to_put = fgetc (file);
	clear_special_char(file, char_to_put, total_chars_read);
	//Put first nibble in.
	hexValue = (Ascii2Hex(*char_to_put));
	//Slide the nibble.
	hexValue = ((hexValue << 4) & 0xF0);
	//Put second nibble in.
	*char_to_put = fgetc (file);
	clear_special_char(file, char_to_put, total_chars_read);
	//Put the nibbles together.
	hexValue |= (Ascii2Hex(*char_to_put));
	//Return the byte.
	*total_chars_read += 2;

	return hexValue;
}

bool read_line_from_hex_file(FILE * file, uint8_t line_of_data[], uint32_t * combined_address, uint8_t * bytes_this_line) {
		uint8_t data_index = 0;
		uint8_t char_to_put;
		uint32_t total_chars_read = 0;
		bool ret = true;
		static uint32_t ext_address = 0;

		//To hold file hex values.
		uint8_t byte_count;
		uint8_t datum_address1;
		uint8_t datum_address2;
		uint8_t datum_record_type;
		uint8_t datum_check_sum;

		//BYTE COUNT
		byte_count = read_byte_from_file(file, &char_to_put, &total_chars_read);

		// No need to read, if no data.
		if (byte_count == 0){return false;}

		//ADDRESS1 //Will create an 8 bit shift. --Bdk6's
		datum_address1 = read_byte_from_file(file, &char_to_put, &total_chars_read);

		//ADDRESS2
		datum_address2 = read_byte_from_file(file, &char_to_put, &total_chars_read);

		//RECORD TYPE
		datum_record_type = read_byte_from_file(file, &char_to_put, &total_chars_read);		

		if(datum_record_type == 0) { // data

			*combined_address = (((uint16_t)datum_address1 << 8) | datum_address2) + ext_address;

			// DATA
			while(data_index < byte_count) {
				line_of_data[data_index] = read_byte_from_file(file, &char_to_put, &total_chars_read);
				data_index++;
			}
			*bytes_this_line = data_index;
		} else if(datum_record_type == 4) { // extended address
			// DATA
			ext_address = 0;
			while(data_index < byte_count) {
				ext_address <<= 8;
				ext_address += read_byte_from_file(file, &char_to_put, &total_chars_read);
				data_index++;
			}
			ext_address <<= 16;
			ret = false;
		}

		// CHECKSUM
		datum_check_sum = read_byte_from_file(file, &char_to_put, &total_chars_read);

		return ret;
}

struct s_bl {
	// settings
	uint16_t n_pages;
	uint16_t iw_1page;
	uint32_t reset;
	// pointers
	uint16_t i_page;
	uint16_t n_used_pages;
	// data
	uint8_t *hex_map;
	uint8_t *page_data;
	bool *used_pages;
};

void bl_print_map(ty_bl bl) {
	uint16_t p, j, i;
	for(p = 0; p < bl->n_pages; p++) {
		if(bl->used_pages[p]) {
			printf("--- Page %d: 0x%08X - 0x%08X ---\n", p, bl->iw_1page * p, bl->iw_1page * (p + 1));
			for(j = 0; j < 128 * 4 * 4; j += 4 * 4) {
				printf("%06X - ", (bl->iw_1page * p + j / 2));
				for(i = 0; i < 4 * 4; i += 4) {
					printf("%02X%02X%02X ", bl->hex_map[bl->iw_1page * 2 * p + j + i + 2], \
						bl->hex_map[bl->iw_1page * 2 * p + j + i + 1], bl->hex_map[bl->iw_1page * 2 * p + j + i]);
				}
				printf("\n");
			}
			fflush(stdout);
		}
	}
}

bool bl_construct(ty_bl * bl, uint16_t n_pages, uint16_t iw_1page, uint32_t reset) {
	if(*bl==NULL) {
		return false;
	}
	
	*bl = malloc(sizeof(ty_bl));
	(*bl)->n_pages = n_pages;
	(*bl)->iw_1page = iw_1page;
	(*bl)->reset = reset;
	(*bl)->hex_map = calloc(n_pages, 2 * iw_1page);
	(*bl)->used_pages = calloc(n_pages, sizeof(bool));
	(*bl)->page_data = calloc(1, 2 * iw_1page);
	return true;
}

void bl_destruct(ty_bl bl) {
	free(bl->hex_map);
	bl->hex_map = NULL;
	free(bl->used_pages);
	bl->used_pages = NULL;
	free(bl->page_data);
	bl->page_data = NULL;
}

bool bl_read_hex_file(ty_bl bl, char* filename) {
	// open file
	FILE *hex_file_p = fopen(filename, "rb");

	if(hex_file_p == NULL) {
		printf("Unable to open file");
		return false;
	}

	// Data per line.
	uint8_t line_of_data[256];
	uint32_t address;

	// Indices and counters
	uint8_t bytes_this_line;
	uint32_t hex_lines_in_file = hex_file_line_count(hex_file_p);

	// Indices for parsing.
	uint32_t line_index = 0;
	bool read_line_ok = false;

	// Parse all lines in file.
	while(line_index < hex_lines_in_file) {
		read_line_ok = read_line_from_hex_file(hex_file_p, line_of_data, &address, &bytes_this_line);
		if (read_line_ok) {
			memcpy(bl->hex_map + address, line_of_data, bytes_this_line);
			bl->used_pages[address / (2 * 1024)] = true;
		}
		line_index++;
	}

	// Print out parsed data.
	bl_print_map(bl);

	// close file
	fclose(hex_file_p);

	// overwrite reset instruction
	memcpy(bl->hex_map, &bl->reset, sizeof(uint32_t));

	// count used pages
	uint16_t k;
	bl->n_used_pages = 0;
	for(k = 0; k < bl->n_pages; k++) {
		if(bl->used_pages[k]) {
			bl->n_used_pages++;
		}
	}
	bl->i_page = 0;
	return true;
}

void bl_get_nextpage(ty_bl bl, uint32_t *address, uint8_t **data, float *percentage) {
	// retrive next page
	uint16_t i, p=0;
	for(i = 0; i < bl->n_pages; i++) {
		if(bl->used_pages[i]) {
			p++;
			if(p == bl->i_page) {
				break;
			}
		}
	}
	*address = p*bl->iw_1page;
	*percentage = (float)bl->i_page++ / bl->n_used_pages;
	// copy data in 24-bit format in bl.page_data and return its pointer to *data
	uint16_t j, w = 0;
	for(j = 0; j < 128 * 4 * 4; j += 4 * 4) {
		for(i = 0; i < 4 * 4; i += 4) {
			*(bl->page_data + w++) = bl->hex_map[bl->iw_1page * 2 * p + j + i + 2];
			*(bl->page_data + w++) = bl->hex_map[bl->iw_1page * 2 * p + j + i + 1];
			*(bl->page_data + w++) = bl->hex_map[bl->iw_1page * 2 * p + j + i];
		}
	}
	*data = bl->page_data;
}

