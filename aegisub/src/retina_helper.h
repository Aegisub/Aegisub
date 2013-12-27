#pragma once

class wxWindow;

class RetinaHelper {
public:
	RetinaHelper(wxWindow* window);
	~RetinaHelper();
	
	void setViewWantsBestResolutionOpenGLSurface(bool value);
	bool getViewWantsBestResolutionOpenGLSurface();
	float getBackingScaleFactor();
	
private:
	void* impl; // pointer to obj-c++ implementation
};
