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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_REVERB2EFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_REVERB2EFFECT_H
#include "Effect.h"
#include "BiquadFilter.h"
#include "DSPUtils.h"
#include "AllpassFilter.h"

#include <vembertech/lipol.h>
#include "sst/basic-blocks/dsp/QuadratureOscillators.h"

class Reverb2Effect : public Effect
{
    // These are set to make sure we have delay lengths up to 384k
    static constexpr int NUM_BLOCKS = 4, NUM_INPUT_ALLPASSES = 4, NUM_ALLPASSES_PER_BLOCK = 2,
                         MAX_ALLPASS_LEN = 16384 * 8, MAX_DELAY_LEN = 16384 * 8,
                         DELAY_LEN_MASK = MAX_DELAY_LEN - 1, DELAY_SUBSAMPLE_BITS = 8,
                         DELAY_SUBSAMPLE_RANGE = (1 << DELAY_SUBSAMPLE_BITS),
                         PREDELAY_BUFFER_SIZE =
                             48000 * 8 * 4,         // max sample rate is 48000 * 8 probably
        PREDELAY_BUFFER_SIZE_LIMIT = 48000 * 8 * 3; // allow for one second of diffusion

    class allpass
    {
      public:
        allpass();
        float process(float x, float coeff);
        void setLen(int len);

      private:
        int _len;
        int _k;
        float _data[MAX_ALLPASS_LEN];
    };

    class delay
    {
      public:
        delay();
        float process(float x, int tap1, float &tap_out1, int tap2, float &tap_out2,
                      int modulation);
        void setLen(int len);

      private:
        int _len;
        int _k;
        float _data[MAX_DELAY_LEN];
    };

    class predelay
    {
      public:
        predelay() { memset(_data, 0, PREDELAY_BUFFER_SIZE * sizeof(float)); }
        float process(float in, int tap)
        {
            k = (k + 1);
            if (k == PREDELAY_BUFFER_SIZE)
                k = 0;
            auto p = k - tap;
            while (p < 0)
                p += PREDELAY_BUFFER_SIZE;
            auto res = _data[p];
            _data[k] = in;
            return res;
        }

      private:
        int k = 0;
        float _data[PREDELAY_BUFFER_SIZE];
    };

    class onepole_filter
    {
      public:
        onepole_filter();
        float process_lowpass(float x, float c0);
        float process_highpass(float x, float c0);

      private:
        float a0;
    };

    lipol_ps_blocksz mix alignas(16), width alignas(16);

  public:
    Reverb2Effect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~Reverb2Effect();
    virtual const char *get_effectname() override { return "reverb2"; }
    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;
    void setvars(bool init);
    void calc_size(float scale);
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;
    virtual int get_ringout_decay() override { return ringout_time; }

    enum rev2_params
    {
        rev2_predelay = 0,

        rev2_room_size,
        rev2_decay_time,
        rev2_diffusion,
        rev2_buildup,
        rev2_modulation,

        rev2_lf_damping,
        rev2_hf_damping,

        rev2_width,
        rev2_mix,

        rev2_num_params,
    };

  private:
    void update_rtime();
    int ringout_time;
    allpass _input_allpass[NUM_INPUT_ALLPASSES];
    allpass _allpass[NUM_BLOCKS][NUM_ALLPASSES_PER_BLOCK];
    onepole_filter _hf_damper[NUM_BLOCKS];
    onepole_filter _lf_damper[NUM_BLOCKS];
    delay _delay[NUM_BLOCKS];
    predelay _predelay;
    int _tap_timeL[NUM_BLOCKS];
    int _tap_timeR[NUM_BLOCKS];
    float _tap_gainL[NUM_BLOCKS];
    float _tap_gainR[NUM_BLOCKS];
    float _state;
    lipol<float, true> _decay_multiply;
    lipol<float, true> _diffusion;
    lipol<float, true> _buildup;
    lipol<float, true> _hf_damp_coefficent;
    lipol<float, true> _lf_damp_coefficent;
    lipol<float, true> _modulation;

    using quadr_osc = sst::basic_blocks::dsp::SurgeQuadrOsc<float>;
    quadr_osc _lfo;
    float last_decay_time = -1.0;
};

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_REVERB2EFFECT_H
