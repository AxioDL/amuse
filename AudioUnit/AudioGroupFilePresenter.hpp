#ifndef __AMUSE_AUDIOUNIT_AUDIOGROUPFILEPRESENTER_HPP__
#define __AMUSE_AUDIOUNIT_AUDIOGROUPFILEPRESENTER_HPP__

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include <map>
#include <string>

@class AudioGroupFilePresenter;

struct AudioGroupDataCollection
{
    NSURL* m_proj;
    NSURL* m_pool;
    NSURL* m_sdir;
    NSURL* m_samp;
    
    std::unique_ptr<uint8_t[]> m_projData;
    std::unique_ptr<uint8_t[]> m_poolData;
    std::unique_ptr<uint8_t[]> m_sdirData;
    std::unique_ptr<uint8_t[]> m_sampData;

    void moveURL(NSURL* oldUrl, NSURL* newUrl);

    bool loadProj(AudioGroupFilePresenter* presenter);
    bool loadPool(AudioGroupFilePresenter* presenter);
    bool loadSdir(AudioGroupFilePresenter* presenter);
    bool loadSamp(AudioGroupFilePresenter* presenter);

    AudioGroupDataCollection(NSURL* proj, NSURL* pool, NSURL* sdir, NSURL* samp)
    : m_proj(proj), m_pool(pool), m_sdir(sdir), m_samp(samp) {}
    bool isDataComplete () const {return m_projData && m_poolData && m_sdirData && m_sampData;}
};

@interface AudioGroupFilePresenter : NSObject <NSFilePresenter, NSOutlineViewDataSource>
{
    NSURL* m_groupURL;
    NSOperationQueue* m_dataQueue;
    std::map<std::string, AudioGroupDataCollection> m_audioGroupCollections;
    
    std::map<std::string, AudioGroupDataCollection>::const_iterator m_audioGroupOutlineIt;
    size_t m_audioGroupOutlineIdx;
}

- (void)update;

@end

#endif // __AMUSE_AUDIOUNIT_AUDIOGROUPFILEPRESENTER_HPP__
