# Loopino — A Minimalist Sampler for Linux

Loopino is a lightweight audio sampler designed for Linux systems, built around the JACK Audio Connection Kit. It allows you to load, trim, and loop audio files with precision, making it ideal for crafting seamless sample loops.
With a clean, minimal interface and smooth JACK integration, Loopino fits perfectly into any Linux-based audio workflow — whether for sound design, live performance, or creative sampling experiments.

## Features

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
