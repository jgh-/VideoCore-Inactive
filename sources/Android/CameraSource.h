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
#ifndef videocore_Android_CameraSource_h
#define videocore_Android_CameraSource_h
#include <videocore/sources/ISource.hpp>

#include <OMXAL/OpenMAXAL.h>
#include <OMXAL/OpenMAXAL_Android.h>

namespace videocore { namespace Android {

	class CameraSource : public ISource, public std::enable_shared_from_this<CameraSource>
	{

	public:
		CameraSource();
		~CameraSource();

		void setupCamera(int fps = 15, bool useFront = true, bool useInterfaceOrientation = false);

		/*! ISource::setOutput */
        void setOutput(std::shared_ptr<IOutput> output) { m_output = output; };

    private:
    	static void cameraCallback(XACameraItf caller, 
    						void* pContext, 
    						XAuint32 eventId, 
    						XAuint32 eventData);
    private:
    	std::weak_ptr<IOutput> m_output;

    	XAObjectItf m_cameraObject;
    	XAObjectItf m_engineObject;
    	XAEngineItf m_engine;

	};

}}


#endif