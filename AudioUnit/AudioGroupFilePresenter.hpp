#pragma once

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
@class AudioGroupSFXToken;
@class AudioGroupSampleToken;
@class AudioGroupToken;

@protocol AudioGroupClient
- (amuse::Engine&)getAmuseEngine;
@end

struct AudioGroupDataCollection {
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

  struct MetaData {
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
  const amuse::AudioGroup* m_loadedGroup;
  std::vector<AudioGroupToken*> m_groupTokens;

  void moveURL(NSURL* oldUrl, NSURL* newUrl);

  bool loadProj(AudioGroupFilePresenter* presenter);
  bool loadPool(AudioGroupFilePresenter* presenter);
  bool loadSdir(AudioGroupFilePresenter* presenter);
  bool loadSamp(AudioGroupFilePresenter* presenter);
  bool loadMeta(AudioGroupFilePresenter* presenter);

  AudioGroupDataCollection(std::string_view name, NSURL* proj, NSURL* pool, NSURL* sdir, NSURL* samp, NSURL* meta);
  bool isDataComplete() const {
    return m_projData.size() && m_poolData.size() && m_sdirData.size() && m_sampData.size() && m_metaData;
  }
  bool _attemptLoad(AudioGroupFilePresenter* presenter);
  bool _indexData(AudioGroupFilePresenter* presenter);

  void enable(AudioGroupFilePresenter* presenter);
  void disable(AudioGroupFilePresenter* presenter);
};

struct AudioGroupCollection {
  NSURL* m_url;

  AudioGroupCollectionToken* m_token;
  std::map<std::string, std::unique_ptr<AudioGroupDataCollection>> m_groups;
  std::vector<std::map<std::string, std::unique_ptr<AudioGroupDataCollection>>::iterator> m_filterGroups;

  AudioGroupCollection(NSURL* url);
  void addCollection(AudioGroupFilePresenter* presenter,
                     std::vector<std::pair<std::string, amuse::IntrusiveAudioGroupData>>&& collection);
  void update(AudioGroupFilePresenter* presenter);
  bool doSearch(std::string_view str);
  bool doActiveFilter();
  void addSFX(std::vector<AudioGroupSFXToken*>& vecOut);
  void addSamples(std::vector<AudioGroupSampleToken*>& vecOut);
};

@interface AudioGroupDataToken : NSObject {
@public
  AudioGroupDataCollection* m_collection;
}
- (id)initWithDataCollection:(AudioGroupDataCollection*)collection;
@end

@interface AudioGroupCollectionToken : NSObject {
@public
  AudioGroupCollection* m_collection;
}
- (id)initWithCollection:(AudioGroupCollection*)collection;
@end

@interface AudioGroupSFXToken : NSObject {
@public
  NSAttributedString* m_name;
  int m_loadId;
  const amuse::SFXGroupIndex::SFXEntry* m_sfx;
}
- (id)initWithName:(NSAttributedString*)name loadId:(int)loadId sfx:(const amuse::SFXGroupIndex::SFXEntry*)sfx;
@end

@interface AudioGroupSampleToken : NSObject {
@public
  NSAttributedString* m_name;
  const std::pair<amuse::AudioGroupSampleDirectory::Entry, amuse::AudioGroupSampleDirectory::ADPCMParms>* m_sample;
}
- (id)initWithName:(NSAttributedString*)name
              samp:(const std::pair<amuse::AudioGroupSampleDirectory::Entry,
                                    amuse::AudioGroupSampleDirectory::ADPCMParms>*)sample;
@end

@interface AudioGroupToken : NSObject {
@public
  NSString* m_name;
  int m_id;
  const amuse::SongGroupIndex* m_song;
  const amuse::SFXGroupIndex* m_sfx;
}
- (id)initWithName:(NSString*)name id:(int)gid songGroup:(const amuse::SongGroupIndex*)group;
- (id)initWithName:(NSString*)name id:(int)gid sfxGroup:(const amuse::SFXGroupIndex*)group;
@end

@interface AudioGroupFilePresenter : NSObject <NSFilePresenter, NSOutlineViewDataSource, NSOutlineViewDelegate> {
@public
  id<AudioGroupClient> m_audioGroupClient;
  NSURL* m_groupURL;
  std::map<std::string, std::unique_ptr<AudioGroupCollection>> m_audioGroupCollections;
  std::vector<std::map<std::string, std::unique_ptr<AudioGroupCollection>>::iterator> m_filterAudioGroupCollections;
  NSOutlineView* m_lastOutlineView;
  NSString* m_searchStr;

  std::vector<AudioGroupSFXToken*> m_sfxTableData;
  std::vector<AudioGroupSampleToken*> m_sampleTableData;
}
- (id)initWithAudioGroupClient:(id<AudioGroupClient>)client;
- (BOOL)addCollectionName:(std::string&&)name
                    items:(std::vector<std::pair<std::string, amuse::IntrusiveAudioGroupData>>&&)collection;
- (void)update;
- (void)resetIterators;
- (void)setSearchFilter:(NSString*)str;
- (void)removeSelectedItem;
@end
