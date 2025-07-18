//=============================================================================
//
// Adventure Game Studio (AGS)
//
// Copyright (C) 1999-2011 Chris Jones and 2011-2025 various contributors
// The full list of copyright holders can be found in the Copyright.txt
// file, which is part of this source code distribution.
//
// The AGS source code is provided under the Artistic License 2.0.
// A copy of this license can be found in the file License.txt and at
// https://opensource.org/license/artistic-2-0/
//
//=============================================================================
//
// Intermediate level drawing utility functions.
//
// GfxUtil namespace is meant for intermediate-to-lower level functions, that
// implement specific conversions, tricks and hacks for drawing bitmaps and
// geometry.
// The suggested convention is to add only those functions, that do not require
// any knowledge of higher-level engine types and objects.
//
//=============================================================================
#ifndef __AGS_EE_GFX__GFXUTIL_H
#define __AGS_EE_GFX__GFXUTIL_H

#include "gfx/bitmap.h"
#include "gfx/gfx_def.h"

namespace AGS
{
namespace Engine
{

using Common::Bitmap;

namespace GfxUtil
{
    // Creates a COPY of the source bitmap, converted to the given format.
    // By default this keeps mask pixels intact, only converting mask color
    // value as necessary; but optionally you may request to ignore mask pixels
    // and treat them as regular color.
    Bitmap *ConvertBitmap(const Bitmap *src, int dst_color_depth, bool keep_mask = true);

    // Considers the given information about source and destination surfaces,
    // then draws a bimtap over another either using requested blending mode,
    // or fallbacks to common "magic pink" transparency mode;
    // optionally uses blending alpha (overall image transparency).
    void DrawSpriteBlend(Bitmap *ds, const Point &ds_at, const Bitmap *sprite,
        Common::BlendMode blend_mode, bool dst_has_alpha = true, bool src_has_alpha = true, int blend_alpha = 0xFF);

    // Draws a bitmap over another one with given alpha level (0 - 255),
    // takes account of the bitmap's mask color,
    // ignores image's alpha channel, even if there's one;
    // does a conversion if sprite and destination color depths do not match.
    void DrawSpriteWithTransparency(Bitmap *ds, const Bitmap *sprite, int x, int y, int alpha = 0xFF);
} // namespace GfxUtil

} // namespace Engine
} // namespace AGS

#endif // __AGS_EE_GFX__GFXUTIL_H
