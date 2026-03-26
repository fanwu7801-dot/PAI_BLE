

SECTIONS
{
    .data : ALIGN(4)
    {
        update_data_start = .;
        update_data_end = .;
    } > ram0

    .bss (NOLOAD) :ALIGN(4)
    {
        update_bss_start = .;

        *(.update_bss)
        . = ALIGN(4);
        update_bss_end = .;
    } > ram0

    .text : ALIGN(4)
    {
		update_code_start = .;

		*(.bt_updata_ram_code)
		*(.update_const)
		*(.update_code)

        . = ALIGN(4);
		update_code_end = .;
    } > code0

	.data_code ALIGN(32):
	{
        update_data_text_start = .;
        update_data_text_end = .;
	} > ram0

    UPDATE_CODE_TOTAL_SIZE = update_code_end - update_code_start;
}


update_ld_data_size = update_data_end - update_data_start;
update_ld_bss_size = update_bss_end - update_bss_start;
update_ld_text_size = update_code_end - update_code_start;
update_ld_data_text_size = update_data_text_end - update_data_text_start;

update_ld_code_size = update_ld_data_size + update_ld_text_size + update_ld_data_text_size;
update_ld_ram_size = update_ld_data_size + update_ld_bss_size + update_ld_data_text_size;

