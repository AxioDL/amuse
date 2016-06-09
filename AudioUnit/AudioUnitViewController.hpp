#ifndef __AMUSE_AUDIOUNIT_VIEWCONTROLLER_HPP__
#define __AMUSE_AUDIOUNIT_VIEWCONTROLLER_HPP__

#import <CoreAudioKit/CoreAudioKit.h>
#import "AudioGroupFilePresenter.hpp"

@class AmuseAudioUnit;

@interface GroupBrowserDelegate : NSObject <NSBrowserDelegate>
{
    AmuseAudioUnit* m_audioUnit;
}
- (id)initWithAudioUnit:(AmuseAudioUnit*)au;
@end

@interface AudioUnitViewController : AUViewController <AUAudioUnitFactory>
{
@public
    AmuseAudioUnit* m_audioUnit;
    IBOutlet NSBrowser* m_groupBrowser;
    GroupBrowserDelegate* m_groupBrowserDelegate;
}
@end

#endif // __AMUSE_AUDIOUNIT_VIEWCONTROLLER_HPP__
