//FILE SYNCHED ACROSS KERNEL AND ENGINE

#ifndef SIM_HW_TYPE_H
#define SIM_HW_TYPE_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

// MSVC doesn't support __attribute__((packed)); use pragma pack instead.
// IMPORTANT: pack(push,1) is opened LATER (just before the inner dev_id structs),
// NOT here, so it does not wrap simcall_header_t. The outer header must keep its
// natural alignment (trailing pad) to be exactly 24 bytes on every compiler —
// packing it makes MSVC emit a 20-byte header and corrupt the wire format.
#ifdef _MSC_VER
  #define SIM_PACKED
#else
  #define SIM_PACKED __attribute__((packed))
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
// 24 bytes, NOT packed (natural alignment), version = 4
typedef struct {
    uint16_t    version;
    cmd_id_t    cmd_id;
    hdwi_type_t type;
    uint8_t     reserved[4];
    uint64_t    time_ns;       // offset 8
    uint32_t    data_len;      // covers data only, not dev_id
} simcall_header_t;

// Inner peripheral headers ARE packed. Open the pragma here so it covers only
// these structs, leaving simcall_header_t above at natural 24-byte alignment on
// MSVC. (Matched by the #pragma pack(pop) after the last dev_id struct.)
#ifdef _MSC_VER
  #pragma pack(push, 1)
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

// ── I2C ──────────────────────────────────────────────────────────────────────
typedef uint8_t i2c_action_t;

#define I2C_WRITE ((i2c_action_t)0x01)  // master -> slave; payload = bytes written
#define I2C_READ  ((i2c_action_t)0x02)  // master <- slave; payload = u16 read length

typedef struct SIM_PACKED {
    uint8_t      bus_index;     // adapter index (== /dev/i2c-N)
    uint8_t      address;       // 7-bit slave address from the i2c_msg
    i2c_action_t action;
    uint8_t      _pad;
} i2c_dev_id_t;

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

// which sysfs attribute the FSW read; the engine returns the matching format
#define ONEWIRE_READ_TEMPERATURE ((onewire_action_t)0x01)  // "temperature" file (millidegrees ASCII)
#define ONEWIRE_READ_SLAVE       ((onewire_action_t)0x02)  // "w1_slave" file (raw bytes + CRC line)
#define ONEWIRE_READ_RAW         ((onewire_action_t)0x03)  // "rw" generic raw byte channel

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
