// SPDX-License-Identifier: AGPL-3.0-or-later
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE || TARGET_OS_MAC

#import <AuthenticationServices/AuthenticationServices.h>
#if TARGET_OS_IPHONE
 #import <UIKit/UIKit.h>
 #import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#else
 #import <AppKit/AppKit.h>
#endif

#include <unordered_map>

#include <juce_events/juce_events.h>
#include "kick_appleCloudBridge.h"

#if TARGET_OS_IPHONE
namespace ui
{
    using IosPasteOverlayFn = void (*)( const void* ownerKey,
                                        void*       nativePeerView,
                                        float       x,
                                        float       y,
                                        float       w,
                                        float       h,
                                        bool        visible,
                                        std::function<void( const juce::String& )> onPaste );
    void setKickIosPasteOverlayFn( IosPasteOverlayFn fn );
}
#endif

#if JUCE_IOS || JUCE_MAC

namespace
{
#if __has_feature( objc_arc )
    #define KICK_RETAIN( x ) ( x )
    #define KICK_RELEASE( x ) ( (void)( x ) )
#else
    #define KICK_RETAIN( x ) [x retain]
    #define KICK_RELEASE( x ) [x release]
#endif
}

@interface KickWebAuthAnchor : NSObject <ASWebAuthenticationPresentationContextProviding>
@property( nonatomic, assign ) ASWebAuthenticationSession* activeSession;
@end

@implementation KickWebAuthAnchor
- (ASPresentationAnchor) presentationAnchorForWebAuthenticationSession: (ASWebAuthenticationSession*) session
{
    (void)session;
#if JUCE_IOS
    for( UIScene* scene in UIApplication.sharedApplication.connectedScenes )
    {
        if( scene.activationState == UISceneActivationStateForegroundActive && [scene isKindOfClass: UIWindowScene.class] )
        {
            UIWindowScene* ws = (UIWindowScene*)scene;
            for( UIWindow* w in ws.windows )
                if( w.isKeyWindow )
                    return w;
            if( UIWindow* fw = ws.windows.firstObject )
                return fw;
        }
    }
#if __has_feature( objc_arc )
    return [[UIWindow alloc] init];
#else
    return [[[UIWindow alloc] init] autorelease];
#endif
#else
    NSWindow* w = NSApp.keyWindow != nil ? NSApp.keyWindow : NSApp.mainWindow;
    if( w != nil )
        return w;
    if( NSWindow* fw = NSApp.windows.firstObject )
        return fw;
#if __has_feature( objc_arc )
    return [[NSWindow alloc] init];
#else
    return [[[NSWindow alloc] init] autorelease];
#endif
#endif
}
@end

#if JUCE_IOS
static UIViewController* kickTopViewController()
{
    UIWindow* keyWin = nil;
    for( UIScene* scene in UIApplication.sharedApplication.connectedScenes )
    {
        if( scene.activationState == UISceneActivationStateForegroundActive && [scene isKindOfClass: UIWindowScene.class] )
        {
            UIWindowScene* ws = (UIWindowScene*)scene;
            for( UIWindow* w in ws.windows )
                if( w.isKeyWindow ) { keyWin = w; break; }
            if( keyWin == nil && ws.windows.count > 0 )
                keyWin = ws.windows.firstObject;
            if( keyWin != nil )
                break;
        }
    }
    UIViewController* vc = keyWin.rootViewController;
    while( vc.presentedViewController != nil && !vc.presentedViewController.isBeingDismissed )
        vc = vc.presentedViewController;
    return vc;
}

@interface KickDocPickerDelegate : NSObject <UIDocumentPickerDelegate>
@property( nonatomic, copy ) void ( ^callback )( juce::StringArray, juce::String );
@end

@implementation KickDocPickerDelegate
#if ! __has_feature( objc_arc )
- (void) dealloc
{
    [_callback release];
    [super dealloc];
}
#endif

- (void) documentPicker: (UIDocumentPickerViewController*) controller didPickDocumentsAtURLs: (NSArray<NSURL*>*) urls
{
    (void)controller;
    juce::StringArray staged;
    NSFileManager*    fm     = [NSFileManager defaultManager];
    NSString*         stageD = [NSTemporaryDirectory() stringByAppendingPathComponent: [NSString stringWithFormat: @"kick_picked_%lld", (long long)( [[NSDate date] timeIntervalSince1970] * 1000.0 )]];
    [fm createDirectoryAtPath: stageD withIntermediateDirectories: YES attributes: nil error: nil];

    for( NSURL* url in urls )
    {
        const BOOL scoped = [url startAccessingSecurityScopedResource];
        NSString*  rawName = url.lastPathComponent;
        NSString*  name    = rawName != nil ? rawName : @"sample.wav";
        NSString*  dstP   = [stageD stringByAppendingPathComponent: name];
        NSURL*     dstU   = [NSURL fileURLWithPath: dstP];
        NSError*   err    = nil;
        [fm removeItemAtURL: dstU error: nil];
        if( [fm copyItemAtURL: url toURL: dstU error: &err] )
            staged.add( juce::String::fromUTF8( dstP.UTF8String ) );
        if( scoped )
            [url stopAccessingSecurityScopedResource];
    }

    auto cb = self.callback;
    if( cb != nil )
        cb( staged, staged.isEmpty() ? juce::String( "No readable files copied" ) : juce::String() );
    KICK_RELEASE( self );
}

