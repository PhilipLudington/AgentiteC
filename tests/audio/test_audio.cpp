/*
 * Agentite Audio Tests
 *
 * Tests for the audio system. Note: Functions requiring actual audio hardware
 * cannot be tested without SDL audio initialization. These tests focus on
 * NULL safety and API contract testing.
 */

#include "catch_amalgamated.hpp"
#include "agentite/audio.h"

/* ============================================================================
 * NULL Safety Tests - Audio System
 * ============================================================================ */

TEST_CASE("Audio system NULL safety", "[audio][null]") {
    SECTION("agentite_audio_shutdown with NULL is safe") {
        agentite_audio_shutdown(nullptr);
        // Should not crash
    }

    SECTION("agentite_audio_update with NULL is safe") {
        agentite_audio_update(nullptr);
        // Should not crash
    }

    SECTION("agentite_audio_set_master_volume with NULL is safe") {
        agentite_audio_set_master_volume(nullptr, 0.5f);
        // Should not crash
    }

    SECTION("agentite_audio_get_master_volume with NULL returns 0") {
        float vol = agentite_audio_get_master_volume(nullptr);
        REQUIRE(vol == 0.0f);
    }

    SECTION("agentite_audio_set_sound_volume with NULL is safe") {
        agentite_audio_set_sound_volume(nullptr, 0.5f);
        // Should not crash
    }

    SECTION("agentite_audio_get_sound_volume with NULL returns 0") {
        float vol = agentite_audio_get_sound_volume(nullptr);
        REQUIRE(vol == 0.0f);
    }

    SECTION("agentite_audio_set_music_volume with NULL is safe") {
        agentite_audio_set_music_volume(nullptr, 0.5f);
        // Should not crash
    }

    SECTION("agentite_audio_get_music_volume with NULL returns 0") {
        float vol = agentite_audio_get_music_volume(nullptr);
        REQUIRE(vol == 0.0f);
    }
}

/* ============================================================================
 * NULL Safety Tests - Sound Operations
 * ============================================================================ */

TEST_CASE("Sound operations NULL safety", "[audio][sound][null]") {
    SECTION("agentite_sound_load with NULL audio returns NULL") {
        Agentite_Sound *sound = agentite_sound_load(nullptr, "test.wav");
        REQUIRE(sound == nullptr);
    }

    SECTION("agentite_sound_load with NULL path returns NULL") {
        Agentite_Sound *sound = agentite_sound_load(nullptr, nullptr);
        REQUIRE(sound == nullptr);
    }

    SECTION("agentite_sound_load_wav_memory with NULL audio returns NULL") {
        const char data[] = "fake data";
        Agentite_Sound *sound = agentite_sound_load_wav_memory(nullptr, data, sizeof(data));
        REQUIRE(sound == nullptr);
    }

    SECTION("agentite_sound_load_wav_memory with NULL data returns NULL") {
        Agentite_Sound *sound = agentite_sound_load_wav_memory(nullptr, nullptr, 100);
        REQUIRE(sound == nullptr);
    }

    SECTION("agentite_sound_destroy with NULL is safe") {
        agentite_sound_destroy(nullptr, nullptr);
        // Should not crash
    }

    SECTION("agentite_sound_play with NULL audio returns invalid handle") {
        Agentite_SoundHandle handle = agentite_sound_play(nullptr, nullptr);
        REQUIRE(handle == AGENTITE_INVALID_SOUND_HANDLE);
    }

    SECTION("agentite_sound_play_ex with NULL audio returns invalid handle") {
        Agentite_SoundHandle handle = agentite_sound_play_ex(nullptr, nullptr, 1.0f, 0.0f, false);
        REQUIRE(handle == AGENTITE_INVALID_SOUND_HANDLE);
    }

    SECTION("agentite_sound_stop with NULL is safe") {
        agentite_sound_stop(nullptr, 0);
        agentite_sound_stop(nullptr, AGENTITE_INVALID_SOUND_HANDLE);
        // Should not crash
    }

    SECTION("agentite_sound_set_volume with NULL is safe") {
        agentite_sound_set_volume(nullptr, 0, 0.5f);
        // Should not crash
    }

    SECTION("agentite_sound_set_pan with NULL is safe") {
        agentite_sound_set_pan(nullptr, 0, -0.5f);
        // Should not crash
    }

    SECTION("agentite_sound_set_loop with NULL is safe") {
        agentite_sound_set_loop(nullptr, 0, true);
        // Should not crash
    }

    SECTION("agentite_sound_is_playing with NULL returns false") {
        bool playing = agentite_sound_is_playing(nullptr, 0);
        REQUIRE_FALSE(playing);
    }

    SECTION("agentite_sound_stop_all with NULL is safe") {
        agentite_sound_stop_all(nullptr);
        // Should not crash
    }
}

