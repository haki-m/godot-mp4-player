#include "mp4_player.h"

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void MP4Player::_bind_methods() {
    ClassDB::bind_method(D_METHOD("open_file", "path"), &MP4Player::open_file);
    ClassDB::bind_method(D_METHOD("decode_next_frame"), &MP4Player::decode_next_frame);
    ClassDB::bind_method(D_METHOD("update", "delta"), &MP4Player::update);
    ClassDB::bind_method(D_METHOD("close_file"), &MP4Player::close_file);

    ClassDB::bind_method(D_METHOD("play"), &MP4Player::play);
    ClassDB::bind_method(D_METHOD("pause"), &MP4Player::pause);
    ClassDB::bind_method(D_METHOD("stop"), &MP4Player::stop);
    ClassDB::bind_method(D_METHOD("seek", "seconds"), &MP4Player::seek);

    ClassDB::bind_method(D_METHOD("set_loop_enabled", "enabled"), &MP4Player::set_loop_enabled);
    ClassDB::bind_method(D_METHOD("is_loop_enabled"), &MP4Player::is_loop_enabled);

    ClassDB::bind_method(D_METHOD("set_playback_speed", "speed"), &MP4Player::set_playback_speed);
    ClassDB::bind_method(D_METHOD("get_playback_speed"), &MP4Player::get_playback_speed);

    ClassDB::bind_method(D_METHOD("is_playing"), &MP4Player::is_playing);
    ClassDB::bind_method(D_METHOD("get_position_seconds"), &MP4Player::get_position_seconds);

    ClassDB::bind_method(D_METHOD("get_frame_texture"), &MP4Player::get_frame_texture);
    ClassDB::bind_method(D_METHOD("get_current_path"), &MP4Player::get_current_path);
    ClassDB::bind_method(D_METHOD("get_video_width"), &MP4Player::get_video_width);
    ClassDB::bind_method(D_METHOD("get_video_height"), &MP4Player::get_video_height);
    ClassDB::bind_method(D_METHOD("get_duration_seconds"), &MP4Player::get_duration_seconds);
    ClassDB::bind_method(D_METHOD("get_video_fps"), &MP4Player::get_video_fps);
    ClassDB::bind_method(D_METHOD("get_audio_sample_rate"), &MP4Player::get_audio_sample_rate);
    ClassDB::bind_method(D_METHOD("get_audio_channels"), &MP4Player::get_audio_channels);
    ClassDB::bind_method(D_METHOD("consume_audio_frames"), &MP4Player::consume_audio_frames);

    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop_enabled"), "set_loop_enabled", "is_loop_enabled");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "playback_speed"), "set_playback_speed", "get_playback_speed");
}

MP4Player::MP4Player() {
    current_path = "";
    video_width = 0;
    video_height = 0;
    duration_seconds = 0.0;
    video_fps = 30.0;
    frame_time = 1.0 / 30.0;
    time_accumulator = 0.0;
    end_of_video = false;
    loop_enabled = false;

    playing = false;
    playback_speed = 1.0;
    current_position_seconds = 0.0;

    format_ctx = nullptr;
    video_codec_ctx = nullptr;
    audio_codec_ctx = nullptr;
    packet = nullptr;
    video_frame = nullptr;
    rgba_frame = nullptr;
    audio_frame = nullptr;
    filtered_audio_frame = nullptr;
    sws_ctx = nullptr;
    swr_ctx = nullptr;
    rgba_buffer = nullptr;
    video_stream_index = -1;
    audio_stream_index = -1;
    file_opened = false;

    audio_filter_graph = nullptr;
    audio_buffersrc_ctx = nullptr;
    audio_buffersink_ctx = nullptr;

    audio_sample_rate = 48000;
    audio_channels = 2;

    UtilityFunctions::print("MP4Player constructor");
}

MP4Player::~MP4Player() {
    close_file();
    UtilityFunctions::print("MP4Player destructor");
}

void MP4Player::_ready() {
    UtilityFunctions::print("MP4Player is ready");
}

void MP4Player::_destroy_audio_filter_graph() {
    if (audio_filter_graph) {
        avfilter_graph_free(&audio_filter_graph);
        audio_filter_graph = nullptr;
    }
    audio_buffersrc_ctx = nullptr;
    audio_buffersink_ctx = nullptr;
}

