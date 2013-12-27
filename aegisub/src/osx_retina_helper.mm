#import "retina_helper.h"
#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#import <wx/window.h>

// forward declaration
class wxEvtHandler;

@interface RetinaHelperImpl : NSObject
{
   NSView *view;
   wxEvtHandler* handler;
}
// designated initializer
-(id)initWithView:(NSView *)view handler:(wxEvtHandler *)handler;
-(void)setViewWantsBestResolutionOpenGLSurface:(BOOL)value;
-(BOOL)getViewWantsBestResolutionOpenGLSurface;
-(float)getBackingScaleFactor;
@end

@implementation RetinaHelperImpl

RetinaHelper::RetinaHelper(wxWindow* window) {
	impl = nullptr;
	impl = [[RetinaHelperImpl alloc] initWithView:window->GetHandle() handler:window->GetEventHandler()];
}

RetinaHelper::~RetinaHelper() {
	[(id)impl release];
}

void RetinaHelper::setViewWantsBestResolutionOpenGLSurface (bool aValue) {
	[(id)impl setViewWantsBestResolutionOpenGLSurface:aValue];
}

bool RetinaHelper::getViewWantsBestResolutionOpenGLSurface() {
	return [(id)impl getViewWantsBestResolutionOpenGLSurface];
}

float RetinaHelper::getBackingScaleFactor() {
	return [(id)impl getBackingScaleFactor];
}

-(id)initWithView:(NSView *)aView handler:(wxEvtHandler *)aHandler {
	self = [super init];
	if(self) {
		handler = aHandler;
		view = aView;
		// register for backing change notifications
		NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
		if(nc){
			[nc addObserver:self
			selector:@selector(windowDidChangeBackingProperties:)
			name:NSWindowDidChangeBackingPropertiesNotification
			object:nil];
		}
	}
	return self;
}

-(void) dealloc {
	// unregister from all notifications
	NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
	if(nc){
		[nc removeObserver:self];
	}
	[super dealloc];
}

-(void)setViewWantsBestResolutionOpenGLSurface:(BOOL)value {
	[view setWantsBestResolutionOpenGLSurface:value];
}

-(BOOL)getViewWantsBestResolutionOpenGLSurface {
	return [view wantsBestResolutionOpenGLSurface];
}

-(float)getBackingScaleFactor {
	return [[view window] backingScaleFactor];
}

- (void)windowDidChangeBackingProperties:(NSNotification *)notification {
	auto *theWindow = (NSWindow *)[notification object];
	if(theWindow == [view window]) {
		CGFloat newBackingScaleFactor = [theWindow backingScaleFactor];
		CGFloat oldBackingScaleFactor = [[[notification userInfo]
											objectForKey:@"NSBackingPropertyOldScaleFactorKey"]
											doubleValue];
		if (newBackingScaleFactor != oldBackingScaleFactor) {
			// generate a wx resize event and pass it to the handler's queue
			auto *event = new wxSizeEvent();
			NSRect nsrect = [view bounds];
			wxRect rect(nsrect.origin.x, nsrect.origin.y, nsrect.size.width, nsrect.size.height);
			event->SetRect(rect);
			event->SetSize(rect.GetSize());
			handler->QueueEvent(event);
		}
	}
}
@end

