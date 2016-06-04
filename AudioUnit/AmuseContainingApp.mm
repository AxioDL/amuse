#import <AppKit/AppKit.h>
#import <AudioUnit/AudioUnit.h>
#import <CoreAudioKit/AUViewController.h>
#import "AudioUnitViewController.hpp"
#import "AmuseContainingApp.hpp"
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


@implementation DataOutlineView
- (id)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    [self registerForDraggedTypes:@[(__bridge NSString*)kUTTypeData]];
    return self;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
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


@implementation SamplesTableController

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView
{
    
}

- (nullable id)tableView:(NSTableView *)tableView objectValueForTableColumn:(nullable NSTableColumn*)tableColumn row:(NSInteger)row
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

- (IBAction)filterDataOutline:(id)sender
{
    [groupFilePresenter setSearchFilter:[sender stringValue]];
}

- (IBAction)removeDataItem:(id)sender
{
    [groupFilePresenter removeSelectedItem];
}

- (void)outlineView:(DataOutlineView *)ov selectionChanged:(id)item
{
    if ([item isKindOfClass:[AudioGroupCollectionToken class]])
    {
        removeDataButton.enabled = TRUE;
        removeDataMenu.enabled = TRUE;
    }
    else
    {
        removeDataButton.enabled = FALSE;
        removeDataMenu.enabled = FALSE;
    }
}

@end

int main(int argc, const char * argv[])
{
    return NSApplicationMain(argc, argv);
}