- (void) documentPickerWasCancelled: (UIDocumentPickerViewController*) controller
{
    (void)controller;
    auto cb = self.callback;
    if( cb != nil )
        cb( juce::StringArray(), juce::String( "Cancelled" ) );
    KICK_RELEASE( self );
}
@end
#endif

namespace kick_apple_cloud
{
    void startWebAuthSession( const juce::String& authUrl,
                              const juce::String& callbackScheme,
                              std::function<void( bool ok, juce::String callbackUrlOrError )> onDone )
    {
        dispatch_async( dispatch_get_main_queue(), ^{
            NSString* nsUrlStr = [NSString stringWithUTF8String: authUrl.toRawUTF8()];
            NSURL*    nsUrl    = nsUrlStr != nil ? [NSURL URLWithString: nsUrlStr] : nil;
            NSString* nsScheme = callbackScheme.isNotEmpty() ? [NSString stringWithUTF8String: callbackScheme.toRawUTF8()] : nil;
            if( nsUrl == nil )
            {
                if( onDone )
                    juce::MessageManager::callAsync( [onDone]() { onDone( false, "Invalid authorization URL" ); } );
                return;
            }

            KickWebAuthAnchor* anchor = [[KickWebAuthAnchor alloc] init];
            __block ASWebAuthenticationSession* session = nil;
            session = [[ASWebAuthenticationSession alloc]
                initWithURL: nsUrl
                callbackURLScheme: nsScheme
                completionHandler: ^( NSURL* _Nullable callbackURL, NSError* _Nullable error ) {
                    const bool   ok  = ( callbackURL != nil && error == nil );
                    juce::String res = ok
                                           ? juce::String::fromUTF8( callbackURL.absoluteString.UTF8String )
                                           : juce::String::fromUTF8( error ? error.localizedDescription.UTF8String : "Sign-in cancelled" );
                    dispatch_async( dispatch_get_main_queue(), ^{
                        KICK_RELEASE( session );
                        KICK_RELEASE( anchor );
                        if( onDone )
                            juce::MessageManager::callAsync( [onDone, ok, res]() { onDone( ok, res ); } );
                    } );
                }];
            anchor.activeSession                  = session;
            session.presentationContextProvider   = anchor;
            session.prefersEphemeralWebBrowserSession = NO;
            if( ![session start] )
            {
                KICK_RELEASE( session );
                KICK_RELEASE( anchor );
                if( onDone )
                    juce::MessageManager::callAsync( [onDone]() { onDone( false, "Could not start sign-in session" ); } );
            }
        } );
    }

    void pickFilesFromIosDocumentProvider( std::function<void( juce::StringArray stagedPaths, juce::String error )> onDone )
    {
#if JUCE_IOS
        dispatch_async( dispatch_get_main_queue(), ^{
            UIViewController* top = kickTopViewController();
            if( top == nil )
            {
                if( onDone )
                    juce::MessageManager::callAsync( [onDone]() { onDone( {}, "No active iOS window" ); } );
                return;
            }
            NSArray<UTType*>* types = @[ UTTypeAudio, UTTypeMIDI, UTTypeData ];
            UIDocumentPickerViewController* picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes: types asCopy: YES];
            picker.allowsMultipleSelection         = YES;

            KickDocPickerDelegate* del = [[KickDocPickerDelegate alloc] init];
            del.callback               = ^( juce::StringArray files, juce::String err ) {
                if( onDone )
                    juce::MessageManager::callAsync( [onDone, files, err]() { onDone( files, err ); } );
            };
            picker.delegate = del;
            [top presentViewController: picker animated: YES completion: nil];
            KICK_RELEASE( picker );
        } );
#else
        if( onDone )
            onDone( {}, "Document picker is only available on iOS" );
#endif
    }
}

#if JUCE_IOS
API_AVAILABLE( ios( 16.0 ) )
@interface KickPasteTargetView : UIView
@property( nonatomic, retain ) UIPasteControl* pasteControl;
- (void) setPasteCallback: (std::function<void( const juce::String& )>) cb;
@end

@implementation KickPasteTargetView
{
    std::function<void( const juce::String& )> _onPaste;
}

- (void) setPasteCallback: (std::function<void( const juce::String& )>) cb
{
    _onPaste = std::move( cb );
}

