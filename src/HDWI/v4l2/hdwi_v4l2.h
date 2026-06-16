#ifndef HDWI_V4L2_H
#define HDWI_V4L2_H

#include "../hdwi.h"
#include "../../IPC/sim_packet_type.h"
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/classes/image.hpp>
#include <unordered_map>

namespace godot {

    // HDWIV4L2Resource — one virtual V4L2 capture device the FSW opens as
    // /dev/video<video_index> (a USB camera).
    //
    // V4L2 is the one peripheral whose data flow is REVERSED. Every other resource
    // answers a kernel-initiated read on the forward channel; this one is a SOURCE.
    // A frame originates engine-side (a Godot node renders/loads an image) and is
    // PUSHED to the kernel over the reverse channel (EventClient :7778) as a
    // CMD_EVENT / SIM_EVENT_FRAME packet. The kernel hands it to the FSW as the
    // next dequeued capture buffer.
    //
    // The resource never frames the wire header itself: it emits on_event with
    // (hdwi_type, device_id, event, payload) and the GDScript comm layer
    // (ComponentRegistry.send_event -> EventClient) stamps the simcall + event
    // headers and ships it. payload = v4l2_frame_hdr_t + raw RGB24 pixels.
    //
    // Interface (push-only):
    //   1. set video_index (the /dev/video minor and the event device_id, assigned
    //      positionally during sync so it must match emission order) and device_name
    //   2. init() to register the device
    //   3. flip streaming on when a consumer wants frames, off when it doesn't —
    //      send_image()/send_frame() are no-ops while it is off, so producers can
    //      call them unconditionally without flooding the link
    //   4. call send_image(img) from any node to push a frame; the registry forwards
    //      it out the reverse channel
    //
    // The FSW can also gate streaming itself: VIDIOC_STREAMON/STREAMOFF arrive as
    // forward-channel control actions and toggle `streaming` (emitting
    // on_stream_state), so a consumer can drive frame production off that signal.
    //
    // Full reference: doc_classes/HDWIV4L2Resource.xml
    class HDWIV4L2Resource : public HDWIResource {
        GDCLASS(HDWIV4L2Resource, HDWIResource)
        private:
            HDWIType type = HDWIType::V4L2;
            // Monotonic counter stamped into every frame's v4l2_frame_hdr_t.sequence.
            uint32_t frame_sequence = 0;

        protected:
            static void _bind_methods();
            static TypedArray<HDWIV4L2Resource> *v4l2_resources;
            // Key: video_index → 0-based index in v4l2_resources.
            static std::unordered_map<uint8_t, int> video_index_to_device_id;

        public:
            HDWIV4L2Resource() = default;
            ~HDWIV4L2Resource() = default;

            // Natural identifier on the wire (inner V4L2 header video_index), the
            // /dev/video<video_index> minor, and the reverse-channel event device_id.
            // Assigned by position during sync, so emission order must match.
            uint8_t video_index = 0;
            void set_video_index(int p_video_index);
            int  get_video_index() const;

            // Default frame geometry advertised at registration so the kernel can seed
            // VIDIOC_G_FMT before the first frame lands. Per-frame width/height still
            // travel in each v4l2_frame_hdr_t, so these are only the initial guess.
            uint32_t frame_width = 640;
            void set_frame_width(int p_width);
            int  get_frame_width() const;

            uint32_t frame_height = 480;
            void set_frame_height(int p_height);
            int  get_frame_height() const;

            // The streaming gate. While false, send_image()/send_frame() drop the
            // frame and emit nothing — producers may call them every render tick
            // without burning the reverse channel. Toggled engine-side or by the FSW
            // via VIDIOC_STREAMON/STREAMOFF.
            bool streaming = false;
            void set_streaming(bool p_streaming);
            bool is_streaming() const;

            virtual void init() override {
                if (v4l2_resources == nullptr) {
                    v4l2_resources = new TypedArray<HDWIV4L2Resource>();
                }
                v4l2_resources->append(this);
                video_index_to_device_id[video_index] = v4l2_resources->size() - 1;
            }

            virtual void clear() override {
                v4l2_resources->erase(this);
            }

            // Push one frame to the FSW. Converts to RGB24 if needed, frames it, and
            // emits on_event for the comm layer to ship. No-op while !streaming.
            void send_image(const Ref<Image> &p_image);
            // Raw escape hatch: ship pre-encoded pixel bytes with an explicit FourCC.
            // No-op while !streaming. width*height must agree with the pixel layout.
            void send_frame(PackedByteArray p_pixels, int p_width, int p_height, uint32_t p_pixfmt);

            // Async error IRQs (SIM_EVENT_IRQ), NOT gated on streaming. Inject these to
            // exercise the FSW's error handling:
            //   send_frame_drop() — kernel errors one queued buffer and bumps sequence
            //   send_overflow()   — kernel logs an overflow (no buffer action)
            //   send_error(errno) — kernel stops the stream and flushes queued buffers
            //                       as errors (pass a negative errno, e.g. -5 for -EIO);
            //                       also flips streaming off engine-side
            void send_frame_drop();
            void send_overflow();
            void send_error(int error_code);

            // Forward-channel control (VIDIOC_STREAMON/STREAMOFF). V4L2 never answers
            // a read, so this only toggles streaming and notifies via on_stream_state.
            // pid is accepted for interface uniformity but unused (no reply is sent).
            virtual void dispatch_action(PackedByteArray request_data, uint32_t pid) override;

            static PackedByteArray get_group_type_representation();
            PackedByteArray get_device_representation() const override;

            // Returns 0-based index in v4l2_resources for the device identified by id;
            // -1 if not registered.
            static int v4l2_dev_id_to_device_id(v4l2_dev_id_t id);
            // GDScript-callable variant.
            static int lookup_v4l2_device_id(int p_video_index);
    };
}

#endif
