
#include "fm_low_struc.h"
#include "radio-s610.h"
#include "fm_rds.h"

void fm_rds_write_data(struct s610_radio *radio, u16 rds_data,
	fm_rds_block_type_enum blk_type, fm_host_rds_errors_enum errors)
{
	u8 *buf_ptr;
	u16 usage;
	u16 capa;

/*	API_ENTRY(radio);*/

	capa = radio->low->rds_buffer->size;
	if (radio->low->rds_buffer->outdex
			> radio->low->rds_buffer->index)
		usage = radio->low->rds_buffer->size
			- radio->low->rds_buffer->outdex
			+ radio->low->rds_buffer->index;
	else
		usage = radio->low->rds_buffer->index
			- radio->low->rds_buffer->outdex;

	if ((capa - usage) >= (HOST_RDS_BLOCK_SIZE * 4)) {
		buf_ptr = radio->low->rds_buffer->base
				+ radio->low->rds_buffer->index;

		buf_ptr[HOST_RDS_BLOCK_FMT_LSB] = (u8)(rds_data & 0xff);
		buf_ptr[HOST_RDS_BLOCK_FMT_MSB] = (u8) (rds_data >> 8);

		buf_ptr[HOST_RDS_BLOCK_FMT_STATUS] = (blk_type
				<< HOST_RDS_DATA_BLKTYPE_POSI)
				| (errors << HOST_RDS_DATA_ERRORS_POSI)
				| HOST_RDS_DATA_AVAIL_MASK;

		/* Advances the buffer's index */
		radio->low->rds_buffer->index
			+= HOST_RDS_BLOCK_SIZE;

		/* Check if the buffer's index wraps */
		if (radio->low->rds_buffer->index >=
				radio->low->rds_buffer->size) {
			radio->low->rds_buffer->index -=
				radio->low->rds_buffer->size;
		}

		if (usage >= HOST_RDS_BLOCK_SIZE)
			radio->low->fm_state.status	|= STATUS_MASK_RDS_AVA;

	}

	if (radio->low->fm_state.rds_mem_thresh != 0) {
		if (usage
				>= (radio->low->fm_state.rds_mem_thresh
					* HOST_RDS_BLOCK_SIZE)) {
			fm_set_flag_bits(radio, FLAG_BUF_FUL);
			radio->rds_cnt_mod++;
			if (!(radio->rds_cnt_mod % 100))
			APIEBUG(radio, ">N ");

			radio->rds_n_count++;
			radio->rds_new = TRUE;
			atomic_set(&radio->is_rds_new, 1);
			wake_up_interruptible(&radio->core->rds_read_queue);
		}
	}

/*	API_EXIT(radio);*/

}


u16 fm_rds_get_avail_bytes(struct s610_radio *radio)
{
	u16 avail_bytes;

	if (radio->low->rds_buffer->outdex >
			radio->low->rds_buffer->index)
		avail_bytes = (radio->low->rds_buffer->size
				- radio->low->rds_buffer->outdex
				+ radio->low->rds_buffer->index);
	else
		avail_bytes = (radio->low->rds_buffer->index
				- radio->low->rds_buffer->outdex);

	return avail_bytes;
}


bool fm_read_rds_data(struct s610_radio *radio, u8 *buffer,
		u16 *size, u16 *blocks)
{
	u16 avail_bytes;
	s16 avail_blocks;
	s16 orig_avail;
	u8 *buf_ptr;

	if (radio->low->rds_buffer == NULL) {
		*size = 0;
		if (blocks)
			*blocks = 0;
		return FALSE;
	}

	orig_avail = avail_bytes = fm_rds_get_avail_bytes(radio);

	if (avail_bytes > *size)
		avail_bytes = *size;

	avail_blocks = avail_bytes / HOST_RDS_BLOCK_SIZE;
	avail_bytes = avail_blocks * HOST_RDS_BLOCK_SIZE;

	if (avail_bytes == 0) {
		*size = 0;
		if (blocks)
			*blocks = 0;
		return FALSE;
	}

	buf_ptr = radio->low->rds_buffer->base
		+ radio->low->rds_buffer->outdex;
	(void) memcpy(buffer, buf_ptr, avail_bytes);

	/* advances the buffer's outdex */
	radio->low->rds_buffer->outdex += avail_bytes;

	/* Check if the buffer's outdex wraps */
	if (radio->low->rds_buffer->outdex >= radio->low->rds_buffer->size)
		radio->low->rds_buffer->outdex -= radio->low->rds_buffer->size;

	if (orig_avail == avail_bytes) {
		buffer[(avail_blocks - 1) * HOST_RDS_BLOCK_SIZE
			   + HOST_RDS_BLOCK_FMT_STATUS] &=
					   ~HOST_RDS_DATA_AVAIL_MASK;
		radio->low->fm_state.status &= ~STATUS_MASK_RDS_AVA;
	}

	*size = avail_bytes; /* number of bytes read */

	if (blocks)
		*blocks = avail_bytes / HOST_RDS_BLOCK_SIZE;

	/* Update RDS flags */
	if ((avail_bytes / HOST_RDS_BLOCK_SIZE)
			< radio->low->fm_state.rds_mem_thresh)
		fm_clear_flag_bits(radio, FLAG_BUF_FUL);

	return TRUE;
}

