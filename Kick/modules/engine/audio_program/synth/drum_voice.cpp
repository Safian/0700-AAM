/*
 * Copyright (c) 2024 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "drum_voice.h"

namespace mt
{
    static inline bool hasSustain( ADSRType Type ) { return Type == Type_ADSR || Type == Type_AHDS || Type == Type_AHDSR; }

    void DrumVoice::noteOn( int Number, int Velocity )
    {
        // sample rates
        lfo.setSampleRate( sampleRate );
        filter.setSampleRate( sampleRate );

        ampEnv.setSampleRate( sampleRate );
        filtEnv.setSampleRate( sampleRate );
        pitchEnv.setSampleRate( sampleRate );

        // Setup LFO.
        lfo.setType( toEnum<LFOType>( pLfo->type() ) );
        lastLfoFreq = pLfo->freq();
        lfo.setFrequency( lastLfoFreq, true );
        lfo.setEnabled( pLfo->isEnabled() );

        // Setup Envelopes.
        // The envelope times are fractions (value / 127) of the sample's length, in SAMPLES (ADSR::setParameters divides by the rate).
        // This used to be SCALER * length in seconds, scaled by SCALER again below: a few thousandths of a sample.
        sampleLength = getLength() * sampleRate;
        updateAmplitudeEnvelope( sampleLength );
        updateFilterEnvelope( sampleLength );
        updatePitchEnvelope( sampleLength );

        ampEnv.setEnabled( pEnv->amp.isEnabled() );
        filtEnv.setEnabled( pEnv->filter.isEnabled() );
        pitchEnv.setEnabled( pEnv->pitch.isEnabled() );

        // Setup filter.
        updateFilter();

        ampEnv.gate( 1 );
        filtEnv.gate( 1 );
        pitchEnv.gate( 1 );

        // Tail off and reset.
        tailOff = 0.0;
        lfo.reset();
        filter.reset();

        ISynthVoice::noteOn( Number, Velocity );
    }

    void DrumVoice::noteOff()
    {
        ISynthVoice::noteOff();
        ampEnv.gate( 0 );
        filtEnv.gate( 0 );
        pitchEnv.gate( 0 );
    }

    void DrumVoice::updateLFO()
    {
        if( !pLfo->isEnabled() )
        {
            lfoToAmp    = 0.0;
            lfoToPan    = 0.0;
            lfoToPitch  = 0.0;
            lfoToFilter = 0.0;
            return;
        }

        lfo.setType( toEnum<LFOType>( (int)pLfo->type() ) );
        // setFrequency() restarts the LFO phase: only when the rate really changed, not at every block (the LFO stood at the start of
        // its cycle: a constant instead of a wave).
        if( !juce::exactlyEqual( lastLfoFreq, (float)pLfo->freq() ) )
        {
            lastLfoFreq = pLfo->freq();
            lfo.setFrequency( lastLfoFreq );
        }

        lfoToAmp    = pLfo->ampMod() * MIDI::SCALER;
        lfoToPan    = pLfo->panMod() * MIDI::SCALER;
        lfoToPitch  = pLfo->pitchMod() * MIDI::SCALER;
        lfoToFilter = pLfo->filterMod() * MIDI::SCALER;
    }

    void DrumVoice::updateFilter( double modulation )
    {
        filter.setFilterType( toEnum<FilterType>( pEnv->filter.type() ) );
        filter.setResonance( pEnv->filter.resonance() );

        auto cutoff = Filter::freqRange.convertTo0to1( pEnv->filter.cutoff() ) + modulation;
        filter.setCutoff( cutoff );
    }

    void DrumVoice::updateAmplitudeEnvelope( double Length )
    {
        auto type  = toEnum<ADSRType>( pEnv->amp.adsrType() );
        auto scale = MIDI::SCALER * Length;

        auto adF = scale * getAttackDecayScaler( type );
        auto hF  = scale * getHoldScaler( type );
        auto rF  = scale * getReleaseScaler( type );

        // The amplitude envelope's level / pan / velocity modulators are not in the clip model and have no control: they stay
        // neutral. (Their atomic defaults 0 / 1 / 1 were mapped to -1 / -0.98 / -0.98 here: the Attack knob turned into Hold and the
        // pad was panned almost hard left as soon as the voice was bound to a clip.)
        const float ampToAttack = 1.0f; // 1 = Attack as set (the shift of Attack into Hold is off)
        ampToVel                = 0.0f;
        ampToPan                = 0.0f;

        auto aa = ( pEnv->amp.a() * adF );
        auto ad = ( pEnv->amp.d() * adF );
        auto ah = ( pEnv->amp.h() * hF );
        auto ar = ( pEnv->amp.r() * rF );
        auto as = hasSustain( type ) ? (double)pEnv->amp.s() : 0.0; // AD / AHD decay to silence

        auto mod = aa - ( ampToAttack * aa );
        aa -= mod;
        ah += mod;

        ampEnv.setParameters( aa, ah, ad, ar, as ); // In terms of samples...
    }

    void DrumVoice::updateFilterEnvelope( double Length )
    {
        auto type  = toEnum<ADSRType>( pEnv->filter.adsrType() );
        auto scale = MIDI::SCALER * Length;

        auto adF = scale * getAttackDecayScaler( type );
        auto hF  = scale * getHoldScaler( type );
        auto rF  = scale * getReleaseScaler( type );

        auto aa = ( pEnv->filter.a() * adF );
        auto ad = ( pEnv->filter.d() * adF );
        auto ah = ( pEnv->filter.h() * hF );
        auto ar = ( pEnv->filter.r() * rF );
        auto as = hasSustain( type ) ? (double)pEnv->filter.s() : 0.0; // AD / AHD decay to silence
        filtEnv.setParameters( aa, ah, ad, ar, as ); // In terms of samples...
    }

    void DrumVoice::updatePitchEnvelope( double Length )
    {
        auto type  = toEnum<ADSRType>( pEnv->pitch.adsrType() );
        auto scale = MIDI::SCALER * Length;

        auto adF = scale * getAttackDecayScaler( type );
        auto hF  = scale * getHoldScaler( type );
        auto rF  = scale * getReleaseScaler( type );

        auto aa = ( pEnv->pitch.a() * adF );
        auto ad = ( pEnv->pitch.d() * adF );
        auto ah = ( pEnv->pitch.h() * hF );
        auto ar = ( pEnv->pitch.r() * rF );
        auto as = hasSustain( type ) ? (double)pEnv->pitch.s() : 0.0; // AD / AHD decay to silence
        pitchEnv.setParameters( aa, ah, ad, ar, as ); // In terms of samples...
    }

    void DrumVoice::process( juce::AudioBuffer<float>& Buffer )
    {
        if( isRunning() )
        {
            update(); // Update parameters...

            const auto masterPitchRatio = master.pitchRatio.load() * master.bendRatio.load();
            const auto masterGainL      = master.gainL.load();
            const auto masterGainR      = master.gainR.load();
            const auto numSamples       = Buffer.getNumSamples();

            auto out0 = tempBuffer.getWritePointer( 0 );
            auto out1 = tempBuffer.getWritePointer( 1 );

            // For each sample...
            for( auto i = 0; i < numSamples; ++i )
            {
                auto rGain     = gain.getNextValue();
                auto lfoOut    = lfo();
                auto ampAdsr   = ampEnv();
                auto filtAdsr  = filtEnv();
                auto pitchAdsr = pitchEnv();

                auto aMod   = rGain * ampAdsr * ( ( 1. - lfoToAmp ) + ( lfoToAmp * lfoOut ) ); // Amplitude modulation factor.
                auto panMod = ( lfoToPan * lfoOut ) + ( gain.getTargetValue() * ampToPan ); // Pan modulation factor.

                double pitchEnvFactor = 1.0;
                if( pEnv->pitch.isEnabled() && pEnv->pitch.depth() != 0 )
                {
                    pitchEnvFactor = std::pow( 2.0, ( ( pitchAdsr - 1.0 ) * pEnv->pitch.depth() ) / 12.0 );
                }
                auto pMod   = pitchEnvFactor * masterPitchRatio * ( 1.0 - ( lfoToPitch * lfoOut ) ); // Pitch modulation factor.
                // Filter cutoff modulation, in the normalised cutoff range: the filter envelope (only when it is on) times its depth
                // (0..127), plus the LFO's filter amount. (The old formula multiplied the envelope with the LFO terms, so the
                // envelope alone never moved the cutoff, and the LFO pitch amount leaked into the filter.)
                const double envToFilter = pEnv->filter.isEnabled() ? filtAdsr * ( pEnv->filter.depth() * MIDI::SCALER ) : 0.0;
                auto         fMod        = envToFilter + ( lfoToFilter * lfoOut );

                // get next sample.
                updateFilter( fMod );
                auto& sample = getNext( pMod );
                filter.process( sample );

                // Copy to Bock.
                panner.setParameters( panMod, aMod );
                *out0++ = panner.getLeft() * sample.getSample( 0, 0 );
                *out1++ = panner.getRight() * sample.getSample( 1, 0 );
            }

            // Apply Master and Pad gain and pan, and add to the Buffer.
            const auto totalGainL = masterGainL * padGainL.load();
            const auto totalGainR = masterGainR * padGainR.load();
            Buffer.addFrom( 0, 0, tempBuffer, 0, 0, numSamples, totalGainL );
            Buffer.addFrom( 1, 0, tempBuffer, 1, 0, numSamples, totalGainR );

            if( hasFinished() )
                makeFree();
        }
    }

} // namespace mt
