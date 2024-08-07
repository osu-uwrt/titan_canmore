#include "canmore/reg_mapped/client.h"
#include "canmore/reg_mapped/protocol.h"

#include <string.h>

int reg_mapped_client_read_register(const reg_mapped_client_cfg_t *cfg, uint8_t page, uint8_t offset,
                                    uint32_t *data_out) {
    reg_mapped_request_t req = { .read_pkt = {
                                     .flags = { .f = { .write = false, .mode = cfg->control_interface_mode } },
                                     .count = 0,
                                     .page = page,
                                     .offset = offset } };

    if (!cfg->clear_rx_func(cfg->arg)) {
        return REG_MAPPED_CLIENT_RESULT_RX_CLEAR_FAIL;
    }

    if (!cfg->tx_func(req.data, sizeof(req.read_pkt), cfg->arg)) {
        return REG_MAPPED_CLIENT_RESULT_TX_FAIL;
    }

    reg_mapped_response_t resp;
    if (!cfg->rx_func(resp.data, sizeof(resp.read_pkt), cfg->timeout_ms, cfg->arg)) {
        return REG_MAPPED_CLIENT_RESULT_RX_FAIL;
    }

    if (resp.read_pkt.result != REG_MAPPED_RESULT_SUCCESSFUL) {
        return resp.read_pkt.result;
    }

    *data_out = resp.read_pkt.data;
    return REG_MAPPED_RESULT_SUCCESSFUL;
}

int reg_mapped_client_write_register(const reg_mapped_client_cfg_t *cfg, uint8_t page, uint8_t offset, uint32_t data) {
    reg_mapped_request_t req = { .write_pkt = {
                                     .flags = { .f = { .write = true, .mode = cfg->control_interface_mode } },
                                     .count = 0,
                                     .page = page,
                                     .offset = offset,
                                     .data = data } };

    if (!cfg->clear_rx_func(cfg->arg)) {
        return REG_MAPPED_CLIENT_RESULT_RX_CLEAR_FAIL;
    }

    if (!cfg->tx_func(req.data, sizeof(req.write_pkt), cfg->arg)) {
        return REG_MAPPED_CLIENT_RESULT_TX_FAIL;
    }

    reg_mapped_response_t resp;
    if (!cfg->rx_func(resp.data, sizeof(resp.write_pkt), cfg->timeout_ms, cfg->arg)) {
        return REG_MAPPED_CLIENT_RESULT_RX_FAIL;
    }

    return resp.write_pkt.result;
}

