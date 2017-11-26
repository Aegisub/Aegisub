// Copyright (c) 2012, Thomas Goyne <plorkyeran@aegisub.org>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
// Aegisub Project http://www.aegisub.org/

/// @file video_out_gl.cpp
/// @brief OpenGL based video renderer
/// @ingroup video
///

#include <algorithm>
#include <utility>

#include <libaegisub/log.h>

// These must be included before local headers.
#ifdef HAVE_OPENGL_GL_H
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "video_out_gl.h"
#include "utils.h"
#include "video_frame.h"

namespace {
template<typename Exception>
BOOST_NOINLINE void throw_error(GLenum err, const char *msg) {
	LOG_E("video/out/gl") << msg << " failed with error code " << err;
	throw Exception(msg, err);
}
}

#define DO_CHECK_ERROR(cmd, Exception, msg) \
	do { \
		cmd; \
		GLenum err = glGetError(); \
		if (BOOST_UNLIKELY(err)) \
			throw_error<Exception>(err, msg); \
	} while(0);
#define CHECK_INIT_ERROR(cmd) DO_CHECK_ERROR(cmd, VideoOutInitException, #cmd)
#define CHECK_ERROR(cmd) DO_CHECK_ERROR(cmd, VideoOutRenderException, #cmd)

/// @brief Structure tracking all precomputable information about a subtexture
struct VideoOutGL::TextureInfo {
	GLuint textureID = 0;
	int dataOffset = 0;
	int sourceH = 0;
	int sourceW = 0;
};

