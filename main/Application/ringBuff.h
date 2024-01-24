#ifndef _ringbuff_h
#define _ringbuff_h

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RBUFF_FAIL ESP_FAIL
#define RBUFF_ABORT -1
#define RBUFF_WRITER_FINISHED -2
#define RBUFF_READER_UNBLOCK -3

#if __has_include("esp_idf_version.h")
#include "esp_idf_version.h"
#else
#define ESP_IDF_VERSION_VAL(major, minor, patch) 0
#endif

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#if !(configENABLE_BACKWARD_COMPATIBILITY == 1)
#define xSemaphoreHandle SemaphoreHandle_t
#endif
#endif

typedef struct ringbuff {
  char* name;
  uint8_t* base; /**< Original pointer */
  /* XXX: these need to be volatile? */
  uint8_t* volatile readptr;  /**< Read pointer */
  uint8_t* volatile writeptr; /**< Write pointer */
  volatile ssize_t fill_cnt;  /**< Number of filled slots */
  ssize_t size;               /**< Buffer size */
  xSemaphoreHandle can_read;
  xSemaphoreHandle can_write;
  xSemaphoreHandle lock;
  int abort_read;
  int abort_write;
  int writer_finished;  // to prevent infinite blocking for buffer read
  int reader_unblock;
} ringbuff_t;

ringbuff_t* rbuff_init(const char* rb_name, uint32_t size);
void rbuff_abort_read(ringbuff_t* rb);
void rbuff_abort_write(ringbuff_t* rb);
void rbuff_abort(ringbuff_t* rb);
void rbuff_reset(ringbuff_t* rb);
/**
 * @brief Special function to reset the buffer while keeping rbuff_write aborted.
 *        This rb needs to be reset again before being useful.
 */
void rbuff_reset_and_abort_write(ringbuff_t* rb);
void rbuff_stat(ringbuff_t* rb);
ssize_t rbuff_filled(ringbuff_t* rb);
ssize_t rbuff_available(ringbuff_t* rb);
int rbuff_read(ringbuff_t* rb, uint8_t* buf, int len, uint32_t ticks_to_wait);
int rbuff_write(ringbuff_t* rb, const uint8_t* buf, int len,
             uint32_t ticks_to_wait);
void rbuff_cleanup(ringbuff_t* rb);
void rbuff_signal_writer_finished(ringbuff_t* rb);
void rbuff_wakeup_reader(ringbuff_t* rb);
int rbuff_is_writer_finished(ringbuff_t* rb);

#ifdef __cplusplus
}
#endif

#endif  /* _ringbuff_h */