int reg_mapped_client_write_array(const reg_mapped_client_cfg_t *cfg, uint8_t page, uint8_t offset_start,
                                  const uint32_t *data_array, uint8_t num_words) {
    // Make sure we don't cross the page boundary
    if (((unsigned int) offset_start) + num_words > REG_MAPPED_PAGE_NUM_WORDS) {
        return REG_MAPPED_CLIENT_RESULT_INVALID_ARG;
    }

    if (!cfg->clear_rx_func(cfg->arg)) {
        return REG_MAPPED_CLIENT_RESULT_RX_CLEAR_FAIL;
    }

#if !CANMORE_CONFIG_DISABLE_MULTIWORD
    if (cfg->transfer_mode == TRANSFER_MODE_MULTIWORD) {
        reg_mapped_request_t *req = (reg_mapped_request_t *) cfg->multiword_scratch_buffer;

        // Compute batch size, and make sure the request is capable of carrying at least 1 word
        if (cfg->multiword_scratch_len < sizeof(struct reg_mapped_multiword_write_request)) {
            return REG_MAPPED_CLIENT_RESULT_MULTIWORD_ALLOC_TOO_SMALL;
        }
        size_t max_req_count = REG_MAPPED_COMPUTE_MAX_REQ_WORD_COUNT(cfg->multiword_scratch_len);
        if (max_req_count < 1) {
            return REG_MAPPED_CLIENT_RESULT_MULTIWORD_ALLOC_TOO_SMALL;
        }

        // This part of the request remains constant
        req->multiword_write_pkt.flags.data = 0;
        req->multiword_write_pkt.flags.f.write = true;
        req->multiword_write_pkt.flags.f.multiword = true;
        req->multiword_write_pkt.flags.f.mode = cfg->control_interface_mode;
        req->multiword_write_pkt.page = page;

        uint8_t offset = offset_start;
        uint8_t remaining_words = num_words;
        while (remaining_words > 0) {
            uint8_t count = (remaining_words > max_req_count ? max_req_count : remaining_words);

            // Construct request
            req->multiword_write_pkt.count = count;
            req->multiword_write_pkt.offset = offset;
            memcpy(req->multiword_write_pkt.data, &data_array[offset], count * sizeof(*data_array));

            // Send this packet
            if (!cfg->tx_func(req->data, REG_MAPPED_COMPUTE_MULTIWORD_REQ_LEN(count), cfg->arg)) {
                return REG_MAPPED_CLIENT_RESULT_TX_FAIL;
            }

            // Get the response
            reg_mapped_response_t resp;
            if (!cfg->rx_func(resp.data, sizeof(resp.write_pkt), cfg->timeout_ms, cfg->arg)) {
                return REG_MAPPED_CLIENT_RESULT_RX_FAIL;
            }

            // Check for errors in the response
            if (resp.write_pkt.result != REG_MAPPED_RESULT_SUCCESSFUL) {
                return resp.write_pkt.result;
            }

            // Note this packet and keep transferring
            offset += count;
            remaining_words -= count;
        }
    }
    else
#endif
        if (cfg->transfer_mode == TRANSFER_MODE_BULK) {
        reg_mapped_request_t req = { .write_pkt = {
                                         .flags = { .f = { .write = true,
                                                           .bulk_req = true,
                                                           .mode = cfg->control_interface_mode } },
                                         .count = 0,
                                         .page = page,
                                         .offset = offset_start,
                                     } };

        // Loop until no words left to read
        while (num_words--) {
            if (!num_words || req.write_pkt.count + 1u >= cfg->max_in_flight) {
                // If we don't have any more data to send, or we've hit the max number of packets in flight,
                // end the bulk transfer and wait for a response
                req.write_pkt.flags.f.bulk_end = true;
            }

            req.write_pkt.data = *data_array++;

            if (!cfg->tx_func(req.data, sizeof(req.write_pkt), cfg->arg)) {
                return REG_MAPPED_CLIENT_RESULT_TX_FAIL;
            }

            req.write_pkt.count++;
            req.write_pkt.offset++;

            if (req.write_pkt.flags.f.bulk_end) {
                // If this is a bulk end packet, get the response back
                reg_mapped_response_t resp;
                if (!cfg->rx_func(resp.data, sizeof(resp.write_bulk_pkt), cfg->timeout_ms, cfg->arg)) {
                    return REG_MAPPED_CLIENT_RESULT_RX_FAIL;
                }

                // Check for errors in the response
                if (resp.write_bulk_pkt.result != REG_MAPPED_RESULT_SUCCESSFUL) {
                    return resp.write_bulk_pkt.result;
                }

                if ((resp.write_bulk_pkt.seq_no + 1) != req.write_pkt.count) {
                    return REG_MAPPED_CLIENT_RESULT_INVALID_BULK_COUNT;
                }

                // Reset the packet for the next bulk transfer
                req.write_pkt.count = 0;
                req.write_pkt.flags.f.bulk_end = false;
            }
        }
    }
    else {
        // TRANSFER_MODE_SINGLE

        uint8_t offset = offset_start;
        for (int i = 0; i < num_words; i++) {
            int ret = reg_mapped_client_write_register(cfg, page, offset++, *data_array++);
            if (ret != REG_MAPPED_RESULT_SUCCESSFUL) {
                return ret;
            }
        }
    }

    return REG_MAPPED_RESULT_SUCCESSFUL;
}

