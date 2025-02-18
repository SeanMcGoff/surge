/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2023, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#include "MSToolEffect.h"
#include "Parameter.h"
#include "SurgeStorage.h"
#include <vembertech/basic_dsp.h>
#include "globals.h"
#include "sst/basic-blocks/dsp/MidSide.h"
#include "sst/basic-blocks/mechanics/block-ops.h"
namespace sdsp = sst::basic_blocks::dsp;
namespace mech = sst::basic_blocks::mechanics;

MSToolEffect::MSToolEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), hpm(storage), hps(storage), lpm(storage), lps(storage),
      bandm(storage), bands(storage)
{
    ampM.set_blocksize(BLOCK_SIZE);
    ampS.set_blocksize(BLOCK_SIZE);
    postampL.set_blocksize(BLOCK_SIZE);
    postampR.set_blocksize(BLOCK_SIZE);

    hpm.setBlockSize(BLOCK_SIZE * slowrate);
    bandm.setBlockSize(BLOCK_SIZE * slowrate);
    lpm.setBlockSize(BLOCK_SIZE * slowrate);
    hps.setBlockSize(BLOCK_SIZE * slowrate);
    bands.setBlockSize(BLOCK_SIZE * slowrate);
    lps.setBlockSize(BLOCK_SIZE * slowrate);
}

MSToolEffect::~MSToolEffect() {}

void MSToolEffect::init()
{
    setvars(true);
    hpm.suspend();
    bandm.suspend();
    lpm.suspend();
    hps.suspend();
    bands.suspend();
    lps.suspend();
}

void MSToolEffect::setvars(bool init)
{

    if (init)
    {
        bandm.coeff_peakEQ(bandm.calc_omega(*pd_float[mstl_freqm] * (1.f / 12.f)), 1, 1.f);
        bands.coeff_peakEQ(bands.calc_omega(*pd_float[mstl_freqs] * (1.f / 12.f)), 1, 1.f);

        hpm.coeff_instantize();
        bandm.coeff_instantize();
        lpm.coeff_instantize();
        hps.coeff_instantize();
        bands.coeff_instantize();
        lps.coeff_instantize();

        ampM.set_target(1.f);
        ampS.set_target(1.f);
        postampL.set_target(-1.f);
        postampR.set_target(1.f);

        ampM.instantize();
        ampS.instantize();
        postampL.instantize();
        postampR.instantize();
    }
    else
    {
        hpm.coeff_HP(hpm.calc_omega(*pd_float[mstl_hpm] / 12.0), 0.4);
        bandm.coeff_peakEQ(bandm.calc_omega(*pd_float[mstl_freqm] * (1.f / 12.f)), 1,
                           *pd_float[mstl_pqm]);
        lpm.coeff_LP(lpm.calc_omega(*pd_float[mstl_lpm] / 12.0), 0.4);
        hps.coeff_HP(hps.calc_omega(*pd_float[mstl_hps] / 12.0), 0.4);
        bands.coeff_peakEQ(bands.calc_omega(*pd_float[mstl_freqs] * (1.f / 12.f)), 1,
                           *pd_float[mstl_pqs]);
        lps.coeff_LP(lps.calc_omega(*pd_float[mstl_lps] / 12.0), 0.4);
    }
}

void MSToolEffect::process(float *dataL, float *dataR)
{
    setvars(false);

    ampM.set_target_smoothed(storage->db_to_linear(*pd_float[mstl_mgain]));
    ampS.set_target_smoothed(storage->db_to_linear(*pd_float[mstl_sgain]));
    postampL.set_target_smoothed(clamp1bp(1 - *pd_float[mstl_outgain]));
    postampR.set_target_smoothed(clamp1bp(1 + *pd_float[mstl_outgain]));

    float M alignas(16)[BLOCK_SIZE], S alignas(16)[BLOCK_SIZE];

    int io = *(pd_int[mstl_matrix]); // (fxdata->p[mstl_matrix].val.i);
    switch (io)
    {
    case 0:
        sdsp::encodeMS<BLOCK_SIZE>(dataL, dataR, M, S);
        break;
    case 1:
        sdsp::encodeMS<BLOCK_SIZE>(dataL, dataR, M, S);
        break;
    case 2:
        mech::copy_from_to<BLOCK_SIZE>(dataL, M);
        mech::copy_from_to<BLOCK_SIZE>(dataR, S);
        break;
    }

    if (!fxdata->p[mstl_hpm].deactivated)
        hpm.process_block(M);
    if (!fxdata->p[mstl_pqm].deactivated)
        bandm.process_block(M);
    if (!fxdata->p[mstl_lpm].deactivated)
        lpm.process_block(M);

    if (!fxdata->p[mstl_hps].deactivated)
        hps.process_block(S);
    if (!fxdata->p[mstl_pqs].deactivated)
        bands.process_block(S);
    if (!fxdata->p[mstl_lps].deactivated)
        lps.process_block(S);

    ampM.multiply_block(M, BLOCK_SIZE_QUAD);
    ampS.multiply_block(S, BLOCK_SIZE_QUAD);

    switch (io)
    {
    case 0:
        sdsp::decodeMS<BLOCK_SIZE>(M, S, dataL, dataR);
        break;
    case 1:
        mech::copy_from_to<BLOCK_SIZE>(M, dataL);
        mech::copy_from_to<BLOCK_SIZE>(S, dataR);
        break;
    case 2:
        sdsp::decodeMS<BLOCK_SIZE>(M, S, dataL, dataR);
        break;
    }

    postampL.multiply_block(dataL, BLOCK_SIZE_QUAD);
    postampR.multiply_block(dataR, BLOCK_SIZE_QUAD);
}

