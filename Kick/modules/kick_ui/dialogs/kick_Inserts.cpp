
#include <engine/audio_engine/engine/mt_clap_effect.h>
#include <engine/audio_engine/engine/mt_airwindopedia.h>
#include <hardware/source/kick_Resorces.h>

namespace kick_ui
{
#define insertGridHorSpacing 3
#define insertGridVerSpacing 30
#define insertTotalRows      2

    struct ChainClipboard
    {
        bool hasData{ false };
        struct Slot
        {
            juce::String name;
            bool bypass{ false };
            std::vector<float> values;
        };
        std::vector<Slot> slots;
    };
    static ChainClipboard g_chainClipboard;

    struct SlotABState
    {
        std::vector<float> stateA;
        std::vector<float> stateB;
        bool isB{ false };
    };
    static std::unordered_map<int, SlotABState> g_slotAB;
    static std::unordered_map<juce::String, std::vector<float>> g_presets;
    static std::set<juce::String> g_insertFavourites;

    class ClapGuiComponent : public juce::Component
    {
    public:
        explicit ClapGuiComponent( mt::ClapEffect* effectIn, const juce::String& /*title*/ = "CLAP Plugin" )
            : effect( effectIn )
        {
            setOpaque( true );
            if( effect && effect->isLoaded() && effect->getHost().supportsGui() )
            {
                auto& host = effect->getHost();
                const char* targetApi = nullptr;
#if JUCE_MAC
                if( host.isGuiApiSupported( CLAP_WINDOW_API_COCOA, false ) )
                    targetApi = CLAP_WINDOW_API_COCOA;
#elif JUCE_LINUX
                if( host.isGuiApiSupported( CLAP_WINDOW_API_X11, false ) )
                    targetApi = CLAP_WINDOW_API_X11;
#elif JUCE_WINDOWS
                if( host.isGuiApiSupported( CLAP_WINDOW_API_WIN32, false ) )
                    targetApi = CLAP_WINDOW_API_WIN32;
#endif
                if( targetApi != nullptr )
                {
                    if( host.createGui( targetApi, false ) )
                    {
                        guiCreated = true;
                        uint32_t w = 640, h = 480;
                        if( host.getGuiSize( &w, &h ) && w > 0 && h > 0 )
                            setSize( static_cast<int>( w ), static_cast<int>( h ) );
                        else
                            setSize( 640, 480 );
                    }
                }
            }
        }

        ~ClapGuiComponent() override
        {
            if( effect && effect->isLoaded() && guiCreated )
            {
                auto& host = effect->getHost();
                host.hideGui();
                host.destroyGui();
            }
        }

        void parentHierarchyChanged() override
        {
            if( guiCreated && !attached && isShowing() )
                attachGui();
        }

        void visibilityChanged() override
        {
            if( guiCreated && isShowing() && !attached )
                attachGui();
            else if( guiCreated && !isShowing() && attached && effect && effect->isLoaded() )
                effect->getHost().hideGui();
        }

        void resized() override
        {
            if( guiCreated && attached && effect && effect->isLoaded() )
            {
                effect->getHost().setGuiSize( static_cast<uint32_t>( getWidth() ), static_cast<uint32_t>( getHeight() ) );
            }
        }

        void paint( juce::Graphics& g ) override
        {
            g.fillAll( juce::Colour( 0xff15191e ) );
            if( !guiCreated )
            {
                g.setColour( juce::Colours::white );
                g.setFont( 16.0f );
                g.drawText( "Native GUI not supported on this platform.\nUsing Generic Parameters.",
                            getLocalBounds(), juce::Justification::centred, true );
            }
        }

        bool isGuiSuccessfullyCreated() const noexcept { return guiCreated; }

    private:
        void attachGui()
        {
            if( !effect || !effect->isLoaded() ) return;
            auto* peer = getPeer();
            if( !peer ) return;
            void* nativeHandle = peer->getNativeHandle();
            if( !nativeHandle ) return;

            auto& host = effect->getHost();
            clap_window_t win{};
#if JUCE_MAC
            win.api = CLAP_WINDOW_API_COCOA;
            win.cocoa = static_cast<clap_nsview>( nativeHandle );
#elif JUCE_LINUX
            win.api = CLAP_WINDOW_API_X11;
            win.x11 = reinterpret_cast<clap_xwnd>( nativeHandle );
#elif JUCE_WINDOWS
            win.api = CLAP_WINDOW_API_WIN32;
            win.win32 = static_cast<clap_hwnd>( nativeHandle );
#endif
            if( win.api != nullptr )
            {
                host.setGuiParent( &win );
                host.setGuiSize( static_cast<uint32_t>( getWidth() ), static_cast<uint32_t>( getHeight() ) );
                host.showGui();
                attached = true;
            }
        }

