
VideoCore
(c) 2013-2014 James G Hurley

SETUP

git clone git@github.com:jamesghurley/VideoCore.git


Find an example of setting up a transform graph at

https://github.com/jamesghurley/VideoCore/blob/master/sample

The SampleBroadcaster project in the sample folder uses CocoaPods to bring in
VideoCore as a dependency:

cd sample/SampleBroadcaster
pod install
open SampleBroadcaster.xcworkspace

... or you can build from the command-line:

xcodebuild -workspace SampleBroadcaster.xcworkspace -scheme SampleBroadcaster build

More on CocoaPods: http://cocoapods.org/

=========
Version history:

0.1.5 ~ Add aspect fill to CameraSource
0.1.4 ~ Switch from LGPL 2.1 to MIT licensing.
      ~ Add Camera preview layer. 
      ~ Add front/back camera toggle.
      ~ Fix aspect ratio bug in Camera source.

0.1.3 ~ Update sample app with a more efficient viewport render
0.1.2 ~ Fixes a serious bug in the GenericAudioMixer that was causing 100% cpu usage and audio lag.
0.1.1 ~ Fixes Cocoapods namespace conflicts for UriParser-cpp
0.1.0 ~ Initial CocoaPods version

=========

This is a work-in-progress library with the intention of being an audio and video manipulation
pipeline for iOS and Mac OS X.

Projects using VideoCore:

- MobCrush (www.mobcrush.com)

If you would like to be included in this list, either make a pull request or contact jamesghurley<at>gmail<dot>com

=========


e.g. Source (GLES) -> Transform (Composite) -> Transform (H.264 Encode) -> Transform (RTMP Packetize) -> Output (RTMP)

videocore/

    sources/
        videocore::ISource
        videocore::IAudioSource : videocore::ISource
        videocore::IVideoSource : videocore::ISource
        videocore::Watermark : videocore:IVideoSource
            iOS/
                videocore::iOS::GLESSource : videocore::IVideoSource
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


============

