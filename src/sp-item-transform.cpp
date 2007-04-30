#define __SP_ITEM_TRANSFORM_C__

/*
 * Transforming single items
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Frank Felfe <innerspace@iname.com>
 *   bulia byak <buliabyak@gmail.com>
 *
 * Copyright (C) 1999-2005 authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <libnr/nr-matrix-ops.h>
#include "libnr/nr-matrix-rotate-ops.h"
#include "libnr/nr-matrix-scale-ops.h"
#include "libnr/nr-matrix-translate-ops.h"
#include "sp-item.h"

static NR::translate inverse(NR::translate const m)
{
	/* TODO: Move this to nr-matrix-fns.h or the like. */
	return NR::translate(-m[0], -m[1]);
}

void
sp_item_rotate_rel(SPItem *item, NR::rotate const &rotation)
{
    NR::Point center = item->getCenter();
    NR::translate const s(item->getCenter());
    NR::Matrix affine = NR::Matrix(inverse(s)) * NR::Matrix(rotation) * NR::Matrix(s);

    // Rotate item.
    sp_item_set_i2d_affine(item, sp_item_i2d_affine(item) * affine);
    // Use each item's own transform writer, consistent with sp_selection_apply_affine()
    sp_item_write_transform(item, SP_OBJECT_REPR(item), item->transform);

    // Restore the center position (it's changed because the bbox center changed)
    if (item->isCenterSet()) {
        item->setCenter(center * affine);
    }
}

void
sp_item_scale_rel (SPItem *item, NR::scale const &scale)
{
    NR::Maybe<NR::Rect> bbox = sp_item_bbox_desktop(item);
    if (bbox) {
        NR::translate const s(bbox->midpoint()); // use getCenter?
        sp_item_set_i2d_affine(item, sp_item_i2d_affine(item) * inverse(s) * scale * s);
        sp_item_write_transform(item, SP_OBJECT_REPR(item), item->transform);
    }
}

void
sp_item_skew_rel (SPItem *item, double skewX, double skewY)
{
    NR::Point center = item->getCenter();
    NR::translate const s(item->getCenter());

    NR::Matrix const skew(1, skewY, skewX, 1, 0, 0);
    NR::Matrix affine = NR::Matrix(inverse(s)) * skew * NR::Matrix(s);

    sp_item_set_i2d_affine(item, sp_item_i2d_affine(item) * affine);
    sp_item_write_transform(item, SP_OBJECT_REPR(item), item->transform);

    // Restore the center position (it's changed because the bbox center changed)
    if (item->isCenterSet()) {
        item->setCenter(center * affine);
    }
}

void sp_item_move_rel(SPItem *item, NR::translate const &tr)
{
	sp_item_set_i2d_affine(item, sp_item_i2d_affine(item) * tr);

	sp_item_write_transform(item, SP_OBJECT_REPR(item), item->transform);
}

/*
** Returns the matrix you need to apply to an object with given bbox and strokewidth to
scale/move it to the new box x0/y0/x1/y1. Takes into account the "scale stroke"
preference value passed to it. Has to solve a quadratic equation to make sure
the goal is met exactly and the stroke scaling is obeyed.
*/

