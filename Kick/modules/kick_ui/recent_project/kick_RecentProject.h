#pragma once

#include "kick_ui/dialogs/kick_StartupScreen.h" // StatusStrip (this header is reached early via the file browser header)
#include "kick_project_names.h"
#include "kick_ProjectActions.h"

namespace ui
{
    // Action menu of a tile (long press / Shift+ENTER): a column of text buttons next to the tile. Touch or the Data Dial + ENTER.
    class LongPressComp : public juce::Component
    {
    public:
        struct Action
        {
            juce::String          label;
            std::function<void()> run;
        };

        LongPressComp();
        ~LongPressComp() {}

        // anchor: the tile in this component's coordinates. The menu sits beside it and stays inside the screen.
        void showActions( std::vector<Action> actions, juce::Rectangle<int> anchor );
        void close();
        bool isOpen() const { return isVisible() && !buttons.empty(); }
        void hwStep( int delta ); // Data Dial
        void hwActivate();        // ENTER

    private:
        std::vector<Action>                      actions;
        std::vector<std::unique_ptr<ui::KickButton>> buttons;
        int                                      selected{ 0 };
        juce::Rectangle<int>                     bounds;

        void paint( juce::Graphics& g ) override;
        void mouseDown( const juce::MouseEvent& e ) override;
        void activate( int index );
    };

    class NewProjectBtn : public juce::Component
    {
    public:
        std::function<void()> onClick = [&]() {};

        NewProjectBtn();
        ~NewProjectBtn() = default;

    private:
        ui::KickImageButton icon{ "", "Icon_NewRecent" };

        void resized() override;
        void paint( juce::Graphics& g ) override;
        void mouseDown( const juce::MouseEvent& e ) override { onClick(); };
    };

    class RecentItems : public juce::Component
        , public juce::Timer
    {
    public:
        std::function<void( int, juce::File )> onMouseDown; // activation: a tap (the Data Dial calls it too)
        std::function<void( int )>             onLongPressed;
        std::function<void()>                  clearAllShowBorderCallback;
        std::function<void( int )>             updateSelectedIndex;

        RecentItems();
        ~RecentItems() = default;

        void setProperties( int Index, const juce ::String& Path, bool factory = false );
        void showBorder( bool );

        inline bool isShowBorder() const noexcept { return bShowBorder; }
        inline int  getID() const noexcept { return index; }
        inline juce::File getPath() const { return path; }

        void mouseDown( const juce::MouseEvent& e ) override;
        void mouseUp( const juce::MouseEvent& e ) override;
        void mouseDrag( const juce::MouseEvent& e ) override;
        void timerCallback() override;

    private:
        bool bShowBorder{ false };
        bool missing{ false }; // the project folder is gone (removed SD card, deleted): shown, and a tap explains
        bool isLongPressTriggered{ false };
        bool isDragging{ false };
        int  longPressThreshold = 500; // Set the long press threshold (in milliseconds)
        int  dragThreshold      = 5;
        int  index{ -1 };

        ui::KickLabel label;
        ui::KickLabel date;
        ui::KickLabel timeStamp;
        ui::KickImage image;
        ui::KickImage icon{ "", "Icon_DefaultFile1" };
        juce::File    path;

        void resized() override;
        void paint( juce::Graphics& g ) override;
    };

    class RecentList
        : public KickViewport
    {
    public:
        explicit RecentList();
        ~RecentList() = default;

        std::vector<RecentItems*> listOptions;
        int                       selectedIndex{ -1 };
        juce::Component           listComp;

        void clear() noexcept;
        void addComp( RecentItems* ) noexcept;
        // Text drawn when the row has no tiles ("No recent projects yet ...").
        void setPlaceholder( const juce::String& text ) { placeholder = text; repaint(); }

        inline int getSize() const noexcept { return listComp.getNumChildComponents(); };

    private:
        juce::String placeholder;
        void         paint( juce::Graphics& g ) override;
        int numOptions{ 0 };
        int reversedRow{ 10 };

        juce::File getRecentProject( int Row );
        void       resized() override;
        void       visibilityChanged() override;
    };

    class RecentProject : public kick_ui::IScreen
        , public KickControlBase<juce::Component>
        , private mt::MTAudioInterfaceListener
    {
    public:
        std::function<void( juce::String, juce::File )> onKeyBoardEnd;
        std::function<void( void )>                     onAutoLoadClick;

        RecentProject();
        ~RecentProject() override { ae.removeListener(this); }
        bool       canClose() override { return true; }
        bool       requiresControlBar() override { return false; }
        bool       requiresStatusBar() override { return false; }
        bool       requiresHandleCommand() override { return true; } // the Data Dial / ENTER reach the tiles with a project open
        void       handleCommand( CPktMgr::HWMsg_t& ) override;
        void       knobClicked() override;
        void       knobRotated( double ) override;
        void       sliderMoved( float ) override;
        void       resized();
        void       removeFileFromVTree( int );
        void       createProject( juce::String value );
        int        getReversedRow( int );
        int        getNumOfChild();
        juce::File getRecentProject( int );
        void       setNumOptions( int NumOptions );

        static bool isProjectLoading;

    private:
        bool bFirstLaunch{ true };
        int  reversedRow, childNum, Rows = 10;
        int  recentProjectChildNum;
        int  maxNumOfRecentProject = 5;
        int  selectedIndex{ -1 };
        int  child;

        mt::MTAudioInterface& ae{ mt::getAudioInterface() };
        mt::KickSettings&     settings{ mt::getSettings() };

        kick_ui::KickModeHeader header{ "Projects", "RecentProject" };
        ui::KickNameLabel       projectName{ "", "Project Name" };
        ui::KickImageButton     fileBrowser{ "File Browser", "Browser_Recent" };
        ui::KickButton          lastProjectBtn{ "LastProject", "Last project" };
        kick_ui::StatusStrip    statusStrip;

        juce::String            seprator = juce::File::getSeparatorString();
        juce::Component         listComp;
        juce::Array<juce::File> demoDirList;
        juce::Array<juce::File> templateDirList;
        juce::File              projectLoadFile;

        RecentList                     demoList;
        RecentList                     templateList;
        RecentList                     projectList;
        NewProjectBtn                  newProj;
        std::unique_ptr<LongPressComp> longPressComp;

        std::vector<std::unique_ptr<RecentItems>> recentItems;
        void                                      warn( const juce::String& message ); // warning dialog
        void                                      openVirtualKeyBoard( juce::File );
        void                                      copyProjIntoProjectDir( juce::String, juce::File );
        void                                      addDemos();
        void                                      addTemplates();
        void                                      addProjectList( int index );
        void                                      clearAllShowBorder();
        void                                      loadProject( const juce::File& Project, LoadSource source = LoadSource::User, bool createNew = false );
        void                                      rebuildAll();
        void                                      showTileActions( int index );
        std::vector<LongPressComp::Action>        actionsFor( int index );
        void                                      renameProjectAt( const juce::File& dir );
        void                                      duplicateProjectAt( const juce::File& dir );
        void                                      deleteProjectAt( const juce::File& dir );
        void                                      showInfoOf( const juce::File& dir );
        void                                      openLaunchOptions();
        void                                      ensureVisible( int index );
        int                                       rowOf( int index ) const;     // 0 demos, 1 templates, 2 recent
        int                                       rowStart( int row ) const;
        void                                      loadLastProject();
        juce::File                                lastProjectFile() const; // empty when there is none (or it was moved/deleted)
        void                                      updateLastProjectButton();

        void visibilityChanged() override;
    };
}
