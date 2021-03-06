#ifndef INKSCAPE_LPE_SPIRO_H
#define INKSCAPE_LPE_SPIRO_H

/*
 * Inkscape::LPESpiro
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "live_effects/effect.h"


namespace Inkscape {
namespace LivePathEffect {

class LPESpiro : public Effect {
public:
    LPESpiro(LivePathEffectObject *lpeobject);
    ~LPESpiro() override;

    LPEPathFlashType pathFlashType() const override { return SUPPRESS_FLASH; }

    void doEffect(SPCurve * curve) override;

private:
    LPESpiro(const LPESpiro&) = delete;
    LPESpiro& operator=(const LPESpiro&) = delete;
};

void sp_spiro_do_effect(SPCurve *curve);

}; //namespace LivePathEffect
}; //namespace Inkscape

#endif
