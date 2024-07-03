#include "canmore/reg_mapped/protocol.h"
#include "canmore/reg_mapped/server.h"

#include <string.h>

static uint8_t reg_mapped_server_handle_single_write(reg_mapped_server_inst_t *inst,
                                                     const struct reg_mapped_write_request *req) {
    if (req->page >= inst->num_pages) {
        return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
    }

    const reg_mapped_server_page_def_t *page = &inst->page_array[req->page];

    if (page->page_type == PAGE_TYPE_MEMORY_MAPPED_WORD) {
        if (req->offset >= page->type.mem_mapped_word.num_words) {
            return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
        }

        if (page->type.mem_mapped_word.perm != REGISTER_PERM_READ_WRITE &&
            page->type.mem_mapped_word.perm != REGISTER_PERM_WRITE_ONLY) {
            return REG_MAPPED_RESULT_INVALID_REGISTER_MODE;
        }

        page->type.mem_mapped_word.base_addr[req->offset] = req->data;

        return REG_MAPPED_RESULT_SUCCESSFUL;
    }
    else if (page->page_type == PAGE_TYPE_MEMORY_MAPPED_BYTE) {
        const unsigned int word_size = sizeof(req->data);

        // If the request is completely outside of the region, return invalid address
        if (req->offset * word_size >= page->type.mem_mapped_byte.size) {
            return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
        }

        if (page->type.mem_mapped_byte.perm != REGISTER_PERM_READ_WRITE &&
            page->type.mem_mapped_byte.perm != REGISTER_PERM_WRITE_ONLY) {
            return REG_MAPPED_RESULT_INVALID_REGISTER_MODE;
        }

        // Write the word
        // Note that this is done to ensure that it can be written if byte buffer length isn't divisible by 4
        // Also necessary in the event the buffer is not word aligned
        int extra_bytes = ((req->offset * word_size) + word_size) - page->type.mem_mapped_byte.size;
        if (extra_bytes < 0)
            extra_bytes = 0;
        uint32_t write_word = req->data;
        size_t write_offset = req->offset * word_size;
        for (unsigned int i = 0; i < (word_size - extra_bytes); i++) {
            page->type.mem_mapped_byte.base_addr[write_offset++] = write_word;
            write_word >>= 8;
        }

        return REG_MAPPED_RESULT_SUCCESSFUL;
    }
    else if (page->page_type == PAGE_TYPE_REGISTER_MAPPED) {
        if (req->offset >= page->type.reg_mapped.num_registers) {
            return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
        }

        const reg_mapped_server_register_def_t *reg = &page->type.reg_mapped.reg_array[req->offset];

        if (reg->reg_type == REGISTER_TYPE_MEMORY) {
            if (reg->type.memory.perm != REGISTER_PERM_READ_WRITE &&
                reg->type.memory.perm != REGISTER_PERM_WRITE_ONLY) {
                return REG_MAPPED_RESULT_INVALID_REGISTER_MODE;
            }

            *reg->type.memory.reg_ptr = req->data;

            return REG_MAPPED_RESULT_SUCCESSFUL;
        }
        else if (reg->reg_type == REGISTER_TYPE_EXEC) {
            if (reg->type.exec.perm != REGISTER_PERM_READ_WRITE && reg->type.exec.perm != REGISTER_PERM_WRITE_ONLY) {
                return REG_MAPPED_RESULT_INVALID_REGISTER_MODE;
            }

            uint32_t data = req->data;
            if (reg->type.exec.callback(reg, true, &data)) {
                return REG_MAPPED_RESULT_SUCCESSFUL;
            }
            else {
                return REG_MAPPED_RESULT_INVALID_DATA;
            }
        }
        else {
            // register type is REGISTER_TYPE_UNIMPLEMENTED
            return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
        }
    }
    else {
        // page_type is PAGE_TYPE_UNIMPLEMENTED
        return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
    }
}

