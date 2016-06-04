#import <AppKit/AppKit.h>
#import <AudioUnit/AudioUnit.h>
#import <CoreAudioKit/AUViewController.h>
#import "AudioUnitViewController.hpp"
#import "AudioGroupFilePresenter.hpp"
#include <amuse/amuse.hpp>

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
    IBOutlet NSSearchField* dataSearchField;
    IBOutlet NSTableView* sfxTable;
    IBOutlet NSTableView* samplesTable;
    IBOutlet NSTextView* creditsView;
    
    AudioGroupFilePresenter* groupFilePresenter;
    
    SamplesTableController* samplesController;
    SFXTableController* sfxController;
}
- (BOOL)importURL:(NSURL*)url;
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
    
    dataOutline.dataSource = groupFilePresenter;
    dataOutline.delegate = groupFilePresenter;
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

- (BOOL)importURL:(NSURL*)url
{
    amuse::ContainerRegistry::Type containerType;
    std::vector<std::pair<std::string, amuse::IntrusiveAudioGroupData>> data =
        amuse::ContainerRegistry::LoadContainer(url.path.UTF8String, containerType);
    if (data.empty())
    {
        NSString* err = [NSString stringWithFormat:@"Unable to load Audio Groups from %s", url.path.UTF8String];
        NSAlert* alert = [[NSAlert alloc] init];
        alert.informativeText = err;
        alert.messageText = @"Invalid Data File";
        [alert runModal];
        return false;
    }
    
    std::string name(amuse::ContainerRegistry::TypeToName(containerType));
    return [groupFilePresenter addCollectionName:std::move(name) items:std::move(data)];
}

- (IBAction)importFile:(id)sender
{
    __block NSOpenPanel* panel = [NSOpenPanel openPanel];
    [panel beginSheetModalForWindow:mainWindow completionHandler:^(NSInteger result) {
        if (result == NSFileHandlingPanelOKButton)
        {
            [self importURL:panel.URL];
        }
    }];
}

@end

int main(int argc, const char * argv[])
{
    return NSApplicationMain(argc, argv);
}
