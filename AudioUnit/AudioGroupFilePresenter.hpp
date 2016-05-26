#ifndef __AMUSE_AUDIOUNIT_AUDIOGROUPFILEPRESENTER_HPP__
#define __AMUSE_AUDIOUNIT_AUDIOGROUPFILEPRESENTER_HPP__

#import <Foundation/Foundation.h>
#include <map>
#include <string>

@class AudioGroupFilePresenter;

struct AudioGroupDataCollection
{
    NSURL* m_proj = nullptr; /* Only this member set for single-file containers */
    NSURL* m_pool = nullptr;
    NSURL* m_sdir = nullptr;
    NSURL* m_samp = nullptr;

    bool invalidateURL(NSURL* url);
    void moveURL(NSURL* oldUrl, NSURL* newUrl);

    std::unique_ptr<uint8_t[]> _coordinateRead(AudioGroupFilePresenter* presenter, size_t& szOut, NSURL* url);

    std::unique_ptr<uint8_t[]> coordinateProjRead(AudioGroupFilePresenter* presenter, size_t& szOut);
    std::unique_ptr<uint8_t[]> coordinatePoolRead(AudioGroupFilePresenter* presenter, size_t& szOut);
    std::unique_ptr<uint8_t[]> coordinateSdirRead(AudioGroupFilePresenter* presenter, size_t& szOut);
    std::unique_ptr<uint8_t[]> coordinateSampRead(AudioGroupFilePresenter* presenter, size_t& szOut);
};

@interface AudioGroupFilePresenter : NSObject <NSFilePresenter>
{
    NSURL* m_groupURL;
    NSOperationQueue* m_dataQueue;
    std::map<std::string, AudioGroupDataCollection> m_audioGroupCollections;
}

@end

#endif // __AMUSE_AUDIOUNIT_AUDIOGROUPFILEPRESENTER_HPP__