int reg_mapped_client_read_array(const reg_mapped_client_cfg_t *cfg, uint8_t page, uint8_t offset_start,
                                 uint32_t *data_array, unsigned int num_words) {
    // Make sure we don't cross the page boundary
    if (((unsigned int) offset_start) + num_words > REG_MAPPED_PAGE_NUM_WORDS) {
        return REG_MAPPED_CLIENT_RESULT_INVALID_ARG;
    }

#if !CANMORE_CONFIG_DISABLE_MULTIWORD
    if (cfg->transfer_mode == TRANSFER_MODE_MULTIWORD) {
        reg_mapped_request_t req;
        reg_mapped_response_t *resp = (reg_mapped_response_t *) cfg->multiword_scratch_buffer;

        // Compute batch size, and make sure the response is capable of carrying at least 1 word
        if (cfg->multiword_scratch_len < sizeof(struct reg_mapped_multiword_read_response)) {
            return REG_MAPPED_CLIENT_RESULT_MULTIWORD_ALLOC_TOO_SMALL;
        }
        size_t max_req_count = REG_MAPPED_COMPUTE_MAX_RESP_WORD_COUNT(cfg->multiword_scratch_len);
        if (max_req_count < 1) {
            return REG_MAPPED_CLIENT_RESULT_MULTIWORD_ALLOC_TOO_SMALL;
        }

        // This part of the request remains constant
        req.read_pkt.flags.data = 0;
        req.read_pkt.flags.f.multiword = true;
        req.read_pkt.flags.f.mode = cfg->control_interface_mode;
        req.read_pkt.page = page;

        uint8_t offset = offset_start;
        uint8_t remaining_words = num_words;
        while (remaining_words > 0) {
            uint8_t count = (remaining_words > max_req_count ? max_req_count : remaining_words);

            // Construct request
            req.read_pkt.count = count;
            req.read_pkt.offset = offset;

            // Send this packet
            if (!cfg->tx_func(req.data, sizeof(req.read_pkt), cfg->arg)) {
                return REG_MAPPED_CLIENT_RESULT_TX_FAIL;
            }

            // Get the response
            size_t resp_len = REG_MAPPED_COMPUTE_MULTIWORD_RESP_LEN(count);
            if (!cfg->rx_func(resp->data, resp_len, cfg->timeout_ms, cfg->arg)) {
                return REG_MAPPED_CLIENT_RESULT_RX_FAIL;
            }

            // Check for errors in the response
            if (resp->multiword_read_pkt.result != REG_MAPPED_RESULT_SUCCESSFUL) {
                return resp->multiword_read_pkt.result;
            }

            // Copy the data into the output buffer
            memcpy(&data_array[offset], resp->multiword_read_pkt.data, count * sizeof(*data_array));

            // Note this packet and keep transferring
            offset += count;
            remaining_words -= count;
        }
    }
    else {
#endif
        // TRANSFER_MODE_BULK does not support optimized reads
        // Fallback to single transfers
        uint8_t offset = offset_start;
        for (unsigned int i = 0; i < num_words; i++) {
            int ret = reg_mapped_client_read_register(cfg, page, offset++, data_array++);
            if (ret != REG_MAPPED_RESULT_SUCCESSFUL) {
                return ret;
            }
        }
#if !CANMORE_CONFIG_DISABLE_MULTIWORD
    }
#endif

    return REG_MAPPED_RESULT_SUCCESSFUL;
}

int reg_mapped_client_read_string_page(const reg_mapped_client_cfg_t *cfg, uint8_t page_num, char *str_out,
                                       size_t max_len) {
    // We unfortunately have to do a slow word-by-word read since the length is determined by the location of the null
    // termination

    // To avoid unnecessary reading of words at max_len, we assume str_out can hold at least one character
    // So this has to be checked here
    if (max_len < 2) {
        if (max_len > 0) {
            str_out[0] = 0;
        }
        return 0;
    }

    // Limit to read up to an entire page
    unsigned int word_num = 0;
    for (word_num = 0; word_num < REG_MAPPED_PAGE_NUM_WORDS; word_num++) {
        union {
            uint32_t word;
            uint8_t bytes[4];
        } read_data;

        int result = reg_mapped_client_read_register(cfg, page_num, word_num, &read_data.word);
        if (result != REG_MAPPED_RESULT_SUCCESSFUL) {
            return (result < 0 ? result : -result);
        }

        for (unsigned int i = 0; i < sizeof(read_data.bytes); i++) {
            size_t str_offset = word_num * 4 + i;
            str_out[str_offset] = read_data.bytes[i];

            if (read_data.bytes[i] == 0) {
                // Read successful, good to exit
                return str_offset;
            }

            // Check if we filled the buffer and if so, return
            str_offset++;
            if (str_offset + 1 == max_len) {
                str_out[str_offset] = 0;
                return str_offset;
            }
        }
    }

    // Already checked length, we should be good to just write the terminator
    str_out[word_num * 4] = 0;
    return word_num * 4;
}
