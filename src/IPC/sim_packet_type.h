//FILE SYNCHED ACROSS KERNEL AND ENGINE

#ifndef SIM_HW_TYPE_H
#define SIM_HW_TYPE_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

// MSVC doesn't support __attribute__((packed)); use pragma pack instead.
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
#define CMD_EVENT  ((cmd_id_t)0x03)  // reverse channel: engine -> kernel async events

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

// which sysfs attribute the FSW touched and in which direction. Reads carry no
// payload and the engine replies with the matching byte format; writes carry the
// raw bytes the FSW wrote and the engine replies with a (data-less) ack. The
// high bit (0x80) flags the write direction.
#define ONEWIRE_READ_TEMPERATURE ((onewire_action_t)0x01)  // "temperature" file (millidegrees ASCII)
#define ONEWIRE_READ_SLAVE       ((onewire_action_t)0x02)  // "w1_slave" file (raw bytes + CRC line)
#define ONEWIRE_READ_RAW         ((onewire_action_t)0x03)  // "rw" generic raw byte channel
#define ONEWIRE_READ_RESOLUTION  ((onewire_action_t)0x04)  // "resolution" file (9..12 ASCII)
#define ONEWIRE_WRITE_RESOLUTION ((onewire_action_t)0x81)  // "resolution" file write (9..12 ASCII)

typedef struct SIM_PACKED {
    uint8_t          sensor_index;
    onewire_action_t action;
} onewire_dev_id_t;

// ── V4L2 ─────────────────────────────────────────────────────────────────────
// V4L2 is a pure source: the engine pushes frames on the reverse channel
// (CMD_EVENT / SIM_EVENT_FRAME), and the kernel only sends fire-and-forget
// STREAMON/OFF gating on the forward channel (CMD_ACTION). No reply is expected
// to either action — treat them like a UART write.
typedef uint8_t v4l2_action_t;

#define V4L2_STREAM_ON  ((v4l2_action_t)0x01)
#define V4L2_STREAM_OFF ((v4l2_action_t)0x02)

// FourCC the engine stamps into v4l2_frame_hdr_t.pixfmt for every frame it emits
// today (see below) — "RGB3" packed little-endian. Suffixed _SIM (rather than the
// stock V4L2_PIX_FMT_RGB24) to avoid colliding with the real macro of the same
// value if this header is ever compiled alongside linux/videodev2.h kernel-side.
#define V4L2_PIX_FMT_RGB24_SIM ((uint32_t)0x33424752)

// dev_id for forward STREAMON/OFF (CMD_ACTION, data_len = 0)
typedef struct SIM_PACKED {
    uint8_t       video_index;   // /dev/video<video_index>, also event device_id
    v4l2_action_t action;        // V4L2_STREAM_ON / V4L2_STREAM_OFF
} v4l2_dev_id_t;                 // 2 bytes

// Per-frame header on the reverse channel. Wire layout of one frame:
//   simcall_header_t | sim_event_hdr_t | v4l2_frame_hdr_t | pixels[bytesused]
// Critical: simcall_header_t.data_len counts ONLY v4l2_frame_hdr_t + pixels
// (= sizeof(v4l2_frame_hdr_t) + bytesused); it does NOT include sim_event_hdr_t.
// pixfmt is a V4L2 FourCC; the engine emits only V4L2_PIX_FMT_RGB24 ("RGB3",
// 0x33424752) today — RGB24 is byte order R,G,B per pixel, row-major, top-to-
// bottom, tightly packed (bytesperline = width*3). Trust bytesused for the copy.
typedef struct SIM_PACKED {
    uint32_t width;
    uint32_t height;
    uint32_t pixfmt;     // V4L2 FourCC
    uint32_t bytesused;  // pixel bytes following this header
    uint32_t sequence;   // monotonic per-device frame counter
} v4l2_frame_hdr_t;      // 20 bytes

// ── Reverse channel: engine → kernel async events ────────────────────────────
// Sent by the engine to the kernel's inbound TCP server (SIM_SERVER_PORT, the
// reverse of the outbound 7777 connection). These are events the engine raises
// on its own clock — interrupts, V4L2 frames — not replies to a kernel request.
//
// Wire format: simcall_header_t (cmd_id = CMD_EVENT) | sim_event_hdr_t | data[data_len]
//   - header.type        selects the target subsystem (HDWI_TYPE_*)
//   - sim_event_hdr_t.device_id selects the instance within that subsystem
//   - header.data_len    bytes of event-specific payload follow the event header
typedef uint8_t sim_event_t;

