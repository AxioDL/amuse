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

@interface MainTabView : NSTabView
{}
- (IBAction)selectDataTab:(id)sender;
- (IBAction)selectSFXTab:(id)sender;
- (IBAction)selectSamplesTab:(id)sender;
- (IBAction)selectCreditsTab:(id)sender;
@end

@implementation MainTabView
- (IBAction)selectDataTab:(id)sender
{
    [self selectTabViewItemAtIndex:0];
}
- (IBAction)selectSFXTab:(id)sender
{
    [self selectTabViewItemAtIndex:1];
}
- (IBAction)selectSamplesTab:(id)sender
{
    [self selectTabViewItemAtIndex:2];
}
- (IBAction)selectCreditsTab:(id)sender
{
    [self selectTabViewItemAtIndex:3];
}
@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
{
    IBOutlet NSWindow* mainWindow;
    IBOutlet NSOutlineView* dataOutline;
    IBOutlet NSTableView* sfxTable;
    IBOutlet NSTableView* samplesTable;
    IBOutlet NSTextView* creditsView;
}
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
    [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"NSConstraintBasedLayoutVisualizeMutuallyExclusiveConstraints"];
    [mainWindow.toolbar setSelectedItemIdentifier:@"DataTab"];
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
    return NSApplicationMain(argc, argv);
}
