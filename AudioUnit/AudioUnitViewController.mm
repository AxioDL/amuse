#import "AudioUnitViewController.hpp"
#import "AudioUnitBackend.hpp"

#if !__has_feature(objc_arc)
#error ARC Required
#endif

@interface AudioUnitView : NSView
{
    NSButton* m_fileButton;
}
- (void)clickFileButton;
@end

@implementation AudioUnitView

- (id)init
{
    self = [super initWithFrame:NSMakeRect(0, 0, 200, 300)];
    m_fileButton = [[NSButton alloc] initWithFrame:NSMakeRect(100, 100, 30, 10)];
    m_fileButton.target = self;
    m_fileButton.action = @selector(clickFileButton);
    [self addSubview:m_fileButton];
    return self;
}

- (void)clickFileButton
{
    NSLog(@"Click");
}

@end

@interface AudioUnitViewController ()

@end

@implementation AudioUnitViewController {
    AUAudioUnit *audioUnit;
}

- (void) viewDidLoad {
    [super viewDidLoad];

    if (!audioUnit) {
        return;
    }

    // Get the parameter tree and add observers for any parameters that the UI needs to keep in sync with the AudioUnit
}

- (void)loadView
{
    self.view = [AudioUnitView new];
}

- (NSSize)preferredContentSize
{
    return NSMakeSize(200, 300);
}

- (NSSize)preferredMaximumSize
{
    return NSMakeSize(200, 300);
}

- (NSSize)preferredMinimumSize
{
    return NSMakeSize(200, 300);
}

- (AUAudioUnit*)createAudioUnitWithComponentDescription:(AudioComponentDescription)desc error:(NSError**)error {
    audioUnit = [[AmuseAudioUnit alloc] initWithComponentDescription:desc error:error];
    return audioUnit;
}

@end
