#pragma once

#define DECLARE_PTR(type, ptr, expr) type* ptr = (type*)(expr);

#define VIDEO_WIDTH         640
#define VIDEO_HEIGHT        480

#define FILE_READY_WRITE	0
#define FILE_READY_READ		1
#define FILE_IN_USE			2

EXTERN_C const GUID CLSID_VirtualCam;

class CVCamStream;
class CVCam : public CSource
{
public:
    //////////////////////////////////////////////////////////////////////////
    //  IUnknown
    //////////////////////////////////////////////////////////////////////////
    static CUnknown * WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT *phr);
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);

    IFilterGraph *GetGraph() {return m_pGraph;}
    ~CVCam();

    bool isReady(); // is file mapping ready?
    void update(); // check if EncomService is alive, update mapping

private:
    CVCam(LPUNKNOWN lpunk, HRESULT *phr);
    void tryMap(); // try to create mapping
    void unMap(); // unmap all
    void testMemory(); // test camera on or off

public:
    int counter = 0;
    bool lastUsedFile1 = false; // is last read file1 ?
    bool readyProcess = false; // is EncomService alive ?
    bool readyMapping = false; // is file mapping ready ?
    bool cameraOn = false; // is camera on?
    // shared memory file mapping
    HANDLE hMapFile1 = NULL;
    LPVOID pBuffer1 = nullptr;
    HANDLE hMapFile2 = NULL;
    LPVOID pBuffer2 = nullptr;
    // define buffer size
    const DWORD BUFFER_SIZE = 1200000UL;
    // set file names
    const wchar_t* szName1 = TEXT("Global\\EncomCamera1");
    const wchar_t* szName2 = TEXT("Global\\EncomCamera2");
    // set property for mutex
    HANDLE hMutex1 = NULL;
    HANDLE hMutex2 = NULL;
    const wchar_t* mutexName1 = TEXT("Global\\EncomCameraMutex1");
    const wchar_t* mutexName2 = TEXT("Global\\EncomCameraMutex2");
    // set EncomService process name
    const wchar_t* processName = TEXT("EncomService.exe");
};

class CVCamStream : public CSourceStream, public IAMStreamConfig, public IKsPropertySet
{
public:

    //////////////////////////////////////////////////////////////////////////
    //  IUnknown
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
    STDMETHODIMP_(ULONG) AddRef() { return GetOwner()->AddRef(); }                                                          \
    STDMETHODIMP_(ULONG) Release() { return GetOwner()->Release(); }

    //////////////////////////////////////////////////////////////////////////
    //  IQualityControl
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP Notify(IBaseFilter * pSender, Quality q);

    //////////////////////////////////////////////////////////////////////////
    //  IAMStreamConfig
    //////////////////////////////////////////////////////////////////////////
    HRESULT STDMETHODCALLTYPE SetFormat(AM_MEDIA_TYPE *pmt);
    HRESULT STDMETHODCALLTYPE GetFormat(AM_MEDIA_TYPE **ppmt);
    HRESULT STDMETHODCALLTYPE GetNumberOfCapabilities(int *piCount, int *piSize);
    HRESULT STDMETHODCALLTYPE GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC);

    //////////////////////////////////////////////////////////////////////////
    //  IKsPropertySet
    //////////////////////////////////////////////////////////////////////////
    HRESULT STDMETHODCALLTYPE Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData, DWORD cbInstanceData, void *pPropData, DWORD cbPropData);
    HRESULT STDMETHODCALLTYPE Get(REFGUID guidPropSet, DWORD dwPropID, void *pInstanceData,DWORD cbInstanceData, void *pPropData, DWORD cbPropData, DWORD *pcbReturned);
    HRESULT STDMETHODCALLTYPE QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport);
    
    //////////////////////////////////////////////////////////////////////////
    //  CSourceStream
    //////////////////////////////////////////////////////////////////////////
    CVCamStream(HRESULT *phr, CVCam *pParent, LPCWSTR pPinName);
    ~CVCamStream();

    HRESULT FillBuffer(IMediaSample *pms);
    HRESULT DecideBufferSize(IMemAllocator *pIMemAlloc, ALLOCATOR_PROPERTIES *pProperties);
    HRESULT CheckMediaType(const CMediaType *pMediaType);
    HRESULT GetMediaType(int iPosition, CMediaType *pmt);
    HRESULT SetMediaType(const CMediaType *pmt);
    HRESULT OnThreadCreate(void);
    
private:
    CVCam *m_pParent;
    REFERENCE_TIME m_rtLastTime;
    HBITMAP m_hLogoBmp;
    CCritSec m_cSharedState;
    IReferenceClock *m_pClock;
};


