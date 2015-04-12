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
#include <videocore/transforms/iOS/H264Encode.h>
#include <videocore/system/h264/Golomb.h>

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#include <sys/stat.h>

namespace videocore { namespace iOS {
    
    H264Encode::H264Encode( int frame_w, int frame_h, int fps, int bitrate )
    : m_lastFilePos(0), m_frameCount(0), m_frameW(frame_w), m_frameH(frame_h), m_fps(fps),  m_bitrate(bitrate), m_currentWriter(0),
    m_queue("com.videcore.h264.7")
    {
        m_tmpFile[0] = [[NSTemporaryDirectory() stringByAppendingString:@"tmp1.mov"] UTF8String];
        m_tmpFile[1] = [[NSTemporaryDirectory() stringByAppendingString:@"tmp2.mov"] UTF8String];
        
        m_assetWriters[0] = nullptr;
        m_assetWriters[1] = nullptr;
        m_pixelBuffers[0] = nullptr;
        m_pixelBuffers[1] = nullptr;
        
        setupWriters();
    }
    H264Encode::~H264Encode()
    {
        teardownWriter(0);
        teardownWriter(1);
    }
    
    bool
    H264Encode::setupWriters()
    {
        bool status = setupWriter(0);
        if(status) status = setupWriter(1);
        
        return status;
    }
    