void MP4Player::_cleanup_ffmpeg() {
    _destroy_audio_filter_graph();

    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = nullptr;
    }

    if (swr_ctx) {
        swr_free(&swr_ctx);
    }

    if (rgba_buffer) {
        av_free(rgba_buffer);
        rgba_buffer = nullptr;
    }

    if (rgba_frame) {
        av_frame_free(&rgba_frame);
    }

    if (video_frame) {
        av_frame_free(&video_frame);
    }

    if (audio_frame) {
        av_frame_free(&audio_frame);
    }

    if (filtered_audio_frame) {
        av_frame_free(&filtered_audio_frame);
    }

    if (packet) {
        av_packet_free(&packet);
    }

    if (video_codec_ctx) {
        avcodec_free_context(&video_codec_ctx);
    }

    if (audio_codec_ctx) {
        avcodec_free_context(&audio_codec_ctx);
    }

    if (format_ctx) {
        avformat_close_input(&format_ctx);
    }

    video_stream_index = -1;
    audio_stream_index = -1;
    file_opened = false;
    end_of_video = false;
    pending_audio_frames.clear();
}

void MP4Player::close_file() {
    _cleanup_ffmpeg();

    current_path = "";
    video_width = 0;
    video_height = 0;
    duration_seconds = 0.0;
    video_fps = 30.0;
    frame_time = 1.0 / 30.0;
    time_accumulator = 0.0;
    frame_texture.unref();

    playing = false;
    playback_speed = 1.0;
    current_position_seconds = 0.0;

    audio_sample_rate = 48000;
    audio_channels = 2;

    UtilityFunctions::print("MP4Player file closed");
}

bool MP4Player::_reopen_current_file() {
    if (current_path.is_empty()) {
        return false;
    }

    String path = current_path;
    bool was_playing = playing;
    bool ok = open_file(path);
    if (ok && was_playing) {
        play();
    }
    return ok;
}

bool MP4Player::_open_video_stream() {
    for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = (int)i;
            break;
        }
    }

    if (video_stream_index == -1) {
        UtilityFunctions::print("No video stream found");
        return false;
    }

    AVStream* video_stream = format_ctx->streams[video_stream_index];
    const AVCodec* decoder = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!decoder) {
        UtilityFunctions::print("Video decoder not found");
        return false;
    }

    video_codec_ctx = avcodec_alloc_context3(decoder);
    if (!video_codec_ctx) {
        UtilityFunctions::print("Failed to allocate video codec context");
        return false;
    }

    if (avcodec_parameters_to_context(video_codec_ctx, video_stream->codecpar) < 0) {
        UtilityFunctions::print("Failed to copy video codec parameters");
        return false;
    }

    if (avcodec_open2(video_codec_ctx, decoder, nullptr) < 0) {
        UtilityFunctions::print("Failed to open video decoder");
        return false;
    }

    video_width = video_codec_ctx->width;
    video_height = video_codec_ctx->height;

    AVRational fps_rational = video_stream->avg_frame_rate;
    if (fps_rational.num > 0 && fps_rational.den > 0) {
        video_fps = av_q2d(fps_rational);
    }
    else if (video_stream->r_frame_rate.num > 0 && video_stream->r_frame_rate.den > 0) {
        video_fps = av_q2d(video_stream->r_frame_rate);
    }
    else {
        video_fps = 30.0;
    }

    if (video_fps <= 0.0) {
        video_fps = 30.0;
    }

    frame_time = 1.0 / video_fps;

    int buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGBA, video_width, video_height, 1);
    rgba_buffer = (uint8_t*)av_malloc(buffer_size);
    if (!rgba_buffer) {
        UtilityFunctions::print("Failed to allocate RGBA buffer");
        return false;
    }

    av_image_fill_arrays(
        rgba_frame->data,
        rgba_frame->linesize,
        rgba_buffer,
        AV_PIX_FMT_RGBA,
        video_width,
        video_height,
        1
    );

    sws_ctx = sws_getContext(
        video_width,
        video_height,
        video_codec_ctx->pix_fmt,
        video_width,
        video_height,
        AV_PIX_FMT_RGBA,
        SWS_BILINEAR,
        nullptr,
        nullptr,
        nullptr
    );

    if (!sws_ctx) {
        UtilityFunctions::print("Failed to create video sws context");
        return false;
    }

    return true;
}

