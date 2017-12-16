#include "AudioGroupFilePresenter.hpp"
#include <athena/FileReader.hpp>
#include <amuse/AudioGroupProject.hpp>
#import "AmuseContainingApp.hpp"
#import "AudioUnitBackend.hpp"
#import "AudioUnitViewController.hpp"

static std::string StrToLower(const std::string& str)
{
    std::string ret = str;
    std::transform(ret.begin(), ret.end(), ret.begin(), tolower);
    return ret;
}

@implementation AudioGroupDataToken
- (id)initWithDataCollection:(AudioGroupDataCollection *)collection
{
    self = [super init];
    m_collection = collection;
    return self;
}
@end

@implementation AudioGroupCollectionToken
- (id)initWithCollection:(AudioGroupCollection *)collection
{
    self = [super init];
    m_collection = collection;
    return self;
}
@end

@implementation AudioGroupSFXToken
- (id)initWithName:(NSAttributedString*)name loadId:(int)loadId sfx:(const amuse::SFXGroupIndex::SFXEntry*)sfx
{
    self = [super init];
    m_name = name;
    m_loadId = loadId;
    m_sfx = sfx;
    return self;
}
@end

@implementation AudioGroupSampleToken
- (id)initWithName:(NSAttributedString*)name samp:(const std::pair<amuse::AudioGroupSampleDirectory::Entry,
                                                   amuse::AudioGroupSampleDirectory::ADPCMParms>*)sample
{
    self = [super init];
    m_name = name;
    m_sample = sample;
    return self;
}
@end

@implementation AudioGroupToken
- (id)initWithName:(NSString*)name id:(int)gid songGroup:(const amuse::SongGroupIndex*)group
{
    self = [super init];
    m_name = name;
    m_song = group;
    m_id = gid;
    return self;
}
- (id)initWithName:(NSString*)name id:(int)gid sfxGroup:(const amuse::SFXGroupIndex*)group
{
    self = [super init];
    m_name = name;
    m_sfx = group;
    m_id = gid;
    return self;
}
@end

@implementation AudioGroupFilePresenter

- (NSURL*)presentedItemURL
{
    return m_groupURL;
}

- (NSOperationQueue*)presentedItemOperationQueue
{
    return [NSOperationQueue mainQueue];
}

AudioGroupCollection::AudioGroupCollection(NSURL* url)
: m_url(url), m_token([[AudioGroupCollectionToken alloc] initWithCollection:this]) {}

void AudioGroupCollection::addCollection(AudioGroupFilePresenter* presenter,
                                         std::vector<std::pair<std::string, amuse::IntrusiveAudioGroupData>>&& collection)
{
    for (std::pair<std::string, amuse::IntrusiveAudioGroupData>& pair : collection)
    {
        NSURL* collectionUrl = [m_url URLByAppendingPathComponent:@(pair.first.c_str())];
        
        amuse::IntrusiveAudioGroupData& dataIn = pair.second;
        auto search = m_groups.find(pair.first);
        if (search == m_groups.end())
        {
            search = m_groups.emplace(pair.first,
                                      std::make_unique<AudioGroupDataCollection>(pair.first,
                                          [collectionUrl URLByAppendingPathComponent:@"proj"],
                                          [collectionUrl URLByAppendingPathComponent:@"pool"],
                                          [collectionUrl URLByAppendingPathComponent:@"sdir"],
                                          [collectionUrl URLByAppendingPathComponent:@"samp"],
                                          [collectionUrl URLByAppendingPathComponent:@"meta"])).first;
        }
        
        AudioGroupDataCollection& dataCollection = *search->second;
        dataCollection.m_projData.resize(dataIn.getProjSize());
        memmove(dataCollection.m_projData.data(), dataIn.getProj(), dataIn.getProjSize());
        
        dataCollection.m_poolData.resize(dataIn.getPoolSize());
        memmove(dataCollection.m_poolData.data(), dataIn.getPool(), dataIn.getPoolSize());
        
        dataCollection.m_sdirData.resize(dataIn.getSdirSize());
        memmove(dataCollection.m_sdirData.data(), dataIn.getSdir(), dataIn.getSdirSize());
        
        dataCollection.m_sampData.resize(dataIn.getSampSize());
        memmove(dataCollection.m_sampData.data(), dataIn.getSamp(), dataIn.getSampSize());
        
        dataCollection.m_metaData.emplace(dataIn.getDataFormat(), dataIn.getAbsoluteProjOffsets(), true);
        dataCollection._indexData(presenter);
    }
}

