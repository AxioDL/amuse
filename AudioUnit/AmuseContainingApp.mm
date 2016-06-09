#import <AppKit/AppKit.h>
#import <AudioUnit/AudioUnit.h>
#import <CoreAudioKit/AUViewController.h>
#import "AudioUnitViewController.hpp"
#import "AmuseContainingApp.hpp"
#include <amuse/amuse.hpp>

@class DataOutlineController;
@class SamplesTableController;
@class SFXTableController;

/* Blocks mousedown events (so button may be used as a visual element only) */
@interface InactiveButton : NSButton {}
@end
@implementation InactiveButton
- (void)mouseDown:(NSEvent *)theEvent {}
@end

/* Restricts mousedown to checkbox */
@interface RestrictedCheckButton : NSButtonCell {}
@end
@implementation RestrictedCheckButton
- (NSCellHitResult)hitTestForEvent:(NSEvent *)event inRect:(NSRect)cellFrame ofView:(NSView *)controlView
{
    NSRect restrictFrame = cellFrame;
    restrictFrame.size.width = 22;
    if (NSPointInRect([controlView convertPoint:[event locationInWindow] fromView:nil], restrictFrame))
        return NSCellHitTrackableArea;
    return NSCellHitNone;
}
@end

@implementation DataOutlineView
- (id)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    [self registerForDraggedTypes:@[NSURLPboardType]];
    return self;
}
- (id)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    [self registerForDraggedTypes:@[NSURLPboardType]];
    return self;
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
    return presenter->m_sampleTableData.size();
}

- (NSView*)tableView:(NSTableView *)tableView viewForTableColumn:(nullable NSTableColumn *)tableColumn row:(NSInteger)row
{
    if (presenter->m_sampleTableData.size() <= row)
        return nil;
    NSTableCellView* view = [tableView makeViewWithIdentifier:@"SampleIDColumn" owner:self];
    AudioGroupSampleToken* sampToken = presenter->m_sampleTableData[row];
    if ([tableColumn.identifier isEqualToString:@"SampleIDColumn"])
        view.textField.attributedStringValue = sampToken->m_name;
    else if ([tableColumn.identifier isEqualToString:@"SampleDetailsColumn"])
        view.textField.stringValue = @"";
    else
        view.textField.attributedStringValue = sampToken->m_name;
    return view;
}

- (BOOL)tableView:(NSTableView *)tableView isGroupRow:(NSInteger)row
{
    if (presenter->m_sampleTableData.size() <= row)
        return NO;
    AudioGroupSampleToken* sampToken = presenter->m_sampleTableData[row];
    if (!sampToken->m_sample)
        return YES;
    return NO;
}

- (BOOL)tableView:(NSTableView *)tableView shouldSelectRow:(NSInteger)row
{
    if (presenter->m_sampleTableData.size() <= row)
        return NO;
    AudioGroupSampleToken* sampToken = presenter->m_sampleTableData[row];
    if (!sampToken->m_sample)
        return NO;
    return YES;
}

- (id)initWithAudioGroupPresenter:(AudioGroupFilePresenter*)present
{
    self = [super init];
    presenter = present;
    return self;
}

@end


@implementation SFXTableController

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView
{
    return presenter->m_sfxTableData.size();
}

- (NSView*)tableView:(NSTableView *)tableView viewForTableColumn:(nullable NSTableColumn *)tableColumn row:(NSInteger)row
{
    if (presenter->m_sfxTableData.size() <= row)
        return nil;
    NSTableCellView* view = [tableView makeViewWithIdentifier:@"SFXIDColumn" owner:self];
    AudioGroupSFXToken* sfxToken = presenter->m_sfxTableData[row];
    if ([tableColumn.identifier isEqualToString:@"SFXIDColumn"])
        view.textField.attributedStringValue = sfxToken->m_name;
    else if ([tableColumn.identifier isEqualToString:@"SFXDetailsColumn"])
        view.textField.stringValue = @"";
    else
        view.textField.attributedStringValue = sfxToken->m_name;
    return view;
}

- (BOOL)tableView:(NSTableView *)tableView isGroupRow:(NSInteger)row
{
    if (presenter->m_sfxTableData.size() <= row)
        return NO;
    AudioGroupSFXToken* sfxToken = presenter->m_sfxTableData[row];
    if (!sfxToken->m_sfx)
        return YES;
    return NO;
}

- (BOOL)tableView:(NSTableView *)tableView shouldSelectRow:(NSInteger)row
{
    if (presenter->m_sfxTableData.size() <= row)
        return NO;
    AudioGroupSFXToken* sfxToken = presenter->m_sfxTableData[row];
    if (!sfxToken->m_sfx)
        return NO;
    return YES;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    NSTableView* table = notification.object;
    NSInteger row = table.selectedRow;
    if (presenter->m_sfxTableData.size() <= row)
        return;
    AudioGroupSFXToken* sfxToken = presenter->m_sfxTableData[row];
    AppDelegate* delegate = (AppDelegate*)NSApp.delegate;
    [delegate startSFX:sfxToken->m_loadId];
}

- (id)initWithAudioGroupPresenter:(AudioGroupFilePresenter*)present
{
    self = [super init];
    presenter = present;
    return self;
}

@end

@implementation AppDelegate

- (void)applicationWillFinishLaunching:(NSNotification*)notification
{
    booEngine = boo::NewAudioVoiceEngine();
    amuseAllocator.emplace(*booEngine);
    amuseEngine.emplace(*amuseAllocator);
    
    [mainWindow.toolbar setSelectedItemIdentifier:@"DataTab"];
    
    groupFilePresenter = [[AudioGroupFilePresenter alloc] initWithAudioGroupClient:self];
    
    dataOutline.dataSource = groupFilePresenter;
    dataOutline.delegate = groupFilePresenter;
    [dataOutline reloadItem:nil reloadChildren:YES];
    
    samplesController = [[SamplesTableController alloc] initWithAudioGroupPresenter:groupFilePresenter];
    samplesTable.dataSource = samplesController;
    samplesTable.delegate = samplesController;
    [samplesTable reloadData];
    
    sfxController = [[SFXTableController alloc] initWithAudioGroupPresenter:groupFilePresenter];
    sfxTable.dataSource = sfxController;
    sfxTable.delegate = sfxController;
    [sfxTable reloadData];
    
    [NSTimer scheduledTimerWithTimeInterval:1.0 / 60.0 target:self selector:@selector(pumpTimer:) userInfo:nil repeats:YES];
}

- (void)pumpTimer:(NSTimer*)timer
{
    amuseEngine->pumpEngine();
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
    if (containerType == amuse::ContainerRegistry::Type::Raw4)
        name = url.URLByDeletingPathExtension.lastPathComponent.UTF8String;
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

- (void)startSFX:(int)sfxId
{
    if (activeSFXVox)
        activeSFXVox->keyOff();
    activeSFXVox = amuseEngine->fxStart(sfxId, 1.f, 0.f);
}

- (void)startSample:(int)sampleId
{
}

- (void)reloadTables
{
    [sfxTable reloadData];
    [samplesTable reloadData];
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

- (BOOL)application:(NSApplication *)sender openFile:(NSString *)filename
{
    NSURL* url = [NSURL fileURLWithPath:filename isDirectory:NO];
    return [self importURL:url];
}

- (amuse::Engine&)getAmuseEngine
{
    return *amuseEngine;
}

@end

int main(int argc, const char * argv[])
{
    return NSApplicationMain(argc, argv);
}
