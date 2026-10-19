#pragma once

#include "mdbl.h"

// Add sparse palette-native facet shading without allocating render resources.
void draw_texture(GContext *ctx, GRect bounds, GPoint centre, const SectorRay rays[3]);