static uint8_t reg_mapped_server_handle_single_read(reg_mapped_server_inst_t *inst,
                                                    const struct reg_mapped_read_request *req, uint32_t *data_out) {
    if (req->page >= inst->num_pages) {
        return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
    }

    const reg_mapped_server_page_def_t *page = &inst->page_array[req->page];

    if (page->page_type == PAGE_TYPE_MEMORY_MAPPED_WORD) {
        if (req->offset >= page->type.mem_mapped_word.num_words) {
            return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
        }

        if (page->type.mem_mapped_word.perm != REGISTER_PERM_READ_WRITE &&
            page->type.mem_mapped_word.perm != REGISTER_PERM_READ_ONLY) {
            return REG_MAPPED_RESULT_INVALID_REGISTER_MODE;
        }

        *data_out = page->type.mem_mapped_word.base_addr[req->offset];

        return REG_MAPPED_RESULT_SUCCESSFUL;
    }
    else if (page->page_type == PAGE_TYPE_MEMORY_MAPPED_BYTE) {
        const unsigned int word_size = sizeof(*data_out);

        // If the request is completely outside of the region, return invalid address
        if (req->offset * word_size >= page->type.mem_mapped_byte.size) {
            return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
        }

        if (page->type.mem_mapped_byte.perm != REGISTER_PERM_READ_WRITE &&
            page->type.mem_mapped_byte.perm != REGISTER_PERM_READ_ONLY) {
            return REG_MAPPED_RESULT_INVALID_REGISTER_MODE;
        }

        // Handle unaligned buffers or reads at the end of the buffer
        int required_padding = ((req->offset * word_size) + word_size) - page->type.mem_mapped_byte.size;
        if (required_padding < 0)
            required_padding = 0;
        uint32_t partial_read = 0;
        size_t read_offset = req->offset * word_size;
        for (unsigned int i = 0; i < (word_size - required_padding); i++) {
            partial_read |= page->type.mem_mapped_byte.base_addr[read_offset++] << (8 * i);
        }

        *data_out = partial_read;

        return REG_MAPPED_RESULT_SUCCESSFUL;
    }
    else if (page->page_type == PAGE_TYPE_REGISTER_MAPPED) {
        if (req->offset >= page->type.reg_mapped.num_registers) {
            return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
        }

        const reg_mapped_server_register_def_t *reg = &page->type.reg_mapped.reg_array[req->offset];

        if (reg->reg_type == REGISTER_TYPE_MEMORY) {
            if (reg->type.memory.perm != REGISTER_PERM_READ_WRITE && reg->type.memory.perm != REGISTER_PERM_READ_ONLY) {
                return REG_MAPPED_RESULT_INVALID_REGISTER_MODE;
            }

            *data_out = *reg->type.memory.reg_ptr;

            return REG_MAPPED_RESULT_SUCCESSFUL;
        }
        else if (reg->reg_type == REGISTER_TYPE_EXEC) {
            if (reg->type.exec.perm != REGISTER_PERM_READ_WRITE && reg->type.exec.perm != REGISTER_PERM_READ_ONLY) {
                return REG_MAPPED_RESULT_INVALID_REGISTER_MODE;
            }

            if (reg->type.exec.callback(reg, false, data_out)) {
                return REG_MAPPED_RESULT_SUCCESSFUL;
            }
            else {
                return REG_MAPPED_RESULT_INVALID_DATA;
            }
        }
        else {
            // register type is REGISTER_TYPE_UNIMPLEMENTED
            return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
        }
    }
    else {
        // page_type is PAGE_TYPE_UNIMPLEMENTED
        return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
    }
}

#if !CANMORE_CONFIG_DISABLE_MULTIWORD

