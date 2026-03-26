/********************************************************************************
** Copyright <2022>[-<2023>]                                                    *
** Licensed under the Unisoc General Software License                           *
** version 1.0 (the License);                                                   *
** you may not use this file except in compliance with the License              *
** You may obtain a copy of the License at                                      *
** https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US       *
** Software distributed under the License is distributed on an "AS IS" BASIS,   *
** WITHOUT WARRANTIES OF ANY KIND, either express or implied.                   *
** See the Unisoc General Software License, version 1.0 for more details.       *
********************************************************************************/
/********************************************************************************
** File Name:       uos_deval.c
** Author: Qiang He
** Copyright (C) 2002 Unisoc Communications Inc.
** Description:
********************************************************************************/
#include "uos_deval.h"
#include "uos_osal.h"
#include "uos_type.h"

#if TCFG_CAT1_UNISOC_ENABLE

#define MODEM_UART_DEFAULT_BAUD   115200

static uos_dev_uart g_dev_serial = {
    .dev_status = UOS_DEVICE_CLOSED
};

static uos_dev_spi g_dev_spi = {
    .dev_status = UOS_DEVICE_CLOSED
};

/* ringbuffer */
uos_inline enum uos_ringbuffer_state uos_ringbuffer_status(struct uos_ringbuffer *rb)
{
    if (rb->read_index == rb->write_index) {
        if (rb->read_mirror == rb->write_mirror) {
            return UOS_RINGBUFFER_EMPTY;
        } else {
            return UOS_RINGBUFFER_FULL;
        }
    }
    return UOS_RINGBUFFER_NOT_EMPTY;
}

/**
 * @brief Initialize the ring buffer object.
 *
 * @param rb        A pointer to the ring buffer object.
 * @param pool      A pointer to the buffer.
 * @param size      The size of the buffer in bytes.
 */
void uos_ringbuffer_init(struct uos_ringbuffer *rb, uos_uint8_t *pool, uos_int16_t size)
{
    UOS_ASSERT(rb != NULL);
    UOS_ASSERT(size > 0);

    /* initialize read and write index */
    rb->read_mirror = rb->read_index = 0;
    rb->write_mirror = rb->write_index = 0;

    /* set buffer pool and size */
    rb->buffer_ptr = pool;
    rb->buffer_size = UOS_ALIGN_DOWN(size, UOS_ALIGN_SIZE);
}

/**
 * @brief Put a block of data into the ring buffer. If the capacity of ring buffer is insufficient, it will discard out-of-range data.
 *
 * @param rb            A pointer to the ring buffer object.
 * @param ptr           A pointer to the data buffer.
 * @param length        The size of data in bytes.
 *
 * @return Return the data size we put into the ring buffer.
 */
uos_uint32_t uos_ringbuffer_put(struct uos_ringbuffer *rb, const uos_uint8_t *ptr, uos_uint16_t length)
{
    uos_uint16_t size;

    UOS_ASSERT(rb != NULL);

    /* whether has enough space */
    size = uos_ringbuffer_space_len(rb);

    /* no space */
    if (size == 0) {
        return 0;
    }

    /* drop some data */
    if (size < length) {
        length = size;
    }

    if (rb->buffer_size - rb->write_index > length) {
        /* read_index - write_index = empty space */
        uos_memcpy(&rb->buffer_ptr[rb->write_index], ptr, length);
        /* this should not cause overflow because there is enough space for
         * length of data in current mirror */
        rb->write_index += length;
        return length;
    }

    uos_memcpy(&rb->buffer_ptr[rb->write_index],
               &ptr[0],
               rb->buffer_size - rb->write_index);
    uos_memcpy(&rb->buffer_ptr[0],
               &ptr[rb->buffer_size - rb->write_index],
               length - (rb->buffer_size - rb->write_index));

    /* we are going into the other side of the mirror */
    rb->write_mirror = ~rb->write_mirror;
    rb->write_index = length - (rb->buffer_size - rb->write_index);

    return length;
}

/**
 * @brief Put a block of data into the ring buffer. If the capacity of ring buffer is insufficient, it will overwrite the existing data in the ring buffer.
 *
 * @param rb            A pointer to the ring buffer object.
 * @param ptr           A pointer to the data buffer.
 * @param length        The size of data in bytes.
 *
 * @return Return the data size we put into the ring buffer.
 */
