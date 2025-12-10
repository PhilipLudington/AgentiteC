# Audio System

Sound effects and music playback with mixing support.

## Quick Start

```c
#include "agentite/audio.h"

// Initialize
Agentite_Audio *audio = agentite_audio_init();

// Load sounds (WAV, fully loaded in memory)
Agentite_Sound *shoot = agentite_sound_load(audio, "assets/sounds/shoot.wav");
Agentite_Music *music = agentite_music_load(audio, "assets/music/background.wav");
```

## Sound Effects

```c
// Simple playback (returns handle for control)
Agentite_SoundHandle h = agentite_sound_play(audio, shoot);

// With options: volume (0-1), pan (-1 to +1), loop
agentite_sound_play_ex(audio, shoot, 0.8f, 0.0f, false);

// Control playing sound
agentite_sound_set_volume(audio, h, 0.5f);
agentite_sound_set_pan(audio, h, -0.5f);  // Pan left
agentite_sound_stop(audio, h);
if (agentite_sound_is_playing(audio, h)) { }
```

## Music

```c
agentite_music_play(audio, music);                 // Loop by default
agentite_music_play_ex(audio, music, 0.7f, true);  // Volume + loop
agentite_music_pause(audio);
agentite_music_resume(audio);
agentite_music_stop(audio);
```

## Volume Controls

```c
agentite_audio_set_master_volume(audio, 0.8f);  // Affects all audio
agentite_audio_set_sound_volume(audio, 1.0f);   // Affects all sounds
agentite_audio_set_music_volume(audio, 0.5f);   // Affects music only
```

## Game Loop

```c
// Update each frame (for streaming, currently no-op)
agentite_audio_update(audio);
```

## Cleanup

```c
agentite_sound_destroy(audio, shoot);
agentite_music_destroy(audio, music);
agentite_audio_shutdown(audio);
```

## Key Features

- Up to 32 simultaneous sound channels
- Automatic format conversion (any WAV → float32 stereo)
- Per-sound volume and stereo panning
- Separate volume controls for master, sounds, and music
