#include "AudioGroupFilePresenter.hpp"
#include <athena/FileReader.hpp>

@implementation AudioGroupFilePresenter

- (NSURL*)presentedItemURL
{
    return m_groupURL;
}

- (NSOperationQueue*)presentedItemOperationQueue
{
    return m_dataQueue;
}

bool AudioGroupDataCollection::invalidateURL(NSURL* url)
{
    bool valid = false;
    if (m_proj)
    {
        if ([m_proj isEqual:url])
            m_proj = nullptr;
        valid |= m_proj != nullptr;
    }
    if (m_pool)
    {
        if ([m_pool isEqual:url])
            m_pool = nullptr;
        valid |= m_pool != nullptr;
    }
    if (m_sdir)
    {
        if ([m_sdir isEqual:url])
            m_sdir = nullptr;
        valid |= m_sdir != nullptr;
    }
    if (m_samp)
    {
        if ([m_samp isEqual:url])
            m_samp = nullptr;
        valid |= m_samp != nullptr;
    }
    return valid;
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

std::unique_ptr<uint8_t[]> AudioGroupDataCollection::_coordinateRead(AudioGroupFilePresenter* presenter, size_t& szOut, NSURL* url)
{
    NSFileCoordinator* coord = [[NSFileCoordinator alloc] initWithFilePresenter:presenter];
    if (!coord)
        return {};
    NSError* err;
    __block std::unique_ptr<uint8_t[]> ret;
    __block size_t retSz = 0;
    [coord coordinateReadingItemAtURL:url options:NSFileCoordinatorReadingResolvesSymbolicLink error:&err
     byAccessor:^(NSURL* newUrl)
    {
        athena::io::FileReader r([[newUrl path] UTF8String], 1024 * 32, false);
        if (r.hasError())
            return;
        retSz = r.length();
        ret = r.readUBytes(retSz);
    }];
    szOut = retSz;
    return std::move(ret);
}

std::unique_ptr<uint8_t[]> AudioGroupDataCollection::coordinateProjRead(AudioGroupFilePresenter* presenter, size_t& szOut)
{
    if (!m_proj)
        return {};
    return _coordinateRead(presenter, szOut, m_proj);
}

std::unique_ptr<uint8_t[]> AudioGroupDataCollection::coordinatePoolRead(AudioGroupFilePresenter* presenter, size_t& szOut)
{
    if (!m_pool)
        return {};
    return _coordinateRead(presenter, szOut, m_pool);
}

std::unique_ptr<uint8_t[]> AudioGroupDataCollection::coordinateSdirRead(AudioGroupFilePresenter* presenter, size_t& szOut)
{
    if (!m_sdir)
        return {};
    return _coordinateRead(presenter, szOut, m_sdir);
}

std::unique_ptr<uint8_t[]> AudioGroupDataCollection::coordinateSampRead(AudioGroupFilePresenter* presenter, size_t& szOut)
{
    if (!m_samp)
        return {};
    return _coordinateRead(presenter, szOut, m_samp);
}

- (void)accommodatePresentedSubitemDeletionAtURL:(NSURL*)url completionHandler:(void (^)(NSError* errorOrNil))completionHandler
{
    for (auto it = m_audioGroupCollections.begin() ; it != m_audioGroupCollections.end() ;)
    {
        std::pair<const std::string, AudioGroupDataCollection>& pair = *it;
        if (pair.second.invalidateURL(url))
        {
            it = m_audioGroupCollections.erase(it);
            continue;
        }
        ++it;
    }
    completionHandler(nil);
}

- (void)presentedSubitemDidAppearAtURL:(NSURL*)url
{
    NSString* path = [url path];
    if (!path)
        return;

    NSString* extension = [url pathExtension];
    NSString* lastComp = [url lastPathComponent];
    lastComp = [lastComp substringToIndex:[lastComp length] - [extension length]];
    AudioGroupDataCollection& collection = m_audioGroupCollections[[lastComp UTF8String]];

    if ([extension isEqualToString:@"pro"] || [extension isEqualToString:@"proj"])
    {
        collection.m_proj = url;
    }
    else if ([extension isEqualToString:@"poo"] || [extension isEqualToString:@"pool"])
    {
        collection.m_pool = url;
    }
    else if ([extension isEqualToString:@"sdi"] || [extension isEqualToString:@"sdir"])
    {
        collection.m_sdir = url;
    }
    else if ([extension isEqualToString:@"sam"] || [extension isEqualToString:@"samp"])
    {
        collection.m_samp = url;
    }
    else
    {
        collection.m_proj = url;
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

- (id)init
{
    m_groupURL = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:@"group.io.github.axiodl.Amuse.AudioGroups"];
    if (!m_groupURL)
        return nil;
    m_dataQueue = [NSOperationQueue new];
    [NSFileCoordinator addFilePresenter:self];
    return self;
}

- (void)dealloc
{
    [NSFileCoordinator removeFilePresenter:self];
}

@end