- (instancetype) initWithFrame: (CGRect) frame
{
    self = [super initWithFrame: frame];
    if( self != nil )
    {
        self.backgroundColor = [UIColor clearColor];

        UIPasteConfiguration* pc = [[UIPasteConfiguration alloc] initWithTypeIdentifiersForAcceptingClass: [NSString class]];
        [pc addTypeIdentifiersForAcceptingClass: [NSURL class]];
        self.pasteConfiguration = pc;
        KICK_RELEASE( pc );

        UIPasteControlConfiguration* cfg = [[UIPasteControlConfiguration alloc] init];
        cfg.displayMode                  = UIPasteControlDisplayModeLabelOnly;
        cfg.cornerStyle                  = UIButtonConfigurationCornerStyleFixed;
        cfg.cornerRadius                 = 4.0f;
        cfg.baseForegroundColor          = [UIColor whiteColor];
        cfg.baseBackgroundColor          = [UIColor colorWithRed: 0.17f green: 0.18f blue: 0.21f alpha: 1.0f];

        UIPasteControl* ctrl = [[UIPasteControl alloc] initWithConfiguration: cfg];
        KICK_RELEASE( cfg );
        ctrl.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        ctrl.frame            = self.bounds;
        ctrl.target           = self;
        [self addSubview: ctrl];
        self.pasteControl = ctrl;
        KICK_RELEASE( ctrl );
    }
    return self;
}

#if !__has_feature( objc_arc )
- (void) dealloc
{
    [_pasteControl release];
    _pasteControl = nil;
    [super dealloc];
}
#endif

- (BOOL) canPasteItemProviders: (NSArray<NSItemProvider*>*) itemProviders
{
    for( NSItemProvider* provider in itemProviders )
    {
        if( [provider canLoadObjectOfClass: [NSString class]] || [provider canLoadObjectOfClass: [NSURL class]] )
            return YES;
    }
    return NO;
}

- (void) pasteItemProviders: (NSArray<NSItemProvider*>*) itemProviders
{
    auto cb = _onPaste;
    if( !cb )
        return;

    for( NSItemProvider* provider in itemProviders )
    {
        if( [provider canLoadObjectOfClass: [NSString class]] )
        {
            [provider loadObjectOfClass: [NSString class]
                      completionHandler: ^( id<NSItemProviderReading> object, NSError* error ) {
                (void)error;
                if( [object isKindOfClass: [NSString class]] )
                {
                    NSString*    ns   = (NSString*)object;
                    juce::String text = ( ns != nil && ns.UTF8String != nullptr )
                                            ? juce::String::fromUTF8( ns.UTF8String )
                                            : juce::String();
                    if( text.isNotEmpty() )
                        juce::MessageManager::callAsync( [cb, text]() { cb( text ); } );
                }
            }];
            return;
        }
        if( [provider canLoadObjectOfClass: [NSURL class]] )
        {
            [provider loadObjectOfClass: [NSURL class]
                      completionHandler: ^( id<NSItemProviderReading> object, NSError* error ) {
                (void)error;
                if( [object isKindOfClass: [NSURL class]] )
                {
                    NSString*    ns   = [(NSURL*)object absoluteString];
                    juce::String text = ( ns != nil && ns.UTF8String != nullptr )
                                            ? juce::String::fromUTF8( ns.UTF8String )
                                            : juce::String();
                    if( text.isNotEmpty() )
                        juce::MessageManager::callAsync( [cb, text]() { cb( text ); } );
                }
            }];
            return;
        }
    }
}
@end

namespace
{
    std::unordered_map<const void*, KickPasteTargetView*>& kickPasteOverlayMap()
    {
        static std::unordered_map<const void*, KickPasteTargetView*> m;
        return m;
    }

    void updateKickIosPasteOverlay( const void* ownerKey,
                                    void*       nativePeerView,
                                    float       x,
                                    float       y,
                                    float       w,
                                    float       h,
                                    bool        visible,
                                    std::function<void( const juce::String& )> onPaste )
    {
        if( @available( iOS 16.0, * ) )
        {
            if( ownerKey == nullptr )
                return;

            auto& map = kickPasteOverlayMap();
            if( !visible || nativePeerView == nullptr || w < 4.0f || h < 4.0f )
            {
                if( auto it = map.find( ownerKey ); it != map.end() )
                {
                    KickPasteTargetView* existing = it->second;
                    [existing setHidden: YES];
                    [existing removeFromSuperview];
                    if( nativePeerView == nullptr && !onPaste )
                    {
                        KICK_RELEASE( existing );
                        map.erase( it );
                    }
                }
                return;
            }

            KickPasteTargetView* view = nil;
            if( auto it = map.find( ownerKey ); it != map.end() )
            {
                view = it->second;
            }
            else
            {
                view = [[KickPasteTargetView alloc] initWithFrame: CGRectMake( x, y, w, h )];
                map.emplace( ownerKey, view );
            }

            [view setPasteCallback: std::move( onPaste )];
            [view setFrame: CGRectMake( x, y, w, h )];
            [view setHidden: NO];

            UIView* peerView = (UIView*)nativePeerView;
            if( view.superview != peerView )
            {
                [view removeFromSuperview];
                [peerView addSubview: view];
            }
            [peerView bringSubviewToFront: view];
        }
    }

    struct KickPasteOverlayRegistrar
    {
        KickPasteOverlayRegistrar()
        {
            ui::setKickIosPasteOverlayFn( &updateKickIosPasteOverlay );
        }
    } s_kickPasteOverlayRegistrar;
} // namespace
#endif

#endif
#endif