/* ============================================================================
 * NULL Safety Tests - Music Operations
 * ============================================================================ */

TEST_CASE("Music operations NULL safety", "[audio][music][null]") {
    SECTION("agentite_music_load with NULL audio returns NULL") {
        Agentite_Music *music = agentite_music_load(nullptr, "test.ogg");
        REQUIRE(music == nullptr);
    }

    SECTION("agentite_music_load with NULL path returns NULL") {
        Agentite_Music *music = agentite_music_load(nullptr, nullptr);
        REQUIRE(music == nullptr);
    }

    SECTION("agentite_music_destroy with NULL is safe") {
        agentite_music_destroy(nullptr, nullptr);
        // Should not crash
    }

    SECTION("agentite_music_play with NULL is safe") {
        agentite_music_play(nullptr, nullptr);
        // Should not crash
    }

    SECTION("agentite_music_play_ex with NULL is safe") {
        agentite_music_play_ex(nullptr, nullptr, 1.0f, true);
        // Should not crash
    }

    SECTION("agentite_music_stop with NULL is safe") {
        agentite_music_stop(nullptr);
        // Should not crash
    }

    SECTION("agentite_music_pause with NULL is safe") {
        agentite_music_pause(nullptr);
        // Should not crash
    }

    SECTION("agentite_music_resume with NULL is safe") {
        agentite_music_resume(nullptr);
        // Should not crash
    }

    SECTION("agentite_music_set_volume with NULL is safe") {
        agentite_music_set_volume(nullptr, 0.5f);
        // Should not crash
    }

    SECTION("agentite_music_is_playing with NULL returns false") {
        bool playing = agentite_music_is_playing(nullptr);
        REQUIRE_FALSE(playing);
    }

    SECTION("agentite_music_is_paused with NULL returns false") {
        bool paused = agentite_music_is_paused(nullptr);
        REQUIRE_FALSE(paused);
    }
}

/* ============================================================================
 * Constants Tests
 * ============================================================================ */

TEST_CASE("Audio constants", "[audio][constants]") {
    SECTION("Invalid sound handle value") {
        REQUIRE(AGENTITE_INVALID_SOUND_HANDLE == -1);
    }

    SECTION("Max channels is reasonable") {
        REQUIRE(AGENTITE_AUDIO_MAX_CHANNELS >= 8);
        REQUIRE(AGENTITE_AUDIO_MAX_CHANNELS <= 256);
    }

    SECTION("Max mix samples is pre-allocated size") {
        // 16384 samples = ~170ms at 48kHz stereo
        REQUIRE(AGENTITE_AUDIO_MAX_MIX_SAMPLES == 16384);
    }
}

/* ============================================================================
 * Handle Validation Tests
 * ============================================================================ */

TEST_CASE("Sound handle validation", "[audio][handle]") {
    SECTION("Invalid handle constant is negative") {
        Agentite_SoundHandle invalid = AGENTITE_INVALID_SOUND_HANDLE;
        REQUIRE(invalid < 0);
    }

    SECTION("Operations with invalid handle on NULL audio are safe") {
        Agentite_SoundHandle invalid = AGENTITE_INVALID_SOUND_HANDLE;

        agentite_sound_stop(nullptr, invalid);
        agentite_sound_set_volume(nullptr, invalid, 0.5f);
        agentite_sound_set_pan(nullptr, invalid, 0.0f);
        agentite_sound_set_loop(nullptr, invalid, false);

        bool playing = agentite_sound_is_playing(nullptr, invalid);
        REQUIRE_FALSE(playing);
    }
}
