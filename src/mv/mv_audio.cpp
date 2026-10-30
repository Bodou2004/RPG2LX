#include "rpg2lx/mv_audio.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <unordered_map>

#ifdef RPG2LX_WITH_SDL
#include <SDL.h>
#endif

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

namespace fs = std::filesystem;

namespace rpg2lx {

namespace {

std::string url_decode_rel(const std::string& url) {
    std::string clean = url;
    const auto q = clean.find('?');
    if (q != std::string::npos) clean.resize(q);
    while (!clean.empty() && (clean[0] == '/' || clean[0] == '\\')) clean.erase(clean.begin());

    std::string out;
    out.reserve(clean.size());
    for (std::size_t i = 0; i < clean.size(); ++i) {
        if (clean[i] == '%' && i + 2 < clean.size()) {
            char hex[3] = {clean[i + 1], clean[i + 2], '\0'};
            char* endp = nullptr;
            long val = std::strtol(hex, &endp, 16);
            if (endp == hex + 2) {
                out.push_back(static_cast<char>(val));
                i += 2;
                continue;
            }
        }
        out.push_back(clean[i] == '\\' ? '/' : clean[i]);
    }
    return out;
}

void decrypt_mv_bytes(std::vector<std::uint8_t>& bytes, const std::string& hex_key) {
    if (bytes.size() < 32 || hex_key.size() < 32) return;
    if (std::memcmp(bytes.data(), "RPGMV", 5) != 0) return;

    std::uint8_t key[16]{};
    for (int i = 0; i < 16; ++i) {
        unsigned int b = 0;
        std::stringstream ss;
        ss << std::hex << hex_key.substr(static_cast<std::size_t>(i * 2), 2);
        ss >> b;
        key[i] = static_cast<std::uint8_t>(b);
    }
    std::vector<std::uint8_t> plain(bytes.size() - 16);
    std::memcpy(plain.data(), bytes.data() + 16, plain.size());
    for (std::size_t i = 0; i < 16 && i < plain.size(); ++i) {
        plain[i] ^= key[i];
    }
    bytes = std::move(plain);
}

struct CachedAudioAsset {
    std::vector<std::uint8_t> ogg_bytes;
    std::vector<std::int16_t> pcm_stereo; // Predecoded for short clips (<= 8s, e.g. SE/ME)
    int sample_rate = 44100;
    int channels = 2;
    std::uint32_t total_frames = 0;
    std::uint32_t loop_start = 0;
    std::uint32_t loop_length = 0;
    bool is_predecoded = false;
    bool valid = false;
};

struct ActivePlayer {
    std::shared_ptr<CachedAudioAsset> asset;
    stb_vorbis* stream = nullptr;
    std::vector<std::int16_t> stream_chunk;
    std::uint32_t chunk_start_frame = 0;
    std::uint32_t chunk_frame_count = 0;

    double cursor_frame = 0.0;
    bool playing = false;
    bool loop = false;
    bool has_error = false;

    float volume = 1.0f;
    float pitch = 1.0f;
    float pan = 0.0f;

    float fade_current = 1.0f;
    float fade_target = 1.0f;
    float fade_step_per_out_frame = 0.0f;

    ~ActivePlayer() {
        if (stream) {
            stb_vorbis_close(stream);
            stream = nullptr;
        }
    }
};

} // namespace

struct MvAudioEngine::Impl {
    mutable std::mutex mtx;
    fs::path project_root;
    std::string encryption_key;
    float master_vol = 1.0f;
    int device_rate = 44100;
    int next_id = 1;
    bool initialized = false;

#ifdef RPG2LX_WITH_SDL
    SDL_AudioDeviceID audio_dev = 0;
#endif

    std::unordered_map<std::string, std::shared_ptr<CachedAudioAsset>> asset_cache;
    std::unordered_map<int, std::unique_ptr<ActivePlayer>> players;

    ~Impl() {
        shutdown();
    }