        mt::ClapEffect* effect{ nullptr };
        bool guiCreated{ false };
        bool attached{ false };
    };

    ///
    /// Inserts : Main screen interface class
    ///
    Inserts::Inserts()
        : kick_ui::IScreen( kick_ui::Screen::Inserts )
    {
        mt::utils::addAndMakeVisible( this, header ); // Visible
        mt::utils::addChildComponents( this, list, selector, options ); // Not visible by default.

        header.onBackButtonPressed = [this]( bool ) { closeScreen(); };
        header.showOptions( true, false ); // no insert-parameter automation exists: the header icon did nothing

        ui::themeChanged.connect( [&]() { header.setIcon( "Icon_Mixer" ); } );

        list.onInsertChanged = [&]( int i, const juce::String& name, bool value ) { onInsertChanged( i, name, value ); };
        list.onInsertDelete  = [&]( int i, const juce::String& name, bool value ) { onInsertDelete( i, name, value ); };
    };

    // Q-Link 1..4 = the first four parameters of the shown insert, Data Dial = next / previous slot.
    void Inserts::knobCommand( int knob, int value )
    {
        using Knob = contracts::DeviceControlContracts::KnobSequence;
        if( knob == Knob::EN_KNOB_CENTER )
            list.stepSelection( value > 0 ? 1 : -1 );
        else if( knob >= Knob::EN_KNOB_1 && knob <= Knob::EN_KNOB_4 )
            options.linkRotated( knob - Knob::EN_KNOB_1, value );
    }

    void Inserts::handleCommand( CPktMgr::HWMsg_t& hwMsg )
    {
        if( contracts::DclContractsHelper::isKnobMessage( hwMsg ) )
            knobCommand( contracts::DclContractsHelper::getKnobCode( hwMsg ), contracts::DclContractsHelper::getKnobValue( hwMsg ) );
    }