const char *MSToolEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Options";
    case 1:
        return "Mid Filter";
    case 2:
        return "Side Filter";
    case 3:
        return "Output";
    }
    return 0;
}

int MSToolEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 5;
    case 2:
        return 15;
    case 3:
        return 25;
    }
    return 0;
}

void MSToolEffect::suspend() { init(); }

void MSToolEffect::init_ctrltypes()
{
    // using deactivation function from EQ3
    static struct EQD : public ParameterDynamicDeactivationFunction
    {
        bool getValue(const Parameter *p) const override
        {
            auto fx = &(p->storage->getPatch().fx[p->ctrlgroup_entry]);
            auto idx = p - fx->p;

            switch (idx)
            {
            case mstl_freqm:
                return fx->p[mstl_pqm].deactivated;
            case mstl_freqs:
                return fx->p[mstl_pqs].deactivated;
            }

            return false;
        }
        Parameter *getPrimaryDeactivationDriver(const Parameter *p) const override
        {
            auto fx = &(p->storage->getPatch().fx[p->ctrlgroup_entry]);
            auto idx = p - fx->p;

            switch (idx)
            {
            case mstl_freqm:
                return &(fx->p[mstl_pqm]);
            case mstl_freqs:
                return &(fx->p[mstl_pqs]);
            }
            return nullptr;
        }
    } eqGroupDeact;

    Effect::init_ctrltypes();

    fxdata->p[mstl_matrix].set_name("Matrix");
    fxdata->p[mstl_matrix].set_type(ct_mscodec);

    fxdata->p[mstl_hpm].set_name("Low Cut");
    fxdata->p[mstl_hpm].set_type(ct_freq_audible_deactivatable_hp);
    fxdata->p[mstl_pqm].set_name("Gain");
    fxdata->p[mstl_pqm].set_type(ct_decibel_narrow_deactivatable);
    fxdata->p[mstl_freqm].set_name("Frequency");
    fxdata->p[mstl_freqm].set_type(ct_freq_audible);
    fxdata->p[mstl_freqm].dynamicDeactivation = &eqGroupDeact;
    fxdata->p[mstl_lpm].set_name("High Cut");
    fxdata->p[mstl_lpm].set_type(ct_freq_audible_deactivatable_lp);

    fxdata->p[mstl_hps].set_name("Low Cut");
    fxdata->p[mstl_hps].set_type(ct_freq_audible_deactivatable_hp);
    fxdata->p[mstl_pqs].set_name("Gain");
    fxdata->p[mstl_pqs].set_type(ct_decibel_narrow_deactivatable);
    fxdata->p[mstl_freqs].set_name("Frequency");
    fxdata->p[mstl_freqs].set_type(ct_freq_audible);
    fxdata->p[mstl_freqs].dynamicDeactivation = &eqGroupDeact;
    fxdata->p[mstl_lps].set_name("High Cut");
    fxdata->p[mstl_lps].set_type(ct_freq_audible_deactivatable_lp);

    fxdata->p[mstl_mgain].set_name("Mid Gain");
    fxdata->p[mstl_mgain].set_type(ct_decibel_attenuation_plus12);
    fxdata->p[mstl_sgain].set_name("Side Gain");
    fxdata->p[mstl_sgain].set_type(ct_decibel_attenuation_plus12);
    fxdata->p[mstl_outgain].set_name("Balance");
    fxdata->p[mstl_outgain].set_type(ct_percent_bipolar_stereo);

    fxdata->p[mstl_matrix].posy_offset = 1;

    fxdata->p[mstl_hpm].posy_offset = 3;
    fxdata->p[mstl_pqm].posy_offset = 3;
    fxdata->p[mstl_freqm].posy_offset = 3;
    fxdata->p[mstl_lpm].posy_offset = 3;

    fxdata->p[mstl_hps].posy_offset = 5;
    fxdata->p[mstl_pqs].posy_offset = 5;
    fxdata->p[mstl_freqs].posy_offset = 5;
    fxdata->p[mstl_lps].posy_offset = 5;

    fxdata->p[mstl_mgain].posy_offset = 7;
    fxdata->p[mstl_sgain].posy_offset = 7;
    fxdata->p[mstl_outgain].posy_offset = 7;
}

void MSToolEffect::init_default_values()
{
    fxdata->p[mstl_matrix].val.i = 0;

    fxdata->p[mstl_hpm].val.f = -60;
    fxdata->p[mstl_hpm].deactivated = true;
    fxdata->p[mstl_pqm].val.f = 0;
    fxdata->p[mstl_pqm].deactivated = true;
    fxdata->p[mstl_freqm].val.f = -6.63049;
    fxdata->p[mstl_lpm].val.f = 70;
    fxdata->p[mstl_lpm].deactivated = true;

    fxdata->p[mstl_hps].val.f = -60;
    fxdata->p[mstl_hps].deactivated = true;
    fxdata->p[mstl_pqs].val.f = 0;
    fxdata->p[mstl_pqs].deactivated = true;
    fxdata->p[mstl_freqs].val.f = 50.2131;
    fxdata->p[mstl_lps].val.f = 70;
    fxdata->p[mstl_lps].deactivated = true;

    fxdata->p[mstl_mgain].val.f = 0;
    fxdata->p[mstl_sgain].val.f = 0;
    fxdata->p[mstl_outgain].val.f = 0;
}

void MSToolEffect::handleStreamingMismatches(int streamingRevision,
                                             int currentSynthStreamingRevision)
{
    if (streamingRevision <= 17)
    {
    }
}
