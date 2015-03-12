

#VideoCore
_&copy; 2013-2014 James G Hurley_

VideoCore is a project inteded to be an audio and video manipulation and streaming graph.  It currently works with iOS and periodic (live) sources.  It is a work in progress and will eventually expand to other platforms such as OS X and Android.  **Contributors welcome!** [![Stories in Ready](https://badge.waffle.io/jgh-/VideoCore.png?label=ready&title=Ready)](https://waffle.io/jgh-/VideoCore)

###Table of Contents
* [Setup](#setup)
* [Projects Using VideoCore](#projects-using-videocore)
* [Architecture Overview](#architecture-overview)
* [Version History](#version-history)

##Setup

####CocoaPods

Create a `Podfile` with the contents
``` ruby
platform :ios, '6.0'
pod 'VideoCore', '~> 0.2.0'
```
Next, run `pod install` and open the `xcworkspace` file that is created.

####Sample Application
The SampleBroadcaster project in the sample folder uses CocoaPods to bring in
VideoCore as a dependency:

```
cd sample/SampleBroadcaster
pod install
open SampleBroadcaster.xcworkspace
```

... or you can build from the command-line:
```
xcodebuild -workspace SampleBroadcaster.xcworkspace -scheme SampleBroadcaster build
```
More on CocoaPods: http://cocoapods.org/

##Projects Using VideoCore

Looking for someone to help you with your video streaming project? Feel free to contact jamesghurley@gmail.com

* Cine.io (http://www.cine.io)

_If you would like to be included in this list, either make a pull request or contact jamesghurley@gmail.com_


##Architecture Overview

VideoCore's architecture is inspired by Microsoft Media Foundation (except with saner naming).  Samples start at the source, are passed through a series of transforms, and end up at the output.

e.g. Source (Camera) -> Transform (Composite) -> Transform (H.264 Encode) -> Transform (RTMP Packetize) -> Output (RTMP)

```
videocore/
sources/
videocore::ISource
videocore::IAudioSource : videocore::ISource
videocore::IVideoSource : videocore::ISource
videocore::Watermark : videocore:IVideoSource
iOS/
videocore::iOS::CameraSource : videocore::IVideoSource
Apple/
videocore::Apple::MicrophoneSource : videocore::IAudioSource
OSX/
videocore::OSX::DisplaySource : videocore::IVideoSource
videocore::OSX::SystemAudioSource : videocore::IAudioSource
outputs/
videocore::IOutput
videocore::ITransform : videocore::IOutput
iOS/
videocore::iOS::H264Transform : videocore::ITransform
videocore::iOS::AACTransform  : videocore::ITransform
OSX/
videocore::OSX::H264Transform : videocore::ITransform
videocore::OSX::AACTransform  : videocore::ITransform
RTMP/
videocore::rtmp::H264Packetizer : videocore::ITransform
videocore::rtmp::AACPacketizer : videocore::ITransform

mixers/
videocore::IMixer
videocore::IAudioMixer : videocore::IMixer
videocore::IVideoMixer : videocore::IMixer
videocore::AudioMixer : videocore::IAudioMixer
iOS/
videocore::iOS::GLESVideoMixer : videocore::IVideoMixer
OSX/
videocore::OSX::GLVideoMixer : videocore::IVideoMixer

rtmp/
videocore::RTMPSession : videocore::IOutput

stream/
videocore::IStreamSession
Apple/
videocore::Apple::StreamSession : videocore::IStreamSession

```

##Version History

* 0.3.0
    * Improvements to audio/video timestamps and synchronization
    * Adds an incompatible API call with previous versions.  Custom
    * graphs must now call IMixer::start() to begin mixing.
* 0.2.3
    * Add support for image filters
* 0.2.2
    * Fix video streaking bug when adaptative bitrate is enabled
    * Increase the aggressiveness of the adaptative bitrate algorithm
    * Add internal pixel buffer format
    * 
* 0.2.0
    * Removes deprecated functions
    * Adds Main Profile video
    * Improves adaptive bitrate algorithm
* 0.1.12 
    * Bugfixes
    * Red5 support
    * Improved Adaptive Bitrate algorithm
* 0.1.10
	* Bugfixes
	* Adaptive Bitrate introduced
* 0.1.9
	* Bugfixes, memory leak fixes
	* Introduces the ability to choose whether to use interface orientation or device orientation for Camera orientation.
* 0.1.8
    * Introduces VideoToolbox encoding for iOS 8+ and OS X 10.9+
    * Adds -lc++ for compatibility with Xcode 6
* 0.1.7 
    * Add a simplified iOS API for the common case of streaming camera/microphone
    * Deprecate camera aspect ratio and position
    * Add a matrix transform for Position
    * Add a matrix transform for Aspect Ratio
    * Bugfixes
* 0.1.6
	* Use device orientation for CameraSource rather than interface orientation
* 0.1.5 
	* Add aspect fill to CameraSource
* 0.1.4 
	* Switch from LGPL 2.1 to MIT licensing.
    * Add Camera preview layer. 
    * Add front/back camera toggle.
    * Fix aspect ratio bug in Camera source.
* 0.1.3 
	* Update sample app with a more efficient viewport render
* 0.1.2 
	* Fixes a serious bug in the GenericAudioMixer that was causing 100% cpu usage and audio lag.
* 0.1.1 
 	* Fixes Cocoapods namespace conflicts for UriParser-cpp
* 0.1.0 
	* Initial CocoaPods version

