#pragma once

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