#define SIM_EVENT_IRQ   ((sim_event_t)0x01)  // assert a virtual interrupt / line change
#define SIM_EVENT_FRAME ((sim_event_t)0x02)  // V4L2 frame ready in payload

typedef struct SIM_PACKED {
    uint8_t     device_id;   // which virtual device instance the event targets
    sim_event_t event;       // what happened in the engine
    uint16_t    _pad;
} sim_event_hdr_t;

// ── Per-peripheral IRQ payloads (SIM_EVENT_IRQ, data[data_len]) ──────────────
// Wire layout of a SIM_EVENT_IRQ event:
//   simcall_header_t  (cmd_id=CMD_EVENT, type=HDWI_TYPE_*, data_len=payload bytes)
//   sim_event_hdr_t   (event=SIM_EVENT_IRQ, device_id=instance index)
//   <per-type payload>[data_len bytes]
//
// The first byte of every payload is always the sub-type enum. The kernel
// dispatches on header.type first, then on payload[0].

// ── GPIO IRQ ─────────────────────────────────────────────────────────────────
typedef uint8_t gpio_irq_t;
#define GPIO_IRQ_LINE_CHANGE ((gpio_irq_t)0x01)

typedef struct SIM_PACKED {
    gpio_irq_t irq;    // GPIO_IRQ_LINE_CHANGE
    uint8_t    line;   // line offset within the chip (chip_index is in device_id)
    uint8_t    value;  // new line level: 0 or 1
} sim_gpio_irq_payload_t;

// ── UART IRQ ─────────────────────────────────────────────────────────────────
typedef uint8_t uart_irq_t;
#define UART_IRQ_RX_DATA     ((uart_irq_t)0x01)  // [irq][byte0..byteN], data_len >= 1
#define UART_IRQ_BREAK       ((uart_irq_t)0x02)  // [irq], data_len = 1
#define UART_IRQ_FRAMING_ERR ((uart_irq_t)0x03)  // [irq], data_len = 1
#define UART_IRQ_PARITY_ERR  ((uart_irq_t)0x04)  // [irq], data_len = 1
#define UART_IRQ_OVERRUN     ((uart_irq_t)0x05)  // [irq], data_len = 1
#define UART_IRQ_TX_EMPTY    ((uart_irq_t)0x06)  // [irq], data_len = 1

// base header; for UART_IRQ_RX_DATA the received bytes follow immediately
typedef struct SIM_PACKED {
    uart_irq_t irq;
} sim_uart_irq_payload_t;

// ── I2C IRQ ──────────────────────────────────────────────────────────────────
typedef uint8_t i2c_irq_t;
#define I2C_IRQ_SMBUS_ALERT ((i2c_irq_t)0x01)

typedef struct SIM_PACKED {
    i2c_irq_t irq;
    uint8_t   alerting_addr;  // 7-bit address of the slave asserting #ALERT
} sim_i2c_irq_payload_t;

// ── 1-Wire IRQ ───────────────────────────────────────────────────────────────
typedef uint8_t w1_irq_t;
#define W1_IRQ_ALARM           ((w1_irq_t)0x01)
#define W1_IRQ_PRESENCE_CHANGE ((w1_irq_t)0x02)

// state:  W1_IRQ_ALARM:           1 = crossed TH/TL, 0 = alarm cleared
//         W1_IRQ_PRESENCE_CHANGE: 1 = sensor arrived, 0 = sensor gone
typedef struct SIM_PACKED {
    w1_irq_t irq;
    uint8_t  state;
} sim_w1_irq_payload_t;

// ── V4L2 IRQ (non-frame async events) ────────────────────────────────────────
typedef uint8_t v4l2_irq_t;
#define V4L2_IRQ_FRAME_DROP ((v4l2_irq_t)0x01)  // engine dropped a frame
#define V4L2_IRQ_OVERFLOW   ((v4l2_irq_t)0x02)  // kernel buffer queue overflowed
#define V4L2_IRQ_ERROR      ((v4l2_irq_t)0x03)  // simulated camera fault

typedef struct SIM_PACKED {
    v4l2_irq_t irq;
    uint32_t   sequence;    // FRAME_DROP: dropped frame's sequence number
    int32_t    error_code;  // ERROR: negative errno; unused for other sub-types
} sim_v4l2_irq_payload_t;

#ifdef _MSC_VER
  #pragma pack(pop)
#endif

// Device description used during device registration
typedef struct {
    char        name[32];
    hdwi_type_t type;
} sim_device_desc_t;

#endif