void AudioGroupCollection::update(AudioGroupFilePresenter* presenter)
{
    NSFileManager* fman = [NSFileManager defaultManager];
    NSArray<NSURL*>* contents =
    [fman contentsOfDirectoryAtURL:m_url
        includingPropertiesForKeys:@[NSURLIsDirectoryKey]
                           options:NSDirectoryEnumerationSkipsSubdirectoryDescendants |
                                   NSDirectoryEnumerationSkipsHiddenFiles
                             error:nil];
    if (!contents)
        return;
    
    for (NSURL* path in contents)
    {
        NSNumber* isDir;
        [path getResourceValue:&isDir forKey:NSURLIsDirectoryKey error:nil];
        
        if (isDir.boolValue)
        {
            auto search = m_groups.find(path.lastPathComponent.UTF8String);
            if (search == m_groups.end())
            {
                std::string nameStr = path.lastPathComponent.UTF8String;
                search =
                m_groups.emplace(nameStr,
                                 std::make_unique<AudioGroupDataCollection>(nameStr,
                                 [path URLByAppendingPathComponent:@"proj"],
                                 [path URLByAppendingPathComponent:@"pool"],
                                 [path URLByAppendingPathComponent:@"sdir"],
                                 [path URLByAppendingPathComponent:@"samp"],
                                 [path URLByAppendingPathComponent:@"meta"])).first;
                search->second->_attemptLoad(presenter);
            }
        }
    }
}

bool AudioGroupCollection::doSearch(std::string_view str)
{
    bool ret = false;
    m_filterGroups.clear();
    m_filterGroups.reserve(m_groups.size());
    for (auto it = m_groups.begin() ; it != m_groups.end() ; ++it)
        if (str.empty() || StrToLower(it->first).find(str.data()) != std::string::npos)
        {
            m_filterGroups.push_back(it);
            ret = true;
        }
    return ret;
}

bool AudioGroupCollection::doActiveFilter()
{
    bool ret = false;
    m_filterGroups.clear();
    m_filterGroups.reserve(m_groups.size());
    for (auto it = m_groups.begin() ; it != m_groups.end() ; ++it)
        if (it->second->m_metaData->active)
        {
            m_filterGroups.push_back(it);
            ret = true;
        }
    return ret;
}

void AudioGroupCollection::addSFX(std::vector<AudioGroupSFXToken*>& vecOut)
{
    for (auto it = m_groups.begin() ; it != m_groups.end() ; ++it)
    {
        if (!it->second->m_metaData->active)
            continue;
        const auto& sfxGroups = it->second->m_loadedGroup->getProj().sfxGroups();
        std::map<int, const amuse::SFXGroupIndex*> sortGroups;
        for (const auto& pair : sfxGroups)
            sortGroups[pair.first] = &pair.second;
        for (const auto& pair : sortGroups)
        {
            NSMutableAttributedString* name = [[NSMutableAttributedString alloc] initWithString:m_url.lastPathComponent attributes:@{NSForegroundColorAttributeName: [NSColor grayColor]}];
            [name appendAttributedString:[[NSAttributedString alloc] initWithString:[NSString stringWithFormat:@"  %s (%d)", it->first.c_str(), pair.first]]];
            vecOut.push_back([[AudioGroupSFXToken alloc] initWithName:name loadId:0 sfx:nil]);
            std::map<int, const amuse::SFXGroupIndex::SFXEntry*> sortSfx;
            for (const auto& pair : pair.second->m_sfxEntries)
                sortSfx[pair.first] = pair.second;
            for (const auto& sfx : sortSfx)
            {
                name = [[NSMutableAttributedString alloc] initWithString:[NSString stringWithFormat:@"%d", sfx.first]];
                vecOut.push_back([[AudioGroupSFXToken alloc] initWithName:name loadId:sfx.first sfx:sfx.second]);
            }
        }
    }
}

