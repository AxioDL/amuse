#import "AudioUnitViewController.hpp"
#import "AudioUnitBackend.hpp"

#if !__has_feature(objc_arc)
#error ARC Required
#endif

@implementation GroupBrowserDelegate

- (BOOL)browser:(NSBrowser *)sender isColumnValid:(NSInteger)column
{
    if (column == 0)
        return YES;
    else if (column == 1)
    {
        AudioGroupCollectionToken* collection = [sender selectedCellInColumn:0];
        if (collection)
            return YES;
    }
    else if (column == 2)
    {
        AudioGroupDataToken* groupFile = [sender selectedCellInColumn:1];
        if (groupFile)
            return YES;
    }
    return NO;
}

- (NSInteger)browser:(NSBrowser *)sender numberOfRowsInColumn:(NSInteger)column
{
    if (column == 0)
        return m_audioUnit->m_filePresenter->m_filterAudioGroupCollections.size();
    else if (column == 1)
    {
        AudioGroupCollectionToken* collection = [sender selectedCellInColumn:0];
        if (!collection)
            return 0;
        return collection->m_collection->m_filterGroups.size();
    }
    else if (column == 2)
    {
        AudioGroupDataToken* groupFile = [sender selectedCellInColumn:1];
        if (!groupFile)
            return 0;
        const amuse::AudioGroup* audioGroupFile = groupFile->m_collection->m_loadedGroup;
        return audioGroupFile->getProj().songGroups().size() + audioGroupFile->getProj().sfxGroups().size();
    }
    return 0;
}

- (NSInteger)browser:(NSBrowser *)browser numberOfChildrenOfItem:(id)item
{
    if (!item)
        return m_audioUnit->m_filePresenter->m_filterAudioGroupCollections.size();
    else if ([item isKindOfClass:[AudioGroupCollectionToken class]])
    {
        AudioGroupCollectionToken* collection = item;
        return collection->m_collection->m_filterGroups.size();
    }
    else if ([item isKindOfClass:[AudioGroupDataToken class]])
    {
        AudioGroupDataToken* groupFile = item;
        const amuse::AudioGroup* audioGroupFile = groupFile->m_collection->m_loadedGroup;
        return audioGroupFile->getProj().songGroups().size() + audioGroupFile->getProj().sfxGroups().size();
    }
    else
        return 0;
}

- (NSString *)browser:(NSBrowser *)sender titleOfColumn:(NSInteger)column
{
    if (column == 0)
        return @"Collection";
    else if (column == 1)
        return @"File";
    else if (column == 2)
        return @"Group";
    return nil;
}

- (id)browser:(NSBrowser *)browser child:(NSInteger)index ofItem:(id)item
{
    if (!item)
        return m_audioUnit->m_filePresenter->m_filterAudioGroupCollections[index]->second->m_token;
    else if ([item isKindOfClass:[AudioGroupCollectionToken class]])
    {
        AudioGroupCollectionToken* collection = item;
        return collection->m_collection->m_filterGroups[index]->second->m_token;
    }
    else if ([item isKindOfClass:[AudioGroupDataToken class]])
    {
        AudioGroupDataToken* groupFile = item;
        return groupFile->m_collection->m_groupTokens[index];
    }
    else
        return 0;
}

- (BOOL)browser:(NSBrowser *)browser isLeafItem:(id)item
{
    if ([item isKindOfClass:[AudioGroupToken class]])
        return YES;
    return NO;
}

- (BOOL)browser:(NSBrowser *)browser shouldEditItem:(id)item
{
    return NO;
}

- (id)browser:(NSBrowser *)browser objectValueForItem:(id)item
{
    if ([item isKindOfClass:[AudioGroupCollectionToken class]])
    {
        AudioGroupCollectionToken* collection = item;
        return collection->m_collection->m_url.lastPathComponent;
    }
    else if ([item isKindOfClass:[AudioGroupDataToken class]])
    {
        AudioGroupDataToken* groupFile = item;
        return @(groupFile->m_collection->m_name.c_str());
    }
    else if ([item isKindOfClass:[AudioGroupToken class]])
    {
        AudioGroupToken* group = item;
        return group->m_name;
    }
    return nil;
}

- (NSIndexSet *)browser:(NSBrowser *)browser selectionIndexesForProposedSelection:(NSIndexSet *)proposedSelectionIndexes inColumn:(NSInteger)column
{
    if (column == 2)
    {
        AudioGroupToken* token = [browser itemAtRow:proposedSelectionIndexes.firstIndex inColumn:column];
        [m_audioUnit requestAudioGroup:token];
    }
    return proposedSelectionIndexes;
}

- (id)initWithAudioUnit:(AmuseAudioUnit*)au
{
    self = [super init];
    m_audioUnit = au;
    return self;
}

@end

@implementation AudioUnitViewController

- (void) viewDidLoad {
    [super viewDidLoad];
    
    self.preferredContentSize = NSMakeSize(510, 312);
    // Get the parameter tree and add observers for any parameters that the UI needs to keep in sync with the AudioUnit
}

- (AUAudioUnit*)createAudioUnitWithComponentDescription:(AudioComponentDescription)desc error:(NSError**)error {
    m_audioUnit = [[AmuseAudioUnit alloc] initWithComponentDescription:desc error:error viewController:self];
    m_groupBrowserDelegate = [[GroupBrowserDelegate alloc] initWithAudioUnit:m_audioUnit];
    dispatch_sync(dispatch_get_main_queue(), ^
    {
        m_groupBrowser.delegate = m_groupBrowserDelegate;
        [m_groupBrowser loadColumnZero];
    });
    return m_audioUnit;
}

@end
