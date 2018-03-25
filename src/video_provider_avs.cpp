// Copyright (c) 2006, Fredrik Mellbin
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

#ifdef WITH_AVISYNTH
#include "include/aegisub/video_provider.h"

#include "options.h"
#include "video_frame.h"

#include <libaegisub/access.h>
#include <libaegisub/charset_conv.h>
#include <libaegisub/fs.h>
#include <libaegisub/log.h>
#include <libaegisub/path.h>
#include <libaegisub/make_unique.h>

#include <boost/algorithm/string/predicate.hpp>
#include <mutex>

#ifdef _WIN32
#include <vfw.h>
#endif

#define VideoFrame AVSVideoFrame
#include "avisynth.h"
#undef VideoFrame
#include "avisynth_wrap.h"

namespace {
class AvisynthVideoProvider: public VideoProvider {
	AviSynthWrapper avs;
	std::string decoder_name;
	agi::vfr::Framerate fps;
	std::vector<int> keyframes;
	std::string warning;
	std::string colorspace;
	std::string real_colorspace;
	bool has_audio = false;

	AVSValue source_clip;
	PClip RGB32Video;
	VideoInfo vi;

	AVSValue Open(agi::fs::path const& filename);
	void Init(std::string const& matrix);

public:
	AvisynthVideoProvider(agi::fs::path const& filename, std::string const& colormatrix);

	void GetFrame(int n, VideoFrame &frame) override;

	void SetColorSpace(std::string const& matrix) override {
		// Can't really do anything if this fails
		try { Init(matrix); } catch (AvisynthError const&) { }
	}

