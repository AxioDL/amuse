#ifndef __AMUSE_AUDIOUNIT_CONTAININGAPP_HPP__
#define __AMUSE_AUDIOUNIT_CONTAININGAPP_HPP__

#import <AppKit/AppKit.h>
#import "AudioGroupFilePresenter.hpp"

@interface DataOutlineView : NSOutlineView
{
    @public
    IBOutlet NSButton* removeDataButton;
    IBOutlet NSMenuItem* deleteMenuItem;
}
@end

@interface SamplesTableController : NSObject <NSTableViewDataSource, NSTableViewDelegate>
{
    
}
@end

@interface SFXTableController : NSObject <NSTableViewDataSource, NSTableViewDelegate>
{
    
}
@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
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
}
- (BOOL)importURL:(NSURL*)url;
- (void)outlineView:(DataOutlineView*)ov selectionChanged:(id)item;
@end

#endif // __AMUSE_AUDIOUNIT_CONTAININGAPP_HPP__