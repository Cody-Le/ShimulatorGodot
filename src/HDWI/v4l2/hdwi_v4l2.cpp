#include "./hdwi_v4l2.h"
#include <algorithm>
#include <vector>

namespace godot {

    TypedArray<HDWIV4L2Resource> *HDWIV4L2Resource::v4l2_resources = nullptr;
    std::unordered_map<uint8_t, int> HDWIV4L2Resource::video_index_to_device_id;

    int HDWIV4L2Resource::v4l2_dev_id_to_device_id(v4l2_dev_id_t id) {
        auto it = video_index_to_device_id.find(id.video_index);
        if (it == video_index_to_device_id.end()) return -1;
        return it->second;
    }

    int HDWIV4L2Resource::lookup_v4l2_device_id(int p_video_index) {
        auto it = video_index_to_device_id.find(static_cast<uint8_t>(p_video_index));
        if (it == video_index_to_device_id.end()) return -1;
        return it->second;
    }

    void HDWIV4L2Resource::_bind_methods() {
        HDWIResource::_bind_methods();

        ClassDB::bind_method(D_METHOD("init"), &HDWIV4L2Resource::init);
        ClassDB::bind_method(D_METHOD("clear"), &HDWIV4L2Resource::clear);

        ClassDB::bind_method(D_METHOD("set_video_index", "video_index"), &HDWIV4L2Resource::set_video_index);
        ClassDB::bind_method(D_METHOD("get_video_index"), &HDWIV4L2Resource::get_video_index);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "video_index"), "set_video_index", "get_video_index");

        ClassDB::bind_method(D_METHOD("set_frame_width", "frame_width"), &HDWIV4L2Resource::set_frame_width);
        ClassDB::bind_method(D_METHOD("get_frame_width"), &HDWIV4L2Resource::get_frame_width);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "frame_width"), "set_frame_width", "get_frame_width");

        ClassDB::bind_method(D_METHOD("set_frame_height", "frame_height"), &HDWIV4L2Resource::set_frame_height);
        ClassDB::bind_method(D_METHOD("get_frame_height"), &HDWIV4L2Resource::get_frame_height);
        ADD_PROPERTY(PropertyInfo(Variant::INT, "frame_height"), "set_frame_height", "get_frame_height");

        ClassDB::bind_method(D_METHOD("set_streaming", "streaming"), &HDWIV4L2Resource::set_streaming);
        ClassDB::bind_method(D_METHOD("is_streaming"), &HDWIV4L2Resource::is_streaming);
        ADD_PROPERTY(PropertyInfo(Variant::BOOL, "streaming"), "set_streaming", "is_streaming");

        ClassDB::bind_method(D_METHOD("send_image", "image"), &HDWIV4L2Resource::send_image);
        ClassDB::bind_method(D_METHOD("send_frame", "pixels", "width", "height", "pixfmt"), &HDWIV4L2Resource::send_frame);

        ClassDB::bind_method(D_METHOD("dispatch_action", "request_data"), &HDWIV4L2Resource::dispatch_action);
        ClassDB::bind_method(D_METHOD("get_device_representation"), &HDWIV4L2Resource::get_device_representation);
        ClassDB::bind_static_method("HDWIV4L2Resource", D_METHOD("get_group_type_representation"), &HDWIV4L2Resource::get_group_type_representation);
        ClassDB::bind_static_method("HDWIV4L2Resource", D_METHOD("lookup_v4l2_device_id", "video_index"), &HDWIV4L2Resource::lookup_v4l2_device_id);

        ClassDB::bind_method(D_METHOD("send_frame_drop"), &HDWIV4L2Resource::send_frame_drop);
        ClassDB::bind_method(D_METHOD("send_overflow"), &HDWIV4L2Resource::send_overflow);
        ClassDB::bind_method(D_METHOD("send_error", "error_code"), &HDWIV4L2Resource::send_error);

        BIND_CONSTANT(V4L2_STREAM_ON);
        BIND_CONSTANT(V4L2_STREAM_OFF);

        // on_event (the reverse-channel push the registry ships out :7778) is declared
        // on the HDWIResource base; frames travel through it as SIM_EVENT_FRAME and
        // the error IRQs below as SIM_EVENT_IRQ.
        // Emitted when the FSW toggles streaming via VIDIOC_STREAMON/STREAMOFF, so a
        // node can start/stop producing frames in response.
        ADD_SIGNAL(MethodInfo("on_stream_state",
            PropertyInfo(Variant::INT, "video_index"),
            PropertyInfo(Variant::BOOL, "streaming")));
    }

    void HDWIV4L2Resource::set_video_index(int p_video_index) {
        video_index = static_cast<uint8_t>(p_video_index);
    }

    int HDWIV4L2Resource::get_video_index() const {
        return static_cast<int>(video_index);
    }

    void HDWIV4L2Resource::set_frame_width(int p_width) {
        frame_width = static_cast<uint32_t>(p_width);
    }

    int HDWIV4L2Resource::get_frame_width() const {
        return static_cast<int>(frame_width);
    }

    void HDWIV4L2Resource::set_frame_height(int p_height) {
        frame_height = static_cast<uint32_t>(p_height);
    }

    int HDWIV4L2Resource::get_frame_height() const {
        return static_cast<int>(frame_height);
    }

    void HDWIV4L2Resource::set_streaming(bool p_streaming) {
        streaming = p_streaming;
    }

    bool HDWIV4L2Resource::is_streaming() const {
        return streaming;
    }

    void HDWIV4L2Resource::send_image(const Ref<Image> &p_image) {
        // Gate first so a producer can call this every render tick for free while a
        // consumer isn't asking for frames.
        if (!streaming || p_image.is_null()) {
            return;
        }

        // Normalize to RGB24 (V4L2_PIX_FMT_RGB24) without mutating the caller's image.
        Ref<Image> img = p_image;
        if (img->get_format() != Image::FORMAT_RGB8) {
            Ref<Image> converted;
            converted.instantiate();
            converted->copy_from(p_image);
            converted->convert(Image::FORMAT_RGB8);
            img = converted;
        }

        send_frame(img->get_data(), img->get_width(), img->get_height(), V4L2_PIX_FMT_RGB24_SIM);
    }

    void HDWIV4L2Resource::send_frame(PackedByteArray p_pixels, int p_width, int p_height, uint32_t p_pixfmt) {
        if (!streaming) {
            return;
        }

        // payload = v4l2_frame_hdr_t (20B) + raw pixels. The comm layer prepends the
        // sim_event_hdr_t and simcall header before it hits the socket.
        PackedByteArray payload;
        payload.resize(sizeof(v4l2_frame_hdr_t));
        payload.encode_u32(0,  static_cast<uint32_t>(p_width));
        payload.encode_u32(4,  static_cast<uint32_t>(p_height));
        payload.encode_u32(8,  p_pixfmt);
        payload.encode_u32(12, static_cast<uint32_t>(p_pixels.size()));
        payload.encode_u32(16, frame_sequence++);
        payload.append_array(p_pixels);

        emit_signal("on_event", (int)HDWIType::V4L2, (int)video_index, (int)SIM_EVENT_FRAME, payload);
    }

    // Reverse-channel error/control IRQs (SIM_EVENT_IRQ). Unlike frames, these are
    // not gated on `streaming` — V4L2_IRQ_ERROR is precisely how you abort a stuck
    // stream. Payload is sim_v4l2_irq_payload_t (9 bytes): [irq][sequence u32][error_code i32].
    static PackedByteArray build_v4l2_irq(uint8_t irq, uint32_t sequence, int32_t error_code) {
        PackedByteArray payload;
        payload.resize(sizeof(sim_v4l2_irq_payload_t));  // 9
        payload.encode_u8(0, irq);
        payload.encode_u32(1, sequence);
        payload.encode_s32(5, error_code);
        return payload;
    }

    void HDWIV4L2Resource::send_frame_drop() {
        // The kernel errors one queued buffer and advances its own sequence; report
        // the sequence we would have stamped next and keep ours in lockstep.
        emit_irq(HDWIType::V4L2, video_index, build_v4l2_irq(V4L2_IRQ_FRAME_DROP, frame_sequence++, 0));
    }

    void HDWIV4L2Resource::send_overflow() {
        emit_irq(HDWIType::V4L2, video_index, build_v4l2_irq(V4L2_IRQ_OVERFLOW, 0, 0));
    }

    void HDWIV4L2Resource::send_error(int error_code) {
        // Mirror the kernel: streaming halts and queued buffers are flushed as errors.
        // Reflect that locally so producers stop pushing frames into a dead stream.
        emit_irq(HDWIType::V4L2, video_index, build_v4l2_irq(V4L2_IRQ_ERROR, 0, (int32_t)error_code));
        if (streaming) {
            streaming = false;
            emit_signal("on_stream_state", (int)video_index, false);
        }
    }

    void HDWIV4L2Resource::dispatch_action(PackedByteArray request_data) {
        // request_data = [0] action (dev_id stripped by the registry). V4L2 is a
        // source — it never replies — so the only forward-channel traffic is the
        // FSW gating the stream.
        if (request_data.size() < 1) {
            return;
        }
        uint8_t action = request_data.decode_u8(0);

        switch (action) {
            case V4L2_STREAM_ON:
                streaming = true;
                emit_signal("on_stream_state", (int)video_index, true);
                break;
            case V4L2_STREAM_OFF:
                streaming = false;
                emit_signal("on_stream_state", (int)video_index, false);
                break;
            default:
                break;
        }
    }

    // Group body for V4L2 (D050): [type][count] followed by count device entries,
    // emitted in video_index order so kernel-assigned indices line up.
    PackedByteArray HDWIV4L2Resource::get_group_type_representation() {
        PackedByteArray group_representation;
        group_representation.resize(2);
        group_representation.encode_u8(0, static_cast<uint8_t>(HDWIType::V4L2));

        if (v4l2_resources == nullptr) {
            group_representation.encode_u8(1, 0);
            return group_representation;
        }

        std::vector<const HDWIV4L2Resource*> sorted;
        for (const godot::Variant &resource_variant : *HDWIV4L2Resource::v4l2_resources) {
            sorted.push_back(Object::cast_to<HDWIV4L2Resource>(resource_variant));
        }
        std::sort(sorted.begin(), sorted.end(),
            [](const HDWIV4L2Resource *a, const HDWIV4L2Resource *b) {
                return a->video_index < b->video_index;
            });

        group_representation.encode_u8(1, static_cast<uint8_t>(sorted.size()));
        for (const HDWIV4L2Resource *resource : sorted) {
            group_representation.append_array(resource->get_device_representation());
        }

        return group_representation;
    }

    // V4L2 entries carry the 32-byte name plus the default format the kernel seeds
    // VIDIOC_G_FMT with: [width u32][height u32][pixfmt u32].
    PackedByteArray HDWIV4L2Resource::get_device_representation() const {
        PackedByteArray representation = HDWIResource::get_device_representation();
        uint64_t base_size = representation.size();
        representation.resize(base_size + sizeof(uint32_t) * 3);
        representation.encode_u32(base_size,     frame_width);
        representation.encode_u32(base_size + 4, frame_height);
        representation.encode_u32(base_size + 8, V4L2_PIX_FMT_RGB24_SIM);
        return representation;
    }

}
