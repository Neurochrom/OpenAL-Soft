/**
 * OpenAL cross platform audio library
 * Copyright (C) 2013 by Mike Gorchak
 * This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA  02111-1307, USA.
 * Or go to http://www.gnu.org/copyleft/lgpl.html
 */

#include "../config.h"

#include <math.h>
#include <stdlib.h>

#include "../alMain.h"
#include "../alFilter.h"
#include "../alAuxEffectSlot.h"
#include "../alError.h"
#include "../alu.h"


typedef struct ALflangerState {
    DERIVE_FROM_TYPE(ALeffectState);

    ALfloat *SampleBuffer[2];
    ALuint BufferLength;
    ALuint offset;
    ALuint lfo_range;
    ALfloat lfo_scale;
    ALint lfo_disp;

    /* Gains for left and right sides */
    ALfloat Gain[2][MaxChannels];

    /* effect parameters */
    ALint waveform;
    ALint delay;
    ALfloat depth;
    ALfloat feedback;
} ALflangerState;

static ALvoid ALflangerState_Destruct(ALflangerState *state)
{
    free(state->SampleBuffer[0]);
    state->SampleBuffer[0] = NULL;
    state->SampleBuffer[1] = NULL;
}

static ALboolean ALflangerState_deviceUpdate(ALflangerState *state, ALCdevice *Device)
{
    ALuint maxlen;
    ALuint it;

    maxlen = fastf2u(AL_FLANGER_MAX_DELAY * 3.0f * Device->Frequency) + 1;
    maxlen = NextPowerOf2(maxlen);

    if(maxlen != state->BufferLength)
    {
        void *temp;

        temp = realloc(state->SampleBuffer[0], maxlen * sizeof(ALfloat) * 2);
        if(!temp) return AL_FALSE;
        state->SampleBuffer[0] = temp;
        state->SampleBuffer[1] = state->SampleBuffer[0] + maxlen;

        state->BufferLength = maxlen;
    }

    for(it = 0;it < state->BufferLength;it++)
    {
        state->SampleBuffer[0][it] = 0.0f;
        state->SampleBuffer[1][it] = 0.0f;
    }

    return AL_TRUE;
}

static ALvoid ALflangerState_update(ALflangerState *state, ALCdevice *Device, const ALeffectslot *Slot)
{
    ALfloat frequency = (ALfloat)Device->Frequency;
    ALfloat rate;
    ALint phase;

    state->waveform = Slot->EffectProps.Flanger.Waveform;
    state->depth = Slot->EffectProps.Flanger.Depth;
    state->feedback = Slot->EffectProps.Flanger.Feedback;
    state->delay = fastf2i(Slot->EffectProps.Flanger.Delay * frequency);

    /* Gains for left and right sides */
    ComputeAngleGains(Device, atan2f(-1.0f, 0.0f), 0.0f, Slot->Gain, state->Gain[0]);
    ComputeAngleGains(Device, atan2f(+1.0f, 0.0f), 0.0f, Slot->Gain, state->Gain[1]);

    phase = Slot->EffectProps.Flanger.Phase;
    rate = Slot->EffectProps.Flanger.Rate;
    if(!(rate > 0.0f))
    {
        state->lfo_scale = 0.0f;
        state->lfo_range = 1;
        state->lfo_disp = 0;
    }
    else
    {
        /* Calculate LFO coefficient */
        state->lfo_range = fastf2u(frequency/rate + 0.5f);
        switch(state->waveform)
        {
            case AL_FLANGER_WAVEFORM_TRIANGLE:
                state->lfo_scale = 4.0f / state->lfo_range;
                break;
            case AL_FLANGER_WAVEFORM_SINUSOID:
                state->lfo_scale = F_2PI / state->lfo_range;
                break;
        }

        /* Calculate lfo phase displacement */
        state->lfo_disp = fastf2i(state->lfo_range * (phase/360.0f));
    }
}

static inline void Triangle(ALint *delay_left, ALint *delay_right, ALuint offset, const ALflangerState *state)
{
    ALfloat lfo_value;

    lfo_value = 2.0f - fabsf(2.0f - state->lfo_scale*(offset%state->lfo_range));
    lfo_value *= state->depth * state->delay;
    *delay_left = fastf2i(lfo_value) + state->delay;

    offset += state->lfo_disp;
    lfo_value = 2.0f - fabsf(2.0f - state->lfo_scale*(offset%state->lfo_range));
    lfo_value *= state->depth * state->delay;
    *delay_right = fastf2i(lfo_value) + state->delay;
}

