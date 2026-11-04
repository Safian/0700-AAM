#pragma once
#include <kick_ui/modes/filebrowser/kick_fileUtils.h>
namespace ui
{
    namespace fileUtils
    {
        int lastPressedPad = -1;

        FolderSizeCache& getFolderSizeCache()
        {
            static FolderSizeCache cache;
            return cache;
        }

        juce::int64 calculateFolderSize( juce::File filePath, std::function<bool()> shouldExit )
        {
            if( !filePath.isDirectory() )
                return filePath.getSize();
            // The walk itself is scanFolderSize (host-tested with a real temp folder); this lists one folder through JUCE.
            const auto list = []( const std::string& dir ) {
                std::vector<DirEntry> out;
                for( const auto& entry : juce::RangedDirectoryIterator( juce::File( juce::String( dir ) ), false, "*", juce::File::findFilesAndDirectories, juce::File::FollowSymlinks::no ) )
                {
                    const auto f = entry.getFile();
                    out.push_back( { f.getFullPathName().toStdString(), entry.isDirectory(), f.isSymbolicLink(), (int64_t)entry.getFileSize(),
                                     entry.isDirectory() ? f.getLastModificationTime().toMilliseconds() : 0 } );
                }
                return out;
            };
            return scanFolderSize( filePath.getFullPathName().toStdString(), list, getFolderSizeCache(), shouldExit );
        }

        void invalidateFolderSizes( const juce::File& changed )
        {
            auto& cache = getFolderSizeCache();
            auto  f     = changed;
            for( int i = 0; i < 64 && f != juce::File(); ++i )
            {
                cache.invalidate( f.getFullPathName().toStdString() );
                const auto parent = f.getParentDirectory();
                if( parent == f )
                    break;
                f = parent;
            }
        }

        juce::String getFilePath( juce::String originalFullPath, juce::String replacementPath )
        {
            juce::String searchLabel = getDefaultPath().getFullPathName();
            int          pos         = originalFullPath.indexOf( searchLabel );
            if( pos != -1 )
                originalFullPath = originalFullPath.replaceSection( pos, searchLabel.length(), replacementPath );
            return originalFullPath;
        }

        bool hasSampleAssignedToPad( int LastPressedPad )
        {
            auto&                       ae = mt::getAudioInterface();
            std::vector<mt::MTISample*> samples;

            ae.getProgramSamples( ae.getSelectedProgram(), samples );
            for( auto& sample : samples )
            {
                if( ae.getSelectedProgramData()->type == mt::MTProgramType::Keygroup )
                    return kick_ui::Pad_Assigned;
                auto index = (int)sample->keyNote.get() - ae.getBankOffset();
                if( juce::isPositiveAndBelow( index, 16 ) && sample->getSource( 0 ).length() )
                    if( index == LastPressedPad )
                        return kick_ui::Pad_Assigned;
            }
            return kick_ui::Pad_NotAssigned;
        }

        int targetPad()
        {
            if( lastPressedPad >= 0 && lastPressedPad < 16 )
                return lastPressedPad;
            std::array<bool, 16> assigned{};
            for( int i = 0; i < 16; ++i )
                assigned[static_cast<size_t>( i )] = hasSampleAssignedToPad( i ) == kick_ui::Pad_Assigned;
            return kick_fblogic::firstFreePad( assigned );
        }