void AudioGroupCollection::addSamples(std::vector<AudioGroupSampleToken*>& vecOut)
{
    for (auto it = m_groups.begin() ; it != m_groups.end() ; ++it)
    {
        if (!it->second->m_metaData->active)
            continue;
        const auto& samps = it->second->m_loadedGroup->getSdir().sampleEntries();
        std::map<int, const std::pair<amuse::AudioGroupSampleDirectory::Entry, amuse::AudioGroupSampleDirectory::ADPCMParms>*> sortSamps;
        for (const auto& pair : samps)
            sortSamps[pair.first] = &pair.second;
        
        NSMutableAttributedString* name = [[NSMutableAttributedString alloc] initWithString:m_url.lastPathComponent attributes:@{NSForegroundColorAttributeName: [NSColor grayColor]}];
        [name appendAttributedString:[[NSAttributedString alloc] initWithString:[NSString stringWithFormat:@"  %s", it->first.c_str()]]];
        vecOut.push_back([[AudioGroupSampleToken alloc] initWithName:name samp:nil]);
        
        for (const auto& pair : sortSamps)
        {
            name = [[NSMutableAttributedString alloc] initWithString:[NSString stringWithFormat:@"%d", pair.first]];
            vecOut.push_back([[AudioGroupSampleToken alloc] initWithName:name samp:pair.second]);
        }
    }
}

AudioGroupDataCollection::AudioGroupDataCollection(std::string_view name, NSURL* proj, NSURL* pool,
                                                   NSURL* sdir, NSURL* samp, NSURL* meta)
: m_name(name), m_proj(proj), m_pool(pool), m_sdir(sdir), m_samp(samp), m_meta(meta),
  m_token([[AudioGroupDataToken alloc] initWithDataCollection:this]) {}

bool AudioGroupDataCollection::_attemptLoad(AudioGroupFilePresenter* presenter)
{
    if (m_metaData && m_loadedData && m_loadedGroup)
        return true;
    if (!loadProj(presenter))
        return false;
    if (!loadPool(presenter))
        return false;
    if (!loadSdir(presenter))
        return false;
    if (!loadSamp(presenter))
        return false;
    if (!loadMeta(presenter))
        return false;
    
    return _indexData(presenter);
}

bool AudioGroupDataCollection::_indexData(AudioGroupFilePresenter* presenter)
{
    amuse::Engine& engine = [presenter->m_audioGroupClient getAmuseEngine];

    switch (m_metaData->fmt)
    {
        case amuse::DataFormat::GCN:
        default:
            m_loadedData.emplace(m_projData.data(), m_projData.size(),
                                 m_poolData.data(), m_poolData.size(),
                                 m_sdirData.data(), m_sdirData.size(),
                                 m_sampData.data(), m_sampData.size(),
                                 amuse::GCNDataTag{});
            break;
        case amuse::DataFormat::N64:
            m_loadedData.emplace(m_projData.data(), m_projData.size(),
                                 m_poolData.data(), m_poolData.size(),
                                 m_sdirData.data(), m_sdirData.size(),
                                 m_sampData.data(), m_sampData.size(),
                                 m_metaData->absOffs, amuse::N64DataTag{});
            break;
        case amuse::DataFormat::PC:
            m_loadedData.emplace(m_projData.data(), m_projData.size(),
                                 m_poolData.data(), m_poolData.size(),
                                 m_sdirData.data(), m_sdirData.size(),
                                 m_sampData.data(), m_sampData.size(),
                                 m_metaData->absOffs, amuse::PCDataTag{});
            break;
    }
    
    m_loadedGroup = engine.addAudioGroup(*m_loadedData);
    m_groupTokens.clear();
    if (m_loadedGroup)
    {
        m_groupTokens.reserve(m_loadedGroup->getProj().songGroups().size() +
                              m_loadedGroup->getProj().sfxGroups().size());
        
        {
            const auto& songGroups = m_loadedGroup->getProj().songGroups();
            std::map<int, const amuse::SongGroupIndex*> sortGroups;
            for (const auto& pair : songGroups)
                sortGroups[pair.first] = &pair.second;
            for (const auto& pair : sortGroups)
            {
                NSString* name = [NSString stringWithFormat:@"%d", pair.first];
                m_groupTokens.push_back([[AudioGroupToken alloc] initWithName:name id:pair.first
                                                                    songGroup:pair.second]);
            }
        }
        {
            const auto& sfxGroups = m_loadedGroup->getProj().sfxGroups();
            std::map<int, const amuse::SFXGroupIndex*> sortGroups;
            for (const auto& pair : sfxGroups)
                sortGroups[pair.first] = &pair.second;
            for (const auto& pair : sortGroups)
            {
                NSString* name = [NSString stringWithFormat:@"%d", pair.first];
                m_groupTokens.push_back([[AudioGroupToken alloc] initWithName:name id:pair.first
                                                                     sfxGroup:pair.second]);
            }
        }
    }
    
    return m_loadedData && m_loadedGroup;
}

