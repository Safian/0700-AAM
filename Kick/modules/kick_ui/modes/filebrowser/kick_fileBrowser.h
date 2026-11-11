#pragma once
#include <set>
#include <unordered_set>
#include "kick_ui/recent_project/kick_RecentProject.h"
#include "kick_fsGuard.h"
#include "kick_fileOps.h"
#include "kick_fileList.h"
#include "kick_mounts.h"
#include "kick_fbLogic.h"
#include "kick_cloudAuthPopup.h"
constexpr int   numButtonWidth = 44;
constexpr float cornerSizeX    = 6.0f;
constexpr float cornerSizeY    = 6.0f;
constexpr float lineThickness  = 1.0f;

//Horizontal Line
constexpr int lineX      = 0;
constexpr int lineY      = 54;
constexpr int lineWidth  = 342;
constexpr int lineHeight = 1;

constexpr int headerLabelFontHeight = 17;

constexpr int leftTableRowHeight  = 53;
constexpr int rightTableRowHeight = 53;

constexpr int   labelLeftMargin = 10;
constexpr float labelFontHeight = 14.0f;

constexpr int fileWidth  = 39;
constexpr int fileHeight = 46;

constexpr int directoryWidth  = 54;
constexpr int directoryHeight = 40;

constexpr int textHeight = 22;

#define DEMOSROW 2

namespace ui
{
    class AuditionComponent : public juce::Component
    {
    public:
        AuditionComponent();

        std::function<void( bool )>   onMuteChanged = []( bool ) {};
        std::function<void( double )> onGainChanged = []( double ) {};

        inline bool   isMuted() const noexcept { return volume.getToggleState(); }
        inline double getVolume() const noexcept { return slider.getNormalized(); }
        inline void   setVolume( double Volume ) { slider.setNormalized( Volume ); }

    private:
        mt::KickSettings& settings{ mt::getSettings() };
        void              resized() override;
        void              paint( juce::Graphics& ) override;

    private:
        ui::KickSlider  slider{ "SLIDER", juce::Slider::LinearBarVertical, juce::Slider::TextBoxLeft };
        KickImageButton volume{ "Volume", "Volume_BrowserActive", "Button_VolumeMute" };
    };

    enum filterSystem
    {
        Null = -1,
        Projects,
        Samples,
        All,
        Midi // MIDI files (.mid); chosen in the browser options, the three filter buttons show none
    };
    class FileBrowserLeftPanel : public juce::Component
    {
    public:
        KickButton                     sourceButton{ "Sources", "+ Cloud" }; // add / edit Cloud source (Splice, Google Drive, WebDAV)
        KickButton                     folderButton{ "LocalFolder", "+ Folder" }; // add / edit a local folder location
        KickImageButton                places{ "Places", "Places" };
        KickImageButton                content{ "Content", "Content" };
        KickImageButton                expansion{ "Expansion", "Expansion" };
        KickTableViewProvider          provider;
        std::unique_ptr<KickTableView> table;

        FileBrowserLeftPanel();

    private:
        juce::Rectangle<int> leftTopBounds    = { 0, 0, 342, 54 };
        juce::Rectangle<int> leftCentreBounds = { 0, 54, 342, 290 };
        KickLabel            location{ "Location" };

        void paint( juce::Graphics& g ) override;
        void resized() override;
    };

    class FileBrowserRightPanel : public juce::Component
    {
    public:
        KickTableViewProvider          provider;
        std::unique_ptr<KickTableView> table;
        KickImageButton                backButton{ "Back", "Back_Browser", "", "", "Back_Browser_Disabled" };
        std::function<void( int )>     onHeaderClicked; // 0 name, 1 date / time, 2 size: sort by that column

        FileBrowserRightPanel();
        void setSortIndicator( int key, bool ascending );
        void setColumns( bool showDate, bool showTime, bool showSize );

    private:
        void                 mouseDown( const juce::MouseEvent& e ) override;
        juce::Rectangle<int> rightTopBounds    = { 0, 0, 652, 54 };
        juce::Rectangle<int> rightCentreBounds = { 0, 54, 652, 290 };
        void                 paint( juce::Graphics& g ) override;
        void                 resized() override;
        KickLabel            name{ "Name" }, date{ "Date" }, timeStamp{ "Time Stamp" }, size{ "Size" };
    };

    class FileBrowserTopControls : public juce::Component
    {
    public:
        std::function<void( void )> onElementChangedCallback;
        KickImageButton             searchButton{ "Search", "Search_Browser", "Search_BrowserActive  " };
        KickImageButton             backButton{ "Back", "Back_Browser", "Back_Browser", "Back_BrowserActive" };
        KickImageButton             projectButton{ "Project", "Project_Browser", "Project_BrowserActive" };
        KickImageButton             sampleButton{ "Sample", "Sample_Browser", "Sample_BrowserActive" };
        KickImageButton             allButton{ "All", "All_Browser", "All_BrowserActive" };

