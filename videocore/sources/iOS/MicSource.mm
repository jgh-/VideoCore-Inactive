/*

 Video Core
 Copyright (c) 2014 James G. Hurley

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.

 */
#include "MicSource.h"
#include <dlfcn.h>
#include <videocore/mixers/IAudioMixer.hpp>
#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>

#define SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(v)  ([[[UIDevice currentDevice] systemVersion] compare:v options:NSNumericSearch] != NSOrderedAscending)


@implementation InterruptionHandler

- (void) handleInterruption:(NSNotification*)notification
{
    NSDictionary* userInfo = notification.userInfo;
    
    if([userInfo[AVAudioSessionInterruptionTypeKey] intValue] == AVAudioSessionInterruptionTypeBegan) {
        _source->interruptionBegan();
    } else {
        _source->interruptionEnded();
    }
}

@end
        
        
static std::weak_ptr<videocore::iOS::MicSource> s_micSource;

static OSStatus handleInputBuffer(void *inRefCon,
                                  AudioUnitRenderActionFlags *ioActionFlags,
                                  const AudioTimeStamp *inTimeStamp,
                                  UInt32 inBusNumber,
                                  UInt32 inNumberFrames,
                                  AudioBufferList *ioData)
{
    videocore::iOS::MicSource* mc =static_cast<videocore::iOS::MicSource*>(inRefCon);

    AudioBuffer buffer;
    buffer.mData = NULL;
    buffer.mDataByteSize = 0;
    buffer.mNumberChannels = 2;

    AudioBufferList buffers;
    buffers.mNumberBuffers = 1;
    buffers.mBuffers[0] = buffer;

    OSStatus status = AudioUnitRender(mc->audioUnit(),
                                      ioActionFlags,
                                      inTimeStamp,
                                      inBusNumber,
                                      inNumberFrames,
                                      &buffers);

    if(!status) {
        mc->inputCallback((uint8_t*)buffers.mBuffers[0].mData, buffers.mBuffers[0].mDataByteSize, inNumberFrames);
    }
    return status;
}


namespace videocore { namespace iOS {

    MicSource::MicSource(double sampleRate, int channelCount, std::function<void(AudioUnit&)> excludeAudioUnit)
    : m_sampleRate(sampleRate), m_channelCount(channelCount), m_audioUnit(nullptr), m_component(nullptr)
    {
        

        AVAudioSession *session = [AVAudioSession sharedInstance];

        __block MicSource* bThis = this;

        PermissionBlock permission = ^(BOOL granted) {
            if(granted) {

                [session setCategory:AVAudioSessionCategoryPlayAndRecord withOptions:AVAudioSessionCategoryOptionDefaultToSpeaker | AVAudioSessionCategoryOptionMixWithOthers error:nil];
                //[session setMode:AVAudioSessionModeVideoChat error:nil];
                [session setActive:YES error:nil];
                
                AudioComponentDescription acd;
                acd.componentType = kAudioUnitType_Output;
                acd.componentSubType = kAudioUnitSubType_RemoteIO;
                acd.componentManufacturer = kAudioUnitManufacturer_Apple;
                acd.componentFlags = 0;
                acd.componentFlagsMask = 0;
                
                bThis->m_component = AudioComponentFindNext(NULL, &acd);
                
                AudioComponentInstanceNew(bThis->m_component, &bThis->m_audioUnit);
                if(!bThis->m_audioUnit) {
                    DLog("AudioComponentInstanceNew failed");
                    return ;
                }
                
                if(excludeAudioUnit) {
                    excludeAudioUnit(bThis->m_audioUnit);
                }
                UInt32 flagOne = 1;
                
                AudioUnitSetProperty(bThis->m_audioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &flagOne, sizeof(flagOne));
                
                AudioStreamBasicDescription desc = {0};
                desc.mSampleRate = bThis->m_sampleRate;
                desc.mFormatID = kAudioFormatLinearPCM;
                desc.mFormatFlags = (kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked);
                desc.mChannelsPerFrame = bThis->m_channelCount;
                desc.mFramesPerPacket = 1;
                desc.mBitsPerChannel = 16;
                desc.mBytesPerFrame = desc.mBitsPerChannel / 8 * desc.mChannelsPerFrame;
                desc.mBytesPerPacket = desc.mBytesPerFrame * desc.mFramesPerPacket;
                
                AURenderCallbackStruct cb;
                cb.inputProcRefCon = this;
                cb.inputProc = handleInputBuffer;
                AudioUnitSetProperty(bThis->m_audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &desc, sizeof(desc));
                AudioUnitSetProperty(bThis->m_audioUnit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, 1, &cb, sizeof(cb));
                
                m_interruptionHandler = [[InterruptionHandler alloc] init];
                m_interruptionHandler->_source = this;
                
                [[NSNotificationCenter defaultCenter] addObserver:m_interruptionHandler selector:@selector(handleInterruption:) name:AVAudioSessionInterruptionNotification object:nil];
                
                AudioUnitInitialize(bThis->m_audioUnit);
                OSStatus ret = AudioOutputUnitStart(bThis->m_audioUnit);
                if(ret != noErr) {
                    DLog("Failed to start microphone!");
                }
            }
        };
        
        if(SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"7.0")) {
            [session requestRecordPermission:permission];
        } else {
            permission(true);
        }

    }
    MicSource::~MicSource() {
        if(m_audioUnit) {
            [[NSNotificationCenter defaultCenter] removeObserver:m_interruptionHandler];
            [m_interruptionHandler release];
            
            AudioOutputUnitStop(m_audioUnit);
            AudioComponentInstanceDispose(m_audioUnit);
        }
        
    }
    void
    MicSource::inputCallback(uint8_t *data, size_t data_size, int inNumberFrames)
    {
        auto output = m_output.lock();
        if(output) {
            videocore::AudioBufferMetadata md (0.);
            
            md.setData(m_sampleRate,
                       16,
                       m_channelCount,
                       kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked,
                       m_channelCount * 2,
                       inNumberFrames,
                       false,
                       false,
                       shared_from_this());
            
            output->pushBuffer(data, data_size, md);
        }
    }
    void
    MicSource::interruptionBegan() {
        DLog("interruptionBegan");
        AudioOutputUnitStop(m_audioUnit);
    }
    void
    MicSource::interruptionEnded() {
        DLog("interruptionEnded");
        AudioOutputUnitStart(m_audioUnit);
    }
    void
    MicSource::setOutput(std::shared_ptr<IOutput> output) {
        m_output = output;
        auto mixer = std::dynamic_pointer_cast<IAudioMixer>(output);
        mixer->registerSource(shared_from_this());
    }
}
}
