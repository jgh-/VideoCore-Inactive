//
//  BasicVideoFilterBGRA.cpp
//  VideoCoreDevelopment
//
//  Created by James Hurley on 9/3/14.
//  Copyright (c) 2014 VideoCore. All rights reserved.
//

#include "BasicVideoFilterBGRA.h"

#if TARGET_IPHONE
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES3/gl.h>
#endif
namespace videocore { namespace filters {
 
    BasicVideoFilterBGRA::BasicVideoFilterBGRA()
    {
        
    }
    BasicVideoFilterBGRA::~BasicVideoFilterBGRA()
    {
        
    }
    
    const char * const
    BasicVideoFilterBGRA::vertexKernel() const
    {
        
        KERNEL(
               attribute vec2 aPos;
               attribute vec2 aCoord;
               varying vec2 vCoord;
               uniform mat4 uMat;
               void main(void) {
                gl_Position = uMat * vec4(aPos,0.,1.);
                vCoord = aCoord;
               }
        )
    }
    
    const char * const
    BasicVideoFilterBGRA::pixelKernel() const
    {
        KERNEL(
               precision mediump float;
               varying vec2 vCoord;
               uniform sampler2D uTex0;
               void main(void) {
                   gl_FragData[0] = texture2D(uTex0, vCoord);
               };
        )
    }
}
}