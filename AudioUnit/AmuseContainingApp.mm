#import <AppKit/AppKit.h>
#import <AudioUnit/AudioUnit.h>
#import <CoreAudioKit/AUViewController.h>
#import "AudioUnitViewController.hpp"
#import "AudioGroupFilePresenter.hpp"

@class DataOutlineController;
@class SamplesTableController;
@class SFXTableController;

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

@interface AppDelegate : NSObject <NSApplicationDelegate>
{
    IBOutlet NSWindow* mainWindow;
    IBOutlet NSOutlineView* dataOutline;
    IBOutlet NSTableView* sfxTable;
    IBOutlet NSTableView* samplesTable;
    IBOutlet NSTextView* creditsView;
    
    AudioGroupFilePresenter* groupFilePresenter;
    
    DataOutlineController* dataController;
    SamplesTableController* samplesController;
    SFXTableController* sfxController;
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

@interface DataOutlineView : NSOutlineView
{
    
}
@end

@implementation DataOutlineView

- (id)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    [self registerForDraggedTypes:@[(__bridge NSString*)kUTTypeData]];
    return self;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
    
}

@end

@interface DataOutlineController : NSObject <NSOutlineViewDataSource, NSOutlineViewDelegate>
{
    
}
@end

@implementation DataOutlineController



@end

@interface SamplesTableController : NSObject <NSTableViewDataSource, NSTableViewDelegate>
{
    
}
@end

@implementation SamplesTableController

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView
{
    
}

- (nullable id)tableView:(NSTableView *)tableView objectValueForTableColumn:(nullable NSTableColumn*)tableColumn row:(NSInteger)row
{
    
}

@end

@interface SFXTableController : NSObject <NSTableViewDataSource, NSTableViewDelegate>
{
    
}
@end

@implementation SFXTableController

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView
{
}

- (nullable id)tableView:(NSTableView *)tableView objectValueForTableColumn:(nullable NSTableColumn*)tableColumn row:(NSInteger)row
{
    
}

@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
    [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"NSConstraintBasedLayoutVisualizeMutuallyExclusiveConstraints"];
    [mainWindow.toolbar setSelectedItemIdentifier:@"DataTab"];
    
    groupFilePresenter = [AudioGroupFilePresenter new];
    
    dataController = [DataOutlineController new];
    dataOutline.dataSource = dataController;
    dataOutline.delegate = dataController;
    [dataOutline reloadItem:nil reloadChildren:YES];
    
    samplesController = [SamplesTableController new];
    samplesTable.dataSource = samplesController;
    samplesTable.delegate = samplesController;
    [samplesTable reloadData];
    
    sfxController = [SFXTableController new];
    sfxTable.dataSource = sfxController;
    sfxTable.delegate = sfxController;
    [sfxTable reloadData];
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
