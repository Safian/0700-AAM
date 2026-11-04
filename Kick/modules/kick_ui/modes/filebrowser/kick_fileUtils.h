#pragma once
#include "kick_folderSizeHelper.h"
#include "kick_fbLogic.h"
#include "kick_fsGuard.h"
namespace ui
{
    namespace fileUtils
    {
        // The one list of sample extensions of the browser (the engine decides in the end: addAsset says Invalid for a file it cannot
        // decode, and that is shown to the user).
        inline const char* sampleExtensions() { return ".wav;bwf;aif;aiff;flac;ogg;mp3;wmv;asf;wm;wma"; }

        // Folder size by one scan; the sizes of the sub-folders found on the way are cached too, so a cancelled scan (the user
        // navigated away) leaves its partial work behind. -1 = cancelled.
        juce::int64 calculateFolderSize( juce::File filePath, std::function<bool()> shouldExit = nullptr );
        // A change below `changed` (copy, move, delete, new file) makes the cached size of it and of all its parents stale.
        void invalidateFolderSizes( const juce::File& changed );
        juce::String getFilePath( juce::String originalFullPath, juce::String replacementPath );

        void setHighlightLED( int index );
        bool hasSampleAssignedToPad( int LastPressedPad );
        // The pad a loaded sample lands on: the last pad that was pressed, else the first free pad of the bank.
        int targetPad();

        extern int lastPressedPad;

        FolderSizeCache& getFolderSizeCache();
    }

    class LoadSamplesAndAnimation
        : public juce::Thread
        , public mt::utils::LoadAnimation
    {
    public:
        LoadSamplesAndAnimation();
        ~LoadSamplesAndAnimation();

        // isMultiple (Load All): every file goes into the pool; otherwise the last file lands on `pad` (0..15 of the current bank).
        // copyExternal: a sample from outside the project is copied into <project>/samples first (a pulled SD / USB stick leaves no
        // missing sample). onComplete runs on the message thread; message() tells what happened.
        void startLoading( const juce::Array<juce::File>& assetsFiles, bool IsMultiple, int pad, bool copyExternal, std::function<void()> onCompleteCallback );
        bool isSampleLoaded() const { return sampleLoaded.load(); }
        juce::String message() const { const juce::ScopedLock l( lock ); return result; }
        bool         busy() { return isThreadRunning(); }

    private:
        void                    run() override;
        bool                    isMultiple{ true };
        int                     targetPadNo{ 0 };
        bool                    copyExt{ true };
        std::atomic<bool>       sampleLoaded{ false };
        juce::Array<juce::File> assetsFile;
        std::function<void()>   completionCallback;
        mutable juce::CriticalSection lock;
        juce::String                  result;
    };

}
