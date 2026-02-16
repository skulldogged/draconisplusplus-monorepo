/**
 * @file now_playing_macos.mm
 * @brief macOS-specific implementation for Now Playing plugin
 *
 * This file contains the Objective-C++ code for fetching now playing
 * information via the macOS MediaRemote private framework.
 *
 * Note: On macOS 15.4+, Apple restricted MediaRemote access to binaries
 * with a com.apple.* signing identifier. The build system handles this
 * by codesigning the binary with a spoofed identifier at install time.
 */

#ifdef __APPLE__

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

#include <Drac++/Utils/Error.hpp>

#include "now_playing_types.hpp"

using namespace draconis::utils::types;
using namespace draconis::utils::error;
using enum DracErrorCode;

namespace now_playing::macos {
  // Forward-declare the function pointer type for the private MediaRemote API.
  using MRMediaRemoteGetNowPlayingInfoFn =
    void (*)(dispatch_queue_t queue, void (^handler)(NSDictionary* information));

  namespace {
    /**
     * @brief Fetch now playing info using native MediaRemote API
     */
    auto fetchViaNativeApi() -> Result<MediaData> {
      // Since MediaRemote.framework is private, we cannot link against it directly.
      // Instead, it must be loaded at runtime using CFURL and CFBundle.
      CFURLRef urlRef = CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault,
        CFSTR("/System/Library/PrivateFrameworks/MediaRemote.framework"),
        kCFURLPOSIXPathStyle,
        false
      );

      if (!urlRef)
        ERR(NotFound, "Failed to create CFURL for MediaRemote.framework");

      // Create a bundle from the URL
      CFBundleRef bundleRef = CFBundleCreate(kCFAllocatorDefault, urlRef);
      CFRelease(urlRef);

      if (!bundleRef)
        ERR(ApiUnavailable, "Failed to create bundle for MediaRemote.framework");

      // Get a pointer to the MRMediaRemoteGetNowPlayingInfo function from the bundle.
      auto mrMediaRemoteGetNowPlayingInfo = std::bit_cast<MRMediaRemoteGetNowPlayingInfoFn>(
        CFBundleGetFunctionPointerForName(bundleRef, CFSTR("MRMediaRemoteGetNowPlayingInfo"))
      );

      if (!mrMediaRemoteGetNowPlayingInfo) {
        CFRelease(bundleRef);
        ERR(ApiUnavailable, "Failed to get MRMediaRemoteGetNowPlayingInfo function pointer");
      }

      // A semaphore is used to make this asynchronous call behave synchronously.
      __block Result<MediaData>  result;
      const dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

      mrMediaRemoteGetNowPlayingInfo(
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
        ^(NSDictionary* information) {
          if (!information) {
            result = Err(DracError(NotFound, "No media is currently playing"));
          } else {
            MediaData data;

            // Extract the title, artist, and album from the dictionary
            const NSString* const titleNS  = [information objectForKey:@"kMRMediaRemoteNowPlayingInfoTitle"];
            const NSString* const artistNS = [information objectForKey:@"kMRMediaRemoteNowPlayingInfoArtist"];
            const NSString* const albumNS  = [information objectForKey:@"kMRMediaRemoteNowPlayingInfoAlbum"];

            if (titleNS)
              data.title = String([titleNS UTF8String]);

            if (artistNS)
              data.artist = String([artistNS UTF8String]);

            if (albumNS)
              data.album = String([albumNS UTF8String]);

            // If we got no title, consider it as no media playing
            if (!data.title) {
              result = Err(DracError(NotFound, "No media is currently playing"));
            } else {
              result = data;
            }
          }

          dispatch_semaphore_signal(semaphore);
        }
      );

      // Block until the callback signals completion
      dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

      // Note: We intentionally don't release bundleRef here.
      // CFBundleCreate may return a cached bundle, and releasing it
      // could cause issues. The bundle will be cleaned up when the
      // process exits.

      return result;
    }
  } // namespace

  auto fetchNowPlaying() -> Result<MediaData> {
    @autoreleasepool {
      return fetchViaNativeApi();
    }
  }
} // namespace now_playing::macos

#endif // __APPLE__
