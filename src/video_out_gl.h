// Copyright (c) 2013, Thomas Goyne <plorkyeran@aegisub.org>
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

/// @file video_out_gl.h
/// @brief OpenGL based video renderer
/// @ingroup video
///

#include <libaegisub/exception.h>

#include <vector>

#ifdef __APPLE__
#include <CoreGraphics/CGDisplayConfiguration.h>
#endif

struct VideoFrame;

/// @class VideoOutGL
/// @brief OpenGL based video renderer
class VideoOutGL {
	struct TextureInfo;

	/// The maximum texture size supported by the user's graphics card
	int maxTextureSize = 0;
	/// Whether rectangular textures are supported by the user's graphics card
	bool supportsRectangularTextures = false;
	/// The internalformat to use
	int internalFormat = 0;

	/// The frame height which the texture grid has been set up for
	int frameWidth = 0;
	/// The frame width which the texture grid has been set up for
	int frameHeight = 0;
	/// The frame format which the texture grid has been set up for
	GLenum frameFormat = 0;
	/// Whether the grid is set up for flipped video
	bool frameFlipped = false;
	/// List of OpenGL texture ids used in the grid
	std::vector<GLuint> textureIdList;
	/// List of precalculated texture display information
	std::vector<TextureInfo> textureList;
	/// OpenGL display list which draws the frames
	GLuint dl = 0;
	/// The total texture count
	int textureCount = 0;
	/// The number of rows of textures
	int textureRows = 0;
	/// The number of columns of textures
	int textureCols = 0;

	void DetectOpenGLCapabilities();
	void InitTextures(int width, int height, GLenum format, int bpp, bool flipped);

	bool needsCapabilityUpdate = true;

#ifdef __APPLE__
	static void CapabilitiesChanged(CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags, void *userInfo);
#endif

	VideoOutGL(const VideoOutGL &) = delete;
	VideoOutGL& operator=(const VideoOutGL&) = delete;
public:
	/// @brief Set the frame to be displayed when Render() is called
	/// @param frame The frame to be displayed
	void UploadFrameData(VideoFrame const& frame);

	/// @brief Render a frame
	/// @param x Bottom left x coordinate
	/// @param y Bottom left y coordinate
	/// @param width Width in pixels of viewport
	/// @param height Height in pixels of viewport
	void Render(int x, int y, int width, int height);

	VideoOutGL();
	~VideoOutGL();
};

/// Base class for all exceptions thrown by VideoOutGL
DEFINE_EXCEPTION(VideoOutException, agi::Exception);

/// An OpenGL error occurred while uploading or displaying a frame
class VideoOutRenderException final : public VideoOutException {
public:
	VideoOutRenderException(const char *func, int err)
	: VideoOutException(std::string(func) + " failed with error code " + std::to_string(err))
	{ }
};

/// An OpenGL error occurred while setting up the video display
class VideoOutInitException final : public VideoOutException {
public:
	VideoOutInitException(const char *func, int err)
	: VideoOutException(std::string(func) + " failed with error code " + std::to_string(err))
	{ }
	VideoOutInitException(const char *err) : VideoOutException(err) { }
};
