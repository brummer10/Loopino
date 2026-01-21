# Loopino — A creative Sampler

Loopino is a lightweight audio sampler. It allows you to load, trim, and loop audio files with precision, making it ideal for crafting seamless sample loops.
With a clean, minimal interface, Loopino fits perfectly into any audio workflow — whether for sound design, live performance, or creative sampling experiments.
Loopino is available as standalone application, as Clap plugin and as vst2 plugin.

## Features

Craft seamless loops in seconds — Loopino keeps sampling simple, fast, and fun.

<p align="center">
    <img src="https://github.com/brummer10/Loopino/blob/main/loopino.png?raw=true" />
</p>

## Key Features

- Drag-and-drop sample loading
- Integrated file browser
- On-the-fly audio recording
- Trim samples to clip marks
- Non-destructive fade-out processing
- Integrated pitch tracker
- Micro Loop Generator with selectable loop number & duration
- Square & sawtooth wave sharpers (non-destructive)
- Full ADSR envelope
- Preset save/load system
- Export processed samples/loops as WAV in selected key
- LP/HP ladder filter with resonance & cutoff
- SEM12 Filter
- Wasp Filter
- TB-303 Filter
- Phase modulators: sine, triangle, noise & Juno-style
- Velocity curve support: Soft, Piano, Punch
- Vibrato & tremolo
- Chorus & Reverb
- Volume & Tone control
- Pitch Wheel support
- MIDI PC support
- Root frequency control
- Up to 48 voices for polyphonic playback
- Full ALSA Audio & RAW MIDI support
- Full jack-audio-connection-kit support


[Documentation](https://github.com/brummer10/Loopino/wiki/User-Documentation)

The GUI is created with libxputty.

## Availability

- Linux: Standalone application, CLAP plugin, VST2 plugin
- Windows: CLAP plugin, VST2 plugin

## Dependencies

- libsndfile1-dev
- libfftw3-dev
- libcairo2-dev
- libx11-dev

# Additional Dependencies for standalone Version

- libjack(-jackd2)-dev
- libasound2-dev

## Building the Standalone from source code

```shell
git clone https://github.com/brummer10/Loopino.git
cd Loopino
git submodule update --init --recursive
make
sudo make install # will install into /usr/bin
```

## Building as Clap plugin from source code

```shell
git clone https://github.com/brummer10/Loopino.git
cd Loopino
git submodule update --init --recursive
make clap
sudo make install # will install into /usr/lib/clap
or
make install # will install into ~/.clap
```

## Building as vst2 plugin from source code

```shell
git clone https://github.com/brummer10/Loopino.git
cd Loopino
git submodule update --init --recursive
make vst2
sudo make install # will install into /usr/lib/vst
or
make install # will install into ~/.vst
```
