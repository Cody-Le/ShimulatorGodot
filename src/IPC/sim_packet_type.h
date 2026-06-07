//FILE SYNCHED ACROSS KERNEL AND ENGINE

#ifndef SIM_HW_TYPE_H
#define SIM_HW_TYPE_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

typedef uint8_t cmd_id_t;
typedef uint8_t action_id_t;

#define NO_FD (-1)

#define CMD_SYNCH  ((cmd_id_t)0x01)
#define CMD_ACTION ((cmd_id_t)0x02)

#define ACTION_OPEN  ((action_id_t)0x01)
#define ACTION_READ  ((action_id_t)0x02)
#define ACTION_WRITE ((action_id_t)0x03)
#define ACTION_IOCTL ((action_id_t)0x04)

typedef uint8_t hdwi_type_t;

#define HDWI_TYPE_GPIO    ((hdwi_type_t)0x00)
#define HDWI_TYPE_UART    ((hdwi_type_t)0x01)
#define HDWI_TYPE_I2C     ((hdwi_type_t)0x02)
#define HDWI_TYPE_SPI     ((hdwi_type_t)0x03)
#define HDWI_TYPE_ONEWIRE ((hdwi_type_t)0x04)
#define HDWI_TYPE_V4L2    ((hdwi_type_t)0x05)

// Wire format: simcall_header_t | dev_id | data
// 24 bytes, NOT packed (natural alignment + 4-byte trailing pad), version = 4
typedef struct {
    uint16_t    version;
    cmd_id_t    cmd_id;
    hdwi_type_t type;
    uint8_t     reserved[4];
    uint64_t    time_ns;       // offset 8
    uint32_t    data_len;      // covers data only, not dev_id
    uint8_t     _pad[4];       // explicit trailing pad — keeps struct 24 bytes under pack(1) on MSVC
} simcall_header_t;

// MSVC doesn't support __attribute__((packed)); use pragma pack instead.
// Only the per-type dev_id structs below are packed — simcall_header_t is NOT.
#ifdef _MSC_VER
  #define SIM_PACKED
  #pragma pack(push, 1)
#else
  #define SIM_PACKED __attribute__((packed))
#endif

// ── GPIO ─────────────────────────────────────────────────────────────────────
typedef uint8_t gpio_action_t;

#define GPIO_GET     ((gpio_action_t)0x01)
#define GPIO_SET     ((gpio_action_t)0x02)
#define GPIO_DIR_OUT ((gpio_action_t)0x03)
#define GPIO_DIR_IN  ((gpio_action_t)0x04)

typedef struct SIM_PACKED {
    uint8_t       chip_index;
    gpio_action_t action;
} gpio_dev_id_t;

// ── SPI ──────────────────────────────────────────────────────────────────────
typedef uint8_t spi_action_t;

#define SPI_SETUP    ((spi_action_t)0x01)
#define SPI_TRANSFER ((spi_action_t)0x02)

typedef struct SIM_PACKED {
    uint8_t      bus_index;
    uint8_t      chip_select;
    spi_action_t action;
    uint8_t      _pad;
} spi_dev_id_t;

// ── UART ─────────────────────────────────────────────────────────────────────
typedef uint8_t uart_action_t;

#define UART_READ  ((uart_action_t)0x01)
#define UART_WRITE ((uart_action_t)0x02)

typedef struct SIM_PACKED {
    uint8_t       port_index;
    uart_action_t action;
} uart_dev_id_t;

// ── 1-Wire ───────────────────────────────────────────────────────────────────
typedef uint8_t onewire_action_t;

#define ONEWIRE_READ ((onewire_action_t)0x01)

typedef struct SIM_PACKED {
    uint8_t          sensor_index;
    onewire_action_t action;
} onewire_dev_id_t;

#ifdef _MSC_VER
  #pragma pack(pop)
#endif

// Device description used during device registration
typedef struct {
    char        name[32];
    hdwi_type_t type;
} sim_device_desc_t;

#endif