#ifdef USE_RDS_HW_DECODER
void fm_rds_change_state(struct s610_radio *radio,
		fm_rds_state_enum new_state)
{
	fm_rds_state_enum old_state =
		(fm_rds_state_enum) radio->low->fm_rds_state.current_state;

	radio->low->fm_rds_state.current_state = new_state;

	if ((old_state == RDS_STATE_FULL_SYNC)
			&& (new_state == RDS_STATE_HAVE_DATA)) {
		fm_update_rds_sync_status(radio, FALSE); /* unsynced */
	} else if ((old_state != RDS_STATE_FULL_SYNC)
			&& (new_state == RDS_STATE_FULL_SYNC)) {
		fm_update_rds_sync_status(radio, TRUE); /* synced */
	}
}
#else	/*USE_RDS_HW_DECODER*/
void fm_rds_change_state(struct s610_radio *radio,
		fm_rds_state_enum new_state)
{
	radio->low->fm_rds_state.current_state = new_state;
}

fm_rds_state_enum fm_rds_get_state(struct s610_radio *radio)
{
	return radio->low->fm_rds_state.current_state;
}
#endif	/*USE_RDS_HW_DECODER*/

void fm_rds_update_error_status(struct s610_radio *radio, u16 errors)
{
	if (errors == 0) {
		radio->low->fm_rds_state.error_bits = 0;
		radio->low->fm_rds_state.error_blks = 0;
	} else {
		radio->low->fm_rds_state.error_bits += errors;
		radio->low->fm_rds_state.error_blks++;
	}

	if (radio->low->fm_rds_state.error_blks
			>= radio->low->fm_state.rds_unsync_blk_cnt) {
		if (radio->low->fm_rds_state.error_bits
			>= radio->low->fm_state.rds_unsync_bit_cnt) {
			/* sync-loss */
#ifdef USE_RDS_HW_DECODER
			fm_rds_change_state(radio, RDS_STATE_HAVE_DATA);
#else
			fm_rds_change_state(radio, RDS_STATE_INIT);
#endif	/*USE_RDS_HW_DECODER*/
		}
		radio->low->fm_rds_state.error_bits = 0;
		radio->low->fm_rds_state.error_blks = 0;
	}
}

static fm_host_rds_errors_enum fm_rds_process_block(
		struct s610_radio *radio,
		u16 data, fm_rds_block_type_enum block_type,
		u16 err_count)
{
	fm_host_rds_errors_enum error_type;

	if (err_count == 0) {
		error_type = HOST_RDS_ERRS_NONE;
	} else if ((err_count <= 2)
			&& (err_count
			<= radio->low->fm_config.rds_error_limit)) {
		error_type = HOST_RDS_ERRS_2CORR;
	} else if ((err_count <= 5)
			&& (err_count
			<= radio->low->fm_config.rds_error_limit)) {
		error_type = HOST_RDS_ERRS_5CORR;
	} else {
		error_type = HOST_RDS_ERRS_UNCORR;
	}

	/* Write the data into the buffer */
	if ((block_type != RDS_BLKTYPE_E)
		|| (radio->low->fm_state.save_eblks)) {
		fm_rds_write_data(radio, data, block_type, error_type);
	}

	return error_type;
}

