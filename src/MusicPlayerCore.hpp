#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <random>
#include <chrono>

#include "miniaudio.h"
#include "Common.hpp"

class MusicPlayerCore {
public:
    MusicPlayerCore() 
        : m_Volume(1.0f)
        , m_Speed(1.0f)
        , m_KeepPitch(false)
        , m_Shuffle(false)
        , m_RepeatMode(0)
        , m_IsPlaying(false)
        , m_IsPaused(false)
        , m_CurrentPlaylistIndex(-1)
        , m_IsDirMode(false)
        , m_EngineInitialized(false)
        , m_SoundInitialized(false) {
        
        ma_result result = ma_engine_init(NULL, &m_Engine);
        if (result == MA_SUCCESS) {
            m_EngineInitialized = true;
        }
    }

    ~MusicPlayerCore() {
        CloseSound();
        if (m_EngineInitialized) {
            ma_engine_uninit(&m_Engine);
        }
    }

    void SetVolume(float vol) {
        m_Volume = std::clamp(vol, 0.0f, 1.0f);
        ApplyVolume();
    }

    float GetVolume() const { return m_Volume; }

    void SetSpeed(float speed) {
        m_Speed = std::clamp(speed, 0.1f, 10.0f);
        ApplySpeed();
    }

    float GetSpeed() const { return m_Speed; }

    void SetKeepPitch(bool keep) {
        if (m_KeepPitch == keep) return;
        m_KeepPitch = keep;
        ApplySpeed();
    }

    bool GetKeepPitch() const { return m_KeepPitch; }

    void SetShuffle(bool enable) {
        if (m_Shuffle == enable) return;
        m_Shuffle = enable;
        RegenerateShuffledPlaylist();
    }

    bool GetShuffle() const { return m_Shuffle; }

    void SetRepeatMode(int mode) {
        m_RepeatMode = std::clamp(mode, 0, 2);
    }

    int GetRepeatMode() const { return m_RepeatMode; }

    bool IsPlaying() const { return m_IsPlaying; }
    bool IsPaused() const { return m_IsPaused; }
    bool IsDirectoryMode() const { return m_IsDirMode; }
    bool IsEngineInitialized() const { return m_EngineInitialized; }
    bool IsSoundInitialized() const { return m_SoundInitialized; }
    
    std::string GetCurrentSongPath() const {
        if (m_CurrentPlaylistIndex >= 0 && m_CurrentPlaylistIndex < (int)m_Playlist.size()) {
            int actualIdx = m_ShuffledPlaylist[m_CurrentPlaylistIndex];
            return m_Playlist[actualIdx];
        }
        return "";
    }

    std::string GetCurrentSongName() const {
        std::string path = GetCurrentSongPath();
        if (path.empty()) return "No Song Loaded";
        return std::filesystem::path(Utf8ToPath(path)).filename().string();
    }

    std::string GetLastPath() const {
        return m_IsDirMode ? m_CurrentDir : GetCurrentSongPath();
    }

    const std::vector<std::string>& GetPlaylist() const {
        return m_Playlist;
    }

    int GetCurrentPlaylistIndex() const {
        if (m_CurrentPlaylistIndex >= 0 && m_CurrentPlaylistIndex < (int)m_Playlist.size()) {
            return m_ShuffledPlaylist[m_CurrentPlaylistIndex];
        }
        return -1;
    }

    void LoadSingleFile(const std::string& filePath, bool autoplay = true) {
        Stop();
        m_Playlist.clear();
        m_ShuffledPlaylist.clear();
        m_Playlist.push_back(filePath);
        m_ShuffledPlaylist.push_back(0);
        m_CurrentPlaylistIndex = 0;
        m_IsDirMode = false;
        m_CurrentDir = "";

        if (autoplay) {
            PlayIndex(0);
        } else {
            OpenSound(filePath);
        }
    }

