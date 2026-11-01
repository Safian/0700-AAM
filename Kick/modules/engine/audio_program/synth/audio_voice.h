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

#include "iSynthVoice.h"

namespace mt
{
    class AVoice : public ISynthVoice
    {
    public:
        using Ptr = juce::ReferenceCountedObjectPtr<AVoice>;

        explicit AVoice( const MTIAtomicProgramMaster& Master, ASoundSlice* Slice = nullptr )
            : ISynthVoice( Master, Slice ) {}

        void setPitchRatio( double ratio ) { pitchRatio = ratio; }

        void process( juce::AudioBuffer<float>& Buffer ) override
        {
            if( isRunning() )
            {
                const auto numSamples = Buffer.getNumSamples();
                ASliceRenderer::getNextBlock( tempBuffer, master.pitchRatio.load() * master.bendRatio.load() * pitchRatio );
                gain.applyGain( tempBuffer, numSamples );
                Buffer.addFrom( 0, 0, tempBuffer, 0, 0, numSamples );
                Buffer.addFrom( 1, 0, tempBuffer, 1, 0, numSamples );
                if( hasFinished() )
                    makeFree();
            }
        }

    private:
        double pitchRatio{ 1.0 };
    };

} // namespace mt