    void shutdown() {
#ifdef RPG2LX_WITH_SDL
        if (audio_dev != 0) {
            SDL_CloseAudioDevice(audio_dev);
            audio_dev = 0;
        }
#endif
        std::lock_guard<std::mutex> lock(mtx);
        players.clear();
        asset_cache.clear();
        initialized = false;
    }

#ifdef RPG2LX_WITH_SDL
    static void SDLCALL sdl_audio_callback(void* userdata, Uint8* stream_out, int len_bytes) {
        auto* self = static_cast<Impl*>(userdata);
        std::memset(stream_out, 0, static_cast<std::size_t>(len_bytes));
        if (!self) return;
        self->mix_frames(reinterpret_cast<std::int16_t*>(stream_out), len_bytes / (2 * static_cast<int>(sizeof(std::int16_t))));
    }
#endif

    void refill_stream_chunk(ActivePlayer& p, std::uint32_t target_frame) {
        if (!p.stream || !p.asset) return;
        if (target_frame < p.chunk_start_frame || target_frame >= p.chunk_start_frame + p.chunk_frame_count + 64) {
            stb_vorbis_seek(p.stream, target_frame);
            p.chunk_start_frame = target_frame;
        } else {
            p.chunk_start_frame += p.chunk_frame_count;
        }
        constexpr int kDecodeFrames = 1024;
        p.stream_chunk.resize(kDecodeFrames * 2);
        const int got = stb_vorbis_get_samples_short_interleaved(p.stream, 2, p.stream_chunk.data(), kDecodeFrames * 2);
        p.chunk_frame_count = got > 0 ? static_cast<std::uint32_t>(got) : 0;
    }

