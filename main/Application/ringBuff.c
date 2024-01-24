/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "ringBuff.h"

#include <esp_heap_caps.h>
#include <sdkconfig.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define RB_TAG "RINGBUF"

ringbuff_t* rbuff_init(const char* name, uint32_t size) {
  ringbuff_t* r;
  unsigned char* buf;

  if (size < 2 || !name) {
    return NULL;
  }

  r = malloc(sizeof(ringbuff_t));
  assert(r);
#if (CONFIG_SPIRAM_SUPPORT && \
     (CONFIG_SPIRAM_USE_CAPS_ALLOC || CONFIG_SPIRAM_USE_MALLOC))
  buf = heap_caps_calloc(1, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
  buf = calloc(1, size);
#endif
  assert(buf);

  r->name = (char*)name;
  r->base = r->readptr = r->writeptr = buf;
  r->fill_cnt = 0;
  r->size = size;

  r->can_read = xSemaphoreCreateBinary();
  assert(r->can_read);
  r->can_write = xSemaphoreCreateBinary();
  assert(r->can_write);
  r->lock = xSemaphoreCreateMutex();
  assert(r->lock);

  r->abort_read = 0;
  r->abort_write = 0;
  r->writer_finished = 0;
  r->reader_unblock = 0;

  return r;
}

void rbuff_cleanup(ringbuff_t* rb) {
  free(rb->base);
  rb->base = NULL;
  vSemaphoreDelete(rb->can_read);
  rb->can_read = NULL;
  vSemaphoreDelete(rb->can_write);
  rb->can_write = NULL;
  vSemaphoreDelete(rb->lock);
  rb->lock = NULL;
  free(rb);
}

/*
 * @brief: get the number of filled bytes in the buffer
 */
ssize_t rbuff_filled(ringbuff_t* rb) { return rb->fill_cnt; }

/*
 * @brief: get the number of empty bytes available in the buffer
 */
ssize_t rbuff_available(ringbuff_t* rb) {
  ESP_LOGD(RB_TAG, "rb leftover %d bytes", rb->size - rb->fill_cnt);
  return (rb->size - rb->fill_cnt);
}

int rbuff_read(ringbuff_t* rb, uint8_t* buf, int buf_len, uint32_t ticks_to_wait) {
  int read_size;
  int total_read_size = 0;

  /**
   * In case where we are able to read buf_len in one go,
   * we are not able to check for abort and keep returning buf_len as bytes
   * read. Check for argument validity check and abort case before entering
   * memcpy loop.
   */

  if (rb == NULL || rb->abort_read == 1) {
    return ESP_FAIL;
  }

  xSemaphoreTake(rb->lock, portMAX_DELAY);

  while (buf_len) {
    if (rb->fill_cnt < buf_len) {
      read_size = rb->fill_cnt;
    } else {
      read_size = buf_len;
    }
    if ((rb->readptr + read_size) > (rb->base + rb->size)) {
      int rlen1 = rb->base + rb->size - rb->readptr;
      int rlen2 = read_size - rlen1;
      if (buf) {
        memcpy(buf, rb->readptr, rlen1);
        memcpy(buf + rlen1, rb->base, rlen2);
      }
      rb->readptr = rb->base + rlen2;
    } else {
      if (buf) {
        memcpy(buf, rb->readptr, read_size);
      }
      rb->readptr = rb->readptr + read_size;
    }

    buf_len -= read_size;
    rb->fill_cnt -= read_size;
    total_read_size += read_size;
    if (buf) {
      buf += read_size;
    }

    xSemaphoreGive(rb->can_write);

    if (buf_len == 0) {
      break;
    }

    xSemaphoreGive(rb->lock);
    if (!rb->writer_finished && !rb->abort_read && !rb->reader_unblock) {
      if (xSemaphoreTake(rb->can_read, ticks_to_wait) != pdTRUE) {
        goto out;
      }
    }
    if (rb->abort_read == 1) {
      total_read_size = RBUFF_ABORT;
      goto out;
    }
    if (rb->writer_finished == 1) {
      goto out;
    }
    if (rb->reader_unblock == 1) {
      if (total_read_size == 0) {
        total_read_size = RBUFF_READER_UNBLOCK;
      }
      goto out;
    }

    xSemaphoreTake(rb->lock, portMAX_DELAY);
  }

  xSemaphoreGive(rb->lock);
out:
  if (rb->writer_finished == 1 && total_read_size == 0) {
    total_read_size = RBUFF_WRITER_FINISHED;
  }
  rb->reader_unblock = 0; /* We are anyway unblocking reader */
  return total_read_size;
}

