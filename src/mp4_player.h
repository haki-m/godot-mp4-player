#ifndef MP4_PLAYER_H
#define MP4_PLAYER_H

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/string.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

using namespace godot;

class MP4Player : public Node {
    GDCLASS(MP4Player, Node);

private:
    String current_path;
    int video_width;
    int video_height;
    double duration_seconds;
    double video_fps;
    double frame_time;
    double time_accumulator;
    bool end_of_video;
    bool loop_enabled;

    bool _ensure_swr_for_filtered_frame();

    bool playing;
    double playback_speed;
    double current_position_seconds;

    Ref<ImageTexture> frame_texture;

    AVFormatContext* format_ctx;
    AVCodecContext* video_codec_ctx;
    AVCodecContext* audio_codec_ctx;
    AVPacket* packet;
    AVFrame* video_frame;
    AVFrame* rgba_frame;
    AVFrame* audio_frame;
    AVFrame* filtered_audio_frame;
    SwsContext* sws_ctx;
    SwrContext* swr_ctx;
    uint8_t* rgba_buffer;
    int video_stream_index;
    int audio_stream_index;
    bool file_opened;

    AVFilterGraph* audio_filter_graph;
    AVFilterContext* audio_buffersrc_ctx;
    AVFilterContext* audio_buffersink_ctx;

    int audio_sample_rate;
    int audio_channels;
    PackedVector2Array pending_audio_frames;

    void _cleanup_ffmpeg();
    bool _create_or_update_texture_from_video_frame();
    bool _reopen_current_file();

    bool _open_video_stream();
    bool _open_audio_stream();
    bool _build_audio_filter_graph();
    void _destroy_audio_filter_graph();

    bool _decode_video_from_packet();
    bool _decode_audio_from_packet();

public:
    static void _bind_methods();

    MP4Player();
    ~MP4Player();

    void _ready() override;

    bool open_file(const String& path);
    bool decode_next_frame();
    bool update(double delta);
    void close_file();

    void play();
    void pause();
    void stop();
    bool seek(double seconds);

    void set_loop_enabled(bool enabled);
    bool is_loop_enabled() const;

    void set_playback_speed(double speed);
    double get_playback_speed() const;

    bool is_playing() const;
    double get_position_seconds() const;

    Ref<ImageTexture> get_frame_texture() const;
    String get_current_path() const;
    int get_video_width() const;
    int get_video_height() const;
    double get_duration_seconds() const;
    double get_video_fps() const;

    int get_audio_sample_rate() const;
    int get_audio_channels() const;
    PackedVector2Array consume_audio_frames();
};

#endif