        KickImageButton     cutBtn{ "Cut Button", "Cut Button", "Cut Button_Active", "", "Cut Disabled" };
        KickImageButton     copyBtn{ "Copy Button", "Copy Button", "Copy Button_Active" };
        KickImageButton     pasteBtn{ "Paste Button", "Paste Button", "Paste Button_Active", "", "Paste Disabled" };
        KickImageButton     deleteBtn{ "Delete Button", "Trash", "Trash_Active", "", "Trash Disabled" };
        KickLabel           searchLabel{ "Search Directory" };
        KickImageButton     renameBtn{ "Rename", "Rename", "Rename_Active", "", "Rename Disabled" };
        ui::KickImageButton selectBtn{ "Select", "Select_Normal", "Select_Active" };
        KickButton          selectAllBtn{ "SelectAll", "All" }; // Select mode: mark every row

        FileBrowserTopControls();

    private:
        void paint( juce::Graphics& g ) override;
        void resized() override;
    };

    class FileBrowserBottomControls : public juce::TabbedComponent
    {
    public:
        KickImageButton projectButton{ "Project", "Project_Browser", "Project_BrowserActive" };
        KickImageButton sampleButton{ "Sample", "Sample_Browser", "Sample_BrowserActive" };
        KickImageButton allButton{ "All", "All_Browser", "All_BrowserActive" };

        KickButton      sampleAssignButton{ "Sample Assign", "Sample Assign" };
        KickImageButton playIconButton{ "Play", "Play_browser", "Play_browser_Active" };

        KickMultiSelectTabBarButton auditionButton{ 3, getTabbedButtonBar(), "OFF" };

        KickButton      openButton{ "Open", "Open" }; // enter a folder (also a project folder, which Load opens as a project)
        KickButton      optionsButton{ "BrowserOptions", "Options" };
        KickButton      loadButton{ "Load", "Load" };
        KickButton      loadAllButton{ "Load All", "Load All" };
        KickImageButton volumeBtn{ "Volume", "Volume_Browser", "Volume_BrowserActive" };
        KickImageButton volumeMuteBtn{ "VolumeMute", "Button_VolumeMute", "Button_VolumeMute" };

        FileBrowserBottomControls();

    private:
        void paint( juce::Graphics& g ) override;
        void resized() override;
    };

    // One entry of the left list (a "Place"): the internal folders, shortcuts, extra locations (WebDAV / local folder) and the
    // drives found in the mount table. `path` is a folder, or for a Source the source id.
    struct Place
    {
        enum class Kind { Internal, Library, Demos, Source, Sd, Usb, Shortcut };
        Kind         kind{ Kind::Internal };
        juce::String label;
        juce::String icon, iconActive;
        float        width{ 16.0f }, height{ 16.0f };
        juce::String path;
        juce::String device; // /dev/sdb1 for a drive
        bool         isDrive() const { return kind == Kind::Sd || kind == Kind::Usb; }
    };
    // What the left list shows (the Places / Content / Expansion tabs).
    enum class LeftTab { Places, Content, Expansion };

    // Browser options (Options popup), kept in the preferences.
    struct BrowserOptions
    {
        bool recursiveSearch{ false }; // search inside sub-folders too
        bool hideSystem{ true };       // .DS_Store, System Volume Information ...
        bool hideProjectData{ false }; // inside a project: samples / export folders
        bool copyExternal{ true };     // Load copies a sample from outside the project into <project>/samples
        bool showDate{ true }, showTime{ true }, showSize{ true };
        int  filter{ 2 };              // the file filter: 0 projects, 1 samples, 2 all, 3 MIDI (not persisted)
        int  sortKey{ 0 };             // 0 name, 1 date, 2 size
        bool sortAscending{ true };
    };

