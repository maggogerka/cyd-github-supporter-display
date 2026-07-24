# PNGdec16

This directory contains a project-local variant of
[PNGdec 1.1.6](https://github.com/bitbank2/PNGdec), licensed under Apache-2.0.

The local change accepts non-interlaced PNG files with 16-bit samples and
converts their most significant sample bytes to RGB565. GitHub can preserve a
user avatar's 16-bit PNG depth even when resizing it to 96×96, while upstream
PNGdec 1.1.6 rejects sample depths above 8 bits.

The original copyright and license are preserved in `LICENSE`.
