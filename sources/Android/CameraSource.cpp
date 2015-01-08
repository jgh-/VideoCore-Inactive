#include <videocore/sources/Android/CameraSource.h>

namespace videocore { namespace Android {

	CameraSource::CameraSource() 
	: m_engineObject(nullptr), m_engine(nullptr)
	{

	}
	CameraSource::~CameraSource()
	{
		// delete engine objects
	}
	void
	CameraSource::setupCamera(int fps, bool useFront, bool useInterfaceOrientation)
	{
		XAresult res;

		res = xaCreateEngine(&m_engineObject, 0, nullptr, 0, nullptr, nullptr);

		if(res == XA_RESULT_SUCCESS) {
			res = (*m_engineObject)->Realize(m_engineObject, XA_BOOLEAN_FALSE);
		}
		if(res == XA_RESULT_SUCCESS) {
			res = (*m_engineObject)->GetInterface(m_engineObject, XA_IID_ENGINE, &m_engine);
		}
		if(res == XA_RESULT_SUCCESS) {
			
			{
				XAboolean iireqd[] = { XA_BOOLEAN_TRUE, XA_BOOLEAN_TRUE };
				XAInterfaceID iid[] = { XA_IID_CAMERA, XA_IID_CAMERACAPABILITIES };

				// Camera Source
				res = (*m_engine)->CreateCameraDevice(m_engine, 
					           					      &m_cameraObject, 
					           					      XA_DEFAULTDEVICEID_CAMERA,
					           					      sizeof(iid)/sizeof(XAInterfaceID),
					           					      iid, iireqd);

				(*m_cameraObject)->Realize(m_cameraObject, XA_BOOLEAN_FALSE);
				
				XACameraItf cam = (XACameraItf)m_cameraObject;

				(*cam)->RegisterCallback(cam, 
										 cameraCallback,
										 this);
			}
			XADataLocator_IODevice camLocator = { XA_DATALOCATOR_IODEVICE, 
												  XA_IODEVICE_CAMERA,
												  XA_DEFAULTDEVICEID_CAMERA,
												  m_cameraObject };

			XADataFormat_RawImage imgFormat = { XA_DATAFORMAT_RAWIMAGE, 
											    XA_COLORFORMAT_32BITBGRA8888,
											    480,
											    640,
											    4
											  };

			XADataSource camSource = { &camLocator, &imgFormat };
			

			m_buffer.resize( 640 * 480 * 4 * 10 );
			// Data Sink

			XADataLocator_Address addr = { XA_DATALOCATOR_ADDRESS, &m_buffer[0], m_buffer.size() };
			XADataSink sink = { &addr, &imgFormat };
			{
				XAboolean iireqd[] = { XA_BOOLEAN_TRUE };
				XAInterfaceID iid[] = { XA_IID_RECORD };
				// Create Media Recorder
				res = (*m_engine)->CreateMediaRecorder(m_engine,			// self
												 &m_recordObject,	// pRecorder
												 nullptr,			// pAudioSrc
												 &camSource,		// pImageVideoSrc
												 &sink,				// pDataSink
												 sizeof(iid) / sizeof(XAInterfaceID),					// numInterfaces
												 iid, 				// pInterfaceIds
												 iireqd				// pInterfaceRequired
												 );
				(*m_recordObject)->Realize(m_recordObject, XA_BOOLEAN_FALSE);

				XARecordItf rec = (XARecordItf)m_recordObject;

				res = (*rec)->RegisterCallback(rec, recordCallback, this);
				//(*rec)->SetRecordState(rec, XA_RECORDSTATE_RECORDING);
			}
		}
	}
	void
	CameraSource::cameraCallback(XACameraItf caller, 
    							void* pContext, 
    							XAuint32 eventId, 
    							XAuint32 eventData)
	{

	}
	void 
	CameraSource::recordCallback(XARecordItf caller,
    							   void* pContext,
    							   XAuint32 event)
	{
		if ( event == XA_RECORDEVENT_BUFFER_FULL ) {

		}
	}
}}