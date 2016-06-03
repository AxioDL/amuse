#include "AudioGroupFilePresenter.hpp"
#include <athena/FileReader.hpp>
#import <AppKit/AppKit.h>

@implementation AudioGroupFilePresenter

- (NSURL*)presentedItemURL
{
    return m_groupURL;
}

- (NSOperationQueue*)presentedItemOperationQueue
{
    return m_dataQueue;
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
    __block std::unique_ptr<uint8_t[]>& ret = m_projData;
    [coord coordinateReadingItemAtURL:m_proj
        options:NSFileCoordinatorReadingResolvesSymbolicLink error:&err
        byAccessor:^(NSURL* newUrl)
     {
         athena::io::FileReader r([[newUrl path] UTF8String], 1024 * 32, false);
         if (r.hasError())
             return;
         ret = r.readUBytes(r.length());
     }];
    return m_projData.operator bool();
}

bool AudioGroupDataCollection::loadPool(AudioGroupFilePresenter* presenter)
{
    if (!m_pool)
        return false;
    NSFileCoordinator* coord = [[NSFileCoordinator alloc] initWithFilePresenter:presenter];
    if (!coord)
        return false;
    NSError* err;
    __block std::unique_ptr<uint8_t[]>& ret = m_poolData;
    [coord coordinateReadingItemAtURL:m_pool
                              options:NSFileCoordinatorReadingResolvesSymbolicLink error:&err
                           byAccessor:^(NSURL* newUrl)
     {
         athena::io::FileReader r([[newUrl path] UTF8String], 1024 * 32, false);
         if (r.hasError())
             return;
         ret = r.readUBytes(r.length());
     }];
    return m_poolData.operator bool();
}

bool AudioGroupDataCollection::loadSdir(AudioGroupFilePresenter* presenter)
{
    if (!m_sdir)
        return false;
    NSFileCoordinator* coord = [[NSFileCoordinator alloc] initWithFilePresenter:presenter];
    if (!coord)
        return false;
    NSError* err;
    __block std::unique_ptr<uint8_t[]>& ret = m_sdirData;
    [coord coordinateReadingItemAtURL:m_sdir
                              options:NSFileCoordinatorReadingResolvesSymbolicLink error:&err
                           byAccessor:^(NSURL* newUrl)
     {
         athena::io::FileReader r([[newUrl path] UTF8String], 1024 * 32, false);
         if (r.hasError())
             return;
         ret = r.readUBytes(r.length());
     }];
    return m_sdirData.operator bool();
}

bool AudioGroupDataCollection::loadSamp(AudioGroupFilePresenter* presenter)
{
    if (!m_samp)
        return false;
    NSFileCoordinator* coord = [[NSFileCoordinator alloc] initWithFilePresenter:presenter];
    if (!coord)
        return false;
    NSError* err;
    __block std::unique_ptr<uint8_t[]>& ret = m_sampData;
    [coord coordinateReadingItemAtURL:m_samp
                              options:NSFileCoordinatorReadingResolvesSymbolicLink error:&err
                           byAccessor:^(NSURL* newUrl)
     {
         athena::io::FileReader r([[newUrl path] UTF8String], 1024 * 32, false);
         if (r.hasError())
             return;
         ret = r.readUBytes(r.length());
     }];
    return m_sampData.operator bool();
}

- (void)presentedSubitemDidAppearAtURL:(NSURL*)url
{
    NSURL* dir = nil;
    if ([url.lastPathComponent isEqualToString:@"proj"] ||
        [url.lastPathComponent isEqualToString:@"pool"] ||
        [url.lastPathComponent isEqualToString:@"sdir"] ||
        [url.lastPathComponent isEqualToString:@"samp"])
        dir = url.baseURL;

    auto search = m_audioGroupCollections.find(dir.lastPathComponent.UTF8String);
    if (search == m_audioGroupCollections.end())
    {
        search =
        m_audioGroupCollections.emplace(dir.lastPathComponent.UTF8String,
                                        AudioGroupDataCollection{
                                        [dir URLByAppendingPathComponent:@"proj"],
                                        [dir URLByAppendingPathComponent:@"pool"],
                                        [dir URLByAppendingPathComponent:@"sdir"],
                                        [dir URLByAppendingPathComponent:@"samp"]}).first;
    }
}

- (void)presentedSubitemAtURL:(NSURL*)oldUrl didMoveToURL:(NSURL*)newUrl
{
    for (auto it = m_audioGroupCollections.begin() ; it != m_audioGroupCollections.end() ; ++it)
    {
        std::pair<const std::string, AudioGroupDataCollection>& pair = *it;
        pair.second.moveURL(oldUrl, newUrl);
    }
}

- (NSInteger)outlineView:(NSOutlineView*)outlineView numberOfChildrenOfItem:(nullable id)item
{
    if (!item)
        return m_audioGroupCollections.size();
    return 0;
}

- (id)outlineView:(NSOutlineView*)outlineView child:(NSInteger)index ofItem:(nullable id)item
{
    if (!item)
    {
        
    }
    return nil;
}

- (BOOL)outlineView:(NSOutlineView*)outlineView isItemExpandable:(id)item
{
    return NO;
}

- (void)update
{
    NSFileCoordinator* coord = [[NSFileCoordinator alloc] initWithFilePresenter:self];
    if (!coord)
        return;
    NSError* coordErr;
    __block NSError* managerErr;
    __block std::map<std::string, AudioGroupDataCollection>& theMap = m_audioGroupCollections;
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
                     search =
                     theMap.emplace(path.lastPathComponent.UTF8String,
                                    AudioGroupDataCollection{
                                    [path URLByAppendingPathComponent:@"proj"],
                                    [path URLByAppendingPathComponent:@"pool"],
                                    [path URLByAppendingPathComponent:@"sdir"],
                                    [path URLByAppendingPathComponent:@"samp"]}).first;
                 }
             }
         }
     }];
}

- (id)init
{
    m_groupURL = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:@"group.io.github.axiodl.Amuse.AudioGroups"];
    if (!m_groupURL)
        return nil;
    m_dataQueue = [NSOperationQueue new];
    [NSFileCoordinator addFilePresenter:self];
    [self update];
    return self;
}

- (void)dealloc
{
    [NSFileCoordinator removeFilePresenter:self];
}

@end