    void LoadDirectory(const std::string& dirPath, bool autoplay = true) {
        Stop();
        m_Playlist.clear();
        m_ShuffledPlaylist.clear();
        m_IsDirMode = true;
        m_CurrentDir = dirPath;

        try {
            std::filesystem::path p = Utf8ToPath(dirPath);
            if (std::filesystem::exists(p) && std::filesystem::is_directory(p)) {
                for (const auto& entry : std::filesystem::directory_iterator(p)) {
                    if (entry.is_regular_file()) {
                        std::string ext = entry.path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (ext == ".mp3" || ext == ".wav" || ext == ".wma" || ext == ".mid" || ext == ".midi") {
                            m_Playlist.push_back(PathToUtf8(entry.path()));
                        }
                    }
                }
                // Sort initially
                std::sort(m_Playlist.begin(), m_Playlist.end());
            }
        } catch (...) {}

        if (m_Playlist.empty()) {
            m_CurrentPlaylistIndex = -1;
            return;
        }

        m_CurrentPlaylistIndex = 0;
        RegenerateShuffledPlaylist();

        if (autoplay) {
            PlayIndex(0);
        } else {
            int actualIdx = m_ShuffledPlaylist[m_CurrentPlaylistIndex];
            OpenSound(m_Playlist[actualIdx]);
        }
    }

    void LoadDirectoryAndSetSong(const std::string& dirPath, const std::string& songPath, bool autoplay) {
        LoadDirectory(dirPath, false); // Load playlist and generate shuffled list
        if (m_Playlist.empty()) return;

        // Find the index of songPath in the sequential playlist
        int playlistIdx = -1;
        for (int i = 0; i < (int)m_Playlist.size(); ++i) {
            if (m_Playlist[i] == songPath) {
                playlistIdx = i;
                break;
            }
        }

        if (playlistIdx != -1) {
            // Find the index in m_ShuffledPlaylist that maps to playlistIdx
            int shuffleIdx = -1;
            for (int i = 0; i < (int)m_ShuffledPlaylist.size(); ++i) {
                if (m_ShuffledPlaylist[i] == playlistIdx) {
                    shuffleIdx = i;
                    break;
                }
            }
            if (shuffleIdx != -1) {
                m_CurrentPlaylistIndex = shuffleIdx;
            }
        }

        if (autoplay) {
            PlayIndex(m_CurrentPlaylistIndex);
        } else {
            int actualIdx = m_ShuffledPlaylist[m_CurrentPlaylistIndex];
            OpenSound(m_Playlist[actualIdx]);
        }
    }

    void PlayIndex(int idx) {
        if (m_Playlist.empty()) return;
        
        CloseSound();

        m_CurrentPlaylistIndex = std::clamp(idx, 0, (int)m_Playlist.size() - 1);
        int actualIdx = m_ShuffledPlaylist[m_CurrentPlaylistIndex];
        std::string songPath = m_Playlist[actualIdx];

        if (OpenSound(songPath)) {
            ma_sound_start(&m_Sound);
            m_IsPlaying = true;
            m_IsPaused = false;
        }
    }

    void PlaySequentialIndex(int seqIdx) {
        if (m_Playlist.empty()) return;
        int targetIdx = -1;
        for (int i = 0; i < (int)m_ShuffledPlaylist.size(); ++i) {
            if (m_ShuffledPlaylist[i] == seqIdx) {
                targetIdx = i;
                break;
            }
        }
        if (targetIdx != -1) {
            PlayIndex(targetIdx);
        }
    }

    void Play() {
        if (m_Playlist.empty()) return;

        if (m_IsPaused) {
            if (m_SoundInitialized) {
                ma_sound_start(&m_Sound);
            }
            m_IsPlaying = true;
            m_IsPaused = false;
        } else if (!m_IsPlaying) {
            PlayIndex(m_CurrentPlaylistIndex >= 0 ? m_CurrentPlaylistIndex : 0);
        }
    }

    void Pause() {
        if (m_IsPlaying && !m_IsPaused) {
            if (m_SoundInitialized) {
                ma_sound_stop(&m_Sound);
            }
            m_IsPlaying = false;
            m_IsPaused = true;
        }
    }

    void Stop() {
        if (m_SoundInitialized) {
            ma_sound_stop(&m_Sound);
            ma_sound_seek_to_pcm_frame(&m_Sound, 0);
        }
        m_IsPlaying = false;
        m_IsPaused = false;
    }

    void PlayNext() {
        if (m_Playlist.empty()) return;

        int nextIdx = m_CurrentPlaylistIndex + 1;
        if (nextIdx >= (int)m_Playlist.size()) {
            if (m_RepeatMode == 2) { // Repeat All
                nextIdx = 0;
                PlayIndex(nextIdx);
            } else {
                Stop();
            }
        } else {
            PlayIndex(nextIdx);
        }
    }

    void PlayPrev() {
        if (m_Playlist.empty()) return;

        int prevIdx = m_CurrentPlaylistIndex - 1;
        if (prevIdx < 0) {
            if (m_RepeatMode == 2) { // Repeat All
                prevIdx = (int)m_Playlist.size() - 1;
                PlayIndex(prevIdx);
            } else {
                PlayIndex(m_CurrentPlaylistIndex);
            }
        } else {
            PlayIndex(prevIdx);
        }
    }

