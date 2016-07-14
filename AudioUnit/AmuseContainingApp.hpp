#ifndef __AMUSE_AUDIOUNIT_CONTAININGAPP_HPP__
#define __AMUSE_AUDIOUNIT_CONTAININGAPP_HPP__

#import <AppKit/AppKit.h>
#import "AudioGroupFilePresenter.hpp"
#include <amuse/BooBackend.hpp>
#include <boo/audiodev/IAudioVoiceEngine.hpp>

@interface DataOutlineView : NSOutlineView
{
@public
    IBOutlet NSButton* removeDataButton;
    IBOutlet NSMenuItem* deleteMenuItem;
}
@end

@interface SamplesTableController : NSObject <NSTableViewDataSource, NSTableViewDelegate>
{
    AudioGroupFilePresenter* presenter;
}
- (id)initWithAudioGroupPresenter:(AudioGroupFilePresenter*)present;
@end

@interface SFXTableController : NSObject <NSTableViewDataSource, NSTableViewDelegate>
{
    AudioGroupFilePresenter* presenter;
}
- (id)initWithAudioGroupPresenter:(AudioGroupFilePresenter*)present;
@end

@interface AppDelegate : NSObject <NSApplicationDelegate, AudioGroupClient>
{
    IBOutlet NSWindow* mainWindow;
    IBOutlet NSOutlineView* dataOutline;
    IBOutlet NSSearchField* dataSearchField;
    IBOutlet NSTableView* sfxTable;
    IBOutlet NSTableView* samplesTable;
    IBOutlet NSTextView* creditsView;

    IBOutlet NSButton* removeDataButton;
    IBOutlet NSMenuItem* removeDataMenu;

    AudioGroupFilePresenter* groupFilePresenter;

    SamplesTableController* samplesController;
    SFXTableController* sfxController;

@public
    std::unique_ptr<boo::IAudioVoiceEngine> booEngine;
    std::experimental::optional<amuse::BooBackendVoiceAllocator> amuseAllocator;
    std::experimental::optional<amuse::Engine> amuseEngine;
    std::shared_ptr<amuse::Voice> activeSFXVox;
}
- (BOOL)importURL:(NSURL*)url;
- (void)outlineView:(DataOutlineView*)ov selectionChanged:(id)item;
- (void)reloadTables;
- (void)startSFX:(int)sfxId;
- (void)startSample:(int)sampId;
@end

#endif // __AMUSE_AUDIOUNIT_CONTAININGAPP_HPP__