#ifdef USE_RDS_HW_DECODER
void fm_process_rds_data(struct s610_radio *radio)
{
	u32 fifo_data;
	u16 i;
	u16 avail_blocks;
	u16 data;
	u8 status;
	u16 err_count;
	fm_rds_block_type_enum block_type;

	if (!radio->low->fm_state.rds_rx_enabled)
		return;

	avail_blocks = (fmspeedy_get_reg_int(0xFFF2BF) + 1) / 4;

	for (i = 0; i < avail_blocks; i++) {
		/* Fetch the RDS word data. */
		fifo_data = fmspeedy_get_reg_int(0xFFF3C0);

		data = (u16)((fifo_data >> 16) & 0xFFFF);
		status = (u8)((fifo_data >> 8) & 0xFF);

		block_type =
			(fm_rds_block_type_enum) ((status & RDS_BLK_TYPE_MASK)
				>> RDS_BLK_TYPE_SHIFT);
		err_count = (status & RDS_ERR_CNT_MASK);

		switch (radio->low->fm_rds_state.current_state) {
		case RDS_STATE_INIT:
			fm_rds_change_state(radio, RDS_STATE_HAVE_DATA);
			break;
		case RDS_STATE_HAVE_DATA:
			if ((block_type == RDS_BLKTYPE_A)
					&& (err_count == 0)) {
				/* Move to full sync */
				fm_rds_change_state(radio,
						RDS_STATE_FULL_SYNC);
				fm_rds_process_block(radio,
						data, block_type, err_count);
			}
			break;
		case RDS_STATE_PRE_SYNC:
			break;
		case RDS_STATE_FULL_SYNC:
			if (fm_rds_process_block(radio,
				data, block_type, err_count)
				== HOST_RDS_ERRS_UNCORR) {
				fm_rds_update_error_status(radio,
				radio->low->fm_state.rds_unsync_uncorr_weight);
			} else {
				fm_rds_update_error_status(radio, err_count);
			}
			break;
		}
	}
}
#else	/*USE_RDS_HW_DECODER*/
struct fm_decoded_data {
	u16 info;
	fm_rds_block_type_enum blk_type;
	u8 err_cnt;
};

void put_bit_to_byte(u8 *fifo, u32 data)
{
	s8 i, j;
	u32 mask = 0x80000000;

	for (i = BUF_LEN - 1, j = 0; i >= 0; i--, j++) {
		*(fifo + j) = (mask & data) ? 1 : 0;
		mask = mask >> 1;
	}
}

u32 get_block_data(u8 *fifo, u32 sp)
{
	u8 i, j;
	u32 data = 0;

	data |= *(fifo + (sp++ % 160));

	for (i = BLK_LEN-1, j = 0; i > 0; i--, j++) {
		data <<= 1;
		data |= *(fifo + (sp++ % 160));
	}

	return data;
}

u8 find_code_word(u32 *data, fm_rds_block_type_enum b_type, bool seq_lock)
{
	u16 first13;
	u16 second13;
	u16 syndrome;

	first13 = *data >> 13;
	second13 = (*data & 0x1FFF) ^ OFFSET_WORD[b_type];

	syndrome = CRC_LUT[(CRC_LUT[first13] << 3) ^ second13];

	if (syndrome) {
		u32 corrected;
		u8 i, j;
		u8 maxerr = (b_type == RDS_BLKTYPE_A) ? 2 : sizeof(burstErrorPattern) / sizeof(u8);

		for (i = 0; i < maxerr; i++) {
			for (j = 0; j <= BLK_LEN - burstErrorLen[i]; j++) {
				corrected = *data ^ (burstErrorPattern[i] << j);

				first13 = corrected >> 13;
				second13 = (corrected & 0x1FFF) ^ OFFSET_WORD[b_type];

				syndrome = CRC_LUT[(CRC_LUT[first13] << 3) ^ second13];

				if (!syndrome) {
					*data = corrected;
					return burstErrorLen[i];
				}
			}
		}
	} else {
		return 0;
	}
	return 6;
}