static uint8_t reg_mapped_server_handle_multiword_write(reg_mapped_server_inst_t *inst,
                                                        const struct reg_mapped_multiword_write_request *req) {
    if (req->page >= inst->num_pages) {
        return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
    }

    const reg_mapped_server_page_def_t *page = &inst->page_array[req->page];
    const size_t word_size = sizeof(req->data[0]);

    if (page->page_type == PAGE_TYPE_MEMORY_MAPPED_WORD) {
        if (((unsigned int) req->offset) + ((unsigned int) req->count) > page->type.mem_mapped_word.num_words) {
            return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
        }

        if (page->type.mem_mapped_word.perm != REGISTER_PERM_READ_WRITE &&
            page->type.mem_mapped_word.perm != REGISTER_PERM_WRITE_ONLY) {
            return REG_MAPPED_RESULT_INVALID_REGISTER_MODE;
        }

        // Request good and fits into the area, copy it in
        memcpy(&page->type.mem_mapped_word.base_addr[req->offset], req->data, req->count * word_size);

        return REG_MAPPED_RESULT_SUCCESSFUL;
    }
    else if (page->page_type == PAGE_TYPE_MEMORY_MAPPED_BYTE) {
        size_t byte_offset = req->offset * word_size;
        size_t copy_len = req->count * word_size;

        // If the request tries to write completely outside of the region, return invalid address
        if (byte_offset + copy_len >= page->type.mem_mapped_byte.size + word_size) {
            return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
        }
        // However, we will allow writing partial words (if the byte region is 7 bytes, 2 words/8 bytes can be written)
        else if (byte_offset + copy_len > page->type.mem_mapped_byte.size) {
            // Fix up copy_len so we don't try to copy outside the buffer
            copy_len = page->type.mem_mapped_byte.size - byte_offset;
        }

        if (page->type.mem_mapped_byte.perm != REGISTER_PERM_READ_WRITE &&
            page->type.mem_mapped_byte.perm != REGISTER_PERM_WRITE_ONLY) {
            return REG_MAPPED_RESULT_INVALID_REGISTER_MODE;
        }

        // Everything looks good, copy it in
        memcpy(&page->type.mem_mapped_byte.base_addr[byte_offset], req->data, copy_len);

        return REG_MAPPED_RESULT_SUCCESSFUL;
    }
    else {
        // page_type is PAGE_TYPE_REGISTER_MAPPED or PAGE_TYPE_UNIMPLEMENTED
        // Can't do register mapped for multiword, it doesn't make much sense since register mapped can be sparse
        return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
    }
}

static uint8_t reg_mapped_server_handle_multiword_read(reg_mapped_server_inst_t *inst,
                                                       const struct reg_mapped_read_request *req, void *data_out) {
    // NOTE: data_out is not gaurenteed to be aligned!
    // Do not cast data_out to a uint32_t pointer!

    if (req->page >= inst->num_pages) {
        return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
    }

    const reg_mapped_server_page_def_t *page = &inst->page_array[req->page];
    const size_t word_size = sizeof(uint32_t);

    if (page->page_type == PAGE_TYPE_MEMORY_MAPPED_WORD) {
        if (((unsigned int) req->offset) + ((unsigned int) req->count) > page->type.mem_mapped_word.num_words) {
            return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
        }

        if (page->type.mem_mapped_word.perm != REGISTER_PERM_READ_WRITE &&
            page->type.mem_mapped_word.perm != REGISTER_PERM_READ_ONLY) {
            return REG_MAPPED_RESULT_INVALID_REGISTER_MODE;
        }

        // Everything looks good, copy it in
        memcpy(data_out, &page->type.mem_mapped_word.base_addr[req->offset], req->count * word_size);

        return REG_MAPPED_RESULT_SUCCESSFUL;
    }
    else if (page->page_type == PAGE_TYPE_MEMORY_MAPPED_BYTE) {
        size_t byte_offset = req->offset * word_size;
        size_t copy_len = req->count * word_size;

        // If the request tries to read completely outside of the region, return invalid address
        if (byte_offset + copy_len >= page->type.mem_mapped_byte.size + word_size) {
            return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
        }
        // However, we will allow reading partial words (if the byte region is 7 bytes, 2 words/8 bytes can be written)
        else if (byte_offset + copy_len > page->type.mem_mapped_byte.size) {
            // Fix up copy_len so we don't try to copy outside the buffer
            copy_len = page->type.mem_mapped_byte.size - byte_offset;
        }

        if (page->type.mem_mapped_byte.perm != REGISTER_PERM_READ_WRITE &&
            page->type.mem_mapped_byte.perm != REGISTER_PERM_READ_ONLY) {
            return REG_MAPPED_RESULT_INVALID_REGISTER_MODE;
        }

        // Clear the last word in data_out since memcpy may only be a parital write, don't want any old data remaining
        if (copy_len % 4 != 0) {
            memset((void *) (((uintptr_t) data_out) + ((req->count - 1) * word_size)), 0, word_size);
        }

        // Everything looks good, copy it in
        memcpy(data_out, &page->type.mem_mapped_byte.base_addr[byte_offset], copy_len);

        return REG_MAPPED_RESULT_SUCCESSFUL;
    }
    else {
        // page_type is PAGE_TYPE_REGISTER_MAPPED or PAGE_TYPE_UNIMPLEMENTED
        // Can't do register mapped for multiword, it doesn't make much sense since register mapped can be sparse
        return REG_MAPPED_RESULT_INVALID_REGISTER_ADDRESS;
    }
}

