# Loopino — A creative Sampler

Loopino is a lightweight audio sampler. It allows you to load, trim, and loop audio files with precision, making it ideal for crafting seamless sample loops.
With a clean, minimal interface, Loopino fits perfectly into any audio workflow — whether for sound design, live performance, or creative sampling experiments.
Loopino is available as standalone application, as Clap plugin and as vst2 plugin.

## Features

- ADSR
- load Samples by drag and drop
- integrated file browser
- record and use Samples on the fly
- trim Samples to clip marks
- integrated non destructive fade out option
- integrated Pitch tracker 
- integrated Micro Loop Generator
- option to select Loop duration
- integrated non destructive wave sharpers ( square - saw tooth )
- Preset handling 
- export wav files from the Samples/Loops processed with the selected Key Note
- Ladder filter (resonance, cutoff)
- Phase Modulators (sine, triangle, noise, juno)
- vibrato
- tremolo
- synth root frequency control
- up to 48 voices


Craft seamless loops in seconds — Loopino keeps sampling simple, fast, and fun.

<p align="center">
    <img src="https://github.com/brummer10/Loopino/blob/main/loopino.png?raw=true" />
</p>


The GUI is created with libxputty.


## Dependencies

- libsndfile1-dev
- libfftw3-dev
- libjack(-jackd2)-dev
- libcairo2-dev
- libx11-dev

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
