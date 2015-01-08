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
			
			XAboolean iireqd[] = { XA_BOOLEAN_TRUE, XA_BOOLEAN_TRUE };
			XAInterfaceID iid[] = { XA_IID_CAMERA, XA_IID_CAMERACAPABILITIES };

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

			/*XADataLocator_IODevice camLocator = { XA_DATALOCATOR_IODEVICE, 
												  XA_IODEVICE_CAMERA,
												  0, 0 };

			XADataFormat_RawImage imgFormat = { XA_DATAFORMAT_RAWIMAGE, 
											    XA_COLORFORMAT_32BITBGRA8888,
											    480,
											    640,
											    4
											  };

			XADataSource camSource = { &camLocator, &imgFormat };
			

			(*m_engine)->CreateMediaRecorder(m_engine,
											 &m_recorderObject,
											 nullptr,
											 &camSource,

											 )*/
		}
	}
	void
	CameraSource::cameraCallback(XACameraItf caller, 
    							void* pContext, 
    							XAuint32 eventId, 
    							XAuint32 eventData)
	{

	}
}}