#endif

void reg_mapped_server_handle_request(reg_mapped_server_inst_t *inst, const uint8_t *msg, size_t len) {
    if (len < 1) {
        // If request is empty, just return
        // There isn't enough data to determine what format the error repsonse should be sent in
        return;
    }

    // Decode flags
    union reg_mapped_request_flags flags = { .data = msg[0] };
    bool request_type_write = (flags.f.write ? true : false);
    bool request_type_bulk = (flags.f.bulk_req ? true : false);
    bool request_type_bulk_end = (flags.f.bulk_end ? true : false);
    bool request_type_multiword = (flags.f.multiword ? true : false);

    // Initialize decode variables
    const reg_mapped_request_t *req = (const reg_mapped_request_t *) msg;
    uint8_t result_code = REG_MAPPED_RESULT_MALFORMED_REQUEST;
    uint32_t read_data = 0;
    reg_mapped_response_t response = { 0 };
    size_t response_size;

    // Check message length
    size_t expected_msg_len;
    if (!request_type_write) {
        // All read requests are the same length
        expected_msg_len = sizeof(req->read_pkt);
    }
    else if (request_type_multiword) {
        // Must be a multiword write, computation is a little more advanced

        // Need to make sure the message is large enough to read the header before we start accessing members
        size_t hdr_len = sizeof(req->multiword_write_pkt);
        if (len < hdr_len) {
            result_code = REG_MAPPED_RESULT_MALFORMED_REQUEST;
            goto finish_request;
        }

        // Compute the real expected message length: header + count * wordlen
        expected_msg_len = hdr_len + sizeof(req->multiword_write_pkt.data[0]) * req->multiword_write_pkt.count;
    }
    else {
        // Non-multiword write
        expected_msg_len = sizeof(req->write_pkt);
    }

    if (len != expected_msg_len) {
        result_code = REG_MAPPED_RESULT_MALFORMED_REQUEST;
        goto finish_request;
    }

    if (inst->in_bulk_request && !request_type_bulk) {
        result_code = REG_MAPPED_RESULT_BULK_REQUEST_SEQ_ERROR;
        inst->in_bulk_request = false;  // If we get a non-bulk request in bulk mode, exit now
        goto finish_request;
    }

    if (request_type_multiword && request_type_bulk) {
        // Bulk and multiword requests are mutually exclusive
        result_code = REG_MAPPED_RESULT_MALFORMED_REQUEST;
        goto finish_request;
    }

    if (request_type_bulk && !request_type_write) {
        // Bulk requests are only supported on write requests
        result_code = REG_MAPPED_RESULT_MALFORMED_REQUEST;
        goto finish_request;
    }

    if (request_type_bulk_end && !request_type_bulk) {
        // Bulk end can only be sent if its a bulk request
        result_code = REG_MAPPED_RESULT_MALFORMED_REQUEST;
        goto finish_request;
    }

    if (flags.f.mode != inst->control_interface_mode) {
        // Make sure this request is using our mode map (in the event a stray request from another mode gets to us)
        result_code = REG_MAPPED_RESULT_INVALID_MODE;
        goto finish_request;
    }

    if (request_type_multiword) {
#if CANMORE_CONFIG_DISABLE_MULTIWORD
        result_code = REG_MAPPED_RESULT_MULTIWORD_UNSUPPORTED;
#else
        if (!inst->multiword_resp_buffer) {
            // If no multiword response buffer is provided, then we can't support multiword mode
            result_code = REG_MAPPED_RESULT_MULTIWORD_UNSUPPORTED;
        }
        // Multiword Request Handling
        else if (request_type_write) {
            result_code = reg_mapped_server_handle_multiword_write(inst, &req->multiword_write_pkt);
        }
        else {
            if (req->read_pkt.count > inst->multiword_resp_buffer_max_count) {
                // If trying to read larger than the provided buffer to the instance, return an error
                result_code = REG_MAPPED_RESULT_MULTIWORD_TOO_LARGE;
            }
            else {
                result_code = reg_mapped_server_handle_multiword_read(
                    inst, &req->read_pkt, inst->multiword_resp_buffer->multiword_read_pkt.data);
            }
        }
#endif
    }
    else {
        // Single & Bulk Requests

        if (request_type_bulk) {
            // Bulk request handling
            if (!inst->in_bulk_request) {
                // Handle starting new bulk request
                if (req->write_pkt.count != 0) {
                    // Bulk requests must start with sequence number 0
                    result_code = REG_MAPPED_RESULT_BULK_REQUEST_SEQ_ERROR;
                    goto finish_request;
                }

                inst->in_bulk_request = true;
                inst->bulk_error_code = 0;
                inst->bulk_last_seq_num = 0;
            }
            else {
                // Check bulk request sequence number (but only if an error is not set as this will overwrite the
                // last sequence number with the error)
                if (inst->bulk_error_code == 0) {
                    inst->bulk_last_seq_num++;

                    if (req->write_pkt.count != inst->bulk_last_seq_num) {
                        result_code = REG_MAPPED_RESULT_BULK_REQUEST_SEQ_ERROR;
                        goto finish_request;
                    }
                }
            }
        }

        // If normal request or a bulk request without an error set, perform the transfer
        if (!inst->in_bulk_request || inst->bulk_error_code == 0) {
            if (request_type_write) {
                result_code = reg_mapped_server_handle_single_write(inst, &req->write_pkt);
            }
            else {
                result_code = reg_mapped_server_handle_single_read(inst, &req->read_pkt, &read_data);
            }
        }
    }

finish_request:
    if (request_type_bulk) {
        // Handle last transfer in request
        if (request_type_bulk_end) {
            if (inst->bulk_error_code != 0) {
                response.write_bulk_pkt.result = inst->bulk_error_code;
            }
            else {
                response.write_bulk_pkt.result = result_code;
            }
            response.write_bulk_pkt.seq_no = inst->bulk_last_seq_num;
            response_size = sizeof(response.write_bulk_pkt);

            // Send response and exit bulk request mode
#if CANMORE_CONFIG_DISABLE_REG_MAPPED_ARG
            inst->tx_func(response.data, response_size);
#else
            inst->tx_func(response.data, response_size, inst->arg);
#endif
            inst->in_bulk_request = false;
        }
        // If it's not the last transfer, store error if occurs in request (but only if it hasn't already errored)
        else if (inst->bulk_error_code == 0) {
            inst->bulk_error_code = result_code;
        }
    }
    else {
        uint8_t *response_ptr;

#if !CANMORE_CONFIG_DISABLE_MULTIWORD
        if (request_type_multiword && !request_type_write && result_code == REG_MAPPED_RESULT_SUCCESSFUL) {
            // If this is a successful multiword read, we need to send the preallocated response buffer (rather than the
            // tiny buffer inside our stack)
            inst->multiword_resp_buffer->multiword_read_pkt.result = result_code;
            // Data is already filled out

            // Set pointers
            response_ptr = inst->multiword_resp_buffer->data;
            response_size = REG_MAPPED_COMPUTE_MULTIWORD_RESP_LEN(req->read_pkt.count);
        }
        else {
#endif
            // Everything else comes from the small response in the stack
            response_ptr = response.data;

            if (request_type_write) {
                response.write_pkt.result = result_code;
                response_size = sizeof(response.write_pkt);
            }
            else {
                response.read_pkt.result = result_code;
                response.read_pkt.data = read_data;
                response_size = sizeof(response.read_pkt);
            }
#if !CANMORE_CONFIG_DISABLE_MULTIWORD
        }
#endif

#if CANMORE_CONFIG_DISABLE_REG_MAPPED_ARG
        inst->tx_func(response_ptr, response_size);
#else
        inst->tx_func(response_ptr, response_size, inst->arg);
#endif
    }
}