bool MP4Player::_build_audio_filter_graph() {
    _destroy_audio_filter_graph();

    if (!audio_codec_ctx) {
        return false;
    }

    const AVFilter* abuffer = avfilter_get_by_name("abuffer");
    const AVFilter* atempo = avfilter_get_by_name("atempo");
    const AVFilter* abuffersink = avfilter_get_by_name("abuffersink");

    if (!abuffer || !atempo || !abuffersink) {
        UtilityFunctions::print("Failed to get audio filters");
        return false;
    }

    audio_filter_graph = avfilter_graph_alloc();
    if (!audio_filter_graph) {
        UtilityFunctions::print("Failed to allocate audio filter graph");
        return false;
    }

    char args[512];
    char layout_desc[128] = { 0 };

    if (av_channel_layout_describe(&audio_codec_ctx->ch_layout, layout_desc, sizeof(layout_desc)) < 0) {
        snprintf(layout_desc, sizeof(layout_desc), "%s", "stereo");
    }

    snprintf(
        args,
        sizeof(args),
        "time_base=1/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
        audio_codec_ctx->sample_rate,
        audio_codec_ctx->sample_rate,
        av_get_sample_fmt_name(audio_codec_ctx->sample_fmt),
        layout_desc
    );

    if (avfilter_graph_create_filter(&audio_buffersrc_ctx, abuffer, "in", args, nullptr, audio_filter_graph) < 0) {
        UtilityFunctions::print("Failed to create abuffer");
        return false;
    }

    char tempo_args[64];
    snprintf(tempo_args, sizeof(tempo_args), "%f", playback_speed);

    AVFilterContext* atempo_ctx = nullptr;
    if (avfilter_graph_create_filter(&atempo_ctx, atempo, "atempo", tempo_args, nullptr, audio_filter_graph) < 0) {
        UtilityFunctions::print("Failed to create atempo");
        return false;
    }

    if (avfilter_graph_create_filter(&audio_buffersink_ctx, abuffersink, "out", nullptr, nullptr, audio_filter_graph) < 0) {
        UtilityFunctions::print("Failed to create abuffersink");
        return false;
    }

    if (avfilter_link(audio_buffersrc_ctx, 0, atempo_ctx, 0) < 0) {
        UtilityFunctions::print("Failed to link abuffer -> atempo");
        return false;
    }

    if (avfilter_link(atempo_ctx, 0, audio_buffersink_ctx, 0) < 0) {
        UtilityFunctions::print("Failed to link atempo -> sink");
        return false;
    }

    if (avfilter_graph_config(audio_filter_graph, nullptr) < 0) {
        UtilityFunctions::print("Failed to config audio filter graph");
        return false;
    }

    UtilityFunctions::print("Audio filter graph built with atempo=", playback_speed);
    return true;
}
bool MP4Player::_ensure_swr_for_filtered_frame() {
    if (!filtered_audio_frame) {
        return false;
    }

    if (swr_ctx) {
        swr_free(&swr_ctx);
        swr_ctx = nullptr;
    }

    AVChannelLayout out_layout;
    av_channel_layout_default(&out_layout, 2);

    AVChannelLayout in_layout;
    if (filtered_audio_frame->ch_layout.nb_channels > 0) {
        if (av_channel_layout_copy(&in_layout, &filtered_audio_frame->ch_layout) < 0) {
            av_channel_layout_uninit(&out_layout);
            return false;
        }
    }
    else {
        av_channel_layout_default(&in_layout, 2);
    }

    int in_sample_rate = filtered_audio_frame->sample_rate > 0
        ? filtered_audio_frame->sample_rate
        : audio_sample_rate;

    AVSampleFormat in_sample_fmt = (AVSampleFormat)filtered_audio_frame->format;

    if (swr_alloc_set_opts2(
        &swr_ctx,
        &out_layout,
        AV_SAMPLE_FMT_FLT,
        audio_sample_rate,
        &in_layout,
        in_sample_fmt,
        in_sample_rate,
        0,
        nullptr) < 0) {
        av_channel_layout_uninit(&in_layout);
        av_channel_layout_uninit(&out_layout);
        UtilityFunctions::print("Failed to allocate swr context for filtered frame");
        return false;
    }

    if (swr_init(swr_ctx) < 0) {
        av_channel_layout_uninit(&in_layout);
        av_channel_layout_uninit(&out_layout);
        UtilityFunctions::print("Failed to init swr context for filtered frame");
        swr_free(&swr_ctx);
        return false;
    }

    av_channel_layout_uninit(&in_layout);
    av_channel_layout_uninit(&out_layout);
    return true;
}
bool MP4Player::_open_audio_stream() {
    for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index = (int)i;
            break;
        }
    }

    if (audio_stream_index == -1) {
        UtilityFunctions::print("No audio stream found");
        return true;
    }

    AVStream* audio_stream = format_ctx->streams[audio_stream_index];
    const AVCodec* decoder = avcodec_find_decoder(audio_stream->codecpar->codec_id);
    if (!decoder) {
        UtilityFunctions::print("Audio decoder not found");
        audio_stream_index = -1;
        return true;
    }

    audio_codec_ctx = avcodec_alloc_context3(decoder);
    if (!audio_codec_ctx) {
        UtilityFunctions::print("Failed to allocate audio codec context");
        audio_stream_index = -1;
        return true;
    }

    if (avcodec_parameters_to_context(audio_codec_ctx, audio_stream->codecpar) < 0) {
        UtilityFunctions::print("Failed to copy audio codec parameters");
        audio_stream_index = -1;
        return true;
    }

    if (avcodec_open2(audio_codec_ctx, decoder, nullptr) < 0) {
        UtilityFunctions::print("Failed to open audio decoder");
        audio_stream_index = -1;
        return true;
    }

    audio_sample_rate = audio_codec_ctx->sample_rate > 0 ? audio_codec_ctx->sample_rate : 48000;
    audio_channels = 2;

    

    if (!_build_audio_filter_graph()) {
        UtilityFunctions::print("Failed to build audio filter graph");
        audio_stream_index = -1;
        return true;
    }

    UtilityFunctions::print("Audio ready: sample_rate=", audio_sample_rate, " channels=", audio_channels, " speed=", playback_speed);
    return true;
}

