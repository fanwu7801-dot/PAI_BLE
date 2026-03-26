#ifndef _BLE_FMY_FMNA_H
#define _BLE_FMY_FMNA_H
int fmy_enable(bool en);

void fmy_open_close_pairing_mode(bool en);

bool fmy_get_pair_state(void);

void fmy_factory_reset(void);

u16 ble_fmy_get_con_handle(void);

void fmy_suffix_name_set(void);
void fmy_original_name_set(void);
bool fmy_is_owner_pair_set(void);
void fmy_owner_info_revert_back(void);
bool fmy_is_owner_address(uint8_t *addr_in);
#endif