    bool
    H264Encode::setupWriter(int wid)
    {
        @autoreleasepool {
            
            const char* writer_cstr = m_tmpFile[wid].c_str();
            NSString * writer = [NSString stringWithUTF8String:writer_cstr];
            
            FILE *fp = fopen(writer_cstr, "r") ;
            if(fp) {
                fclose(fp);
                remove(writer_cstr);
            }
            
            NSDictionary* settings = nil;
            
            if(&AVVideoAllowFrameReorderingKey != nullptr) {
                settings = @{AVVideoCodecKey: AVVideoCodecH264,
                             AVVideoCompressionPropertiesKey: @{AVVideoAverageBitRateKey: @(m_bitrate),
                                                                AVVideoMaxKeyFrameIntervalKey: @(m_fps*2),
                                                                AVVideoProfileLevelKey: AVVideoProfileLevelH264Baseline41/*,
                                                                AVVideoAllowFrameReorderingKey: @NO*/
                                                                },
                             AVVideoWidthKey: @(m_frameW),
                             AVVideoHeightKey: @(m_frameH)
                             };
            } else {
                settings = @{AVVideoCodecKey: AVVideoCodecH264,
                             AVVideoCompressionPropertiesKey: @{AVVideoAverageBitRateKey: @(m_bitrate),
                                                                AVVideoMaxKeyFrameIntervalKey: @(m_fps*2),
                                                                AVVideoProfileLevelKey: AVVideoProfileLevelH264Baseline31
                                                                },
                             AVVideoWidthKey: @(m_frameW),
                             AVVideoHeightKey: @(m_frameH)
                             };
            }
            AVAssetWriterInput* input = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo outputSettings:settings];
            input.expectsMediaDataInRealTime = YES;
            
            NSURL* target = [NSURL fileURLWithPath:writer];
            m_assetWriters[wid] = [[AVAssetWriter alloc] initWithURL:target fileType:AVFileTypeQuickTimeMovie error:nil];
            m_pixelBuffers[wid] = [[AVAssetWriterInputPixelBufferAdaptor alloc] initWithAssetWriterInput:input sourcePixelBufferAttributes:nil];
            
            if([(id)m_assetWriters[wid] canAddInput:input]) {
                [(id)m_assetWriters[wid] addInput:input];
            } else {
                teardownWriter(wid);
                return false;
            }
            
            CMTime time = {0};
            time.timescale = m_fps;
            time.flags = kCMTimeFlags_Valid;
            
            @try {
                [(id)m_assetWriters[wid] startWriting];
                [(id)m_assetWriters[wid] startSessionAtSourceTime:time];
            } @catch (NSException* e) {
                teardownWriter(wid);
            }
        }
        return true;
    }
    void
    H264Encode::teardownWriter(int wid)
    {
        if(m_assetWriters[wid] != nullptr)
        {
            [(id)m_assetWriters[wid] release], m_assetWriters[wid] = nullptr;
        }
        if(m_pixelBuffers[wid] != nullptr)
        {
            [(id)m_pixelBuffers[wid] release], m_pixelBuffers[wid] = nullptr;
        }
    }
    void
    H264Encode::swapWriters( bool force )
    {
        if(force || ((m_frameCount++ > ((m_fps * 2) * 5)) && m_sps.size() && m_pps.size())) {
            // swap.
            int old = m_currentWriter;
            m_currentWriter = !m_currentWriter;
            m_frameCount = 0;
            m_lastFilePos = 36;
            m_queue.enqueue([this,old]() {
#if 0
                [(id)this->m_assetWriters[old] finishWritingWithCompletionHandler:^{
#endif
                    this->teardownWriter(old);
                    this->setupWriter(old);
#if 0
                }];
#endif
            });
        } else if(m_sps.size() == 0 || m_pps.size() == 0) {
            if(m_frameCount > 3) {
                // after one GOP stop movie and get the sps and pps.
                
                int old = m_currentWriter;
                m_currentWriter = !m_currentWriter;
                int lastFrame = m_frameCount;
                m_frameCount = 0;
                m_lastFilePos = 36;
                
                CMTime time = {0};
                time.timescale = m_fps;
                time.value = lastFrame;
                time.flags = 1;
                m_queue.enqueue([=](){
                    [(id)m_assetWriters[old] endSessionAtSourceTime:time];
                    [(id)m_assetWriters[old] finishWritingWithCompletionHandler:^{
                        
                        this->extractSpsAndPps(old);
                        this->teardownWriter(old);
                        this->setupWriter(old);
                    }];
                });
            }
        }
    }
    void
    H264Encode::extractSpsAndPps(int wid)
    {
        FILE* fp = fopen(m_tmpFile[wid].c_str(), "r");
        if(fp) {
            uint32_t size ;
            
            fread(&size, 4, 1, fp);
            size = __builtin_bswap32(size);     // ftyp
            fseek(fp, (long)size-4, SEEK_CUR);
            fread(&size,4,1, fp);               // wide
            size = __builtin_bswap32(size);
            fseek(fp,(long)size-4, SEEK_CUR);
            fread(&size,4,1, fp);               // mdat
            size = __builtin_bswap32(size);
            fseek(fp,(long)size-4, SEEK_CUR);
            
            long pos = ftell(fp);               // moov atom
            fseek(fp,0,SEEK_END);
            long len = ftell(fp) - pos;         // get length to read
            fseek(fp,pos,SEEK_SET);
            
            std::unique_ptr<uint8_t[]> data(new uint8_t[len]);
            uint8_t* p = &data[0];
            fread(p,(size_t)len,1,fp);
            fclose(fp);

            while ( p < (&data[0] + len))
            {
                if(*((int*)p) == __builtin_bswap32('avcC'))
                {
                    p += 4 ; // skip 'avcC'
                    p += 4 ; // skip avcC header
                    p += 2; // skip skip length and sps count
                    uint16_t sps_size = *((uint16_t*)p);
                    sps_size = __builtin_bswap16(sps_size);
                    p += 2 ; // move pointer as we have just read sps size
                    *(int*)(p-4) = sps_size;
                    m_sps.resize(sps_size+4);
                    m_sps.put(p-4, sps_size+4);
                    p += sps_size;
                    p ++ ; // skip pps count
                    uint16_t pps_size = *((uint16_t*)p);
                    p+= 2;
                    pps_size = __builtin_bswap16(pps_size);
                    *(int*)(p-4) = pps_size;
                    m_pps.resize(pps_size+4);
                    m_pps.put(p-4, pps_size+4);

                    break;
                }
                ++p;
            }
            
        }
        
    }
    bool
    H264Encode::isFirstSlice(uint8_t *nalu, size_t size)
    {
        int ret = 0;
        uint8_t* bytes = nalu;
        uint8_t type = bytes[0] & 0x1F;
        
        if(type <= 5)
        {
            h264::WORD* toswap = (h264::WORD*)(bytes+1);
            h264::WORD swapped[4] = {
                h264::swap(toswap[0]),
                h264::swap(toswap[1]),
                h264::swap(toswap[2]),
                h264::swap(toswap[3])
            };
            
            h264::GolombDecode decode(&swapped[0]);
            ret = decode.unsignedDecode();  // first_mb_in_slice
        }
        return (ret == 0);
    }
    