    void mix_frames(std::int16_t* out_stereo, int out_frames) {
        if (out_frames <= 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        if (master_vol <= 0.0001f || players.empty()) return;

        std::vector<std::int32_t> accum(static_cast<std::size_t>(out_frames * 2), 0);

        for (auto& kv : players) {
            ActivePlayer& p = *kv.second;
            if (!p.playing || !p.asset || !p.asset->valid || p.asset->total_frames == 0) continue;

            const std::uint32_t total = p.asset->total_frames;
            const std::uint32_t loop_start = p.asset->loop_start < total ? p.asset->loop_start : 0;
            const std::uint32_t loop_end = (p.asset->loop_length > 0 && loop_start + p.asset->loop_length <= total)
                                               ? (loop_start + p.asset->loop_length)
                                               : total;
            const std::uint32_t loop_span = loop_end > loop_start ? (loop_end - loop_start) : total;

            const double step = (static_cast<double>(p.asset->sample_rate) / static_cast<double>(device_rate)) *
                                std::clamp(static_cast<double>(p.pitch), 0.25, 4.0);

            const float pan_clamped = std::clamp(p.pan, -1.0f, 1.0f);
            const float angle = (pan_clamped + 1.0f) * 0.78539816339f; // pi/4
            const float pan_l = std::cos(angle);
            const float pan_r = std::sin(angle);

            for (int f = 0; f < out_frames; ++f) {
                if (p.fade_step_per_out_frame != 0.0f) {
                    p.fade_current += p.fade_step_per_out_frame;
                    if ((p.fade_step_per_out_frame > 0.0f && p.fade_current >= p.fade_target) ||
                        (p.fade_step_per_out_frame < 0.0f && p.fade_current <= p.fade_target)) {
                        p.fade_current = p.fade_target;
                        p.fade_step_per_out_frame = 0.0f;
                        if (p.fade_current <= 0.0001f) {
                            p.playing = false;
                            break;
                        }
                    }
                }

                if (p.loop) {
                    if (p.cursor_frame >= static_cast<double>(loop_end) && loop_span > 0) {
                        p.cursor_frame = static_cast<double>(loop_start) +
                                         std::fmod(p.cursor_frame - static_cast<double>(loop_start), static_cast<double>(loop_span));
                    }
                } else if (p.cursor_frame >= static_cast<double>(total)) {
                    p.playing = false;
                    break;
                }

                const auto idx = static_cast<std::uint32_t>(p.cursor_frame);
                std::int16_t s_l = 0, s_r = 0;

                if (p.asset->is_predecoded) {
                    if (idx * 2 + 1 < p.asset->pcm_stereo.size()) {
                        s_l = p.asset->pcm_stereo[idx * 2];
                        s_r = p.asset->pcm_stereo[idx * 2 + 1];
                    }
                } else if (p.stream) {
                    if (p.chunk_frame_count == 0 || idx < p.chunk_start_frame || idx >= p.chunk_start_frame + p.chunk_frame_count) {
                        refill_stream_chunk(p, idx);
                    }
                    if (idx >= p.chunk_start_frame && idx < p.chunk_start_frame + p.chunk_frame_count) {
                        const std::size_t off = static_cast<std::size_t>(idx - p.chunk_start_frame) * 2;
                        s_l = p.stream_chunk[off];
                        s_r = p.stream_chunk[off + 1];
                    } else if (!p.loop) {
                        p.playing = false;
                        break;
                    }
                }

                const float gain = master_vol * p.volume * p.fade_current;
                accum[static_cast<std::size_t>(f * 2)] += static_cast<std::int32_t>(s_l * gain * pan_l);
                accum[static_cast<std::size_t>(f * 2 + 1)] += static_cast<std::int32_t>(s_r * gain * pan_r);

                p.cursor_frame += step;
            }
        }

        for (int i = 0; i < out_frames * 2; ++i) {
            out_stereo[i] = static_cast<std::int16_t>(std::clamp(accum[static_cast<std::size_t>(i)], -32768, 32767));
        }
    }

    std::shared_ptr<CachedAudioAsset> load_asset_unlocked(const std::string& rel_url) {
        const std::string norm = url_decode_rel(rel_url);
        auto it = asset_cache.find(norm);
        if (it != asset_cache.end()) {
            return it->second;
        }

        auto asset = std::make_shared<CachedAudioAsset>();

        // Try exact path, .ogg, .rpgmvo
        fs::path base = project_root / fs::u8path(norm);
        std::vector<fs::path> candidates = {base};
        const std::string ext = base.extension().u8string();
        if (ext == ".ogg" || ext == ".m4a") {
            fs::path stem_p = base;
            stem_p.replace_extension(".ogg");
            candidates.push_back(stem_p);
            fs::path enc_p = base;
            enc_p.replace_extension(".rpgmvo");
            candidates.push_back(enc_p);
        } else if (ext == ".rpgmvo" || ext == ".rpgmvm") {
            fs::path ogg_p = base;
            ogg_p.replace_extension(".ogg");
            candidates.push_back(ogg_p);
        }

        fs::path chosen;
        std::error_code ec;
        for (const auto& c : candidates) {
            if (fs::exists(c, ec) && fs::is_regular_file(c, ec)) {
                chosen = c;
                break;
            }
        }
        if (chosen.empty()) {
            asset_cache[norm] = asset;
            return asset;
        }

        const auto fsize = fs::file_size(chosen, ec);
        if (ec || fsize == 0 || fsize > 48 * 1024 * 1024) {
            asset_cache[norm] = asset;
            return asset;
        }

        std::ifstream f(chosen, std::ios::binary);
        if (!f.is_open()) {
            asset_cache[norm] = asset;
            return asset;
        }
        asset->ogg_bytes.resize(static_cast<std::size_t>(fsize));
        if (!f.read(reinterpret_cast<char*>(asset->ogg_bytes.data()), static_cast<std::streamsize>(fsize))) {
            asset->ogg_bytes.clear();
            asset_cache[norm] = asset;
            return asset;
        }

        decrypt_mv_bytes(asset->ogg_bytes, encryption_key);

        int vorb_err = 0;
        stb_vorbis* v = stb_vorbis_open_memory(asset->ogg_bytes.data(),
                                               static_cast<int>(asset->ogg_bytes.size()),
                                               &vorb_err, nullptr);
        if (!v) {
            asset->ogg_bytes.clear();
            asset_cache[norm] = asset;
            return asset;
        }

        const stb_vorbis_info info = stb_vorbis_get_info(v);
        asset->sample_rate = info.sample_rate > 0 ? static_cast<int>(info.sample_rate) : 44100;
        asset->channels = info.channels > 0 ? info.channels : 2;
        asset->total_frames = stb_vorbis_stream_length_in_samples(v);

        // Parse LOOPSTART / LOOPLENGTH comments (RPG Maker MV BGM loop standard)
        const stb_vorbis_comment cm = stb_vorbis_get_comment(v);
        for (int i = 0; i < cm.comment_list_length; ++i) {
            if (!cm.comment_list[i]) continue;
            std::string entry(cm.comment_list[i]);
            if (entry.rfind("LOOPSTART=", 0) == 0) {
                asset->loop_start = static_cast<std::uint32_t>(std::strtoul(entry.c_str() + 10, nullptr, 10));
            } else if (entry.rfind("LOOPLENGTH=", 0) == 0) {
                asset->loop_length = static_cast<std::uint32_t>(std::strtoul(entry.c_str() + 11, nullptr, 10));
            }
        }

        // Predecode short effects (<= 8 seconds) to stereo PCM so polyphonic SE costs ~0 CPU!
        if (asset->total_frames > 0 && asset->total_frames <= static_cast<std::uint32_t>(asset->sample_rate * 8)) {
            asset->pcm_stereo.resize(static_cast<std::size_t>(asset->total_frames) * 2, 0);
            const int decoded = stb_vorbis_get_samples_short_interleaved(
                v, 2, asset->pcm_stereo.data(), static_cast<int>(asset->pcm_stereo.size()));
            if (decoded > 0) {
                asset->total_frames = static_cast<std::uint32_t>(decoded);
                asset->pcm_stereo.resize(static_cast<std::size_t>(decoded) * 2);
                asset->is_predecoded = true;
                asset->ogg_bytes.clear();
                asset->ogg_bytes.shrink_to_fit();
            }
        }

        stb_vorbis_close(v);
        asset->valid = asset->total_frames > 0;

        if (asset_cache.size() > 64) {
            // Keep memory bounded on 1 GB handhelds
            asset_cache.clear();
        }
        asset_cache[norm] = asset;
        return asset;
    }
};

MvAudioEngine::MvAudioEngine() : impl_(std::make_unique<Impl>()) {}
MvAudioEngine::~MvAudioEngine() = default;

bool MvAudioEngine::init(const fs::path& project_root, const std::string& encryption_key) {
    impl_->shutdown();
    {
        std::lock_guard<std::mutex> lock(impl_->mtx);
        impl_->project_root = project_root;
        impl_->encryption_key = encryption_key;
        impl_->initialized = true;
    }

#ifdef RPG2LX_WITH_SDL
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            // Audio device unavailable (e.g., headless CI); continue silently without failing the game!
            return true;
        }
    }
    SDL_AudioSpec desired{};
    desired.freq = 44100;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = 1024;
    desired.callback = &Impl::sdl_audio_callback;
    desired.userdata = impl_.get();

