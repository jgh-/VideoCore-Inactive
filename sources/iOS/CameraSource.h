/*
 
 Video Core
 Copyright (C) 2014 James G. Hurley
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 USA
 
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
        CameraSource(float x, float y, float w, float h, float videow, float videoh, float aspect);
        ~CameraSource();
        
        void setOutput(std::shared_ptr<IOutput> output);
        

        void getPreviewLayer(void** outAVCaptureVideoPreviewLayer);

        void setupCamera(int fps = 15, bool useFront = true);
        
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
        
        int  m_fps;
        bool m_isFirst;
        
    };
    
}
}
#endif /* defined(__videocore__CameraSource__) */