    void
    H264Encode::releaseFrame(videocore::IMetadata& metadata)
    {
        auto output = m_output.lock();
        
        if(m_frameQueue.size() > 0 && output) {
            auto & nal = m_frameQueue.front();
            
            if(((*nal)()[4] & 0x1F) == 5) {
                output->pushBuffer(m_sps(), m_sps.size(), metadata);
                output->pushBuffer(m_pps(), m_pps.size(), metadata);
            }
            bool isfirst = false;
            std::vector<uint8_t> outBuff;
            
            while(!isfirst) {
                outBuff.insert(outBuff.end(), (*m_frameQueue.front())(), (*m_frameQueue.front())()+m_frameQueue.front()->size());
                m_frameQueue.pop_front();
                if(m_frameQueue.size() > 0) {
                    auto & next = m_frameQueue.front();
                    isfirst = isFirstSlice((*next)()+4, next->size()-4);
                } else {
                    break;
                }
            }
            output->pushBuffer(&outBuff[0], outBuff.size(), metadata);
        }
    }
    void
    H264Encode::pushBuffer(const uint8_t *const data, size_t size, videocore::IMetadata &metadata)
    {
        
        releaseFrame(metadata); // Send the next queued frame
        
        @autoreleasepool {
            
            CVPixelBufferRef pb = (CVPixelBufferRef)(const_cast<uint8_t*>(data));
            
            CMTime presentationTime = {0};
            presentationTime.timescale = m_fps;
            presentationTime.value = m_frameCount;
            presentationTime.flags = kCMTimeFlags_Valid;
            
            if(!m_assetWriters[m_currentWriter] || !m_pixelBuffers[m_currentWriter] || [(id)m_assetWriters[m_currentWriter] status] != 1) {
                swapWriters(true);
                return ;
            }
            
            AVAssetWriterInputPixelBufferAdaptor* adaptor = static_cast<AVAssetWriterInputPixelBufferAdaptor*>(m_pixelBuffers[m_currentWriter]);
            BOOL ready = adaptor.assetWriterInput.readyForMoreMediaData;
            
            if(!ready) {
                return;
            }
            
            CVPixelBufferLockBaseAddress(pb, kCVPixelBufferLock_ReadOnly);
            [(id)m_pixelBuffers[m_currentWriter] appendPixelBuffer:pb withPresentationTime:presentationTime];
            CVPixelBufferUnlockBaseAddress(pb, kCVPixelBufferLock_ReadOnly);
            
            
            // Open the file the h.264 data is being written to.
            const char * file = m_tmpFile[m_currentWriter].c_str();
            
            struct stat s;
            stat(file, &s);
            
            FILE* fp = NULL;
            
            if(s.st_size > m_lastFilePos && (fp = fopen(file, "rb"))) {
                
                off_t pos = s.st_size;
                off_t len = (off_t)std::max((pos - m_lastFilePos),off_t(0));
                
                fseek(fp, m_lastFilePos, SEEK_SET);
                
                size_t read = 0;
                if(len) {
                    
                    std::unique_ptr<uint8_t[]> data(new uint8_t[len]);
                    
                    fread(data.get(), static_cast<size_t>(len), 1, fp);
                    uint8_t* p = data.get();
                    while(read < len) {
                        
                        uint32_t nal_length;
                        
                        nal_length = __builtin_bswap32(*((uint32_t*)p));
                        
                        if(nal_length <= (len - read - 4) ) {
                            // We have a full NAL unit. Queue it.
                            
                            int type = p[4] & 0x1F;
                            
                            if(type <= 5 && m_sps.size() > 0 && m_pps.size() > 0) {
                                auto output = m_output.lock();
                                if(output) {
                                    
                                    std::unique_ptr<Buffer> nal ( new Buffer(nal_length+4) );
                                    nal->put(p, nal_length+4);
                                    m_frameQueue.push_back(std::move(nal));
                                }
                            }
                            read+=nal_length+4;
                            p+=nal_length+4;
                            
                        } else {
                            break;
                        }
                        
                    }
                    m_lastFilePos += read;
                    
                }
                fclose(fp);
            }
            
            swapWriters();
            
        }
        
    }
    
    void
    H264Encode::setBitrate(int bitrate)
    {
        m_bitrate = bitrate;
        teardownWriter(!m_currentWriter);
        setupWriter(!m_currentWriter);
        swapWriters(true);
    }
}
}