void AudioGroupDataCollection::enable(AudioGroupFilePresenter* presenter)
{
    m_metaData->active = true;
    NSFileCoordinator* coord = [[NSFileCoordinator alloc] initWithFilePresenter:presenter];
    
    [coord coordinateWritingItemAtURL:m_meta options:0 error:nil
                           byAccessor:^(NSURL* newUrl)
    {
        FILE* fp = fopen(newUrl.path.UTF8String, "wb");
        if (fp)
        {
            fwrite(&*m_metaData, 1, sizeof(*m_metaData), fp);
            fclose(fp);
        }
    }];
}

void AudioGroupDataCollection::disable(AudioGroupFilePresenter* presenter)
{
    m_metaData->active = false;
    NSFileCoordinator* coord = [[NSFileCoordinator alloc] initWithFilePresenter:presenter];
    
    [coord coordinateWritingItemAtURL:m_meta options:0 error:nil
                           byAccessor:^(NSURL* newUrl)
    {
        FILE* fp = fopen(newUrl.path.UTF8String, "wb");
        if (fp)
        {
            fwrite(&*m_metaData, 1, sizeof(*m_metaData), fp);
            fclose(fp);
        }
    }];
}

void AudioGroupDataCollection::moveURL(NSURL* oldUrl, NSURL* newUrl)
{
    if (m_proj)
    {
        if ([m_proj isEqual:oldUrl])
            m_proj = newUrl;
    }
    if (m_pool)
    {
        if ([m_pool isEqual:oldUrl])
            m_pool = newUrl;
    }
    if (m_sdir)
    {
        if ([m_sdir isEqual:oldUrl])
            m_sdir = newUrl;
    }
    if (m_samp)
    {
        if ([m_samp isEqual:oldUrl])
            m_samp = newUrl;
    }
}

bool AudioGroupDataCollection::loadProj(AudioGroupFilePresenter* presenter)
{
    if (!m_proj)
        return false;
    NSFileCoordinator* coord = [[NSFileCoordinator alloc] initWithFilePresenter:presenter];
    if (!coord)
        return false;
    NSError* err;
    __block std::vector<uint8_t>& ret = m_projData;
    [coord coordinateReadingItemAtURL:m_proj
        options:NSFileCoordinatorReadingResolvesSymbolicLink error:&err
        byAccessor:^(NSURL* newUrl)
     {
         athena::io::FileReader r([[newUrl path] UTF8String], 1024 * 32, false);
         if (r.hasError())
             return;
         size_t len = r.length();
         ret.resize(len);
         r.readUBytesToBuf(ret.data(), len);
     }];
    return ret.size();
}

bool AudioGroupDataCollection::loadPool(AudioGroupFilePresenter* presenter)
{
    if (!m_pool)
        return false;
    NSFileCoordinator* coord = [[NSFileCoordinator alloc] initWithFilePresenter:presenter];
    if (!coord)
        return false;
    NSError* err;
    __block std::vector<uint8_t>& ret = m_poolData;
    [coord coordinateReadingItemAtURL:m_pool
                              options:NSFileCoordinatorReadingResolvesSymbolicLink error:&err
                           byAccessor:^(NSURL* newUrl)
     {
         athena::io::FileReader r([[newUrl path] UTF8String], 1024 * 32, false);
         if (r.hasError())
             return;
         size_t len = r.length();
         ret.resize(len);
         r.readUBytesToBuf(ret.data(), len);
     }];
    return ret.size();
}

