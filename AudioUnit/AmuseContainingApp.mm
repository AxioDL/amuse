#import <AppKit/AppKit.h>
#import <AudioUnit/AudioUnit.h>
#import <CoreAudioKit/AUViewController.h>
#import "AudioUnitViewController.hpp"

@interface AppDelegate : NSObject <NSApplicationDelegate>
{}
@end

@interface ViewController : NSViewController
{
    NSButton* playButton;
    AudioUnitViewController* amuseVC;
}

@property (weak) IBOutlet NSView *containerView;
-(IBAction)togglePlay:(id)sender;

@end

@implementation ViewController

- (void)viewDidLoad {
  [super viewDidLoad];
#if 0
  AudioComponentDescription desc;
  /*  Supply the correct AudioComponentDescription based on your AudioUnit type, manufacturer and creator.

   You need to supply matching settings in the AUAppExtension info.plist under:

   NSExtension
   NSExtensionAttributes
   AudioComponents
   Item 0
   type
   subtype
   manufacturer

   If you do not do this step, your AudioUnit will not work!!!
   */
  desc.componentType = kAudioUnitType_MusicDevice;
  desc.componentSubType = 'sin3';
  desc.componentManufacturer = 'Demo';
  desc.componentFlags = 0;
  desc.componentFlagsMask = 0;

  [AUAudioUnit registerSubclass: AUv3InstrumentDemo.class asComponentDescription:desc name:@"Local InstrumentDemo" version: UINT32_MAX];

  playEngine = [[SimplePlayEngine alloc] initWithComponentType: desc.componentType componentsFoundCallback: nil];
  [playEngine selectAudioUnitWithComponentDescription2:desc completionHandler:^{
      [self connectParametersToControls];
  }];
#endif
}

-(void)loadView {
  amuseVC = [[AudioUnitViewController alloc] initWithNibName:nil bundle:nil];
  self.view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 200, 300)];
  [self.view addSubview:amuseVC.view];

  self.view.translatesAutoresizingMaskIntoConstraints = NO;

  NSArray *constraints = [NSLayoutConstraint constraintsWithVisualFormat: @"H:|-[view]-|" options:0 metrics:nil views:NSDictionaryOfVariableBindings(self.view)];
  [self.view addConstraints: constraints];

  constraints = [NSLayoutConstraint constraintsWithVisualFormat: @"V:|-[view]-|" options:0 metrics:nil views:NSDictionaryOfVariableBindings(self.view)];
  [self.view addConstraints: constraints];
}

@end

@implementation AppDelegate

@end

int main(int argc, const char * argv[]) {
    NSApplication* app = [NSApplication sharedApplication];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];

    /* Delegate (OS X callbacks) */
    AppDelegate* delegate = [AppDelegate new];
    [app setDelegate:delegate];
    [app run];
    return 0;
}
