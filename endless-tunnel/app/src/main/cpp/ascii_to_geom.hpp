// Copyright (C) Google Inc.
// SPDX-License-Identifier: Apache-2.0
#ifndef endlesstunnel_ascii_to_geom_hpp
#define endlesstunnel_ascii_to_geom_hpp

#include "engine.hpp"

/* Converts ASCII art into a Vbo/Ibo pair. Useful for retro-looking
 * drawings/text! scale is the size of each character. The center of the
 * rendering will be 0,0.
 *
 * Examples:
 *
 * Square:
 *     +---+
 *     |   |
 *     +---+
 *
 * Triangle:
 *     +-----+
 *      `   /
 *       ` /
 *        +
 *
 * The + sign represents a vertex; lines are represented by -, /, ` and |.
 */
SimpleGeom* AsciiArtToGeom(const char* art, float scale);

#endif