bool AudioGroupDataCollection::loadSdir(AudioGroupFilePresenter* presenter)
{
    if (!m_sdir)
        return false;
    NSFileCoordinator* coord = [[NSFileCoordinator alloc] initWithFilePresenter:presenter];
    if (!coord)
        return false;
    NSError* err;
    __block std::vector<uint8_t>& ret = m_sdirData;
    [coord coordinateReadingItemAtURL:m_sdir
                              options:NSFileCoordinatorReadingResolvesSymbolicLink error:&err
                           byAccessor:^(NSURL* newUrl)
     {
         athena::io::FileReader r([[newUrl path] UTF8String], 1024 * 32, false);
         if (r.hasError())
             return;
         size_t len = r.length();
         ret.resize(len);
         r.readUBytesToBuf(ret.data(), len);
     }];
    return ret.size();
}

bool AudioGroupDataCollection::loadSamp(AudioGroupFilePresenter* presenter)
{
    if (!m_samp)
        return false;
    NSFileCoordinator* coord = [[NSFileCoordinator alloc] initWithFilePresenter:presenter];
    if (!coord)
        return false;
    NSError* err;
    __block std::vector<uint8_t>& ret = m_sampData;
    [coord coordinateReadingItemAtURL:m_samp
                              options:NSFileCoordinatorReadingResolvesSymbolicLink error:&err
                           byAccessor:^(NSURL* newUrl)
     {
         athena::io::FileReader r([[newUrl path] UTF8String], 1024 * 32, false);
         if (r.hasError())
             return;
         size_t len = r.length();
         ret.resize(len);
         r.readUBytesToBuf(ret.data(), len);
     }];
    return ret.size();
}

bool AudioGroupDataCollection::loadMeta(AudioGroupFilePresenter* presenter)
{
    if (!m_meta)
        return false;
    NSFileCoordinator* coord = [[NSFileCoordinator alloc] initWithFilePresenter:presenter];
    if (!coord)
        return false;
    NSError* err;
    __block std::experimental::optional<MetaData>& ret = m_metaData;
    [coord coordinateReadingItemAtURL:m_meta
                              options:NSFileCoordinatorReadingResolvesSymbolicLink error:&err
                           byAccessor:^(NSURL* newUrl)
     {
         athena::io::FileReader r([[newUrl path] UTF8String], 1024 * 32, false);
         if (r.hasError())
             return;
         ret.emplace(r);
     }];
    return ret.operator bool();
}

- (void)presentedSubitemDidChangeAtURL:(NSURL *)url
{
    size_t relComps = url.pathComponents.count - m_groupURL.pathComponents.count;
    if (relComps <= 1)
        [self update];
}

- (void)presentedSubitemAtURL:(NSURL*)oldUrl didMoveToURL:(NSURL*)newUrl
{
    for (auto& pair : m_audioGroupCollections)
    {
        for (auto& pair2 : pair.second->m_groups)
        {
            pair2.second->moveURL(oldUrl, newUrl);
        }
    }
}

- (NSInteger)outlineView:(NSOutlineView*)outlineView numberOfChildrenOfItem:(nullable id)item
{
    m_lastOutlineView = outlineView;
    if (!item)
        return m_filterAudioGroupCollections.size();
    
    AudioGroupCollection& collection = *((AudioGroupCollectionToken*)item)->m_collection;
    return collection.m_filterGroups.size();
}

- (id)outlineView:(NSOutlineView*)outlineView child:(NSInteger)index ofItem:(nullable id)item
{
    if (!item)
    {
        if (index >= m_filterAudioGroupCollections.size())
            return nil;
        return m_filterAudioGroupCollections[index]->second->m_token;
    }
    
    AudioGroupCollection& collection = *((AudioGroupCollectionToken*)item)->m_collection;
    if (index >= collection.m_filterGroups.size())
        return nil;
    return collection.m_filterGroups[index]->second->m_token;
}