static inline void Sinusoid(ALint *delay_left, ALint *delay_right, ALuint offset, const ALflangerState *state)
{
    ALfloat lfo_value;

    lfo_value = 1.0f + sinf(state->lfo_scale*(offset%state->lfo_range));
    lfo_value *= state->depth * state->delay;
    *delay_left = fastf2i(lfo_value) + state->delay;

    offset += state->lfo_disp;
    lfo_value = 1.0f + sinf(state->lfo_scale*(offset%state->lfo_range));
    lfo_value *= state->depth * state->delay;
    *delay_right = fastf2i(lfo_value) + state->delay;
}

#define DECL_TEMPLATE(Func)                                                   \
static void Process##Func(ALflangerState *state, const ALuint SamplesToDo,    \
  const ALfloat *restrict SamplesIn, ALfloat (*restrict out)[2])              \
{                                                                             \
    const ALuint bufmask = state->BufferLength-1;                             \
    ALfloat *restrict leftbuf = state->SampleBuffer[0];                       \
    ALfloat *restrict rightbuf = state->SampleBuffer[1];                      \
    ALuint offset = state->offset;                                            \
    const ALfloat feedback = state->feedback;                                 \
    ALuint it;                                                                \
                                                                              \
    for(it = 0;it < SamplesToDo;it++)                                         \
    {                                                                         \
        ALint delay_left, delay_right;                                        \
        Func(&delay_left, &delay_right, offset, state);                       \
                                                                              \
        out[it][0] = leftbuf[(offset-delay_left)&bufmask];                    \
        leftbuf[offset&bufmask] = (out[it][0]+SamplesIn[it]) * feedback;      \
                                                                              \
        out[it][1] = rightbuf[(offset-delay_right)&bufmask];                  \
        rightbuf[offset&bufmask] = (out[it][1]+SamplesIn[it]) * feedback;     \
                                                                              \
        offset++;                                                             \
    }                                                                         \
    state->offset = offset;                                                   \
}

DECL_TEMPLATE(Triangle)
DECL_TEMPLATE(Sinusoid)

#undef DECL_TEMPLATE

static ALvoid ALflangerState_process(ALflangerState *state, ALuint SamplesToDo, const ALfloat *restrict SamplesIn, ALfloat (*restrict SamplesOut)[BUFFERSIZE])
{
    ALuint it, kt;
    ALuint base;

    for(base = 0;base < SamplesToDo;)
    {
        ALfloat temps[64][2];
        ALuint td = minu(SamplesToDo-base, 64);

        if(state->waveform == AL_FLANGER_WAVEFORM_TRIANGLE)
            ProcessTriangle(state, td, SamplesIn+base, temps);
        else if(state->waveform == AL_FLANGER_WAVEFORM_SINUSOID)
            ProcessSinusoid(state, td, SamplesIn+base, temps);

        for(kt = 0;kt < MaxChannels;kt++)
        {
            ALfloat gain = state->Gain[0][kt];
            if(gain > GAIN_SILENCE_THRESHOLD)
            {
                for(it = 0;it < td;it++)
                    SamplesOut[kt][it+base] += temps[it][0] * gain;
            }

            gain = state->Gain[1][kt];
            if(gain > GAIN_SILENCE_THRESHOLD)
            {
                for(it = 0;it < td;it++)
                    SamplesOut[kt][it+base] += temps[it][1] * gain;
            }
        }

        base += td;
    }
}

static void ALflangerState_Delete(ALflangerState *state)
{
    free(state);
}

DEFINE_ALEFFECTSTATE_VTABLE(ALflangerState);


typedef struct ALflangerStateFactory {
    DERIVE_FROM_TYPE(ALeffectStateFactory);
} ALflangerStateFactory;

ALeffectState *ALflangerStateFactory_create(ALflangerStateFactory *UNUSED(factory))
{
    ALflangerState *state;

    state = malloc(sizeof(*state));
    if(!state) return NULL;
    SET_VTABLE2(ALflangerState, ALeffectState, state);

    state->BufferLength = 0;
    state->SampleBuffer[0] = NULL;
    state->SampleBuffer[1] = NULL;
    state->offset = 0;
    state->lfo_range = 1;

    return STATIC_CAST(ALeffectState, state);
}

DEFINE_ALEFFECTSTATEFACTORY_VTABLE(ALflangerStateFactory);