    void Inserts::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
        list.setBounds( b.removeFromLeft( b.proportionOfWidth( .3f ) ).reduced( 5, 10 ) );
        //auto bb = b.reduced( 5, 10 );
        options.setBounds( b.reduced( 5, 10 ) );
        selector.setBounds( options.getBounds() );
    }

    void Inserts::visibilityChanged()
    {
        auto visible = isVisible();

        if( !visible )
            kick_ui::signals::onScreenData( kick_ui::Screen::Inserts, nullptr );

        for( auto& it : getChildren() )
            it->setVisible( visible );
    }

    ///
    /// InsertsList
    ///
    InsertsList::InsertsList( InsertsSelector& Selector, InsertsOptions& InsertsOptions )
        : selector( Selector ), options( InsertsOptions )
    {
        mt::utils::addAndMakeVisible( this, allInserts, allDelete, allEnable, copyChain, pasteChain );

        mt::utils::setValueFontHeight( 16, allInserts );
        mt::utils::setValueColour( ui::Theme::textColour( 2 ), allInserts );
        mt::utils::setValueJustification( juce::Justification::centredBottom, allInserts );

        allEnable.setToggleButtonText( "ENABLED", "BYPASSED" );
        mt::utils::setValueFontHeight( 13, copyChain, pasteChain, allDelete, allEnable );

        copyChain.onClick = [&]() {
            if( !pEffects )
                return;
            g_chainClipboard.slots.clear();
            g_chainClipboard.slots.resize( numRows );
            bool any = false;
            for( int i = 0; i < numRows; ++i )
            {
                auto inf = pEffects->getInsertsInfo( i );
                if( inf.name.isNotEmpty() && !inf.name.equalsIgnoreCase( "None" ) )
                {
                    g_chainClipboard.slots[i].name   = inf.name;
                    g_chainClipboard.slots[i].bypass = inf.bypass;
                    int numP = pEffects->getNumParameters( i );
                    for( int p = 0; p < numP; ++p )
                        g_chainClipboard.slots[i].values.push_back( pEffects->getParameterValue( i, p ) );
                    any = true;
                }
            }
            g_chainClipboard.hasData = any;
            mt::utils::showNotification( any ? "Inserts Chain Copied" : "Chain is empty" );
        };

        pasteChain.onClick = [&]() {
            if( !pEffects || !g_chainClipboard.hasData )
            {
                mt::utils::showNotification( "Clipboard is empty" );
                return;
            }
            pEffects->removeAllInserts();
            for( int i = 0; i < numRows; ++i )
            {
                if( g_chainClipboard.slots[i].name.isNotEmpty() )
                {
                    pEffects->addInsert( g_chainClipboard.slots[i].name, i );
                    pEffects->setInsertBypass( i, g_chainClipboard.slots[i].bypass );
                    for( size_t p = 0; p < g_chainClipboard.slots[i].values.size(); ++p )
                        pEffects->setParameterValue( i, (int)p, g_chainClipboard.slots[i].values[p] );
                }
            }
            refresh();
            inserts[selected != -1 ? selected : 0].triggerClick();
            mt::utils::showNotification( "Inserts Chain Pasted" );
        };
        //allDelete.setEnabled( false );
        //allEnable.setEnabled( false );

        // Connect signal.
        kick_ui::signals::onScreenData.connect( [&]( Screen Scr, void* PData ) {
            if( Screen::Inserts == Scr )
            {
                pEffects = (mt::MTIInsertsInterface*)PData;
            }
        } );

        for( auto i = 0; i < numRows; ++i )
        {
            add[i].setEnabled( false );
            enable[i].setEnabled( false );
            moveUp[i].setEnabled( false );
            moveDown[i].setEnabled( false );
            add[i].setToggleButtonText( "DELETE", "ADD" );
            enable[i].setToggleButtonText( "ENABLED", "BYPASSED" );
            moveUp[i].setButtonText( "▲" );
            moveDown[i].setButtonText( "▼" );
            inserts[i].setValueFontHeight( 18 );

            mt::utils::addAndMakeVisible( this, inserts[i], enable[i], add[i], moveUp[i], moveDown[i] );

            moveUp[i].onClick = [&, i] {
                if( i > 0 && pEffects )
                {
                    pEffects->swapInserts( i, i - 1 );
                    refresh();
                    inserts[i - 1].triggerClick();
                }
            };

            moveDown[i].onClick = [&, i] {
                if( i < numRows - 1 && pEffects )
                {
                    pEffects->swapInserts( i, i + 1 );
                    refresh();
                    inserts[i + 1].triggerClick();
                }
            };

            inserts[i].onClick = [&, i] {
                auto insName = inserts[i].getButtonText();
                if( insName.isEmpty() || insName.equalsIgnoreCase( "None" ) )
                {
                    options.setVisible( false );
                    selector.setVisible( true );
                    add[i].setEnabled( pEffects && pEffects->isAvailable() ); // a MIDI / CV program has no insert chain
                }
                else if( pEffects )
                {
                    options.setVisible( true );
                    options.setupControls( pEffects, i );
                }
                selected = i;
                repaint();
            };

            add[i].onClick = [&, i] {
                const bool adding = add[i].getToggleState();
                if( adding )
                {
                    // Only a real choice is added, and only where the program has an insert chain; a failed add says so
                    // (it used to flip the button to DELETE and flip it back on the next refresh, without a word).
                    const auto s = selector.getSelected();
                    if( !pEffects || !pEffects->isAvailable() )
                    {
                        add[i].setToggleState( false, juce::dontSendNotification );
                        ui::KickAlertWindow::showCancelAlert( ui::KickAlertWindow::KickAlertWindowType::Warning, "This program type has no insert effects.", "Inserts", []( int ) {} );
                        return;
                    }
                    if( s.isEmpty() || pEffects->addInsert( s, i ) < 0 )
                    {
                        add[i].setToggleState( false, juce::dontSendNotification );
                        refresh();
                        if( s.isNotEmpty() )
                            ui::KickAlertWindow::showCancelAlert( ui::KickAlertWindow::KickAlertWindowType::Warning, "The effect \"" + s + "\" could not be added.", "Inserts", []( int ) {} );
                        return;
                    }
                    selector.setVisible( false );
                    options.setVisible( true );
                }
                else
                {
                    selector.setVisible( true );
                    options.setVisible( false );
                    // Remove insert...
                    if( pEffects )
                        pEffects->removeInsert( i );
                    inserts[i].triggerClick();
                }
                if( pEffects )
                {
                    refresh();
                    if( adding ) options.setupControls( pEffects, i );
                }
            };

            enable[i].onClick = [&, i] {
                if( pEffects )
                {
                    pEffects->setInsertBypass( i, !enable[i].getToggleState() );
                    refresh();
                    options.refresh();
                }
            };
        }

        options.onEnableClicked = [&]( int Position, bool State ) {
            enable[Position].setToggleState( State, juce::sendNotification );
        };

        options.onRotaryChanged = [&]( int Position, int Parameter, float Value ) {
            if( pEffects )
            {
                pEffects->setParameterValue( Position, Parameter, Value );
                refresh();
                options.refresh(); // the shown value text (e.g. "-6.0 dB") follows the parameter
            }
        };

        allEnable.onClick = [&]() {
            auto bEnable = allEnable.getToggleState();
            for( auto i = 0; i < numRows; ++i )
            {
                if( enable[i].isEnabled() )
                    enable[i].setToggleState( bEnable, juce::sendNotification );
            }
        };

        // Delete all: asks first (it deleted every insert at once, without a question).
        allDelete.onClick = [&]() {
            allDelete.setToggleState( false, juce::dontSendNotification );
            if( !pEffects )
                return;
            auto* p = new ui::FilePopup( "Delete all inserts of this chain?" );
            mt::utils::showDialog(
                "WARNING!", p, [this, p]( int R ) {
                    if( R == UI::APPLY && pEffects )
                    {
                        for( auto i = 0; i < numRows; ++i )
                            if( add[i].isEnabled() )
                                pEffects->removeInsert( i );
                        refresh();
                        inserts[selected != -1 ? selected : 0].triggerClick();
                    }
                    delete p;
                },
                ui::layout::warningPopup );
        };
    };

    void InsertsList::stepSelection( int Delta )
    {
        const int next = juce::jlimit( 0, numRows - 1, ( selected < 0 ? 0 : selected ) + Delta );
        inserts[next].triggerClick();
    }

    int InsertsList::getDefaultIndex()
    {
        if( pEffects )
        {
            for( auto i = 0; i < numRows; ++i )
            {
                auto insertInfo = pEffects ? pEffects->getInsertsInfo( i ) : mt::MTInsertsInfo{};
                if( insertInfo.name.isNotEmpty() )
                    return i;
            }
        }
        return 0;
    }

    void InsertsList::refresh()
    {
        auto infoSz = pEffects ? pEffects->getInserts()->size() : 0;

        auto isMasterEnable = false;
        auto anyEnabled     = false;
        const bool available = pEffects && pEffects->isAvailable();
        allInserts.setText( pEffects && !available ? "NO INSERTS FOR THIS PROGRAM TYPE" : "ALL INSERTS", juce::dontSendNotification );
        for( auto i = 0; i < numRows; ++i )
        {
            auto insertInfo = pEffects ? pEffects->getInsertsInfo( i ) : mt::MTInsertsInfo{};

            auto name      = insertInfo.name;
            auto isAdded   = name.isNotEmpty();
            auto isEnabled = !insertInfo.bypass;

            if( insertInfo.missing )
                inserts[i].setButtonText( "[MISSING] " + name );
            else
                inserts[i].setButtonText( name );

            add[i].setToggleState( isAdded, juce::dontSendNotification );
            enable[i].setToggleState( isEnabled, juce::dontSendNotification );

            enable[i].setEnabled( isAdded );
            add[i].setEnabled( isAdded );
            moveUp[i].setEnabled( isAdded && i > 0 );
            moveDown[i].setEnabled( isAdded && i < numRows - 1 );

            if( isAdded )
                isMasterEnable = true;
            if( isAdded && isEnabled )
                anyEnabled = true;
        }
        allEnable.setToggleState( anyEnabled, juce::dontSendNotification ); // shows the real state (it used to keep its own)
        allEnable.setEnabled( isMasterEnable );
        allDelete.setEnabled( isMasterEnable );
        if( selected == -1 )
        {
            selected = isMasterEnable ? 0 : -1;
            repaint();
        }
    }

    void InsertsList::visibilityChanged()
    {
        auto visible = isVisible();
        if( visible )
        {
            refresh();
            auto defaultIndex = getDefaultIndex();
            options.setupControls( pEffects, defaultIndex ); //default
            inserts[defaultIndex].triggerClick();
        }
    }

    void InsertsList::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, getLocalBounds(), ui::Theme::backgroundColour( 1 ) );
        if( selected != -1 )
            mt::utils::renderRoundedRect( g, { inserts[0].getX() - 5, inserts[selected].getY() - 5, getWidth() - 20, inserts[selected].getHeight() + 10 }, ui::Theme::outlineColour( 1 ) );
    }

    void InsertsList::resized()
    {
        auto b = getLocalBounds().reduced( 10 );
        b.removeFromTop( 10 );
        auto topRow = b.removeFromTop( 16 );

        const auto rowHeight   = b.proportionOfHeight( ( 1.f / ( numRows + 1 ) ) );
        const auto buttonWidth = b.proportionOfWidth( 0.25f );

        auto allRow = b.removeFromTop( rowHeight );
        // Positioning of all insert controls
        allInserts.setBounds( topRow.removeFromRight( 2 * buttonWidth ) );
        pasteChain.setBounds( allRow.removeFromRight( 54 ).reduced( 2, 8 ) );
        copyChain.setBounds( allRow.removeFromRight( 54 ).reduced( 2, 8 ) );
        allDelete.setBounds( allRow.removeFromRight( 64 ).reduced( 2, 8 ) );
        allEnable.setBounds( allRow.removeFromRight( 80 ).reduced( 2, 8 ) );
        // Position insert rows.
        for( auto i = 0; i < numRows; ++i )
        {
            auto rowB = b.removeFromTop( rowHeight );
            add[i].setBounds( rowB.removeFromRight( 48 ).reduced( 2, 8 ) );
            enable[i].setBounds( rowB.removeFromRight( 58 ).reduced( 2, 8 ) );
            moveDown[i].setBounds( rowB.removeFromRight( 22 ).reduced( 1, 8 ) );
            moveUp[i].setBounds( rowB.removeFromRight( 22 ).reduced( 1, 8 ) );
            inserts[i].setBounds( rowB.reduced( 2 ) );
        }
    }

    ///
    /// InsertsSelector
    ///
    InsertsSelector::InsertsSelector()
    {
        mt::utils::addAndMakeVisible( this, searchLabel, searchBox, favToggle, insertCategories, insertList, docTitle, docEditor );

        mt::utils::setValueFontHeight( 14, searchLabel );
        mt::utils::setValueColour( ui::Theme::textColour( 2 ), searchLabel );

        mt::utils::setValueFontHeight( 13, favToggle );
        favToggle.onClick = [this]() {
            auto sel = insertList.getSelected();
            if( sel.isNotEmpty() )
            {
                if( g_insertFavourites.find( sel ) != g_insertFavourites.end() )
                {
                    g_insertFavourites.erase( sel );
                    favToggle.setToggleState( false, juce::dontSendNotification );
                }
                else
                {
                    g_insertFavourites.insert( sel );
                    favToggle.setToggleState( true, juce::dontSendNotification );
                }
                if( insertCategories.getSelected() == "Favourites" )
                {
                    allCategoryItems.clear();
                    for( const auto& f : g_insertFavourites )
                        allCategoryItems.push_back( f );
                    updateFilteredList();
                }
            }
        };

        mt::utils::setValueFontHeight( 14, docTitle );
        mt::utils::setValueColour( ui::Theme::textColour( 2 ), docTitle );

        docEditor.setMultiLine( true );
        docEditor.setReadOnly( true );
        docEditor.setScrollbarsShown( true );
        docEditor.setCaretVisible( false );
        docEditor.setFont( juce::Font( 13.0f ) );
        docEditor.setColour( juce::TextEditor::backgroundColourId, ui::Theme::backgroundColour( 1 ) );
        docEditor.setColour( juce::TextEditor::textColourId, ui::Theme::textColour( 1 ) );
        docEditor.setColour( juce::TextEditor::outlineColourId, ui::Theme::outlineColour( 1 ) );

        searchBox.onTextChange = [this]() {
            updateFilteredList();
        };

        insertCategories.onSelectionChanged = [&]( int, const juce::String& selectedOption ) {
            if( selectedOption == "Favourites" )
            {
                allCategoryItems.clear();
                for( const auto& f : g_insertFavourites )
                    allCategoryItems.push_back( f );
            }
            else
            {
                allCategoryItems = mt::getAudioInterface().getAvailableInsertsListByCatogary( selectedOption );
            }
            updateFilteredList();
        };

        insertList.onSelectionChanged = [&]( int, const juce::String& selectedOption ) {
            favToggle.setToggleState( g_insertFavourites.find( selectedOption ) != g_insertFavourites.end(), juce::dontSendNotification );
            onClick( selectedOption );
            showDocFor( selectedOption );
        };

        juce::MessageManager::callAsync( [&]() {
            auto category = mt::getAudioInterface().getInsertCategory();
            category.insert( category.begin() + 1, "Favourites" );
            insertCategories.setContent( category, juce::sendNotification );
        } );
    }

    void InsertsSelector::updateFilteredList()
    {
        auto query = searchBox.getText().trim();
        std::vector<juce::String> filtered;
        if( query.isEmpty() )
        {
            filtered = allCategoryItems;
        }
        else
        {
            for( const auto& item : allCategoryItems )
            {
                if( item.containsIgnoreCase( query ) )
                    filtered.emplace_back( item );
            }
            if( filtered.empty() )
            {
                auto awMatches = mt::airwindows::search( query.toStdString() );
                for( const auto* doc : awMatches )
                {
                    if( doc ) filtered.emplace_back( juce::String( doc->name ) );
                }
                auto clapEffects = mt::ClapEffectRegistry::getInstance().getEffectDescriptors();
                for( const auto& desc : clapEffects )
                {
                    if( juce::String( desc.name ).containsIgnoreCase( query ) || juce::String( desc.id ).containsIgnoreCase( query ) )
                    {
                        filtered.emplace_back( juce::String( desc.name ) );
                    }
                }
            }
        }
        insertList.setContent( filtered, juce::dontSendNotification );
        if( !filtered.empty() )
        {
            showDocFor( filtered[0] );
            onClick( filtered[0] );
        }
    }

    void InsertsSelector::showDocFor( const juce::String& name )
    {
        const auto* doc = mt::airwindows::getDoc( name.toStdString() );
        if( doc )
        {
            juce::String text;
            text << doc->name << " [" << doc->category << "]\n\n";
            text << "Summary:\n" << doc->summary << "\n\n";
            text << "Description:\n" << doc->description;
            docEditor.setText( text );
        }
        else
        {
            const auto* desc = mt::ClapEffectRegistry::getInstance().findByName( name.toStdString() );
            if( desc )
            {
                juce::String text;
                text << desc->name << " [CLAP Plugin]\n\n";
                text << "Vendor: " << ( desc->vendor.empty() ? "CLAP" : desc->vendor ) << "\n";
                text << "Version: " << ( desc->version.empty() ? "1.0.0" : desc->version ) << "\n";
                text << "ID: " << desc->id << "\n";
                text << "Path: " << desc->path << "\n\n";
                text << "Description:\n" << ( desc->description.empty() ? "No description provided." : desc->description );
                docEditor.setText( text );
            }
            else
            {
                docEditor.setText( name + "\n\nBuilt-in effect." );
            }
        }
    }

    void InsertsSelector::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, insertCategories.getBounds(), ui::Theme::backgroundColour( 2 ) );
        mt::utils::fillRoundedRect( g, insertList.getBounds(), ui::Theme::backgroundColour( 3 ) );
        mt::utils::fillRoundedRect( g, docEditor.getBounds(), ui::Theme::backgroundColour( 1 ) );
    }

    void InsertsSelector::resized()
    {
        auto b = getLocalBounds().reduced( 5 );
        auto topRow = b.removeFromTop( 28 );
        searchLabel.setBounds( topRow.removeFromLeft( 65 ) );
        searchBox.setBounds( topRow.removeFromLeft( 180 ) );
        topRow.removeFromLeft( 6 );
        favToggle.setBounds( topRow.removeFromLeft( 70 ) );
        b.removeFromTop( 5 );

        auto leftW = 160;
        auto midW = 200;
        insertCategories.setBounds( b.removeFromLeft( leftW ) );
        b.removeFromLeft( 6 );
        insertList.setBounds( b.removeFromLeft( midW ) );
        b.removeFromLeft( 6 );
        auto rightB = b;
        docTitle.setBounds( rightB.removeFromTop( 20 ) );
        docEditor.setBounds( rightB );
    }

    ///
    /// InsertsOptions
    ///
    InsertsOptions::InsertsOptions()
    {
        mt::utils::setValueFontHeight( 20, insertName );
        mt::utils::setValueColour( ui::Theme::textColour( 2 ), insertName );
        mt::utils::setValueJustification( juce::Justification::centred, insertName );

        mt::utils::setValueFontHeight( 13, latencyTailLabel, guiNoticeLabel, cpuNoticeLabel );
        mt::utils::setValueColour( ui::Theme::textColour( 3 ), latencyTailLabel );
        mt::utils::setValueColour( ui::Theme::textColour( 2 ), guiNoticeLabel, cpuNoticeLabel );

        enable.setToggleButtonText( "ENABLED", "BYPASSED" );
        enable.onClick = [&]() { onEnableClicked( pos, enable.getToggleState() ); };

        abButton.setToggleButtonText( "B", "A" );
        abButton.onClick = [this]() {
            if( !pInsert )
                return;
            auto& st = g_slotAB[pos];
            if( !st.isB )
            {
                st.stateA.clear();
                int num = pInsert->getNumParameters( pos );
                for( int p = 0; p < num; ++p )
                    st.stateA.push_back( pInsert->getParameterValue( pos, p ) );
                st.isB = true;
                if( !st.stateB.empty() )
                {
                    for( size_t p = 0; p < st.stateB.size(); ++p )
                        pInsert->setParameterValue( pos, (int)p, st.stateB[p] );
                }
                mt::utils::showNotification( "A/B: State B" );
            }
            else
            {
                st.stateB.clear();
                int num = pInsert->getNumParameters( pos );
                for( int p = 0; p < num; ++p )
                    st.stateB.push_back( pInsert->getParameterValue( pos, p ) );
                st.isB = false;
                if( !st.stateA.empty() )
                {
                    for( size_t p = 0; p < st.stateA.size(); ++p )
                        pInsert->setParameterValue( pos, (int)p, st.stateA[p] );
                }
                mt::utils::showNotification( "A/B: State A" );
            }
            refresh();
        };

        presetSave.onClick = [this]() {
            if( !pInsert || info.name.isEmpty() )
                return;
            auto& vals = g_presets[info.name];
            vals.clear();
            int num = pInsert->getNumParameters( pos );
            for( int p = 0; p < num; ++p )
                vals.push_back( pInsert->getParameterValue( pos, p ) );
            mt::utils::showNotification( "Preset Saved: " + info.name );
        };

        presetLoad.onClick = [this]() {
            if( !pInsert || info.name.isEmpty() )
                return;
            auto it = g_presets.find( info.name );
            if( it != g_presets.end() && !it->second.empty() )
            {
                for( size_t p = 0; p < it->second.size(); ++p )
                    pInsert->setParameterValue( pos, (int)p, it->second[p] );
                refresh();
                mt::utils::showNotification( "Preset Loaded: " + info.name );
            }
            else
            {
                mt::utils::showNotification( "No preset saved for " + info.name );
            }
        };

        openGuiButton.onClick = [this] { openNativeGui(); };
    }

    void InsertsOptions::setupControls( mt::MTIInsertsInterface* pEffect, int Position )
    {
        pInsert = pEffect;
        pos     = Position;

        // Resize controls.
        info = pInsert ? pInsert->getInsertsInfo( pos ) : mt::MTInsertsInfo{};
        setupControls( static_cast<int>( info.params.size() ) );

        // Update values.
        refresh();
    }

    void InsertsOptions::refresh()
    {
        info = pInsert ? pInsert->getInsertsInfo( pos ) : mt::MTInsertsInfo{};

        if( info.missing )
            insertName.setText( "[MISSING] " + info.name, juce::dontSendNotification );
        else
            insertName.setText( info.name, juce::dontSendNotification );

        enable.setToggleState( !info.bypass, juce::dontSendNotification );

        if( info.isClap )
        {
            latencyTailLabel.setText( "Latency: " + juce::String( info.latency ) + " smp | Tail: " + juce::String( info.tail ) + " smp", juce::dontSendNotification );
            if( info.missing )
            {
                guiNoticeLabel.setText( "MISSING PLUGIN: " + info.pluginId, juce::dontSendNotification );
                openGuiButton.setVisible( false );
            }
            else if( info.supportsGui )
            {
                guiNoticeLabel.setText( "Native GUI available", juce::dontSendNotification );
                openGuiButton.setVisible( true );
            }
            else
            {
                guiNoticeLabel.setText( "Native GUI not supported on this platform. Using Generic Parameters.", juce::dontSendNotification );
                openGuiButton.setVisible( false );
            }
        }
        else
        {
            latencyTailLabel.setText( "", juce::dontSendNotification );
            guiNoticeLabel.setText( "", juce::dontSendNotification );
            openGuiButton.setVisible( false );
        }

        double cpu = 0.0;
        if( mt::SystemResource::getResourceInterface().cpuInfo( cpu ) )
        {
            if( cpu > 80.0 )
            {
                cpuNoticeLabel.setText( "CPU: " + juce::String( (int)cpu ) + "% [HIGH LOAD]", juce::dontSendNotification );
                cpuNoticeLabel.setValueColour( juce::Colours::red );
            }
            else
            {
                cpuNoticeLabel.setText( "CPU: " + juce::String( (int)cpu ) + "%", juce::dontSendNotification );
                cpuNoticeLabel.setValueColour( ui::Theme::textColour( 2 ) );
            }
        }
        else
        {
            cpuNoticeLabel.setText( "", juce::dontSendNotification );
        }

        for( size_t i = 0; i < info.params.size(); ++i )
        {
            if( i < rotary.size() )
            {
                rotary[i]->setTitle( info.params[i].name );
                // The effect's own value text ("-6.00 dB", "250 Hz") instead of the bare 0..1 number.
                const auto shown = info.params[i].displayValue.trim() + ( info.params[i].unit.isNotEmpty() ? " " + info.params[i].unit : juce::String() );
                if( shown.trim().isNotEmpty() )
                    rotary[i]->textFromValueFunction = [shown]( double ) { return shown; };
                else
                    rotary[i]->textFromValueFunction = nullptr;

                if( info.params[i].isStepped )
                {
                    float st = info.params[i].step > 0.f ? info.params[i].step : 0.05f;
                    rotary[i]->setRange( 0.f, 1.f, st );
                }
                else
                {
                    rotary[i]->setRange( 0.f, 1.f, 0.001f );
                }

                rotary[i]->setValue( info.params[i].value, juce::dontSendNotification );
                rotary[i]->updateText();
            }
        }
    }

    void InsertsOptions::linkRotated( int Index, int Ticks )
    {
        if( Index < 0 || Index >= static_cast<int>( info.params.size() ) || Index >= static_cast<int>( rotary.size() ) || !isVisible() )
            return;
        if( info.params[static_cast<size_t>( Index )].isStepped )
        {
            float step = info.params[static_cast<size_t>( Index )].step > 0.f ? info.params[static_cast<size_t>( Index )].step : 0.05f;
            rotary[static_cast<size_t>( Index )]->knobRotated( Ticks * step );
        }
        else
        {
            rotary[static_cast<size_t>( Index )]->knobRotated( mt::mixlogic::qlinkDelta( kick_ui::knob_logic::acceleratedCount( Ticks ), 0.01 ) );
        }
    }

    void InsertsOptions::openNativeGui()
    {
        if( pInsert )
        {
            auto* clap = pInsert->getClapEffect( static_cast<unsigned int>( pos ) );
            if( clap && clap->isLoaded() && clap->getHost().supportsGui() )
            {
                juce::DialogWindow::LaunchOptions dialog;
                dialog.content.setOwned( new ClapGuiComponent( clap, info.name ) );
                dialog.dialogTitle = "CLAP - " + info.name;
                dialog.dialogBackgroundColour = juce::Colour( 0xff15191e );
                dialog.escapeKeyTriggersCloseButton = true;
                dialog.useNativeTitleBar = true;
                dialog.resizable = true;
                dialog.launchAsync();
            }
        }
    }

    void InsertsOptions::setupControls( int NumControls )
    {
        // Resize; if required.
        if( NumControls > static_cast<int>( rotary.size() ) )
        {
            auto i = static_cast<int>( rotary.size() );
            for( ; i < NumControls; ++i )
            {
                auto ptr = std::make_unique<ui::KickRotary>( "" );
                // Setup rotary
                ptr->showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
                ptr->setDisplayProportions( 0.6f, 0.2f, 0.2f );
                ptr->setRange( 0.f, 1.f, 0.001f );
                ptr->onValueChanged = [&, i]( double Value ) { onRotaryChanged( pos, i, static_cast<float>( Value ) ); };
                ptr->onDragStart = [&, i]() {
                    if( pInsert )
                    {
                        if( auto* c = pInsert->getClapEffect( static_cast<unsigned int>( pos ) ) )
                            c->beginGesture( i );
                    }
                };
                ptr->onDragEnd = [&, i]() {
                    if( pInsert )
                    {
                        if( auto* c = pInsert->getClapEffect( static_cast<unsigned int>( pos ) ) )
                            c->endGesture( i );
                    }
                };

                // Insert this control to vector.
                rotary.emplace_back( std::move( ptr ) );
            }
        }

        // Clear all rotaries...
        removeAllChildren();
        for( auto& it : rotary )
        {
            it->setTitle( "" );
            it->setValue( 0.0, juce::dontSendNotification );
        }

        // Enable required number of controls...
        addAndMakeVisible( insertName );
        addAndMakeVisible( enable );
        addAndMakeVisible( abButton );
        addAndMakeVisible( presetSave );
        addAndMakeVisible( presetLoad );
        addAndMakeVisible( latencyTailLabel );
        addAndMakeVisible( guiNoticeLabel );
        addAndMakeVisible( cpuNoticeLabel );
        addAndMakeVisible( openGuiButton );
        for( auto i = 0; i < NumControls; ++i )
            addAndMakeVisible( rotary[static_cast<size_t>( i )].get() );

        resized();
    }

    void InsertsOptions::visibilityChanged()
    {
        if( !isVisible() )
            setupControls( 0 );
    }

    void InsertsOptions::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, getLocalBounds(), ui::Theme::backgroundColour( 3 ) );
    }

    void InsertsOptions::resized()
    {
        auto b = getLocalBounds().reduced( 20 );
        // Name
        auto topBar = b.removeFromTop( 30 ).reduced( 5, 1 );
        presetLoad.setBounds( topBar.removeFromRight( 50 ).reduced( 2, 2 ) );
        presetSave.setBounds( topBar.removeFromRight( 50 ).reduced( 2, 2 ) );
        abButton.setBounds( topBar.removeFromRight( 45 ).reduced( 2, 2 ) );
        enable.setBounds( topBar.removeFromRight( 75 ) );
        topBar.removeFromRight( 8 );
        openGuiButton.setBounds( topBar.removeFromRight( 85 ) );
        topBar.removeFromRight( 8 );
        insertName.setBounds( topBar );

        // Latency and Tail info bar
        auto infoBar = b.removeFromTop( 24 ).reduced( 5, 2 );
        cpuNoticeLabel.setBounds( infoBar.removeFromRight( 140 ) );
        latencyTailLabel.setBounds( infoBar.removeFromLeft( infoBar.proportionOfWidth( 0.45f ) ) );
        guiNoticeLabel.setBounds( infoBar );

        // Controls
        auto w = b.proportionOfWidth( 0.25f );
        auto h = b.proportionOfHeight( 0.25f );

        auto numControls = static_cast<int>( info.params.size() );
        auto idx         = 0;
        while( idx < numControls )
        {
            auto rowB = b.removeFromTop( h ).reduced( 5 );
            for( int j = 0; j < 4; ++j, ++idx )
            {
                auto colB = rowB.removeFromLeft( w ).reduced( 5 );
                if( idx < numControls )
                {
                    rotary[static_cast<size_t>( idx )]->setBounds( colB );
                }
                else
                    break;
            }
        }
    }

#undef insertGridHorSpacing
#undef insertGridVerSpacing
#undef insertTotalRows
}