    SDL_AudioSpec obtained{};
    impl_->audio_dev = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (impl_->audio_dev != 0) {
        impl_->device_rate = obtained.freq > 0 ? obtained.freq : 44100;
        SDL_PauseAudioDevice(impl_->audio_dev, 0);
    }
#endif
    return true;
}

void MvAudioEngine::shutdown() {
    impl_->shutdown();
}

void MvAudioEngine::set_encryption_key(const std::string& encryption_key) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    impl_->encryption_key = encryption_key;
}

void MvAudioEngine::set_master_volume(float volume) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    impl_->master_vol = std::clamp(volume, 0.0f, 1.0f);
}

float MvAudioEngine::master_volume() const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    return impl_->master_vol;
}

int MvAudioEngine::create_player(const std::string& relative_url) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    const int id = impl_->next_id++;
    auto player = std::make_unique<ActivePlayer>();
    player->asset = impl_->load_asset_unlocked(relative_url);
    player->has_error = !player->asset || !player->asset->valid;
    impl_->players[id] = std::move(player);
    return id;
}

void MvAudioEngine::destroy_player(int player_id) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    impl_->players.erase(player_id);
}

bool MvAudioEngine::is_ready(int player_id) const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    auto it = impl_->players.find(player_id);
    return it != impl_->players.end() && it->second->asset && it->second->asset->valid;
}

bool MvAudioEngine::is_error(int player_id) const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    auto it = impl_->players.find(player_id);
    return it == impl_->players.end() || it->second->has_error;
}

bool MvAudioEngine::is_playing(int player_id) const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    auto it = impl_->players.find(player_id);
    return it != impl_->players.end() && it->second->playing;
}