uos_uint32_t uos_ringbuffer_put_force(struct uos_ringbuffer *rb, const uos_uint8_t *ptr, uos_uint16_t length)
{
    uos_uint16_t space_length;

    UOS_ASSERT(rb != NULL);

    space_length = uos_ringbuffer_space_len(rb);

    if (length > rb->buffer_size) {
        ptr = &ptr[length - rb->buffer_size];
        length = rb->buffer_size;
    }

    if (rb->buffer_size - rb->write_index > length) {
        /* read_index - write_index = empty space */
        uos_memcpy(&rb->buffer_ptr[rb->write_index], ptr, length);
        /* this should not cause overflow because there is enough space for
         * length of data in current mirror */
        rb->write_index += length;

        if (length > space_length) {
            rb->read_index = rb->write_index;
        }

        return length;
    }

    uos_memcpy(&rb->buffer_ptr[rb->write_index],
               &ptr[0],
               rb->buffer_size - rb->write_index);
    uos_memcpy(&rb->buffer_ptr[0],
               &ptr[rb->buffer_size - rb->write_index],
               length - (rb->buffer_size - rb->write_index));

    /* we are going into the other side of the mirror */
    rb->write_mirror = ~rb->write_mirror;
    rb->write_index = length - (rb->buffer_size - rb->write_index);

    if (length > space_length) {
        if (rb->write_index <= rb->read_index) {
            rb->read_mirror = ~rb->read_mirror;
        }
        rb->read_index = rb->write_index;
    }

    return length;
}

/**
 * @brief Get data from the ring buffer.
 *
 * @param rb            A pointer to the ring buffer.
 * @param ptr           A pointer to the data buffer.
 * @param length        The size of the data we want to read from the ring buffer.
 *
 * @return Return the data size we read from the ring buffer.
 */
uos_uint32_t uos_ringbuffer_get(struct uos_ringbuffer *rb, uos_uint8_t *ptr, uos_uint16_t length)
{
    uos_uint32_t size;

    UOS_ASSERT(rb != NULL);

    /* whether has enough data  */
    size = uos_ringbuffer_data_len(rb);

    /* no data */
    if (size == 0) {
        return 0;
    }

    /* less data */
    if (size < length) {
        length = size;
    }

    if (rb->buffer_size - rb->read_index > length) {
        /* copy all of data */
        uos_memcpy(ptr, &rb->buffer_ptr[rb->read_index], length);
        /* this should not cause overflow because there is enough space for
         * length of data in current mirror */
        rb->read_index += length;
        return length;
    }

    uos_memcpy(&ptr[0],
               &rb->buffer_ptr[rb->read_index],
               rb->buffer_size - rb->read_index);
    uos_memcpy(&ptr[rb->buffer_size - rb->read_index],
               &rb->buffer_ptr[0],
               length - (rb->buffer_size - rb->read_index));

    /* we are going into the other side of the mirror */
    rb->read_mirror = ~rb->read_mirror;
    rb->read_index = length - (rb->buffer_size - rb->read_index);

    return length;
}

/**
 * @brief Put a byte into the ring buffer. If ring buffer is full, this operation will fail.
 *
 * @param rb        A pointer to the ring buffer object.
 * @param ch        A byte put into the ring buffer.
 *
 * @return Return the data size we put into the ring buffer. The ring buffer is full if returns 0. Otherwise, it will return 1.
 */
uos_uint32_t uos_ringbuffer_putchar(struct uos_ringbuffer *rb, const uos_uint8_t ch)
{
    UOS_ASSERT(rb != NULL);

    /* whether has enough space */
    if (!uos_ringbuffer_space_len(rb)) {
        return 0;
    }

    rb->buffer_ptr[rb->write_index] = ch;

    /* flip mirror */
    if (rb->write_index == rb->buffer_size - 1) {
        rb->write_mirror = ~rb->write_mirror;
        rb->write_index = 0;
    } else {
        rb->write_index++;
    }

    return 1;
}

/**
 * @brief Put a byte into the ring buffer. If ring buffer is full, it will discard an old data and put into a new data.
 *
 * @param rb        A pointer to the ring buffer object.
 * @param ch        A byte put into the ring buffer.
 *
 * @return Return the data size we put into the ring buffer. Always return 1.
 */