ALeffectStateFactory *ALflangerStateFactory_getFactory(void)
{
    static ALflangerStateFactory FlangerFactory = { { GET_VTABLE2(ALflangerStateFactory, ALeffectStateFactory) } };

    return STATIC_CAST(ALeffectStateFactory, &FlangerFactory);
}


void ALflanger_setParami(ALeffect *effect, ALCcontext *context, ALenum param, ALint val)
{
    ALeffectProps *props = &effect->Props;
    switch(param)
    {
        case AL_FLANGER_WAVEFORM:
            if(!(val >= AL_FLANGER_MIN_WAVEFORM && val <= AL_FLANGER_MAX_WAVEFORM))
                SET_ERROR_AND_RETURN(context, AL_INVALID_VALUE);
            props->Flanger.Waveform = val;
            break;

        case AL_FLANGER_PHASE:
            if(!(val >= AL_FLANGER_MIN_PHASE && val <= AL_FLANGER_MAX_PHASE))
                SET_ERROR_AND_RETURN(context, AL_INVALID_VALUE);
            props->Flanger.Phase = val;
            break;

        default:
            SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM);
    }
}
void ALflanger_setParamiv(ALeffect *effect, ALCcontext *context, ALenum param, const ALint *vals)
{
    ALflanger_setParami(effect, context, param, vals[0]);
}
void ALflanger_setParamf(ALeffect *effect, ALCcontext *context, ALenum param, ALfloat val)
{
    ALeffectProps *props = &effect->Props;
    switch(param)
    {
        case AL_FLANGER_RATE:
            if(!(val >= AL_FLANGER_MIN_RATE && val <= AL_FLANGER_MAX_RATE))
                SET_ERROR_AND_RETURN(context, AL_INVALID_VALUE);
            props->Flanger.Rate = val;
            break;

        case AL_FLANGER_DEPTH:
            if(!(val >= AL_FLANGER_MIN_DEPTH && val <= AL_FLANGER_MAX_DEPTH))
                SET_ERROR_AND_RETURN(context, AL_INVALID_VALUE);
            props->Flanger.Depth = val;
            break;

        case AL_FLANGER_FEEDBACK:
            if(!(val >= AL_FLANGER_MIN_FEEDBACK && val <= AL_FLANGER_MAX_FEEDBACK))
                SET_ERROR_AND_RETURN(context, AL_INVALID_VALUE);
            props->Flanger.Feedback = val;
            break;

        case AL_FLANGER_DELAY:
            if(!(val >= AL_FLANGER_MIN_DELAY && val <= AL_FLANGER_MAX_DELAY))
                SET_ERROR_AND_RETURN(context, AL_INVALID_VALUE);
            props->Flanger.Delay = val;
            break;

        default:
            SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM);
    }
}
void ALflanger_setParamfv(ALeffect *effect, ALCcontext *context, ALenum param, const ALfloat *vals)
{
    ALflanger_setParamf(effect, context, param, vals[0]);
}

void ALflanger_getParami(const ALeffect *effect, ALCcontext *context, ALenum param, ALint *val)
{
    const ALeffectProps *props = &effect->Props;
    switch(param)
    {
        case AL_FLANGER_WAVEFORM:
            *val = props->Flanger.Waveform;
            break;

        case AL_FLANGER_PHASE:
            *val = props->Flanger.Phase;
            break;

        default:
            SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM);
    }
}
void ALflanger_getParamiv(const ALeffect *effect, ALCcontext *context, ALenum param, ALint *vals)
{
    ALflanger_getParami(effect, context, param, vals);
}
void ALflanger_getParamf(const ALeffect *effect, ALCcontext *context, ALenum param, ALfloat *val)
{
    const ALeffectProps *props = &effect->Props;
    switch(param)
    {
        case AL_FLANGER_RATE:
            *val = props->Flanger.Rate;
            break;

        case AL_FLANGER_DEPTH:
            *val = props->Flanger.Depth;
            break;

        case AL_FLANGER_FEEDBACK:
            *val = props->Flanger.Feedback;
            break;

        case AL_FLANGER_DELAY:
            *val = props->Flanger.Delay;
            break;

        default:
            SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM);
    }
}
void ALflanger_getParamfv(const ALeffect *effect, ALCcontext *context, ALenum param, ALfloat *vals)
{
    ALflanger_getParamf(effect, context, param, vals);
}

DEFINE_ALEFFECT_VTABLE(ALflanger);
