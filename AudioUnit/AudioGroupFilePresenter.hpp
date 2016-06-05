#ifndef __AMUSE_AUDIOUNIT_AUDIOGROUPFILEPRESENTER_HPP__
#define __AMUSE_AUDIOUNIT_AUDIOGROUPFILEPRESENTER_HPP__

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include <map>
#include <string>
#include "optional.hpp"
#include <amuse/amuse.hpp>
#include <athena/FileReader.hpp>

@class AudioGroupFilePresenter;
@class AudioGroupDataToken;
@class AudioGroupCollectionToken;

struct AudioGroupDataCollection
{
    std::string m_name;
    NSURL* m_proj;
    NSURL* m_pool;
    NSURL* m_sdir;
    NSURL* m_samp;
    NSURL* m_meta;

    AudioGroupDataToken* m_token;
    
    std::vector<uint8_t> m_projData;
    std::vector<uint8_t> m_poolData;
    std::vector<uint8_t> m_sdirData;
    std::vector<uint8_t> m_sampData;
    
    struct MetaData
    {
        amuse::DataFormat fmt;
        uint32_t absOffs;
        uint32_t active;
        MetaData(amuse::DataFormat fmtIn, uint32_t absOffsIn, uint32_t activeIn)
        : fmt(fmtIn), absOffs(absOffsIn), active(activeIn) {}
        MetaData(athena::io::FileReader& r)
        : fmt(amuse::DataFormat(r.readUint32Little())), absOffs(r.readUint32Little()), active(r.readUint32Little()) {}
    };
    std::experimental::optional<MetaData> m_metaData;
    
    std::experimental::optional<amuse::AudioGroupData> m_loadedData;
    std::experimental::optional<amuse::AudioGroupProject> m_loadedProj;

    void moveURL(NSURL* oldUrl, NSURL* newUrl);

    bool loadProj(AudioGroupFilePresenter* presenter);
    bool loadPool(AudioGroupFilePresenter* presenter);
    bool loadSdir(AudioGroupFilePresenter* presenter);
    bool loadSamp(AudioGroupFilePresenter* presenter);
    bool loadMeta(AudioGroupFilePresenter* presenter);

    AudioGroupDataCollection(const std::string& name, NSURL* proj, NSURL* pool, NSURL* sdir, NSURL* samp, NSURL* meta);
    bool isDataComplete() const {return m_projData.size() && m_poolData.size() && m_sdirData.size() && m_sampData.size() && m_metaData;}
    bool _attemptLoad(AudioGroupFilePresenter* presenter);
    
    void enable(AudioGroupFilePresenter* presenter);
    void disable(AudioGroupFilePresenter* presenter);
};

struct AudioGroupCollection
{
    NSURL* m_url;
    
    AudioGroupCollectionToken* m_token;
    std::map<std::string, std::unique_ptr<AudioGroupDataCollection>> m_groups;
    std::vector<std::map<std::string, std::unique_ptr<AudioGroupDataCollection>>::iterator> m_filterGroups;
    
    AudioGroupCollection(NSURL* url);
    void addCollection(std::vector<std::pair<std::string, amuse::IntrusiveAudioGroupData>>&& collection);
    void update(AudioGroupFilePresenter* presenter);
    bool doSearch(const std::string& str);
};

@interface AudioGroupDataToken : NSObject
{
@public
    AudioGroupDataCollection* m_collection;
}
- (id)initWithDataCollection:(AudioGroupDataCollection*)collection;
@end

@interface AudioGroupCollectionToken : NSObject
{
@public
    AudioGroupCollection* m_collection;
}
- (id)initWithCollection:(AudioGroupCollection*)collection;
@end

@interface AudioGroupFilePresenter : NSObject <NSFilePresenter, NSOutlineViewDataSource, NSOutlineViewDelegate>
{
    NSURL* m_groupURL;
    NSOperationQueue* m_dataQueue;
    std::map<std::string, std::unique_ptr<AudioGroupCollection>> m_audioGroupCollections;
    std::vector<std::map<std::string, std::unique_ptr<AudioGroupCollection>>::iterator> m_filterAudioGroupCollections;
    NSOutlineView* lastOutlineView;
    NSString* searchStr;
}
- (BOOL)addCollectionName:(std::string&&)name items:(std::vector<std::pair<std::string, amuse::IntrusiveAudioGroupData>>&&)collection;
- (void)update;
- (void)resetIterators;
- (void)setSearchFilter:(NSString*)str;
- (void)removeSelectedItem;
@end

#endif // __AMUSE_AUDIOUNIT_AUDIOGROUPFILEPRESENTER_HPP__