void MvAudioEngine::play(int player_id, bool loop, double offset_sec) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    auto it = impl_->players.find(player_id);
    if (it == impl_->players.end() || !it->second->asset || !it->second->asset->valid) return;

    ActivePlayer& p = *it->second;
    p.loop = loop;
    const double start_f = std::max(0.0, offset_sec * p.asset->sample_rate);
    p.cursor_frame = start_f;
    p.fade_current = 1.0f;
    p.fade_target = 1.0f;
    p.fade_step_per_out_frame = 0.0f;

    if (!p.asset->is_predecoded) {
        if (p.stream) {
            stb_vorbis_close(p.stream);
            p.stream = nullptr;
        }
        int err = 0;
        p.stream = stb_vorbis_open_memory(p.asset->ogg_bytes.data(),
                                          static_cast<int>(p.asset->ogg_bytes.size()),
                                          &err, nullptr);
        p.chunk_frame_count = 0;
        p.chunk_start_frame = 0;
        if (p.stream && start_f > 0.0) {
            stb_vorbis_seek(p.stream, static_cast<unsigned int>(start_f));
            p.chunk_start_frame = static_cast<std::uint32_t>(start_f);
        }
    }
    p.playing = p.asset->is_predecoded || (p.stream != nullptr);
}

void MvAudioEngine::stop(int player_id) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    auto it = impl_->players.find(player_id);
    if (it != impl_->players.end()) {
        it->second->playing = false;
        it->second->cursor_frame = 0.0;
        if (it->second->stream) {
            stb_vorbis_close(it->second->stream);
            it->second->stream = nullptr;
        }
    }
}

void MvAudioEngine::stop_all() {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    for (auto& kv : impl_->players) {
        kv.second->playing = false;
        if (kv.second->stream) {
            stb_vorbis_close(kv.second->stream);
            kv.second->stream = nullptr;
        }
    }
}

void MvAudioEngine::set_volume(int player_id, float volume) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    auto it = impl_->players.find(player_id);
    if (it != impl_->players.end()) {
        it->second->volume = std::clamp(volume, 0.0f, 1.0f);
    }
}

float MvAudioEngine::volume(int player_id) const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    auto it = impl_->players.find(player_id);
    return it != impl_->players.end() ? it->second->volume : 0.0f;
}

void MvAudioEngine::set_pitch(int player_id, float pitch) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    auto it = impl_->players.find(player_id);
    if (it != impl_->players.end()) {
        it->second->pitch = std::clamp(pitch, 0.25f, 4.0f);
    }
}

float MvAudioEngine::pitch(int player_id) const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    auto it = impl_->players.find(player_id);
    return it != impl_->players.end() ? it->second->pitch : 1.0f;
}

void MvAudioEngine::set_pan(int player_id, float pan) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    auto it = impl_->players.find(player_id);
    if (it != impl_->players.end()) {
        it->second->pan = std::clamp(pan, -1.0f, 1.0f);
    }
}

float MvAudioEngine::pan(int player_id) const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    auto it = impl_->players.find(player_id);
    return it != impl_->players.end() ? it->second->pan : 0.0f;
}

void MvAudioEngine::fade_in(int player_id, double duration_sec) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    auto it = impl_->players.find(player_id);
    if (it == impl_->players.end()) return;
    if (duration_sec <= 0.01) {
        it->second->fade_current = 1.0f;
        it->second->fade_target = 1.0f;
        it->second->fade_step_per_out_frame = 0.0f;
        return;
    }
    it->second->fade_current = 0.0f;
    it->second->fade_target = 1.0f;
    it->second->fade_step_per_out_frame = static_cast<float>(1.0 / (duration_sec * impl_->device_rate));
}

void MvAudioEngine::fade_out(int player_id, double duration_sec) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    auto it = impl_->players.find(player_id);
    if (it == impl_->players.end()) return;
    if (duration_sec <= 0.01) {
        it->second->playing = false;
        return;
    }
    it->second->fade_target = 0.0f;
    it->second->fade_step_per_out_frame = -static_cast<float>(it->second->fade_current / (duration_sec * impl_->device_rate));
}

double MvAudioEngine::seek_position(int player_id) const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    auto it = impl_->players.find(player_id);
    if (it == impl_->players.end() || !it->second->asset || it->second->asset->sample_rate <= 0) return 0.0;
    return it->second->cursor_frame / static_cast<double>(it->second->asset->sample_rate);
}

double MvAudioEngine::total_duration(int player_id) const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    auto it = impl_->players.find(player_id);
    if (it == impl_->players.end() || !it->second->asset || it->second->asset->sample_rate <= 0) return 0.0;
    return static_cast<double>(it->second->asset->total_frames) / static_cast<double>(it->second->asset->sample_rate);
}

} // namespace rpg2lx