int rbuff_write(ringbuff_t* rb, const uint8_t* buf, int buf_len,
             uint32_t ticks_to_wait) {
  int write_size;
  int total_write_size = 0;

  /**
   * In case where we are able to write buf_len in one go,
   * we are not able to check for abort and keep returning buf_len as bytes
   * written. Check for arguments' validity and abort case before entering
   * memcpy loop.
   */

  if (rb == NULL || buf == NULL || rb->abort_write == 1) {
    return RBUFF_FAIL;
  }

  xSemaphoreTake(rb->lock, portMAX_DELAY);

  while (buf_len) {
    if ((rb->size - rb->fill_cnt) < buf_len) {
      write_size = rb->size - rb->fill_cnt;
    } else {
      write_size = buf_len;
    }
    if ((rb->writeptr + write_size) > (rb->base + rb->size)) {
      int wlen1 = rb->base + rb->size - rb->writeptr;
      int wlen2 = write_size - wlen1;
      memcpy(rb->writeptr, buf, wlen1);
      memcpy(rb->base, buf + wlen1, wlen2);
      rb->writeptr = rb->base + wlen2;
    } else {
      memcpy(rb->writeptr, buf, write_size);
      rb->writeptr = rb->writeptr + write_size;
    }

    buf_len -= write_size;
    rb->fill_cnt += write_size;
    total_write_size += write_size;
    buf += write_size;

    xSemaphoreGive(rb->can_read);

    if (buf_len == 0) {
      break;
    }

    xSemaphoreGive(rb->lock);
    if (rb->writer_finished) {
      return write_size > 0 ? write_size : RBUFF_WRITER_FINISHED;
    }
    if (xSemaphoreTake(rb->can_write, ticks_to_wait) != pdTRUE) {
      goto out;
    }
    if (rb->abort_write == 1) {
      goto out;
    }
    xSemaphoreTake(rb->lock, portMAX_DELAY);
  }

  xSemaphoreGive(rb->lock);
out:
  return total_write_size;
}

/**
 * abort and set abort_read and abort_write to asked values.
 */
static void _rbuff_reset(ringbuff_t* rb, int abort_read, int abort_write) {
  if (rb == NULL) {
    return;
  }
  xSemaphoreTake(rb->lock, portMAX_DELAY);
  rb->readptr = rb->writeptr = rb->base;
  rb->fill_cnt = 0;
  rb->writer_finished = 0;
  rb->reader_unblock = 0;
  rb->abort_read = abort_read;
  rb->abort_write = abort_write;
  xSemaphoreGive(rb->lock);
}

void rbuff_reset(ringbuff_t* rb) { _rbuff_reset(rb, 0, 0); }

void rbuff_abort_read(ringbuff_t* rb) {
  if (rb == NULL) {
    return;
  }
  rb->abort_read = 1;
  xSemaphoreGive(rb->can_read);
  xSemaphoreGive(rb->lock);
}

void rbuff_abort_write(ringbuff_t* rb) {
  if (rb == NULL) {
    return;
  }
  rb->abort_write = 1;
  xSemaphoreGive(rb->can_write);
  xSemaphoreGive(rb->lock);
}

void rbuff_abort(ringbuff_t* rb) {
  if (rb == NULL) {
    return;
  }
  rb->abort_read = 1;
  rb->abort_write = 1;
  xSemaphoreGive(rb->can_read);
  xSemaphoreGive(rb->can_write);
  xSemaphoreGive(rb->lock);
}

/**
 * Reset the ringbuffer and keep rb_write aborted.
 * Note that we are taking lock before even toggling `abort_write` variable.
 * This serves a special purpose to not allow this abort to be mixed with
 * rb_write.
 */
void rbuff_reset_and_abort_write(ringbuff_t* rb) {
  _rbuff_reset(rb, 0, 1);
  xSemaphoreGive(rb->can_write);
}

void rbuff_signal_writer_finished(ringbuff_t* rb) {
  if (rb == NULL) {
    return;
  }
  rb->writer_finished = 1;
  xSemaphoreGive(rb->can_read);
}

int rbuff_is_writer_finished(ringbuff_t* rb) {
  if (rb == NULL) {
    return RBUFF_FAIL;
  }
  return (rb->writer_finished);
}

void rbuff_wakeup_reader(ringbuff_t* rb) {
  if (rb == NULL) {
    return;
  }
  rb->reader_unblock = 1;
  xSemaphoreGive(rb->can_read);
}

void rbuff_stat(ringbuff_t* rb) {
  xSemaphoreTake(rb->lock, portMAX_DELAY);
  ESP_LOGI(RB_TAG,
           "filled: %d, base: %p, read_ptr: %p, write_ptr: %p, size: %d\n",
           rb->fill_cnt, rb->base, rb->readptr, rb->writeptr, rb->size);
  xSemaphoreGive(rb->lock);
}