uos_uint32_t uos_ringbuffer_putchar_force(struct uos_ringbuffer *rb, const uos_uint8_t ch)
{
    enum uos_ringbuffer_state old_state;

    UOS_ASSERT(rb != NULL);

    old_state = uos_ringbuffer_status(rb);

    rb->buffer_ptr[rb->write_index] = ch;

    /* flip mirror */
    if (rb->write_index == rb->buffer_size - 1) {
        rb->write_mirror = ~rb->write_mirror;
        rb->write_index = 0;
        if (old_state == UOS_RINGBUFFER_FULL) {
            rb->read_mirror = ~rb->read_mirror;
            rb->read_index = rb->write_index;
        }
    } else {
        rb->write_index++;
        if (old_state == UOS_RINGBUFFER_FULL) {
            rb->read_index = rb->write_index;
        }
    }

    return 1;
}

/**
 * @brief Get a byte from the ring buffer.
 *
 * @param rb        The pointer to the ring buffer object.
 * @param ch        A pointer to the buffer, used to store one byte.
 *
 * @return 0    The ring buffer is empty.
 * @return 1    Success
 */
uos_uint32_t uos_ringbuffer_getchar(struct uos_ringbuffer *rb, uos_uint8_t *ch)
{
    UOS_ASSERT(rb != NULL);

    /* ringbuffer is empty */
    if (!uos_ringbuffer_data_len(rb)) {
        return 0;
    }

    /* put byte */
    *ch = rb->buffer_ptr[rb->read_index];

    if (rb->read_index == rb->buffer_size - 1) {
        rb->read_mirror = ~rb->read_mirror;
        rb->read_index = 0;
    } else {
        rb->read_index++;
    }

    return 1;
}

/**
 * @brief Get the size of data in the ring buffer in bytes.
 *
 * @param rb        The pointer to the ring buffer object.
 *
 * @return Return the size of data in the ring buffer in bytes.
 */
uos_uint32_t uos_ringbuffer_data_len(struct uos_ringbuffer *rb)
{
    switch (uos_ringbuffer_status(rb)) {
    case UOS_RINGBUFFER_EMPTY:
        return 0;
    case UOS_RINGBUFFER_FULL:
        return rb->buffer_size;
    case UOS_RINGBUFFER_NOT_EMPTY:
    default: {
        uos_uint32_t wi = rb->write_index, ri = rb->read_index;

        if (wi > ri) {
            return wi - ri;
        } else {
            return rb->buffer_size - (ri - wi);
        }
    }
    }
}

/**
 * @brief Reset the ring buffer object, and clear all contents in the buffer.
 *
 * @param rb        A pointer to the ring buffer object.
 */
void uos_ringbuffer_reset(struct uos_ringbuffer *rb)
{
    UOS_ASSERT(rb != NULL);

    rb->read_mirror = 0;
    rb->read_index = 0;
    rb->write_mirror = 0;
    rb->write_index = 0;
}

/**
 * @brief Create a ring buffer object with a given size.
 *
 * @param size      The size of the buffer in bytes.
 *
 * @return Return a pointer to ring buffer object. When the return value is UOS_NULL, it means this creation failed.
 */
struct uos_ringbuffer *uos_ringbuffer_create(uos_uint16_t size)
{
    struct uos_ringbuffer *rb;
    uos_uint8_t *pool;

    UOS_ASSERT(size > 0);

    size = UOS_ALIGN_DOWN(size, UOS_ALIGN_SIZE);

    rb = (struct uos_ringbuffer *)uos_malloc(sizeof(struct uos_ringbuffer));
    if (rb == UOS_NULL) {
        goto exit;
    }

    pool = (uos_uint8_t *)uos_malloc(size);
    if (pool == UOS_NULL) {
        uos_free(rb);
        rb = UOS_NULL;
        goto exit;
    }
    uos_ringbuffer_init(rb, pool, size);

exit:
    return rb;
}

/**
 * @brief Destroy the ring buffer object, which is created by uos_ringbuffer_create() .
 *
 * @param rb        A pointer to the ring buffer object.
 */
void uos_ringbuffer_destroy(struct uos_ringbuffer *rb)
{
    UOS_ASSERT(rb != NULL);

    uos_free(rb->buffer_ptr);
    uos_free(rb);
}
#endif
