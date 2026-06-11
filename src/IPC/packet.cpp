#include "packet.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

// The wire header is 24 bytes (natural alignment, version 4). If the pragma pack
// in sim_packet_type.h ever wraps simcall_header_t again, MSVC packs it to 20
// and silently corrupts every frame — fail the build here instead.
static_assert(sizeof(simcall_header_t) == 24, "simcall_header_t must be 24 bytes on the wire");


PacketCPP::PacketCPP() {
    this->valid = false;
}

PacketCPP::~PacketCPP() {
    // No dynamic memory to free, so nothing to do here
}

//Bind all methods and write appropriate signatures for Godot to recognize type + field type correction
void PacketCPP::_bind_methods() {
    ClassDB::bind_method(D_METHOD("generate", "cmd_type", "hdwi_type", "time_ns", "bytes"), &PacketCPP::generate);
    ClassDB::bind_method(D_METHOD("generate_from_bytes", "header_bytes"), &PacketCPP::generate_from_bytes);
    ClassDB::bind_method(D_METHOD("get_validity"), &PacketCPP::get_validity);
    ClassDB::bind_method(D_METHOD("get_cmd_id"), &PacketCPP::get_cmd_id);
    ClassDB::bind_method(D_METHOD("get_type"), &PacketCPP::get_type);
    ClassDB::bind_method(D_METHOD("get_time_ns"), &PacketCPP::get_time_ns);
    ClassDB::bind_method(D_METHOD("get_data_len"), &PacketCPP::get_data_len);
    ClassDB::bind_method(D_METHOD("_to_string"), &PacketCPP::_to_string);
    ClassDB::bind_method(D_METHOD("to_string_robust"), &PacketCPP::to_string_robust);
    ClassDB::bind_method(D_METHOD("set_robust", "enabled"), &PacketCPP::set_robust);
    ClassDB::bind_method(D_METHOD("get_robust"), &PacketCPP::get_robust);
    ClassDB::add_property("PacketCPP", PropertyInfo(Variant::BOOL, "robust"), "set_robust", "get_robust");
    ClassDB::bind_method(D_METHOD("set_data", "data"), &PacketCPP::set_data);
    ClassDB::bind_method(D_METHOD("get_data"), &PacketCPP::get_data);
    ClassDB::bind_static_method("PacketCPP", D_METHOD("get_header_len"), &PacketCPP::get_header_len);
    ClassDB::bind_method(D_METHOD("convert_to_bytes"), &PacketCPP::convert_to_bytes);

    // In _bind_methods()
    ClassDB::bind_integer_constant(get_class_static(), "CmdType", "CMD_SYNCH",  (int64_t)CmdType::SYNCH);
    ClassDB::bind_integer_constant(get_class_static(), "CmdType", "CMD_ACTION", (int64_t)CmdType::ACTION);

    ClassDB::bind_integer_constant(get_class_static(), "HDWIType", "GPIO",    (int64_t)HDWIType::GPIO);
    ClassDB::bind_integer_constant(get_class_static(), "HDWIType", "UART",    (int64_t)HDWIType::UART);
    ClassDB::bind_integer_constant(get_class_static(), "HDWIType", "I2C",     (int64_t)HDWIType::I2C);
    ClassDB::bind_integer_constant(get_class_static(), "HDWIType", "SPI",     (int64_t)HDWIType::SPI);
    ClassDB::bind_integer_constant(get_class_static(), "HDWIType", "ONEWIRE", (int64_t)HDWIType::ONEWIRE);
    ClassDB::bind_integer_constant(get_class_static(), "HDWIType", "V4L2",    (int64_t)HDWIType::V4L2);

}


void PacketCPP::generate(
                CmdType cmd_type, 
                HDWIType hdwi_type,
                uint64_t time_ns,
                PackedByteArray bytes) {
    this->header = {0};
    this->header.version = VERSION;
    this->header.cmd_id = static_cast<uint8_t>(cmd_type);
    this->header.type    = static_cast<uint8_t>(hdwi_type);
    this->header.time_ns = time_ns;
    this->data = bytes;
    this->header.data_len = bytes.size();
}  