void fm_process_rds_data(struct s610_radio *radio)
{
	u16 i, j, k, l;
	u16 avail_blocks;
	u32 fifo_data;
	u32 data;
	fm_host_rds_errors_enum err_cnt;
	u8 min_pos = 0;
	u8 min_blk = 0;
	u8 min_blk_tmp = 0;
	u8 min_pos_sum, min_blk_sum;

	static u32 idx;
	static u8 fifo[160];
	static u32 start_pos;
	static u32 end_pos;
	static bool fetch_data;
	static bool seq_lock;
	static bool remains;
	static struct fm_decoded_data decoded[BLK_LEN][RDS_NUM_BLOCK_TYPES - 1][RDS_NUM_BLOCK_TYPES - 1];

	avail_blocks = (fmspeedy_get_reg(0xFFF2BF) + 1) / 4;

	while (avail_blocks) {
		/* Fetch the RDS raw data. */
		if (fetch_data) {
			fifo_data = fmspeedy_get_reg(0xFFF3C0);
			put_bit_to_byte(fifo + 32 * ((idx++) % 5), fifo_data);
			avail_blocks--;
		}

		switch (fm_rds_get_state(radio)) {
		case RDS_STATE_INIT:
			fm_rds_change_state(radio, RDS_STATE_HAVE_DATA);
			fm_update_rds_sync_status(radio, false);
			radio->low->fm_rds_state.error_bits = 0;
			radio->low->fm_rds_state.error_blks = 0;

			idx = 0;
			start_pos = 0;
			end_pos = BLK_LEN - 1;
			fetch_data = false;
			seq_lock = false;
			memset(fifo, 0, sizeof(fifo) / sizeof(u8));

		case RDS_STATE_HAVE_DATA:
			if (idx < 5) {
				fifo_data = fmspeedy_get_reg(0xFFF3C0);
				put_bit_to_byte(fifo + 32 * ((idx++) % 5), fifo_data);
				avail_blocks--;

				if (idx == 5) {
					for (i = 0; i < BLK_LEN; i++) {
						for (j = 0; j < RDS_NUM_BLOCK_TYPES - 1; j++) {
							start_pos = i;

							for (k = 0, l = j; k < RDS_NUM_BLOCK_TYPES - 1; k++) {
								data = get_block_data(fifo, start_pos);
								err_cnt = find_code_word(&data, l, seq_lock);

								decoded[i][j][k].info = data >> 10;
								decoded[i][j][k].blk_type = l;
								decoded[i][j][k].err_cnt = err_cnt;

								start_pos += BLK_LEN;
								l = (l + 1) % (RDS_NUM_BLOCK_TYPES - 1);
							}
						}
					}

					for (i = 0, min_pos_sum = 0xFF; i < BLK_LEN; i++) {
						for (j = 0, min_blk_sum = 0xFF; j < RDS_NUM_BLOCK_TYPES - 1; j++) {
							for (k = 0, err_cnt = 0; k < RDS_NUM_BLOCK_TYPES - 1; k++) {
								err_cnt += decoded[i][j][k].err_cnt;
							}

							if (min_blk_sum > err_cnt) {
								min_blk_sum = err_cnt;
								min_blk_tmp = j;
							}
						}

						if (min_pos_sum > min_blk_sum) {
							min_pos_sum = min_blk_sum;
							min_pos = i;
							min_blk = min_blk_tmp;
						}

					}

					fm_rds_change_state(radio, RDS_STATE_PRE_SYNC);
				} else
					break;
			}

		case RDS_STATE_PRE_SYNC:
			for (i = 0; i < RDS_NUM_BLOCK_TYPES - 1; i++) {
				if (decoded[min_pos][min_blk][i].blk_type == RDS_BLKTYPE_A) {
					fm_update_rds_sync_status(radio, TRUE);
				}

				if (fm_get_rds_sync_status(radio)) {

					if (fm_rds_process_block(radio, decoded[min_pos][min_blk][i].info,
							decoded[min_pos][min_blk][i].blk_type, decoded[min_pos][min_blk][i].err_cnt)
							== HOST_RDS_ERRS_UNCORR) {
						fm_rds_update_error_status(radio,
								radio->low->fm_state.rds_unsync_uncorr_weight);
					} else {
						fm_rds_update_error_status(radio, decoded[min_pos][min_blk][i].err_cnt);
					}
				}
			}

			start_pos = min_pos + BLK_LEN * 3;
			end_pos = start_pos + BLK_LEN - 1;

			fm_rds_change_state(radio, (min_blk + 3) % (RDS_NUM_BLOCK_TYPES - 1));
			seq_lock = false;
			remains = true;


		case RDS_STATE_FOUND_BL_A:
		case RDS_STATE_FOUND_BL_B:
		case RDS_STATE_FOUND_BL_C:
		case RDS_STATE_FOUND_BL_D:
			if ((end_pos / BUF_LEN != (end_pos + BLK_LEN) / BUF_LEN) && !fetch_data && !remains) {
				fetch_data = true;
			} else {
				if (end_pos + BLK_LEN >= 160 && remains) {
					remains = false;
					fetch_data = true;
					break;
				}

				start_pos += BLK_LEN;
				end_pos += BLK_LEN;

				data = get_block_data(fifo, start_pos);
				fetch_data = false;

				i = (fm_rds_get_state(radio) + 1) % (RDS_NUM_BLOCK_TYPES - 1);
				fm_rds_change_state(radio, i);
				err_cnt = find_code_word(&data, i, seq_lock);

				if (fm_rds_process_block(radio, data >> 10, i, err_cnt) == HOST_RDS_ERRS_UNCORR) {
					fm_rds_update_error_status(radio, radio->low->fm_state.rds_unsync_uncorr_weight);
				} else {
					fm_rds_update_error_status(radio, err_cnt);
				}
			}
			break;

		default:
			break;
		}
	}
}
#endif	/*USE_RDS_HW_DECODER*/