    class FileRightEntry;
    class FileBrowser : public KickControlBase<juce::Component>
        , juce::Timer
        , public kick_ui::IScreen
    {
    public:
        std::function<void()>                       closeScreenCallback;
        std::function<void( ui::KickImageButton* )> onFirstClick;
        std::function<void( ui::KickImageButton* )> onSecondClick;
        std::function<void( ui::KickImageButton* )> onDoubleClick;

        FileBrowser();
        ~FileBrowser();

        bool requiresControlBar() override { return false; }
        bool requiresStatusBar() override { return true; }
        bool requiresHandleCommand() override { return true; }

        void handleCommand( CPktMgr::HWMsg_t& ) override;
        void sliderMoved( float ) override{};
        void knobRotated( double ) override;
        void knobClicked() override;
        void visibilityChanged() override;

        // Selects the place that holds `path` and lists it (a favourite, the smoke test).
        void showFolder( const juce::String& path );
        void refreshPlacesAndListing();
        void importIncomingFiles( const juce::StringArray& paths, bool removeSourceOnSuccess = false );
        void configureGoogleDriveFromSettings() { showGoogleAuthQr(); }
        void showGoogleAuthQr();
        void showSpliceAuthQr();
        // The names of the rows in the right list, in order (the smoke test checks sorting and hidden files).
        juce::StringArray getShownNames() const;

        void cancelFolderSizeCalculations();
        void queueFolderSizeCalculation( const juce::File& folder, int64_t mtimeMs, FileRightEntry* comp );
        bool isGenerationCancelled( uint64_t gen ) const noexcept;

        mt::MTAudioInterface& ae;

        FileBrowserLeftPanel left;

    private:
        kick_ui::AppStatusbar& statusBar{ kick_ui::AppStatusbar::Instance() };

        // ---- left list: places
        std::vector<Place> fixedPlaces, sourcePlaces, drivePlaces, shortcutPlaces;
        std::vector<Place> shownPlaces; // the rows of the left table (depends on the tab)
        LeftTab            leftTab{ LeftTab::Places };
        std::set<std::string> ejectedDevices;
        void rebuildShortcuts();
        void rebuildShown();
        void showPlace( int row, bool load ); // highlights the row, and loads its root folder when `load`
        int  rowOfPlaceContaining( const juce::String& path ) const;
        const Place* currentPlaceRow() const;
        void pollDrives();
        void ejectDrive( const Place& p );
        void formatDrive( const Place& p );
        bool driveHoldsOpenProject( const Place& p ) const;
        void setLeftTab( LeftTab t );
        void updateEditButtons();

        // ---- right list: the folder
        std::vector<kick_filelist::Entry> entries; // the rows of the right table
        juce::Array<juce::File>           selectedFile;
        juce::String                      pwd, placeRoot;
        int                               filterNum{ 2 };
        bool                              searchMode{ false };
        BrowserOptions                    opts;
        uint64_t                          listGen{ 0 };
        juce::ThreadPool                  listPool{ 1 };
        std::shared_ptr<std::atomic<bool>> searchToken; // set = the running search stops
        enum class Clip { None, Copy, Cut };
        Clip                              clip{ Clip::None };
        juce::Array<juce::File>           clipFiles;
        juce::int64                       favDownAt{ 0 };
        ui::KickImageButton*              favDownBtn{ nullptr };
        std::set<std::string>             manualSizeRequested;
        bool                              held{ false }; // the Play button is held down (MAN audition)
        kick_fblogic::HeldPads            heldPads;
        void navigate( const juce::String& dir, const juce::String& selectName = {} );
        juce::String formatDisplayPath( const juce::String& path ) const;
        void startListing( const juce::String& selectName );
        void onListed( uint64_t gen, kick_filelist::Listing l, const juce::String& selectName, bool wasSearch, bool truncated );
        void startSearch( const juce::String& text );
        kick_filelist::ListOptions listOptions() const;
        void exitDirectory();
        bool canGoUp() const;
        void syncSelection();
        void activateRow( int row );
        void openSelected();
        void loadSelected();
        void loadAllVisible();
        void loadProjectFolder( const juce::File& dir );
        void audition( juce::File f );
        void renamePopup();
        void renameImplementation( juce::String );
        void deleteSystem();
        void pasteHere();
        void afterFileChange( const juce::Array<juce::File>& touched );
        void favouriteButtonImplementation();
        void showNextFavourite();
        void recallFavourite( const juce::String& path );
        bool favouritesRequested{ false };
        int  favouriteCursor{ -1 };
        juce::String pendingImportSelectName;
        juce::String pendingImportBanner;
        void selectBtnOnClick();
        bool checkIsCurrProj();
        bool isProtectedPath( const juce::File& f ) const;
        std::vector<std::string> placeRootsForGuard() const;
        void warn( const juce::String& message );
        void hideAuditionComponent();
        void showAuditionComponent();
        void loadSample( const juce::Array<juce::File>& assetFiles, bool IsMultiple, std::function<void()> onFinished = {} );
        void showOptions();
        void showDriveInfo();
        void applyColumnOptions();
        void loadOptions();
        void saveOptions();
        void updateLoadTargetStatus();
        void mouseDown( const juce::MouseEvent& e ) override;
        void mouseUp( const juce::MouseEvent& e ) override;
        void timerCallback() override;

        kick_ui::KickModeHeader header{ "File Browser", "Icon_Browser" };
        KickLabel               pathLabel;

        FileBrowserRightPanel     right;
        FileBrowserTopControls    top;
        FileBrowserBottomControls bottom;
        AuditionComponent         auditionComp;

        int timerInt = 1;
        int buttonId;

        mt::KickSettings&                             settings{ mt::getSettings() };
        std::unique_ptr<LoadSamplesAndAnimation>      sampleAnimation;
        std::vector<std::unique_ptr<KickImageButton>> grpNum;
        juce::String                                  projectFileList{ ".kickPrj" };
        juce::String                                  replacementPath = "[Internal]";

        // ---- extra locations (FileBrowserSource): WebDAV / local-folder sources
        static constexpr const char*                   sourceRowPrefix = "src:";
        std::unique_ptr<fileSource::SourceHub>         hub;
        bool                                           remoteActive{ false };
        std::shared_ptr<fileSource::FileBrowserSource> activeSource;
        std::string                                    remotePath{ "/" };
        std::vector<fileSource::Entry>                 remoteEntries, remoteShown;
        juce::String                                   remoteSearch;
        uint64_t                                       remoteReq{ 0 };
        uint64_t                                       remoteDownloadId{ 0 };
        int                                            lastDownloadPercent{ -1 };
        void refreshSourceRows();
        void openSource( const Place& p );
        void leaveSource();
        void listRemote( const std::string& path );
        void onRemoteListed( uint64_t gen, const std::string& path, const fileSource::ListResult& lr );
        void applyRemoteFilter();
        void remoteActivate( int row );
        void remoteBack();
        void remoteFetchAndLoad( const fileSource::Entry& e );
        void addOrEditCloudSource();
        void editSpliceSource();
        void editGoogleDriveSource();
        void startGoogleOAuthSignIn( const std::string& clientId, const std::string& clientSecret, const juce::String& emailHint, const std::string& existingId );
        void finishGoogleDriveSourceSetup( const fileSource::SourceConfig& cfg, const std::string& secretText, const juce::String& statusMsg );
        void editWebDavSource();
        void editLocalSource();
        juce::File findBestSpliceDirectory() const;
        juce::Component* remoteCell( int row, juce::Component* existing );

        ui::fileUtils::GenerationTracker              generationTracker;
        std::unordered_set<std::string>               pendingPaths;
        juce::ThreadPool                              folderSizeThreadPool{ 1 };

        void paint( juce::Graphics& g ) override;
        void resized() override;
        void setupProvider();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( FileBrowser )
    };