- (BOOL)outlineView:(NSOutlineView*)outlineView isItemExpandable:(id)item
{
    if ([item isKindOfClass:[AudioGroupCollectionToken class]])
        return YES;
    return NO;
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    if ([item isKindOfClass:[AudioGroupCollectionToken class]])
    {
        AudioGroupCollection& collection = *((AudioGroupCollectionToken*)item)->m_collection;
        if ([tableColumn.identifier isEqualToString:@"CollectionColumn"])
        {
            size_t totalOn = 0;
            for (auto& pair : collection.m_groups)
                if (pair.second->m_metaData->active)
                    ++totalOn;
            if (totalOn == 0)
                return [NSNumber numberWithInt:NSOffState];
            else if (totalOn == collection.m_groups.size())
                return [NSNumber numberWithInt:NSOnState];
            else
                return [NSNumber numberWithInt:NSMixedState];
        }
        else if ([tableColumn.identifier isEqualToString:@"DetailsColumn"])
            return [NSString stringWithFormat:@"%zu Group File%s",
                    collection.m_groups.size(),
                    collection.m_groups.size() > 1 ? "s" : ""];
    }
    else if ([item isKindOfClass:[AudioGroupDataToken class]])
    {
        AudioGroupDataCollection& data = *((AudioGroupDataToken*)item)->m_collection;
        if ([tableColumn.identifier isEqualToString:@"CollectionColumn"])
            return [NSNumber numberWithInt:data.m_metaData->active ? NSOnState : NSOffState];
        else if ([tableColumn.identifier isEqualToString:@"DetailsColumn"])
        {
            if (!data.m_loadedGroup)
                return @"";
            if (data.m_loadedGroup->getProj().songGroups().size() && data.m_loadedGroup->getProj().sfxGroups().size())
                return [NSString stringWithFormat:@"%zu Song Group%s, %zu SFX Group%s",
                        data.m_loadedGroup->getProj().songGroups().size(),
                        data.m_loadedGroup->getProj().songGroups().size() > 1 ? "s" : "",
                        data.m_loadedGroup->getProj().sfxGroups().size(),
                        data.m_loadedGroup->getProj().sfxGroups().size() > 1 ? "s" : ""];
            else if (data.m_loadedGroup->getProj().songGroups().size())
                return [NSString stringWithFormat:@"%zu Song Group%s",
                        data.m_loadedGroup->getProj().songGroups().size(),
                        data.m_loadedGroup->getProj().songGroups().size() > 1 ? "s" : ""];
            else if (data.m_loadedGroup->getProj().sfxGroups().size())
                return [NSString stringWithFormat:@"%zu SFX Group%s",
                        data.m_loadedGroup->getProj().sfxGroups().size(),
                        data.m_loadedGroup->getProj().sfxGroups().size() > 1 ? "s" : ""];
            else
                return @"";
        }
    }
    
    return nil;
}

- (void)outlineView:(NSOutlineView *)outlineView setObjectValue:(nullable id)object forTableColumn:(nullable NSTableColumn *)tableColumn byItem:(nullable id)item
{
    bool dirty = false;
    
    if ([item isKindOfClass:[AudioGroupCollectionToken class]])
    {
        AudioGroupCollection& collection = *((AudioGroupCollectionToken*)item)->m_collection;
        if ([tableColumn.identifier isEqualToString:@"CollectionColumn"])
        {
            NSInteger active = [object integerValue];
            if (active)
                for (auto& pair : collection.m_groups)
                    pair.second->enable(self);
            else
                for (auto& pair : collection.m_groups)
                    pair.second->disable(self);
            dirty = true;
        }
    }
    else if ([item isKindOfClass:[AudioGroupDataToken class]])
    {
        AudioGroupDataCollection& data = *((AudioGroupDataToken*)item)->m_collection;
        if ([tableColumn.identifier isEqualToString:@"CollectionColumn"])
        {
            NSInteger active = [object integerValue];
            if (active)
                data.enable(self);
            else
                data.disable(self);
            dirty = true;
        }
    }
    
    if (dirty)
        [self resetIterators];
}

- (void)outlineView:(NSOutlineView *)outlineView willDisplayCell:(nonnull id)cell forTableColumn:(nullable NSTableColumn *)tableColumn item:(nonnull id)item
{
    if ([item isKindOfClass:[AudioGroupCollectionToken class]])
    {
        AudioGroupCollection& collection = *((AudioGroupCollectionToken*)item)->m_collection;
        if ([tableColumn.identifier isEqualToString:@"CollectionColumn"])
            ((NSButtonCell*)cell).title = collection.m_url.lastPathComponent;
    }
    else if ([item isKindOfClass:[AudioGroupDataToken class]])
    {
        AudioGroupDataCollection& data = *((AudioGroupDataToken*)item)->m_collection;
        if ([tableColumn.identifier isEqualToString:@"CollectionColumn"])
            ((NSButtonCell*)cell).title = @(data.m_name.c_str());
    }
}