bool MP4Player::open_file(const String& path) {
    close_file();

    current_path = path;
    time_accumulator = 0.0;
    end_of_video = false;
    current_position_seconds = 0.0;

    String absolute_path = ProjectSettings::get_singleton()->globalize_path(path);
    CharString utf8_path = absolute_path.utf8();

    if (avformat_open_input(&format_ctx, utf8_path.get_data(), nullptr, nullptr) < 0) {
        UtilityFunctions::print("FFmpeg failed to open file: ", absolute_path);
        _cleanup_ffmpeg();
        return false;
    }

    if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
        UtilityFunctions::print("FFmpeg failed to read stream info");
        _cleanup_ffmpeg();
        return false;
    }

    packet = av_packet_alloc();
    video_frame = av_frame_alloc();
    rgba_frame = av_frame_alloc();
    audio_frame = av_frame_alloc();
    filtered_audio_frame = av_frame_alloc();

    if (!packet || !video_frame || !rgba_frame || !audio_frame || !filtered_audio_frame) {
        UtilityFunctions::print("Failed to allocate packet/frame objects");
        _cleanup_ffmpeg();
        return false;
    }

    if (!_open_video_stream()) {
        _cleanup_ffmpeg();
        return false;
    }

    _open_audio_stream();

    if (format_ctx->duration != AV_NOPTS_VALUE) {
        duration_seconds = (double)format_ctx->duration / AV_TIME_BASE;
    }

    file_opened = true;

    UtilityFunctions::print("Opened file: ", absolute_path);
    UtilityFunctions::print("Width: ", video_width);
    UtilityFunctions::print("Height: ", video_height);
    UtilityFunctions::print("Duration: ", duration_seconds);
    UtilityFunctions::print("FPS: ", video_fps);

    return true;
}

