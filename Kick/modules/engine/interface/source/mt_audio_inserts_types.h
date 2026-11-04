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

namespace mt
{
    ///
    /// INSERTS
    ///

    struct MTInsertsParamInfo
    {
        juce::String name{ "" };
        juce::String unit{ "" };
        juce::String displayValue{ "" };
        float        value{ 0.f };
        bool         isStepped{ false };
        float        step{ 0.001f };
    };

    using InsertsParams = std::vector<MTInsertsParamInfo>;
    struct MTInsertsInfo
    {
        juce::String  name{ "" };
        bool          bypass{ false };
        InsertsParams params;
        bool          isClap{ false };
        juce::String  pluginId{ "" };
        juce::String  version{ "" };
        juce::String  path{ "" };
        uint32_t      latency{ 0 };
        uint32_t      tail{ 0 };
        bool          missing{ false };
        juce::String  statusMessage{ "" };
        bool          supportsGui{ false };
    };

    class ClapEffect;

    // Touch FX (the XY screen) of a chain: plain values, converted to AudIO::TouchFx in the engine.
    struct MTTouchFxSettings
    {
        int    mode{ 0 };      // AudIO::TouchFx::Mode
        int    filter{ 0 };    // AudIO::TouchFx::FilterType
        float  x{ 0.f };
        float  y{ 0.f };
        int    division{ 0 };  // 0..7 = 2 bar .. 1/64
        bool   gate{ false };
        float  attackMs{ 10.f };
        float  releaseMs{ 100.f };
        float  wet{ 1.f };
        double bpm{ 120.0 };
    };

    ///
    struct MTIInsertsImpl;
    class MTIInsertsInterface
    {
    public:
        MTIInsertsInterface();
        ~MTIInsertsInterface();
        void setup( juce::ValueTree& VT, juce::UndoManager* UM, AudIO::DawInserts* Impl );

        void                        getInserts( std::vector<juce::String>& InsertNames );
        std::vector<MTInsertsInfo>* getInserts();
        MTInsertsInfo               getInsertsInfo( unsigned int Position );
        std::vector<bool>           getInsertBypassState();

        bool isAllInsertsBypassed();
        bool isInsertBypassed( unsigned int Position );
        void setAllInsertsBypass( bool Bypass );
        int  setInsertBypass( unsigned int Position, bool Bypass );

        int  addInsert( const juce::String& Effect, unsigned int Position );
        int  removeInsert( const juce::String& Effect );
        int  removeInsert( unsigned int Position );
        void removeAllInserts();

        int  swapInserts( unsigned int fromPosition, unsigned int toPosition );
        int  moveInsert( unsigned int fromPosition, unsigned int toPosition );
        ClapEffect* getClapEffect( unsigned int Position );

        int          getNumInserts();
        // False for programs that have no insert chain (MIDI / CV): ADD would do nothing, the screen says so instead.
        bool         isAvailable() const noexcept;
        void         setTouchFx( const MTTouchFxSettings& Settings ) noexcept; // lock-free, any thread
        void         closeTouchFx() noexcept;
        // Automation playback (any thread, no allocation, the value tree is not touched): the Touch FX gesture, and one effect parameter.
        void         setTouchFxGesture( float X, float Y, bool Gate ) noexcept;
        void         setParameterAutomated( unsigned int InsertPosition, int Parameter, float Value ) noexcept;
        // Message thread: puts the slot's stored (value tree) parameters back after automation played over them.
        void         restoreInsertFromState( unsigned int InsertPosition );
        int          getNumParameters( unsigned int Position );
        juce::String getInsertName( unsigned int InsertPosition );
        juce::String getParameterName( unsigned int InsertPosition, int Parameter );
        juce::String getParameterValueString( unsigned int InsertPosition, int Parameter );

        float getParameterValue( unsigned int InsertPosition, int Parameter );
        void  setParameterValue( unsigned int InsertPosition, int Parameter, float Value );

    private:
        std::unique_ptr<MTIInsertsImpl> pImpl;
        juce::ValueTree                 state;
        juce::UndoManager*              um{ nullptr };
        std::vector<juce::String>       inserts;
    };

} // namespace mt