- (void)outlineViewSelectionDidChange:(NSNotification *)notification
{
    DataOutlineView* ov = notification.object;
    id item = [ov itemAtRow:ov.selectedRow];
    [(AppDelegate*)NSApp.delegate outlineView:ov selectionChanged:item];
}

- (NSDragOperation)outlineView:(NSOutlineView *)outlineView validateDrop:(id<NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(NSInteger)index
{
    [outlineView setDropItem:nil dropChildIndex:NSOutlineViewDropOnItemIndex];
    NSPasteboard* pboard = [info draggingPasteboard];
    if ([[pboard types] containsObject:NSURLPboardType])
        return NSDragOperationCopy;
    return NSDragOperationNone;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView acceptDrop:(id<NSDraggingInfo>)info item:(id)item childIndex:(NSInteger)index
{
    NSPasteboard* pboard = [info draggingPasteboard];
    if ([[pboard types] containsObject:NSURLPboardType])
    {
        NSURL* url = [NSURL URLFromPasteboard:pboard];
        [(AppDelegate*)NSApp.delegate importURL:url];
        return YES;
    }
    return NO;
}

- (BOOL)addCollectionName:(std::string&&)name items:(std::vector<std::pair<std::string, amuse::IntrusiveAudioGroupData>>&&)collection
{
    NSFileCoordinator* coord = [[NSFileCoordinator alloc] initWithFilePresenter:self];
    if (!coord)
        return false;
    
    NSURL* dir = [m_groupURL URLByAppendingPathComponent:@(name.c_str())];
    __block AudioGroupCollection& insert = *m_audioGroupCollections.emplace(name, std::make_unique<AudioGroupCollection>(dir)).first->second;
    insert.addCollection(self, std::move(collection));
    
    [coord coordinateWritingItemAtURL:m_groupURL options:0 error:nil
                           byAccessor:^(NSURL* newUrl)
    {
        for (std::pair<const std::string, std::unique_ptr<AudioGroupDataCollection>>& pair : insert.m_groups)
        {
            NSURL* collectionUrl = [insert.m_url URLByAppendingPathComponent:@(pair.first.c_str())];
            [[NSFileManager defaultManager] createDirectoryAtURL:collectionUrl withIntermediateDirectories:YES attributes:nil error:nil];

            FILE* fp = fopen(pair.second->m_proj.path.UTF8String, "wb");
            if (fp)
            {
                fwrite(pair.second->m_projData.data(), 1, pair.second->m_projData.size(), fp);
                fclose(fp);
            }
            
            fp = fopen(pair.second->m_pool.path.UTF8String, "wb");
            if (fp)
            {
                fwrite(pair.second->m_poolData.data(), 1, pair.second->m_poolData.size(), fp);
                fclose(fp);
            }
            
            fp = fopen(pair.second->m_sdir.path.UTF8String, "wb");
            if (fp)
            {
                fwrite(pair.second->m_sdirData.data(), 1, pair.second->m_sdirData.size(), fp);
                fclose(fp);
            }
            
            fp = fopen(pair.second->m_samp.path.UTF8String, "wb");
            if (fp)
            {
                fwrite(pair.second->m_sampData.data(), 1, pair.second->m_sampData.size(), fp);
                fclose(fp);
            }
            
            fp = fopen(pair.second->m_meta.path.UTF8String, "wb");
            if (fp)
            {
                fwrite(&*pair.second->m_metaData, 1, sizeof(*pair.second->m_metaData), fp);
                fclose(fp);
            }
        }
    }];
    
    [self resetIterators];
    return true;
}

- (void)update
{
    NSFileCoordinator* coord = [[NSFileCoordinator alloc] initWithFilePresenter:self];
    if (!coord)
        return;
    NSError* coordErr;
    __block NSError* managerErr;
    __block std::map<std::string, std::unique_ptr<AudioGroupCollection>>& theMap = m_audioGroupCollections;
    __block AudioGroupFilePresenter* presenter = self;
    [coord coordinateReadingItemAtURL:m_groupURL options:NSFileCoordinatorReadingResolvesSymbolicLink error:&coordErr
                           byAccessor:^(NSURL* newUrl)
    {
        NSFileManager* fman = [NSFileManager defaultManager];
        NSArray<NSURL*>* contents =
        [fman contentsOfDirectoryAtURL:newUrl
            includingPropertiesForKeys:@[NSURLIsDirectoryKey]
            options:NSDirectoryEnumerationSkipsSubdirectoryDescendants |
                    NSDirectoryEnumerationSkipsHiddenFiles
            error:&managerErr];
        if (!contents)
            return;
         
        for (NSURL* path in contents)
        {
            NSNumber* isDir;
            [path getResourceValue:&isDir forKey:NSURLIsDirectoryKey error:nil];
            
            if (isDir.boolValue)
            {
                auto search = theMap.find(path.lastPathComponent.UTF8String);
                if (search == theMap.end())
                {
                    search = theMap.emplace(path.lastPathComponent.UTF8String, std::make_unique<AudioGroupCollection>(path)).first;
                    search->second->update(presenter);
                }
            }
        }
    }];
    
    [self resetIterators];
}

- (void)resetIterators
{
    if ([(NSObject*)m_audioGroupClient isKindOfClass:[AppDelegate class]])
    {
        std::string search;
        if (m_searchStr)
            search = m_searchStr.UTF8String;
        
        m_sfxTableData.clear();
        m_sampleTableData.clear();
        m_filterAudioGroupCollections.clear();
        m_filterAudioGroupCollections.reserve(m_audioGroupCollections.size());
        for (auto it = m_audioGroupCollections.begin() ; it != m_audioGroupCollections.end() ; ++it)
        {
            it->second->addSFX(m_sfxTableData);
            it->second->addSamples(m_sampleTableData);
            if (it->second->doSearch(search) || !m_searchStr || StrToLower(it->first).find(search) != std::string::npos)
                m_filterAudioGroupCollections.push_back(it);
        }
        [m_lastOutlineView reloadItem:nil reloadChildren:YES];
        [(AppDelegate*)m_audioGroupClient reloadTables];
    }
    else
    {
        m_sfxTableData.clear();
        m_sampleTableData.clear();
        m_filterAudioGroupCollections.clear();
        m_filterAudioGroupCollections.reserve(m_audioGroupCollections.size());
        for (auto it = m_audioGroupCollections.begin() ; it != m_audioGroupCollections.end() ; ++it)
        {
            it->second->addSFX(m_sfxTableData);
            it->second->addSamples(m_sampleTableData);
            if (it->second->doActiveFilter())
                m_filterAudioGroupCollections.push_back(it);
        }
        [((AmuseAudioUnit*)m_audioGroupClient)->m_viewController->m_groupBrowser loadColumnZero];
    }
}

- (void)setSearchFilter:(NSString*)str
{
    m_searchStr = [str lowercaseString];
    [self resetIterators];
}

- (void)removeSelectedItem
{
    id item = [m_lastOutlineView itemAtRow:m_lastOutlineView.selectedRow];
    if ([item isKindOfClass:[AudioGroupCollectionToken class]])
    {
        AudioGroupCollection& collection = *((AudioGroupCollectionToken*)item)->m_collection;
        NSURL* collectionURL = collection.m_url;
        NSString* lastComp = collectionURL.lastPathComponent;
        m_audioGroupCollections.erase(lastComp.UTF8String);
        [self resetIterators];
        [[NSFileManager defaultManager] removeItemAtURL:collectionURL error:nil];
        if (m_audioGroupCollections.empty())
            [(AppDelegate*)NSApp.delegate outlineView:(DataOutlineView*)m_lastOutlineView selectionChanged:nil];
    }
}

- (id)initWithAudioGroupClient:(id<AudioGroupClient>)client
{
    m_audioGroupClient = client;
    m_groupURL = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:@"group.io.github.axiodl.Amuse.AudioGroups"];
    if (!m_groupURL)
        return nil;
    [NSFileCoordinator addFilePresenter:self];
    [self update];
    return self;
}

- (void)dealloc
{
    [NSFileCoordinator removeFilePresenter:self];
}

@end