    class FileLeftEntryPopup : public juce::Component
    {
    public:
        FileLeftEntryPopup( juce::String, juce::String image, int, int, juce::String fileName, juce::String date );
        ~FileLeftEntryPopup(){};
        void close();

        ui::KickImageButton button1{ "No", "No" };
        ui::KickImageButton button2{ "Yes", "Yes" };

    private:
        void            resized() override;
        void            paint( juce::Graphics& g ) override;
        ui::KickLabel   info, fileName, date;
        KickImageButton folder{ "Folder" };
        int             width, height;
        int             x = 98, y;
    };

    class FileLeftEntry : public juce::Component
    {
    public:
        std::function<void( int )> ejectCallback;
        std::function<void( int )> formatCallback;
        FileLeftEntry( int rowId );
        ~FileLeftEntry() {}
        void            setProperties( const Place& place );
        KickLabel       file;
        int             getRowID() { return row; }
        void            setHighlight( const bool state );
        KickImageButton ejectButton{ "Eject", "Eject", "Eject", "Eject_Active" };

    private:
        void resized() override;
        void paint( juce::Graphics& g ) override;

        KickImageButton imageButton;
        KickImageButton formatButton{ "Format", "Format", "Format", "Format_Active" };
        bool            enableEjectBtn{ false };
        float           width{ 0.0f }, height{ 0.0f };
        int             row;
    };

    class FileRightEntry : public juce::Component
    {
    public:
        FileRightEntry();
        ~FileRightEntry() {}
        void setProperties( const juce::String& imageButtonFile, bool, const float Width, const float Height, const juce::String& File, const juce::String& Date, const juce::String& Time, const juce::String& Size );
        void setSizeText( const juce::String& Size );
        void setColumns( bool showDate, bool showTime, bool showSize );
        void setCurrentPath( const juce::String& path ) { currentPath = path; }
        const juce::String& getCurrentPath() const noexcept { return currentPath; }

    private:
        float width{ 0.0f }, height{ 0.0f };
        bool  isFolder{ false };
        bool  addedLabels{ false };
        juce::String currentPath;

        KickImageButton imageBtn;
        KickLabel       file;
        KickLabel       date;
        KickLabel       time;
        KickLabel       size;

        void resized() override;
        void paint( juce::Graphics& g ) override;
    };

}
