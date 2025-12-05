# Audio System

Sound effects and music playback with mixing support.

## Quick Start

```c
#include "carbon/audio.h"

// Initialize
Carbon_Audio *audio = carbon_audio_init();

// Load sounds (WAV, fully loaded in memory)
Carbon_Sound *shoot = carbon_sound_load(audio, "assets/sounds/shoot.wav");
Carbon_Music *music = carbon_music_load(audio, "assets/music/background.wav");
```

## Sound Effects

```c
// Simple playback (returns handle for control)
Carbon_SoundHandle h = carbon_sound_play(audio, shoot);

// With options: volume (0-1), pan (-1 to +1), loop
carbon_sound_play_ex(audio, shoot, 0.8f, 0.0f, false);

// Control playing sound
carbon_sound_set_volume(audio, h, 0.5f);
carbon_sound_set_pan(audio, h, -0.5f);  // Pan left
carbon_sound_stop(audio, h);
if (carbon_sound_is_playing(audio, h)) { }
```

## Music

```c
carbon_music_play(audio, music);                 // Loop by default
carbon_music_play_ex(audio, music, 0.7f, true);  // Volume + loop
carbon_music_pause(audio);
carbon_music_resume(audio);
carbon_music_stop(audio);
```

## Volume Controls

```c
carbon_audio_set_master_volume(audio, 0.8f);  // Affects all audio
carbon_audio_set_sound_volume(audio, 1.0f);   // Affects all sounds
carbon_audio_set_music_volume(audio, 0.5f);   // Affects music only
```

## Game Loop

```c
// Update each frame (for streaming, currently no-op)
carbon_audio_update(audio);
```

## Cleanup

```c
carbon_sound_destroy(audio, shoot);
carbon_music_destroy(audio, music);
carbon_audio_shutdown(audio);
```

## Key Features

- Up to 32 simultaneous sound channels
- Automatic format conversion (any WAV → float32 stereo)
- Per-sound volume and stereo panning
- Separate volume controls for master, sounds, and music
