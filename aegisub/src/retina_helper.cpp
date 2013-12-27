
#include "retina_helper.h"

// OSX implementation in retina_helper.mm
#ifndef __WXOSX_COCOA__
RetinaHelper::RetinaHelper(wxWindow* window) : window(window) { }
RetinaHelper::~RetinaHelper() { }
void RetinaHelper::setViewWantsBestResolutionOpenGLSurface (bool aValue) { }
bool RetinaHelper::getViewWantsBestResolutionOpenGLSurface() {
	return 1;
}
float RetinaHelper::getBackingScaleFactor() {
	return (float) 1;
}
#endif