void PacketCPP::generate_from_bytes(PackedByteArray header_bytes) {
    this->header = {0};
    this->header = *((simcall_header_t*)header_bytes.ptrw());
    if(this->header.version != VERSION || unlikely(header_bytes.size() != sizeof(simcall_header_t))) {
        this->valid = false;
    } else {
        this->valid = true;
    }
}



bool PacketCPP::get_validity() {
    return this->valid;
}

uint32_t PacketCPP::get_header_length() {
    return sizeof(simcall_header_t);
}

uint32_t PacketCPP::get_header_len() {
    return sizeof(simcall_header_t);
}

int64_t PacketCPP::get_cmd_id() {
    return (int64_t)header.cmd_id;
}
int64_t PacketCPP::get_type() {
    return (int64_t)header.type;
}
uint64_t PacketCPP::get_time_ns() {
    return header.time_ns;

}
uint32_t PacketCPP::get_data_len() {
    return header.data_len;

}

// ── Robust-print helpers ─────────────────────────────────────────────────────
namespace {
    // Two-char lowercase hex for a single byte.
    String hex2(uint8_t b) {
        const char *digits = "0123456789abcdef";
        char out[3] = { digits[(b >> 4) & 0xF], digits[b & 0xF], 0 };
        return String(out);
    }

    // Space-separated hex for a byte range.
    String hex_range(const uint8_t *p, int len) {
        String s;
        for (int i = 0; i < len; ++i) {
            if (i) s += " ";
            s += hex2(p[i]);
        }
        return s;
    }

    // Byte-offset label like "[02]" or "[04..07]".
    String span(int start, int len) {
        if (len <= 1) return "[" + String::num_int64(start) + "]";
        return "[" + String::num_int64(start) + ".." + String::num_int64(start + len - 1) + "]";
    }

    String hdwi_type_name(uint8_t t) {
        switch (t) {
            case HDWI_TYPE_GPIO:    return "GPIO";
            case HDWI_TYPE_UART:    return "UART";
            case HDWI_TYPE_I2C:     return "I2C";
            case HDWI_TYPE_SPI:     return "SPI";
            case HDWI_TYPE_ONEWIRE: return "ONEWIRE";
            case HDWI_TYPE_V4L2:    return "V4L2";
            default:                return "0x" + hex2(t);
        }
    }

    // dev_id size on the wire by HDWI type (must match _DEV_ID_SIZES in tcp_port.gd).
    int dev_id_size(uint8_t t) {
        switch (t) {
            case HDWI_TYPE_GPIO:    return 2;
            case HDWI_TYPE_UART:    return 2;
            case HDWI_TYPE_I2C:     return 4;
            case HDWI_TYPE_SPI:     return 4;
            case HDWI_TYPE_ONEWIRE: return 2;
            default:                return 0;
        }
    }
}