/// @brief Test if a texture can be created
/// @param width The width of the texture
/// @param height The height of the texture
/// @param format The texture's format
/// @return Whether the texture could be created.
static bool TestTexture(int width, int height, GLint format) {
	glTexImage2D(GL_PROXY_TEXTURE_2D, 0, format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
	while (glGetError()) { } // Silently swallow all errors as we don't care why it failed if it did

	LOG_I("video/out/gl") << "VideoOutGL::TestTexture: " << width << "x" << height;
	return format != 0;
}

VideoOutGL::VideoOutGL() {
#ifdef __APPLE__
	CGDisplayRegisterReconfigurationCallback(VideoOutGL::CapabilitiesChanged, this);
#endif
}

/// @brief Runtime detection of required OpenGL capabilities
void VideoOutGL::DetectOpenGLCapabilities() {
	if (!needsCapabilityUpdate) return;
	needsCapabilityUpdate = false;

	// Test for supported internalformats
	if (TestTexture(64, 64, GL_RGBA8)) internalFormat = GL_RGBA8;
	else if (TestTexture(64, 64, GL_RGBA)) internalFormat = GL_RGBA;
	else throw VideoOutInitException("Could not create a 64x64 RGB texture in any format.");

	// Test for the maximum supported texture size
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
	while (maxTextureSize > 64 && !TestTexture(maxTextureSize, maxTextureSize, internalFormat)) maxTextureSize >>= 1;
	LOG_I("video/out/gl") << "Maximum texture size is " << maxTextureSize << "x" << maxTextureSize;

	// Test for rectangular texture support
	supportsRectangularTextures = TestTexture(maxTextureSize, maxTextureSize >> 1, internalFormat);
}

/// @brief If needed, create the grid of textures for displaying frames of the given format
/// @param width The frame's width
/// @param height The frame's height
/// @param format The frame's format
/// @param bpp The frame's bytes per pixel
void VideoOutGL::InitTextures(int width, int height, GLenum format, int bpp, bool flipped) {
	using namespace std;

	// Do nothing if the frame size and format are unchanged
	if (width == frameWidth && height == frameHeight && format == frameFormat && flipped == frameFlipped) return;
	frameWidth  = width;
	frameHeight = height;
	frameFormat = format;
	frameFlipped = flipped;
	LOG_I("video/out/gl") << "Video size: " << width << "x" << height;

	DetectOpenGLCapabilities();

	// Clean up old textures
	if (textureIdList.size() > 0) {
		CHECK_INIT_ERROR(glDeleteTextures(textureIdList.size(), &textureIdList[0]));
		textureIdList.clear();
		textureList.clear();
	}

	// Create the textures
	int textureArea = maxTextureSize - 2;
	textureRows  = (int)ceil(double(height) / textureArea);
	textureCols  = (int)ceil(double(width) / textureArea);
	textureCount = textureRows * textureCols;
	textureIdList.resize(textureCount);
	textureList.resize(textureCount);
	CHECK_INIT_ERROR(glGenTextures(textureIdList.size(), &textureIdList[0]));
	vector<pair<int, int>> textureSizes;
	textureSizes.reserve(textureCount);

	/* Unfortunately, we can't simply use one of the two standard ways to do
	 * tiled textures to work around texture size limits in OpenGL, due to our
	 * need to support Microsoft's OpenGL emulation for RDP/VPC/video card
	 * drivers that don't support OpenGL (such as the ones which Windows
	 * Update pushes for ATI cards in Windows 7). GL_CLAMP_TO_EDGE requires
	 * OpenGL 1.2, but the emulation only supports 1.1. GL_CLAMP + borders has
	 * correct results, but takes several seconds to render each frame. As a
	 * result, the code below essentially manually reimplements borders, by
	 * just not using the edge when mapping the texture onto a quad. The one
	 * exception to this is the texture edges which are also frame edges, as
	 * there does not appear to be a trivial way to mirror the edges, and the
	 * nontrivial ways are more complex that is worth to avoid a single row of
	 * slightly discolored pixels along the edges at zooms over 100%.
	 *
	 * Given a 64x64 maximum texture size:
	 *     Quads touching the top of the frame are 63 pixels tall
	 *     Quads touching the bottom of the frame are up to 63 pixels tall
	 *     All other quads are 62 pixels tall
	 *     Quads not on the top skip the first row of the texture
	 *     Quads not on the bottom skip the last row of the texture
	 *     Width behaves in the same way with respect to left/right edges
	 */

	// Set up the display list
	CHECK_ERROR(dl = glGenLists(1));
	CHECK_ERROR(glNewList(dl, GL_COMPILE));

	CHECK_ERROR(glClearColor(0,0,0,0));
	CHECK_ERROR(glClearStencil(0));
	CHECK_ERROR(glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));

	CHECK_ERROR(glShadeModel(GL_FLAT));
	CHECK_ERROR(glDisable(GL_BLEND));

	// Switch to video coordinates
	CHECK_ERROR(glMatrixMode(GL_PROJECTION));
	CHECK_ERROR(glLoadIdentity());
	CHECK_ERROR(glPushMatrix());
	if (frameFlipped) {
		CHECK_ERROR(glOrtho(0.0f, frameWidth, 0.0f, frameHeight, -1000.0f, 1000.0f));
	}
	else {
		CHECK_ERROR(glOrtho(0.0f, frameWidth, frameHeight, 0.0f, -1000.0f, 1000.0f));
	}

	CHECK_ERROR(glEnable(GL_TEXTURE_2D));

	// Calculate the position information for each texture
	int lastRow = textureRows - 1;
	int lastCol = textureCols - 1;
	for (int row = 0; row < textureRows; ++row) {
		for (int col = 0; col < textureCols; ++col) {
			TextureInfo& ti = textureList[row * textureCols + col];

			// Width and height of the area read from the frame data
			int sourceX = col * textureArea;
			int sourceY = row * textureArea;
			ti.sourceW  = std::min(frameWidth  - sourceX, maxTextureSize);
			ti.sourceH  = std::min(frameHeight - sourceY, maxTextureSize);

			// Used instead of GL_PACK_SKIP_ROWS/GL_PACK_SKIP_PIXELS due to
			// performance issues with the emulation
			ti.dataOffset = sourceY * frameWidth * bpp + sourceX * bpp;

			int textureHeight = SmallestPowerOf2(ti.sourceH);
			int textureWidth  = SmallestPowerOf2(ti.sourceW);
			if (!supportsRectangularTextures) {
				textureWidth = textureHeight = std::max(textureWidth, textureHeight);
			}

			// Location where this texture is placed
			// X2/Y2 will be offscreen unless the video frame happens to
			// exactly use all of the texture
			float x1 = sourceX + (col != 0);
			float y1 = sourceY + (row != 0);
			float x2 = sourceX + textureWidth - (col != lastCol);
			float y2 = sourceY + textureHeight - (row != lastRow);

			// Portion of the texture actually used
			float top    = row == 0 ? 0 : 1.0f / textureHeight;
			float left   = col == 0 ? 0 : 1.0f / textureWidth;
			float bottom = row == lastRow ? 1.0f : 1.0f - 1.0f / textureHeight;
			float right  = col == lastCol ? 1.0f : 1.0f - 1.0f / textureWidth;

			// Store the stuff needed later
			ti.textureID = textureIdList[row * textureCols + col];
			textureSizes.push_back(make_pair(textureWidth, textureHeight));

			CHECK_ERROR(glBindTexture(GL_TEXTURE_2D, ti.textureID));
			CHECK_ERROR(glColor4f(1.0f, 1.0f, 1.0f, 1.0f));

			// Place the texture
			glBegin(GL_QUADS);
				glTexCoord2f(left,  top);     glVertex2f(x1, y1);
				glTexCoord2f(right, top);     glVertex2f(x2, y1);
				glTexCoord2f(right, bottom);  glVertex2f(x2, y2);
				glTexCoord2f(left,  bottom);  glVertex2f(x1, y2);
			glEnd();
			if (GLenum err = glGetError()) throw VideoOutRenderException("GL_QUADS", err);
		}
	}
	CHECK_ERROR(glDisable(GL_TEXTURE_2D));
	CHECK_ERROR(glPopMatrix());

	glEndList();

	// Create the textures outside of the display list as there's no need to
	// remake them on every frame
	for (int i = 0; i < textureCount; ++i) {
		LOG_I("video/out/gl") << "Using texture size: " << textureSizes[i].first << "x" << textureSizes[i].second;
		CHECK_INIT_ERROR(glBindTexture(GL_TEXTURE_2D, textureIdList[i]));
		CHECK_INIT_ERROR(glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, textureSizes[i].first, textureSizes[i].second, 0, format, GL_UNSIGNED_BYTE, nullptr));
		CHECK_INIT_ERROR(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		CHECK_INIT_ERROR(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
		CHECK_INIT_ERROR(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP));
		CHECK_INIT_ERROR(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP));
	}
}