    void SeekTo(int ms) {
        if (m_SoundInitialized) {
            float seconds = static_cast<float>(ms) / 1000.0f;
            ma_sound_seek_to_second(&m_Sound, seconds);
        }
    }

    int GetPositionMs() {
        if (m_SoundInitialized) {
            float seconds = 0.0f;
            ma_sound_get_cursor_in_seconds(&m_Sound, &seconds);
            return static_cast<int>(seconds * 1000.0f);
        }
        return 0;
    }

    int GetDurationMs() {
        if (m_SoundInitialized) {
            float seconds = 0.0f;
            ma_sound_get_length_in_seconds(&m_Sound, &seconds);
            return static_cast<int>(seconds * 1000.0f);
        }
        return 0;
    }

    void Update() {
        if (m_IsPlaying && !m_IsPaused) {
            if (m_SoundInitialized && ma_sound_at_end(&m_Sound)) {
                if (m_RepeatMode == 1) { // Repeat One (automatic loop)
                    PlayIndex(m_CurrentPlaylistIndex);
                } else {
                    PlayNext();
                }
            }
        }
    }

private:
    void CloseSound() {
        if (m_SoundInitialized) {
            ma_sound_uninit(&m_Sound);
            m_SoundInitialized = false;
        }
    }

    bool OpenSound(const std::string& path) {
        CloseSound();
        if (!m_EngineInitialized) return false;

#ifdef _WIN32
        std::wstring wPath = Utf8ToUtf16(path);
        ma_result result = ma_sound_init_from_file_w(&m_Engine, wPath.c_str(), MA_SOUND_FLAG_STREAM, NULL, NULL, &m_Sound);
#else
        ma_result result = ma_sound_init_from_file(&m_Engine, path.c_str(), MA_SOUND_FLAG_STREAM, NULL, NULL, &m_Sound);
#endif

        if (result == MA_SUCCESS) {
            m_SoundInitialized = true;
            ApplyVolume();
            ApplySpeed();
            return true;
        }
        return false;
    }

    void ApplyVolume() {
        if (m_SoundInitialized) {
            ma_sound_set_volume(&m_Sound, m_Volume);
        }
    }

    void ApplySpeed() {
        if (m_SoundInitialized) {
            float pitch = m_KeepPitch ? 1.0f : m_Speed;
            ma_sound_set_pitch(&m_Sound, pitch);
        }
    }

    void RegenerateShuffledPlaylist() {
        int size = (int)m_Playlist.size();
        m_ShuffledPlaylist.resize(size);
        for (int i = 0; i < size; ++i) {
            m_ShuffledPlaylist[i] = i;
        }

        if (m_Shuffle && size > 1) {
            unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
            std::default_random_engine e(seed);
            
            if (m_CurrentPlaylistIndex >= 0 && m_CurrentPlaylistIndex < size) {
                int playingIdx = m_ShuffledPlaylist[m_CurrentPlaylistIndex];
                std::swap(m_ShuffledPlaylist[0], m_ShuffledPlaylist[m_CurrentPlaylistIndex]);
                std::shuffle(m_ShuffledPlaylist.begin() + 1, m_ShuffledPlaylist.end(), e);
                m_CurrentPlaylistIndex = 0;
            } else {
                std::shuffle(m_ShuffledPlaylist.begin(), m_ShuffledPlaylist.end(), e);
            }
        } else {
            if (m_CurrentPlaylistIndex >= 0 && m_CurrentPlaylistIndex < size) {
                m_CurrentPlaylistIndex = m_ShuffledPlaylist[m_CurrentPlaylistIndex];
            }
            for (int i = 0; i < size; ++i) {
                m_ShuffledPlaylist[i] = i;
            }
        }
    }

    ma_engine m_Engine;
    ma_sound m_Sound{};
    bool m_EngineInitialized;
    bool m_SoundInitialized;

    float m_Volume;
    float m_Speed;
    bool m_KeepPitch;
    bool m_Shuffle;
    int m_RepeatMode;
    bool m_IsPlaying;
    bool m_IsPaused;

    std::vector<std::string> m_Playlist;
    std::vector<int> m_ShuffledPlaylist;
    int m_CurrentPlaylistIndex;
    std::string m_CurrentDir;
    bool m_IsDirMode;
};