NR::Matrix
get_scale_transform_with_stroke (NR::Rect &bbox_param, gdouble strokewidth, bool transform_stroke, gdouble x0, gdouble y0, gdouble x1, gdouble y1)
{
    NR::Rect bbox (bbox_param);

    NR::Matrix p2o = NR::Matrix (NR::translate (-bbox.min()));
    NR::Matrix o2n = NR::Matrix (NR::translate (x0, y0));

    NR::Matrix scale = NR::Matrix (NR::scale (1, 1)); // scale component
    NR::Matrix unbudge = NR::Matrix (NR::translate (0, 0)); // move component to compensate for the drift caused by stroke width change

    gdouble w0 = bbox.extent(NR::X);
    gdouble h0 = bbox.extent(NR::Y);
    gdouble w1 = x1 - x0;
    gdouble h1 = y1 - y0;
    gdouble r0 = strokewidth;

    if (bbox.isEmpty()) {
        NR::Matrix move = NR::Matrix(NR::translate(x0 - bbox.min()[NR::X], y0 - bbox.min()[NR::Y]));
        return (move); // cannot scale from empty boxes at all, so only translate
    }

    NR::Matrix direct = NR::Matrix (NR::scale(w1 / w0,   h1 / h0));

    if (fabs(w0 - r0) < 1e-6 || fabs(h0 - r0) < 1e-6 || (!transform_stroke && (fabs(w1 - r0) < 1e-6 || fabs(h1 - r0) < 1e-6))) {
        return (p2o * direct * o2n); // can't solve the equation: one of the dimensions is equal to stroke width, so return the straightforward scaler
    }

    // Flip when the width or height changes sign
    int flip_x = ((w1 < 0) == (w0 < 0)) ? 1 : -1;
    int flip_y = ((h1 < 0) == (h0 < 0)) ? 1 : -1;
    
    // w1 and h1 can be negative, but if so then e.g. w1-r0 won't make sense
    // therefore we should use fabs() all over the place
    gdouble ratio_x = (fabs(w1) - fabs(r0)) / (fabs(w0) - fabs(r0));
    gdouble ratio_y = (fabs(h1) - fabs(r0)) / (fabs(h0) - fabs(r0));    
    
    NR::Matrix direct_constant_r = NR::Matrix (NR::scale(flip_x * ratio_x, flip_y*ratio_y));

    if (transform_stroke && r0 != 0 && r0 != NR_HUGE) { // there's stroke, and we need to scale it
        // These coefficients are obtained from the assumption that scaling applies to the
        // non-stroked "shape proper" and that stroke scale is scaled by the expansion of that
        // matrix
        // In fact, we're trying to solve this equation:
        // r1 = r0 * sqrt (((w1-r0)/(w0-r0))*((h1-r1)/(h0-r0)))
        // To make sense of this, the operant of the sqrt() should
        // be positive, hence all the fabs() below
        // (w1 and h1 will be negative when mirroring, w0 and h0 will probably never be negative)
        gdouble A = -fabs(w0*h0) + fabs(r0)*(fabs(w0) + fabs(h0));
        gdouble B = -(fabs(w1) + fabs(h1)) * r0*r0;
        gdouble C = fabs(w1 * h1 * r0*r0);
        if (B*B - 4*A*C > 0) {
            gdouble r1 = (-B - sqrt (B*B - 4*A*C))/(2*A);
            //gdouble r2 = (-B + sqrt (B*B - 4*A*C))/(2*A);
            //std::cout << "r0" << r0 << " r1" << r1 << " r2" << r2 << "\n";
            //
            // I think r1 will always be positive if r0 is (mathematical proof?)
            // but if w1 becomes negative, then the scale will be wrong if we just do  
            // gdouble scale_x = (w1 - r1)/(w0 - r0);
            // gdouble scale_y = (h1 - r1)/(h0 - r0);
            // So let's do it like this: Calculate the absolute scale
            gdouble scale_x = (fabs(w1) - fabs(r1))/(fabs(w0) - fabs(r0));
            gdouble scale_y = (fabs(h1) - fabs(r1))/(fabs(h0) - fabs(r0));
            scale *= NR::scale(flip_x*scale_x, flip_y*scale_y);
            unbudge *= NR::translate (-flip_x * 0.5 * (fabs(r0) * scale_x - fabs(r1)), -flip_y * 0.5 * (fabs(r0) * scale_y - fabs(r1)));
        } else {
            scale *= direct;
        }
    } else {
        if (r0 == 0 || r0 == NR_HUGE) { // no stroke to scale
            scale *= direct;
        } else {// nonscaling strokewidth
            scale *= direct_constant_r;
            unbudge *= NR::translate (flip_x * 0.5 * fabs(r0) * (1 - ratio_x), flip_y * 0.5 * fabs(r0) * (1 - ratio_y));
        }
    }

    return (p2o * scale * unbudge * o2n);
}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=99 :