bool MP4Player::_create_or_update_texture_from_video_frame() {
    if (!video_frame || !rgba_frame || !sws_ctx) {
        return false;
    }

    sws_scale(
        sws_ctx,
        video_frame->data,
        video_frame->linesize,
        0,
        video_frame->height,
        rgba_frame->data,
        rgba_frame->linesize
    );

    PackedByteArray bytes;
    bytes.resize(video_frame->width * video_frame->height * 4);
    memcpy(bytes.ptrw(), rgba_buffer, video_frame->width * video_frame->height * 4);

    Ref<Image> image = memnew(Image);
    image->set_data(video_frame->width, video_frame->height, false, Image::FORMAT_RGBA8, bytes);

    if (frame_texture.is_null()) {
        frame_texture = ImageTexture::create_from_image(image);
    }
    else {
        Ref<Image> current_image = frame_texture->get_image();
        if (current_image.is_null() ||
            current_image->get_width() != video_frame->width ||
            current_image->get_height() != video_frame->height) {
            frame_texture = ImageTexture::create_from_image(image);
        }
        else {
            frame_texture->update(image);
        }
    }

    return true;
}

bool MP4Player::_decode_video_from_packet() {
    int send_result = avcodec_send_packet(video_codec_ctx, packet);
    av_packet_unref(packet);

    if (send_result < 0) {
        return false;
    }

    int receive_result = avcodec_receive_frame(video_codec_ctx, video_frame);
    if (receive_result >= 0) {
        if (video_frame->best_effort_timestamp != AV_NOPTS_VALUE) {
            AVStream* vs = format_ctx->streams[video_stream_index];
            current_position_seconds = video_frame->best_effort_timestamp * av_q2d(vs->time_base);
        }
        return _create_or_update_texture_from_video_frame();
    }

    return false;
}

bool MP4Player::_decode_audio_from_packet() {
    if (audio_stream_index == -1 || !audio_codec_ctx || !audio_buffersrc_ctx || !audio_buffersink_ctx || !filtered_audio_frame) {
        av_packet_unref(packet);
        return false;
    }

    int send_result = avcodec_send_packet(audio_codec_ctx, packet);
    av_packet_unref(packet);

    if (send_result < 0) {
        UtilityFunctions::print("Audio send packet failed");
        return false;
    }

    bool produced_audio = false;

    while (true) {
        int receive_result = avcodec_receive_frame(audio_codec_ctx, audio_frame);
        if (receive_result < 0) {
            break;
        }

        UtilityFunctions::print("Decoded raw audio frame: samples=", audio_frame->nb_samples);

        if (av_buffersrc_add_frame(audio_buffersrc_ctx, audio_frame) < 0) {
            UtilityFunctions::print("Failed to push audio frame into filter");
            av_frame_unref(audio_frame);
            continue;
        }

        while (true) {
            int filter_result = av_buffersink_get_frame(audio_buffersink_ctx, filtered_audio_frame);
            if (filter_result < 0) {
                break;
            }

            UtilityFunctions::print("Filtered audio frame: samples=", filtered_audio_frame->nb_samples);
            if (!_ensure_swr_for_filtered_frame()) {
                av_frame_unref(filtered_audio_frame);
                continue;
            }
            int out_samples = swr_get_out_samples(swr_ctx, filtered_audio_frame->nb_samples);
            if (out_samples <= 0) {
                av_frame_unref(filtered_audio_frame);
                continue;
            }

            int out_channels = 2;
            int bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_FLT);
            int out_buffer_size = out_samples * out_channels * bytes_per_sample;

            uint8_t* out_buffer = (uint8_t*)av_malloc(out_buffer_size);
            if (!out_buffer) {
                av_frame_unref(filtered_audio_frame);
                continue;
            }

            uint8_t* out_planes[1] = { out_buffer };

            int converted_samples = swr_convert(
                swr_ctx,
                out_planes,
                out_samples,
                (const uint8_t**)filtered_audio_frame->extended_data,
                filtered_audio_frame->nb_samples
            );

            if (converted_samples > 0) {
                float* samples = (float*)out_buffer;
                PackedVector2Array chunk;
                chunk.resize(converted_samples);

                for (int i = 0; i < converted_samples; i++) {
                    float left = samples[i * 2 + 0];
                    float right = samples[i * 2 + 1];
                    chunk.set(i, Vector2(left, right));
                }

                pending_audio_frames.append_array(chunk);
                UtilityFunctions::print("Queued audio frames: ", converted_samples);
                produced_audio = true;
            }

            av_free(out_buffer);
            av_frame_unref(filtered_audio_frame);
        }

        av_frame_unref(audio_frame);
    }

    return produced_audio;
}