// Robust, sectioned & labeled dump. Decodes header fields with byte offsets,
// then interprets the data section based on cmd_id:
//   CMD_SYNCH  → group-registration: one or more [type][count][name[32]xN] groups
//   CMD_ACTION → dev_id (sized by type) + action payload
String PacketCPP::build_robust() const {
    const String LINE = "----------------------------------------------------------\n";
    String r;
    r += LINE;
    r += "PacketCPP (robust)\n";
    r += LINE;

    // ── HEADER ───────────────────────────────────────────────────────────────
    r += "HEADER (" + String::num_int64((int64_t)sizeof(simcall_header_t)) + " bytes)\n";
    r += "  " + span(0, 2) + " version   : " + String::num_int64(header.version) + "\n";

    String cmd_name = (header.cmd_id == CMD_SYNCH) ? "CMD_SYNCH"
                    : (header.cmd_id == CMD_ACTION) ? "CMD_ACTION"
                    : "UNKNOWN";
    r += "  " + span(2, 1) + " cmd_id    : " + cmd_name + " (0x" + hex2(header.cmd_id) + ")\n";
    String tname = hdwi_type_name(header.type);
    if (!tname.begins_with("0x")) tname += " (0x" + hex2(header.type) + ")";
    r += "  " + span(3, 1) + " type      : " + tname + "\n";
    r += "  " + span(4, 4) + " reserved  : " + hex_range(header.reserved, 4) + "\n";
    r += "  " + span(8, 8) + " time_ns   : " + String::num_uint64(header.time_ns) + "\n";
    r += "  " + span(16, 4) + " data_len  : " + String::num_int64(header.data_len) + "\n";

    // ── DATA ───────────────────────────────────────────────────────────────────
    const int n = data.size();
    const uint8_t *p = data.ptr();
    r += LINE;

    if (n == 0) {
        r += "DATA (0 bytes) — none";
        if (header.cmd_id == CMD_SYNCH) r += " (time-seed only)";
        r += "\n" + LINE;
        return r;
    }

    if (header.cmd_id == CMD_SYNCH) {
        // Group registration: concatenated [hdwi_type u8][count u8][name[32] x count]
        r += "DATA (" + String::num_int64(n) + " bytes) — GROUP REGISTRATION\n";
        int off = 0;
        int g = 0;
        while (off + 2 <= n) {
            uint8_t gtype = p[off];
            uint8_t count = p[off + 1];
            String gname = hdwi_type_name(gtype);
            if (!gname.begins_with("0x")) gname += " (0x" + hex2(gtype) + ")";
            r += "  group #" + String::num_int64(g) + "  "
               + span(off, 2) + " type=" + gname
               + ", count=" + String::num_int64(count) + "\n";
            off += 2;
            for (int i = 0; i < count && off + 32 <= n; ++i, off += 32) {
                int len = 0;
                while (len < 32 && p[off + len] != 0) ++len;
                r += "    #" + String::num_int64(i) + " " + span(off, 32)
                   + " \"" + String::utf8((const char *)(p + off), len) + "\"\n";
            }
            ++g;
        }
        if (off < n) {
            r += "  [trailing " + String::num_int64(n - off) + " byte(s)] "
               + hex_range(p + off, n - off) + "\n";
        }
    } else if (header.cmd_id == CMD_ACTION) {
        // dev_id (sized by type) + action payload
        int idsz = dev_id_size(header.type);
        r += "DATA (" + String::num_int64(n) + " bytes) — ACTION\n";
        if (idsz > 0 && n >= idsz) {
            r += "  " + span(0, idsz) + " dev_id    : " + hex_range(p, idsz) + "\n";
            int payload = n - idsz;
            r += "  " + span(idsz, payload) + " payload   : ";
            r += (payload > 0) ? hex_range(p + idsz, payload) : "(empty)";
            r += "\n";
        } else {
            r += "  [data] " + hex_range(p, n) + "\n";
        }
    } else {
        r += "DATA (" + String::num_int64(n) + " bytes) — RAW\n";
        r += "  " + hex_range(p, n) + "\n";
    }

    r += LINE;
    return r;
}

String PacketCPP::to_string_robust() const {
    return build_robust();
}

void PacketCPP::set_robust(bool enabled) {
    this->robust = enabled;
}

bool PacketCPP::get_robust() const {
    return this->robust;
}

// This method is for debugging, print all header field + data as bytes
String PacketCPP::_to_string() const {
    if (robust) {
        return build_robust();
    }
    String result = "PacketCPP { ";
    result += "version: " + String::num_int64(header.version) + ", ";
    if(header.cmd_id == CMD_SYNCH) {
        result += "cmd_id: CMD_SYNCH, ";
    } else if(header.cmd_id == CMD_ACTION) {
        result += "cmd_id: CMD_ACTION, ";
    } else {
        result += "cmd_id: UNKNOWN, ";
    }
    result += "type: " + String::num_int64((uint8_t)header.type) + ", ";
    result += "time_ns: " + String::num_int64(header.time_ns) + ", ";
    result += "data_len: " + String::num_int64(header.data_len) + ", ";
    result += "data: " + data.hex_encode();
    result += " }";
    return result;
}

void PacketCPP::set_data(PackedByteArray data) {
    this->data = data;
    this->header.data_len = data.size();
}

PackedByteArray PacketCPP::get_data() const {
    return this->data;
}


PackedByteArray PacketCPP::convert_to_bytes() {
    PackedByteArray bytes;
    bytes.resize(sizeof(simcall_header_t) + this->data.size());
    // Copy header
    memcpy(bytes.ptrw(), &this->header, sizeof(simcall_header_t));
    // Copy data
    memcpy(bytes.ptrw() + sizeof(simcall_header_t), this->data.ptr(), this->data.size());
    return bytes;
}