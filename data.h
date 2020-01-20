typedef struct s_bl *ty_bl;

bool bl_construct(ty_bl * bl, uint16_t n_pages, uint16_t iw_1page, uint32_t reset);
void bl_destruct(ty_bl bl);
bool bl_read_hex_file(ty_bl bl, char* filename);
void bl_get_nextpage(ty_bl bl, uint32_t *address, uint8_t **data, float *percentage);