bool MP4Player::decode_next_frame() {
    if (!file_opened || !format_ctx || !packet || !video_codec_ctx) {
        return false;
    }

    while (av_read_frame(format_ctx, packet) >= 0) {
        if (packet->stream_index == video_stream_index) {
            bool got_video = _decode_video_from_packet();
            if (got_video) {
                return true;
            }
        }
        else if (packet->stream_index == audio_stream_index) {
            _decode_audio_from_packet();
        }
        else {
            av_packet_unref(packet);
        }
    }

    end_of_video = true;

    if (loop_enabled) {
        return _reopen_current_file();
    }

    return false;
}

bool MP4Player::update(double delta) {
    if (!file_opened || end_of_video || !playing) {
        return false;
    }

    time_accumulator += delta * playback_speed;

    bool got_video = false;

    while (time_accumulator >= frame_time) {
        time_accumulator -= frame_time;

        if (decode_next_frame()) {
            got_video = true;
        }
        else {
            break;
        }
    }

    return got_video;
}

void MP4Player::play() {
    playing = true;
}

void MP4Player::pause() {
    playing = false;
}

void MP4Player::stop() {
    if (!current_path.is_empty()) {
        String path = current_path;
        open_file(path);
    }
    playing = false;
    current_position_seconds = 0.0;
    time_accumulator = 0.0;
}

bool MP4Player::seek(double seconds) {
    if (!file_opened || !format_ctx || video_stream_index < 0) {
        return false;
    }

    if (seconds < 0.0) {
        seconds = 0.0;
    }

    if (duration_seconds > 0.0 && seconds > duration_seconds) {
        seconds = duration_seconds;
    }

    AVStream* video_stream = format_ctx->streams[video_stream_index];
    int64_t target_ts = (int64_t)(seconds / av_q2d(video_stream->time_base));

    if (av_seek_frame(format_ctx, video_stream_index, target_ts, AVSEEK_FLAG_BACKWARD) < 0) {
        UtilityFunctions::print("Seek failed");
        return false;
    }

    if (video_codec_ctx) {
        avcodec_flush_buffers(video_codec_ctx);
    }

    if (audio_codec_ctx) {
        avcodec_flush_buffers(audio_codec_ctx);
    }

    _build_audio_filter_graph();

    pending_audio_frames.clear();
    end_of_video = false;
    time_accumulator = 0.0;
    current_position_seconds = seconds;

    return true;
}

void MP4Player::set_loop_enabled(bool enabled) {
    loop_enabled = enabled;
}

bool MP4Player::is_loop_enabled() const {
    return loop_enabled;
}

void MP4Player::set_playback_speed(double speed) {
    if (speed < 0.5) {
        speed = 0.5;
    }
    if (speed > 2.0) {
        speed = 2.0;
    }

    playback_speed = speed;
    UtilityFunctions::print("New playback speed: ", playback_speed);

    if (audio_codec_ctx) {
        _build_audio_filter_graph();
        pending_audio_frames.clear();
    }
}

double MP4Player::get_playback_speed() const {
    return playback_speed;
}

bool MP4Player::is_playing() const {
    return playing;
}

double MP4Player::get_position_seconds() const {
    return current_position_seconds;
}

Ref<ImageTexture> MP4Player::get_frame_texture() const {
    return frame_texture;
}

String MP4Player::get_current_path() const {
    return current_path;
}

int MP4Player::get_video_width() const {
    return video_width;
}

int MP4Player::get_video_height() const {
    return video_height;
}

double MP4Player::get_duration_seconds() const {
    return duration_seconds;
}

double MP4Player::get_video_fps() const {
    return video_fps;
}

int MP4Player::get_audio_sample_rate() const {
    return audio_sample_rate;
}

int MP4Player::get_audio_channels() const {
    return audio_channels;
}

PackedVector2Array MP4Player::consume_audio_frames() {
    PackedVector2Array out = pending_audio_frames;
    pending_audio_frames.clear();
    return out;
}