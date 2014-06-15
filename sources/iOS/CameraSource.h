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
#ifndef __videocore__CameraSource__
#define __videocore__CameraSource__

#include <iostream>
#include <videocore/sources/ISource.hpp>
#include <videocore/transforms/IOutput.hpp>
#include <CoreVideo/CoreVideo.h>
#include <glm/glm.hpp>


namespace videocore { namespace iOS {
    
    class CameraSource : public ISource, public std::enable_shared_from_this<CameraSource>
    {
    public:
        enum AspectMode
        {
            kAspectFit,
            kAspectFill
        };
        
    public:
        CameraSource(float x, float y, float w, float h, float videow, float videoh, float aspect);
        ~CameraSource();
        
        void setOutput(std::shared_ptr<IOutput> output);
        
        void getPreviewLayer(void** outAVCaptureVideoPreviewLayer);

        void setupCamera(int fps = 15, bool useFront = true);
        void setAspectMode( AspectMode aspectMode );
        
        void toggleCamera();
        
        
    public:
        // Used by Objective-C callbacks
        void bufferCaptured(CVPixelBufferRef pixelBufferRef);
        void reorientCamera();
        
    private:
        
        void* cameraWithPosition(int position);
        
    private:
        
        glm::mat4 m_matrix;
        struct { float x, y, w, h, vw, vh, a; } m_size, m_targetSize;
        
        std::weak_ptr<IOutput> m_output;
        
        void* m_captureSession;
        void* m_captureDevice;
        void* m_callbackSession;
        void* m_previewLayer;
        
        AspectMode m_aspectMode;
        int  m_fps;
        bool m_isFirst;
        
    };
    
}
}
#endif /* defined(__videocore__CameraSource__) */
