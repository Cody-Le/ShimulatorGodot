#include "./hdwi_onewire.h"
#include <algorithm>
#include <vector>

namespace godot {

    TypedArray<HDWIOneWireResource> *HDWIOneWireResource::onewire_resources = nullptr;
    std::unordered_map<uint8_t, int> HDWIOneWireResource::sensor_index_to_device_id;

    int HDWIOneWireResource::onewire_dev_id_to_device_id(onewire_dev_id_t id) {
        auto it = sensor_index_to_device_id.find(id.sensor_index);
        if (it == sensor_index_to_device_id.end()) return -1;
        return it->second;
    }

    int HDWIOneWireResource::lookup_onewire_device_id(int p_sensor_index) {
        auto it = sensor_index_to_device_id.find(static_cast<uint8_t>(p_sensor_index));
        if (it == sensor_index_to_device_id.end()) return -1;
        return it->second;
    }

    void HDWIOneWireResource::_bind_methods() {
        HDWIResource::_bind_methods();

        ClassDB::bind_method(D_METHOD("init"), &HDWIOneWireResource::init);
        ClassDB::bind_method(D_METHOD("clear"), &HDWIOneWireResource::clear);

        ClassDB::bind_method(D_METHOD("set_sensor_index", "sensor_index"), &HDWIOneWireResource::set_sensor_index);
        ClassDB::bind_method(D_METHOD("get_sensor_index"), &HDWIOneWireResource::get_sensor_index);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "sensor_index"), "set_sensor_index", "get_sensor_index");

        ClassDB::bind_method(D_METHOD("set_read_buffer", "read_buffer"), &HDWIOneWireResource::set_read_buffer);
        ClassDB::bind_method(D_METHOD("get_read_buffer"), &HDWIOneWireResource::get_read_buffer);
        ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "read_buffer"), "set_read_buffer", "get_read_buffer");

        ClassDB::bind_method(D_METHOD("dispatch_action", "request_data"), &HDWIOneWireResource::dispatch_action);
        ClassDB::bind_method(D_METHOD("handle_onewire_read", "action"), &HDWIOneWireResource::handle_onewire_read);

        BIND_CONSTANT(ONEWIRE_READ_TEMPERATURE);
        BIND_CONSTANT(ONEWIRE_READ_SLAVE);
        BIND_CONSTANT(ONEWIRE_READ_RAW);
        ClassDB::bind_method(D_METHOD("get_device_representation"), &HDWIOneWireResource::get_device_representation);
        ClassDB::bind_static_method("HDWIOneWireResource", D_METHOD("get_group_type_representation"), &HDWIOneWireResource::get_group_type_representation);
        ClassDB::bind_static_method("HDWIOneWireResource", D_METHOD("lookup_onewire_device_id", "sensor_index"), &HDWIOneWireResource::lookup_onewire_device_id);

        ADD_SIGNAL(MethodInfo("on_onewire_read",
            PropertyInfo(Variant::INT, "sensor_index"),
            PropertyInfo(Variant::INT, "action")));
    }

    void HDWIOneWireResource::set_sensor_index(int p_sensor_index) {
        sensor_index = static_cast<uint8_t>(p_sensor_index);
    }

    int HDWIOneWireResource::get_sensor_index() const {
        return static_cast<int>(sensor_index);
    }

    void HDWIOneWireResource::set_read_buffer(PackedByteArray p_buffer) {
        read_buffer = p_buffer;
    }

    PackedByteArray HDWIOneWireResource::get_read_buffer() const {
        return read_buffer;
    }

    // Group body for 1-Wire (D050): [type][count] followed by count × 32-byte
    // null-padded sensor names. The kernel assigns sensor_index sequentially as it
    // walks these entries, so we emit them in sensor_index order to keep position
    // and sensor_index in sync.
    PackedByteArray HDWIOneWireResource::get_group_type_representation() {
        PackedByteArray group_representation;
        group_representation.resize(2);
        group_representation.encode_u8(0, static_cast<uint8_t>(HDWIType::ONEWIRE));

        if (onewire_resources == nullptr) {
            group_representation.encode_u8(1, 0);
            return group_representation;
        }

        std::vector<const HDWIOneWireResource*> sorted;
        for (const godot::Variant &resource_variant : *HDWIOneWireResource::onewire_resources) {
            sorted.push_back(Object::cast_to<HDWIOneWireResource>(resource_variant));
        }
        std::sort(sorted.begin(), sorted.end(),
            [](const HDWIOneWireResource *a, const HDWIOneWireResource *b) {
                return a->sensor_index < b->sensor_index;
            });

        group_representation.encode_u8(1, static_cast<uint8_t>(sorted.size()));
        for (const HDWIOneWireResource *resource : sorted) {
            group_representation.append_array(resource->get_device_representation());
        }

        return group_representation;
    }

    // 1-Wire entries carry only the 32-byte name — no extra per-sensor fields.
    PackedByteArray HDWIOneWireResource::get_device_representation() const {
        return HDWIResource::get_device_representation();
    }

    void HDWIOneWireResource::dispatch_action(PackedByteArray request_data) {
        // request_data = sim_w1_request_t (1 byte): [0] action. The dev_id (sensor
        // index) was already stripped and used for routing by the registry.
        if (request_data.size() < 1) {
            return;
        }
        uint8_t action = request_data.decode_u8(0);

        switch (action) {
            case ONEWIRE_READ_TEMPERATURE:
            case ONEWIRE_READ_SLAVE:
            case ONEWIRE_READ_RAW:
                // All reads share one path; the action tells GDScript which sysfs
                // attribute the FSW opened, so it can fill read_buffer in the format
                // that file expects (millidegrees ASCII / w1_slave / raw bytes).
                handle_onewire_read(action);
                break;
            default:
                break;
        }
    }

    void HDWIOneWireResource::handle_onewire_read(uint8_t action) {
        // Let GDScript fill read_buffer synchronously before we ship it.
        emit_signal("on_onewire_read", (int)sensor_index, (int)action);

        // Response carries no dev_id — the kernel knows which sensor it asked for.
        PacketCPP *packet = memnew(PacketCPP);
        packet->generate(CmdType::ACTION, HDWIType::ONEWIRE, sim_time_ns, read_buffer);
        emit_signal("on_send", packet->convert_to_bytes());
        memdelete(packet);
    }

}