	int GetFrameCount() const override             { return vi.num_frames; }
	agi::vfr::Framerate GetFPS() const override    { return fps; }
	int GetWidth() const override                  { return vi.width; }
	int GetHeight() const override                 { return vi.height; }
	double GetDAR() const override                 { return 0; }
	std::vector<int> GetKeyFrames() const override { return keyframes; }
	std::string GetWarning() const override        { return warning; }
	std::string GetDecoderName() const override    { return decoder_name; }
	std::string GetColorSpace() const override     { return colorspace; }
	std::string GetRealColorSpace() const override { return real_colorspace; }
	bool HasAudio() const override                 { return has_audio; }
};

AvisynthVideoProvider::AvisynthVideoProvider(agi::fs::path const& filename, std::string const& colormatrix) {
	agi::acs::CheckFileRead(filename);

	std::lock_guard<std::mutex> lock(avs.GetMutex());

#ifdef _WIN32
	if (agi::fs::HasExtension(filename, "avi")) {
		// Try to read the keyframes before actually opening the file as trying
		// to open the file while it's already open can cause problems with
		// badly written VFW decoders
		AVIFileInit();

		PAVIFILE pfile;
		long hr = AVIFileOpen(&pfile, filename.c_str(), OF_SHARE_DENY_WRITE, 0);
		if (hr) {
			warning = "Unable to open AVI file for reading keyframes:\n";
			switch (hr) {
				case AVIERR_BADFORMAT:
					warning += "The file is corrupted, incomplete or has an otherwise bad format.";
					break;
				case AVIERR_MEMORY:
					warning += "The file could not be opened because of insufficient memory.";
					break;
				case AVIERR_FILEREAD:
					warning += "An error occurred reading the file. There might be a problem with the storage media.";
					break;
				case AVIERR_FILEOPEN:
					warning += "The file could not be opened. It might be in use by another application, or you do not have permission to access it.";
					break;
				case REGDB_E_CLASSNOTREG:
					warning += "There is no handler installed for the file extension. This might indicate a fundamental problem in your Video for Windows installation, and can be caused by extremely stripped Windows installations.";
					break;
				default:
					warning += "Unknown error.";
					break;
			}
			goto file_exit;
		}

		PAVISTREAM ppavi;
		if (hr = AVIFileGetStream(pfile, &ppavi, streamtypeVIDEO, 0)) {
			warning = "Unable to open AVI video stream for reading keyframes:\n";
			switch (hr) {
				case AVIERR_NODATA:
					warning += "The file does not contain a usable video stream.";
					break;
				case AVIERR_MEMORY:
					warning += "Not enough memory.";
					break;
				default:
					warning += "Unknown error.";
					break;
			}
			goto file_release;
		}

		AVISTREAMINFO avis;
		if (FAILED(AVIStreamInfo(ppavi,&avis,sizeof(avis)))) {
			warning = "Unable to read keyframes from AVI file:\nCould not get stream information.";
			goto stream_release;
		}

		for (size_t i = 0; i < avis.dwLength; i++) {
			if (AVIStreamIsKeyFrame(ppavi, i))
				keyframes.push_back(i);
		}

		// If every frame is a keyframe then just discard the keyframe data as it's useless
		if (keyframes.size() == (size_t)avis.dwLength)
			keyframes.clear();

		// Clean up
stream_release:
		AVIStreamRelease(ppavi);
file_release:
		AVIFileRelease(pfile);
file_exit:
		AVIFileExit();
	}
#endif

	try {
		source_clip = Open(filename);
		Init(colormatrix);
	}
	catch (AvisynthError const& err) {
		throw VideoOpenError("Avisynth error: " + std::string(err.msg));
	}
}

void AvisynthVideoProvider::Init(std::string const& colormatrix) {
	auto script = source_clip;
	vi = script.AsClip()->GetVideoInfo();
	has_audio = vi.HasAudio();
	if (vi.IsRGB())
		real_colorspace = colorspace = "None";
	else {
		/// @todo maybe read ColorMatrix hints for d2v files?
		AVSValue args[2] = { script, "Rec709" };
		bool bt709 = vi.width > 1024 || vi.height >= 600;
		if (colormatrix == "TV.601") {
			args[1] = "Rec601";
			colorspace = "TV.601"
		}
		else {
			colorspace = "TV.709";
		}
		real_colorspace = bt709 ? "TV.709" : "TV.601"
		const char *argnames[2] = { 0, "matrix" };
		script = avs.GetEnv()->Invoke("ConvertToRGB32", AVSValue(args, 2), argnames);
	}

	RGB32Video = avs.GetEnv()->Invoke("Cache", script).AsClip();
	vi = RGB32Video->GetVideoInfo();
	fps = (double)vi.fps_numerator / vi.fps_denominator;
}

AVSValue AvisynthVideoProvider::Open(agi::fs::path const& filename) {
	IScriptEnvironment *env = avs.GetEnv();
	char *videoFilename = env->SaveString(agi::fs::ShortName(filename).c_str());

	// Avisynth file, just import it
	if (agi::fs::HasExtension(filename, "avs")) {
		LOG_I("avisynth/video") << "Opening .avs file with Import";
		decoder_name = "Avisynth/Import";
		return env->Invoke("Import", videoFilename);
	}

	// Open avi file with AviSource
	if (agi::fs::HasExtension(filename, "avi")) {
		LOG_I("avisynth/video") << "Opening .avi file with AviSource";
		try {
			const char *argnames[2] = { 0, "audio" };
			AVSValue args[2] = { videoFilename, false };
			decoder_name = "Avisynth/AviSource";
			return env->Invoke("AviSource", AVSValue(args,2), argnames);
		}
		// On Failure, fallback to DSS
		catch (AvisynthError &err) {
			LOG_E("avisynth/video") << err.msg;
			LOG_I("avisynth/video") << "Failed to open .avi file with AviSource, trying DirectShowSource";
		}
	}

	// Open d2v with mpeg2dec3
	if (agi::fs::HasExtension(filename, "d2v") && env->FunctionExists("Mpeg2Dec3_Mpeg2Source")) {
		LOG_I("avisynth/video") << "Opening .d2v file with Mpeg2Dec3_Mpeg2Source";
		auto script = env->Invoke("Mpeg2Dec3_Mpeg2Source", videoFilename);
		decoder_name = "Avisynth/Mpeg2Dec3_Mpeg2Source";

		//if avisynth is 2.5.7 beta 2 or newer old mpeg2decs will crash without this
		if (env->FunctionExists("SetPlanarLegacyAlignment")) {
			AVSValue args[2] = { script, true };
			script = env->Invoke("SetPlanarLegacyAlignment", AVSValue(args,2));
		}
		return script;
	}

	// If that fails, try opening it with DGDecode
	if (agi::fs::HasExtension(filename, "d2v") && env->FunctionExists("DGDecode_Mpeg2Source")) {
		LOG_I("avisynth/video") << "Opening .d2v file with DGDecode_Mpeg2Source";
		decoder_name = "DGDecode_Mpeg2Source";
		return env->Invoke("Avisynth/Mpeg2Source", videoFilename);

		//note that DGDecode will also have issues like if the version is too
		// ancient but no sane person would use that anyway
	}

	if (agi::fs::HasExtension(filename, "d2v") && env->FunctionExists("Mpeg2Source")) {
		LOG_I("avisynth/video") << "Opening .d2v file with other Mpeg2Source";
		AVSValue script = env->Invoke("Mpeg2Source", videoFilename);
		decoder_name = "Avisynth/Mpeg2Source";

		//if avisynth is 2.5.7 beta 2 or newer old mpeg2decs will crash without this
		if (env->FunctionExists("SetPlanarLegacyAlignment"))
			script = env->Invoke("SetPlanarLegacyAlignment", script);

		return script;
	}

	// Try loading DirectShowSource2
	if (!env->FunctionExists("dss2")) {
		auto dss2path(config::path->Decode("?data/avss.dll"));
		if (agi::fs::FileExists(dss2path))
			env->Invoke("LoadPlugin", env->SaveString(agi::fs::ShortName(dss2path).c_str()));
	}

	// If DSS2 loaded properly, try using it
	if (env->FunctionExists("dss2")) {
		LOG_I("avisynth/video") << "Opening file with DSS2";
		decoder_name = "Avisynth/DSS2";
		return env->Invoke("DSS2", videoFilename);
	}

	// Try DirectShowSource
	// Load DirectShowSource.dll from app dir if it exists
	auto dsspath(config::path->Decode("?data/DirectShowSource.dll"));
	if (agi::fs::FileExists(dsspath))
		env->Invoke("LoadPlugin", env->SaveString(agi::fs::ShortName(dsspath).c_str()));

	// Then try using DSS
	if (env->FunctionExists("DirectShowSource")) {
		const char *argnames[3] = { 0, "video", "audio" };
		AVSValue args[3] = { videoFilename, true, false };
		decoder_name = "Avisynth/DirectShowSource";
		warning = "Warning! The file is being opened using Avisynth's DirectShowSource, which has unreliable seeking. Frame numbers might not match the real number. PROCEED AT YOUR OWN RISK!";
		LOG_I("avisynth/video") << "Opening file with DirectShowSource";
		return env->Invoke("DirectShowSource", AVSValue(args,3), argnames);
	}

	// Failed to find a suitable function
	LOG_E("avisynth/video") << "DSS function not found";
	throw VideoNotSupported("No function suitable for opening the video found");
}

void AvisynthVideoProvider::GetFrame(int n, VideoFrame &out) {
	std::lock_guard<std::mutex> lock(avs.GetMutex());

	auto frame = RGB32Video->GetFrame(n, avs.GetEnv());
	auto ptr = frame->GetReadPtr();
	out.data.assign(ptr, ptr + frame->GetPitch() * frame->GetHeight());
	out.flipped = true;
	out.height = frame->GetHeight();
	out.width = frame->GetRowSize() / 4;
	out.pitch = frame->GetPitch();
}
}

namespace agi { class BackgroundRunner; }
std::unique_ptr<VideoProvider> CreateAvisynthVideoProvider(agi::fs::path const& path, std::string const& colormatrix, agi::BackgroundRunner *) {
	return agi::make_unique<AvisynthVideoProvider>(path, colormatrix);
}
#endif // HAVE_AVISYNTH
