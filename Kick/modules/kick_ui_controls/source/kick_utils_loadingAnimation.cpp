/*
 * Copyright (c) 2021 MUSIC Tribe
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

///
/// KickUtils...
///

namespace mt
{
    namespace utils
    {
        LoadAnimation::LoadAnimation()
        {
            ui::themeChanged.connect( [&]() {
                frames->images.clear();
                loadImages();
            } );

            loadImages();
        }

        LoadAnimation ::~LoadAnimation()
        {
            DBG( "LoadAnimation Destructor" );
            stopTimer();
        }

        void LoadAnimation::loadImages()
        {
            auto& gifImages = frames->images;
            if( 0 == gifImages.size() )
            {
                gifImages.reserve( 14 );

                for( auto idx = 1; idx < 14; ++idx )
                {
                    auto text = juce::String( idx ).paddedLeft( '0', 2 );
                    auto relName = "animation/Load Animation B - " + text + ".png";
                    auto f = ui::Theme::getAssetsDirectory().getChildFile( relName );
                    if( !f.exists() )
                    {
                        auto appBundle = juce::File::getSpecialLocation( juce::File::currentApplicationFile );
                        f = appBundle.getChildFile( "Contents/Resources/Proper/" + relName );
                        if( !f.exists() ) f = appBundle.getChildFile( "assets/Proper/" + relName );
                        if( !f.exists() ) f = appBundle.getChildFile( "Proper/" + relName );
                        if( !f.exists() ) f = juce::File::getCurrentWorkingDirectory().getChildFile( "Kick/assets/Proper/" + relName );
                    }

                    if( f.exists() )
                        gifImages.push_back( juce::ImageFileFormat::loadFrom( f ) );
                    else
                        gifImages.push_back( juce::ImageCache::getFromFile( ui::Theme::getAssetsDirectory().getChildFile( relName ) ) );
                }
            }
        }

        void LoadAnimation::startAnimation()
        {
            id          = 0;
            isAnimating = true;

            if( hostComponent != nullptr )
            {
                hostComponent->addChildComponent( this );
                setBounds( hostComponent->getLocalBounds() );
                setInterceptsMouseClicks( true, true );
                setVisible( true );
                toFront( true );
            }
            else
            {
                popup = showPopup( this, { 0, 0, 80, 80 }, []( int R ) {} );
            }

            startTimerHz( 22 );
        }

        void LoadAnimation::stopAnimation()
        {
            isAnimating = false;
            close();
        }

        void LoadAnimation::stopAnimationAndFree()
        {
            stopAnimation();
            if( this )
                delete this;
        }

        void LoadAnimation::paint( juce::Graphics& g )
        {
            if( isAnimating )
            {
                if( hostComponent != nullptr )
                {
                    // Dark translucent shade over DAW screen only
                    g.fillAll( juce::Colour( 0xcc121216 ) );

                    bool drewFrame = false;
                    if( !frames->images.empty() && juce::isPositiveAndBelow( (int)id, (int)frames->images.size() ) )
                    {
                        auto& img = frames->images[id];
                        if( img.isValid() )
                        {
                            auto b = getLocalBounds();
                            const int size = 80;
                            int x = ( b.getWidth() - size ) / 2;
                            int y = ( b.getHeight() - size ) / 2;
                            g.drawImage( img, (float)x, (float)y, (float)size, (float)size, 0.0f, 0.0f, (float)img.getWidth(), (float)img.getHeight() );
                            drewFrame = true;
                        }
                    }

                    if( !drewFrame )
                    {
                        // Clean spinning neon cyan ring as fallback in exact center
                        auto b = getLocalBounds();
                        const float size = 64.0f;
                        float cx = (float)b.getWidth() * 0.5f;
                        float cy = (float)b.getHeight() * 0.5f;
                        juce::Path arc;
                        float startAngle = (float)id * juce::MathConstants<float>::twoPi / 14.0f;
                        arc.addCentredArc( cx, cy, size * 0.5f, size * 0.5f, 0.0f, startAngle, startAngle + juce::MathConstants<float>::pi * 1.5f, true );
                        g.setColour( juce::Colour( 0xff00e5ff ) );
                        g.strokePath( arc, juce::PathStrokeType( 3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded ) );
                    }
                }
                else
                {
                    if( !frames->images.empty() && juce::isPositiveAndBelow( (int)id, (int)frames->images.size() ) )
                        g.drawImage( frames->images[id], getLocalBounds().toFloat() );
                }
            }
        }

        void LoadAnimation::timerCallback()
        {
            ++id;
            if( id >= frames->images.size() )
                id = 0;

            repaint();
        }

        void LoadAnimation::close()
        {
            stopTimer();
            isAnimating = false;
            setVisible( false );
            if( auto* p = getParentComponent() )
                p->removeChildComponent( this );

            if( popup )
            {
                popup->close();
                popup = nullptr;
            }
        }

    } // namespace utils

} // namespace mt
