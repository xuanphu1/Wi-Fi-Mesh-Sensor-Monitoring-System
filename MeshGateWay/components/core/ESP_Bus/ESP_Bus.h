#ifndef ESP_BUS_H
#define ESP_BUS_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ESP_BUS_MAX_SUBSCRIBERS
#define ESP_BUS_MAX_SUBSCRIBERS 32
#endif

#ifndef ESP_BUS_MAX_DISPATCH_SNAPSHOT
#define ESP_BUS_MAX_DISPATCH_SNAPSHOT ESP_BUS_MAX_SUBSCRIBERS
#endif

/** Đăng ký nhận mọi sự kiện (wildcard). */
#define ESP_BUS_EVENT_ANY ((uint32_t)0xFFFFFFFFu)

typedef int32_t esp_bus_sub_handle_t;

#define ESP_BUS_SUB_INVALID ((esp_bus_sub_handle_t)(-1))

/**
 * @brief Callback khi có sự kiện.
 *
 * @param event_id Mã sự kiện được publish.
 * @param payload Dữ liệu kèm (có thể NULL nếu len == 0). Chỉ hợp lệ trong lời gọi callback.
 * @param len Độ dài payload (byte).
 * @param user_ctx Ngữ cảnh do subscriber truyền vào lúc đăng ký.
 */
typedef void (*esp_bus_handler_t)(uint32_t event_id,
                                  const void *payload,
                                  size_t len,
                                  void *user_ctx);

esp_err_t esp_bus_init(void);
esp_err_t esp_bus_deinit(void);

/**
 * @brief Đăng ký callback cho một event_id hoặc ESP_BUS_EVENT_ANY.
 *
 * @param out_handle Trả về handle để hủy đăng ký; có thể NULL nếu không cần.
 */
esp_err_t esp_bus_subscribe(uint32_t event_id,
                            esp_bus_handler_t handler,
                            void *user_ctx,
                            esp_bus_sub_handle_t *out_handle);

esp_err_t esp_bus_unsubscribe(esp_bus_sub_handle_t handle);

/**
 * @brief Phát sự kiện đồng bộ: gọi tất cả handler khớp trên ngăn xếp luồng hiện tại.
 *
 * Không gọi từ ISR. Payload phải tồn tại trong suốt thời gian dispatch (thường là stack/static).
 */
esp_err_t esp_bus_publish(uint32_t event_id, const void *payload, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* ESP_BUS_H */
