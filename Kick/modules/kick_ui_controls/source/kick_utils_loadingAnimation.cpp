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
                    gifImages.push_back( juce::ImageCache::getFromFile( ui::Theme::getAssetsDirectory().getChildFile( "animation/Load Animation B - " + text + ".png" ) ) );
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
                        }
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
