#import <AppKit/AppKit.h>
#import <AudioUnit/AudioUnit.h>
#import <CoreAudioKit/AUViewController.h>
#import "AudioUnitViewController.hpp"

@interface MainView : NSView
{
    AudioUnitViewController* amuseVC;
}
@end

@implementation MainView


- (id)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (!self)
        return nil;
    amuseVC = [[AudioUnitViewController alloc] initWithNibName:nil bundle:nil];
    [self addSubview:amuseVC.view];
    return self;
}

- (BOOL)translatesAutoresizingMaskIntoConstraints
{
    return NO;
}

@end

@interface ViewController : NSViewController
{
    NSWindow* _window;
    NSButton* playButton;
}
@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
{
    NSWindow* mainWindow;
    ViewController* containerVC;
}
@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
#if 0
    AudioComponentDescription desc;
    /*  Supply the correct AudioComponentDescription based on your AudioUnit type, manufacturer and creator.

     You need to supply matching settings in the AUAppExtension info.plist under:

     NSExtension
     NSExtensionAttributes
     AudioComponents
     Item 0
     type
     subtype
     manufacturer

     If you do not do this step, your AudioUnit will not work!!!
     */
    desc.componentType = kAudioUnitType_MusicDevice;
    desc.componentSubType = 'sin3';
    desc.componentManufacturer = 'Demo';
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    [AUAudioUnit registerSubclass: AUv3InstrumentDemo.class asComponentDescription:desc name:@"Local InstrumentDemo" version: UINT32_MAX];

    playEngine = [[SimplePlayEngine alloc] initWithComponentType: desc.componentType componentsFoundCallback: nil];
    [playEngine selectAudioUnitWithComponentDescription2:desc completionHandler:^{
        [self connectParametersToControls];
    }];
#endif
}

-(void)loadView {
    self.view = [[MainView alloc] initWithFrame:[_window contentRectForFrameRect:_window.frame]];
}

- (id)initWithWindow:(NSWindow*)window
{
    self = [super initWithNibName:@"AmuseContainerMainMenu" bundle:nil];
    if (!self)
        return nil;
    _window = window;
    return self;
}

@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
    [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"NSConstraintBasedLayoutVisualizeMutuallyExclusiveConstraints"];
    
    /* App menu */
#if 0
    [NSMenu alloc] ini
    NSMenu* rootMenu = [[NSMenu alloc] initWithTitle:@"main"];
    NSMenu* appMenu = [[NSMenu alloc] initWithTitle:@"Amuse"];
    NSMenuItem* quitItem = [appMenu addItemWithTitle:@"Quit Amuse"
                                              action:@selector(quitApp:)
                                       keyEquivalent:@"q"];
    [quitItem setKeyEquivalentModifierMask:NSCommandKeyMask];
    [[rootMenu addItemWithTitle:@"Amuse"
                         action:nil keyEquivalent:@""] setSubmenu:appMenu];
    [[NSApplication sharedApplication] setMainMenu:rootMenu];
    
    NSRect mainScreenRect = [[NSScreen mainScreen] frame];
    mainWindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(mainScreenRect.size.width / 2 - 100,
                                                                  mainScreenRect.size.height / 2 - 150, 200, 300)
                                             styleMask:NSClosableWindowMask|NSTitledWindowMask
                                               backing:NSBackingStoreBuffered
                                                 defer:YES];
    [mainWindow setTitle:@"Amuse"];
    [[mainWindow windowController] setShouldCascadeWindows:NO];
    [mainWindow setFrameAutosaveName:@"AmuseDataWindow"];
    containerVC = [[ViewController alloc] initWithWindow:mainWindow];
    [mainWindow setContentViewController:containerVC];
    [mainWindow makeKeyAndOrderFront:nil];
#endif
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    return YES;
}

- (IBAction)quitApp:(id)sender
{
    [NSApp terminate:sender];
}

@end

int main(int argc, const char * argv[])
{
    NSApplicationMain(argc, argv);
}