#ifdef __APPLE__
void VideoOutGL::CapabilitiesChanged(CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags, void *userInfo) {
	if (flags & kCGDisplaySetModeFlag) {
		VideoOutGL *self = (VideoOutGL*)userInfo;
		self->needsCapabilityUpdate = true;
	}
}
#endif

void VideoOutGL::UploadFrameData(VideoFrame const& frame) {
	if (frame.height == 0 || frame.width == 0) return;

	InitTextures(frame.width, frame.height, GL_BGRA_EXT, 4, frame.flipped);

	// Set the row length, needed to be able to upload partial rows
	CHECK_ERROR(glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.pitch / 4));

	for (auto& ti : textureList) {
		CHECK_ERROR(glBindTexture(GL_TEXTURE_2D, ti.textureID));
		CHECK_ERROR(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ti.sourceW,
			ti.sourceH, GL_BGRA_EXT, GL_UNSIGNED_BYTE, &frame.data[ti.dataOffset]));
	}

	CHECK_ERROR(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
}

void VideoOutGL::Render(int dx1, int dy1, int dx2, int dy2) {
	CHECK_ERROR(glViewport(dx1, dy1, dx2, dy2));
	CHECK_ERROR(glCallList(dl));
	CHECK_ERROR(glMatrixMode(GL_MODELVIEW));
	CHECK_ERROR(glLoadIdentity());

}

VideoOutGL::~VideoOutGL() {
	if (textureIdList.size() > 0) {
		glDeleteTextures(textureIdList.size(), &textureIdList[0]);
		glDeleteLists(dl, 1);
	}
	CGDisplayRemoveReconfigurationCallback(VideoOutGL::CapabilitiesChanged, this);
}