        void setHighlightLED( int hwPadIndex )
        {
            if( juce::isPositiveAndBelow( hwPadIndex, _NUM_PADS_IN_COMPONENT ) )
            {
                if( ui::fileUtils::lastPressedPad < 0 )
                {
                    ui::fileUtils::lastPressedPad = hwPadIndex;
                    contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                                 setPadDataMessage( lastPressedPad,
                                                                                    contracts::DeviceControlContracts::PadButtonState::StateOn,
                                                                                    contracts::DeviceControlContracts::padColourlightGreen1 ) );
                }
                if( ui::fileUtils::lastPressedPad != hwPadIndex )
                {
                    if( hasSampleAssignedToPad( ui::fileUtils::lastPressedPad ) == kick_ui::Pad_Assigned )
                        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                                     setPadDataMessage( lastPressedPad,
                                                                                        contracts::DeviceControlContracts::PadButtonState::StateOn,
                                                                                        contracts::DeviceControlContracts::padColourDarkYellow ) );
                    else
                        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                                     setPadDataMessage( lastPressedPad,
                                                                                        contracts::DeviceControlContracts::PadButtonState::StateOn,
                                                                                        contracts::DeviceControlContracts::PadColourTrans ) );
                    contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                                 setPadDataMessage( hwPadIndex,
                                                                                    contracts::DeviceControlContracts::PadButtonState::StateOn,
                                                                                    contracts::DeviceControlContracts::padColourlightGreen1 ) );
                    ui::fileUtils::lastPressedPad = hwPadIndex;
                }
            }
        }
    }

    /////////////////////////////////////////////////////////////////
    ////////   LoadSamplesAndAnimation   ////////////////////////////
    /////////////////////////////////////////////////////////////////

    LoadSamplesAndAnimation::LoadSamplesAndAnimation()
        : juce::Thread( "LoadSample" )
    {
    }

    LoadSamplesAndAnimation::~LoadSamplesAndAnimation()
    {
        stopThread( 5000 );
    }

    void LoadSamplesAndAnimation::startLoading( const juce::Array<juce::File>& assetsFiles, bool IsMultiple, int pad, bool copyExternal, std::function<void()> onCompleteCallback )
    {
        if( isThreadRunning() )
            return; // one load at a time (a double tap on Load must not start two)
        sampleLoaded       = false;
        completionCallback = std::move( onCompleteCallback );
        assetsFile         = assetsFiles;
        isMultiple         = IsMultiple;
        targetPadNo        = pad;
        copyExt            = copyExternal;
        {
            const juce::ScopedLock l( lock );
            result.clear();
        }
        startThread();
        startAnimation();
    }

    // Worker thread: decoding, copying and the engine calls all happen here, never on the message thread. The result is a message
    // for the status line, built from the engine's real answer (kick_fblogic::mapAssetResult).
    void LoadSamplesAndAnimation::run()
    {
        auto&                   ae = mt::getAudioInterface();
        int                     added = 0, already = 0, failed = 0;
        juce::String            firstName, lastText;
        kick_fblogic::AssetStatus lastStatus = kick_fblogic::AssetStatus::Invalid;
        const bool              projectLoaded = ae.isProjectLoaded();
        const int               program       = projectLoaded ? ae.getSelectedProgram() : -1;
        const auto              projDir       = projectLoaded ? ae.getProjectDir() : juce::File();

        for( int i = 0; i < assetsFile.size(); ++i )
        {
            auto file = assetsFile[i];
            if( !file.hasFileExtension( fileUtils::sampleExtensions() ) )
                continue;
            if( threadShouldExit() )
                break;
            if( firstName.isEmpty() )
                firstName = file.getFileNameWithoutExtension();
            // From outside the project (SD, USB, library): copy into <project>/samples so the sample stays when the stick is pulled.
            if( copyExt && projectLoaded && projDir.isDirectory() && !file.isAChildOf( projDir ) )
            {
                auto samples = projDir.getChildFile( "samples" );
                samples.createDirectory();
                const auto name = kick_fsguard::uniqueName( file.getFileName().toStdString(), [&]( const std::string& n ) {
                    const auto c = samples.getChildFile( juce::String( n ) );
                    return c.exists() && !c.hasIdenticalContentTo( file ); // an identical copy from an earlier load is reused
                } );
                const auto copy = samples.getChildFile( juce::String( name ) );
                if( !name.empty() && ( copy.existsAsFile() || file.copyFileTo( copy ) ) )
                    file = copy;
                else
                    kick_log( "warn", "[WARN] [BROWSER] could not copy %s into the project; loading it from where it is\n", file.getFullPathName().toRawUTF8() );
            }
            const int  code   = static_cast<int>( ae.addAsset( file.getFullPathName(), mt::AssetTypes::Samples ) );
            const auto status = kick_fblogic::mapAssetResult( code, projectLoaded, program );
            lastStatus        = status;
            if( status == kick_fblogic::AssetStatus::Added ) ++added;
            else if( status == kick_fblogic::AssetStatus::AlreadyExists ) ++already;
            else ++failed;
            if( !isMultiple && kick_fblogic::assignsToPad( status ) )
                ae.addSampleToProgram( program, targetPadNo + ae.getBankOffset(), file.getFullPathName() );
            if( status == kick_fblogic::AssetStatus::NoProject || status == kick_fblogic::AssetStatus::NoProgram )
                break;
        }

        juce::String text;
        const auto   pad = kick_fblogic::padText( targetPadNo, ae.getBankOffset() );
        if( added + already + failed == 0 )
            text = "Selected file cannot be loaded (not an audio file)";
        else if( !isMultiple )
            text = juce::String( kick_fblogic::describe( lastStatus, firstName.toStdString(), pad ) );
        else
        {
            text = juce::String( added ) + " loaded into the pool";
            if( already ) text << ", " << already << " already there";
            if( failed ) text << ", " << failed << " could not be read";
        }
        sampleLoaded = added + already > 0;
        {
            const juce::ScopedLock l( lock );
            result = text;
        }
        if( completionCallback ) // on the message thread
            juce::MessageManager::callAsync( completionCallback );
    }

}
