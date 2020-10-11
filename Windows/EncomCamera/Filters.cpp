#pragma warning(disable:4244)
#pragma warning(disable:4711)

#include <Windows.h>
#include <TlHelp32.h>

#include <streams.h>
#include <stdio.h>
#include <olectl.h>
#include <dvdmedia.h>
#include <thread>
#include <chrono>
#include "filters.h"

//////////////////////////////////////////////////////////////////////////
//  CVCam is the source filter which masquerades as a capture device
//////////////////////////////////////////////////////////////////////////
CUnknown * WINAPI CVCam::CreateInstance(LPUNKNOWN lpunk, HRESULT *phr)
{
    ASSERT(phr);
    CUnknown *punk = new CVCam(lpunk, phr);
    return punk;
}

CVCam::CVCam(LPUNKNOWN lpunk, HRESULT *phr) : 
    CSource(NAME("Virtual Cam"), lpunk, CLSID_VirtualCam)
{
    ASSERT(phr);
    CAutoLock cAutoLock(&m_cStateLock);
    // Create the one and only output pin
    m_paStreams = (CSourceStream **) new CVCamStream*[1];
    m_paStreams[0] = new CVCamStream(phr, this, L"Virtual Cam");
    
    update();
}

HRESULT CVCam::QueryInterface(REFIID riid, void **ppv)
{
    //Forward request for IAMStreamConfig & IKsPropertySet to the pin
    if(riid == _uuidof(IAMStreamConfig) || riid == _uuidof(IKsPropertySet))
        return m_paStreams[0]->QueryInterface(riid, ppv);
    else
        return CSource::QueryInterface(riid, ppv);
}

CVCam::~CVCam()
{
    unMap();
}

bool CVCam::isReady()
{
    return readyProcess && readyMapping && cameraOn;
}

void CVCam::update()
{
    // check if process is alive
    PROCESSENTRY32 processInfo{};
    processInfo.dwSize = sizeof(PROCESSENTRY32);
    HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    DWORD processID = NULL;
    if (processesSnapshot != INVALID_HANDLE_VALUE)
    {
        Process32First(processesSnapshot, &processInfo);
        if (!lstrcmpW(processName, processInfo.szExeFile))
        {
            processID = processInfo.th32ProcessID;
        }
        if (!processID)
        {
            while (Process32Next(processesSnapshot, &processInfo))
            {
                if (!lstrcmpW(processName, processInfo.szExeFile))
                {
                    processID = processInfo.th32ProcessID;
                    break;
                }
            }
        }
        CloseHandle(processesSnapshot);
    }
    if (processID)
    {
        DbgLog((LOG_ERROR, 1, TEXT("Process found")));
        readyProcess = true;
        tryMap();
    }
    else
    {
        DbgLog((LOG_ERROR, 1, TEXT("Process not found")));
        readyProcess = false;
        unMap();
    }
    testMemory();
}

void CVCam::tryMap()
{
    if (!readyProcess) return;
    if (readyMapping) return;
    unMap();
    // open file handles
    hMapFile1 = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, szName1);
    if (!hMapFile1)
    {
        int error = GetLastError();
        return;
    }
    hMapFile2 = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, szName2);
    if (!hMapFile2)
    {
        int error = GetLastError();
        CloseHandle(hMapFile1); hMapFile1 = NULL;
        return;
    }
    // open buffer pointers
    pBuffer1 = MapViewOfFile(hMapFile1, FILE_MAP_ALL_ACCESS, 0, 0, BUFFER_SIZE);
    if (!pBuffer1)
    {
        int error = GetLastError();
        CloseHandle(hMapFile1); hMapFile1 = NULL;
        CloseHandle(hMapFile2); hMapFile2 = NULL;
        return;
    }
    pBuffer2 = MapViewOfFile(hMapFile2, FILE_MAP_ALL_ACCESS, 0, 0, BUFFER_SIZE);
    if (!pBuffer2)
    {
        int error = GetLastError();
        UnmapViewOfFile(pBuffer1); pBuffer1 = nullptr;
        CloseHandle(hMapFile1); hMapFile1 = NULL;
        CloseHandle(hMapFile2); hMapFile2 = NULL;
        return;
    }
    // create mutex
    hMutex1 = OpenMutex(MUTEX_ALL_ACCESS, FALSE, mutexName1);
    if (!hMutex1)
    {
        int error = GetLastError();
        UnmapViewOfFile(pBuffer1); pBuffer1 = nullptr;
        UnmapViewOfFile(pBuffer2); pBuffer2 = nullptr;
        CloseHandle(hMapFile1); hMapFile1 = NULL;
        CloseHandle(hMapFile2); hMapFile2 = NULL;
        return;
    }
    hMutex2 = OpenMutex(MUTEX_ALL_ACCESS, FALSE, mutexName2);
    if (!hMutex2)
    {
        int error = GetLastError();
        UnmapViewOfFile(pBuffer1); pBuffer1 = nullptr;
        UnmapViewOfFile(pBuffer2); pBuffer2 = nullptr;
        CloseHandle(hMapFile1); hMapFile1 = NULL;
        CloseHandle(hMapFile2); hMapFile2 = NULL;
        CloseHandle(hMutex1); hMutex1 = NULL;
        return;
    }
    readyMapping = true;
}

void CVCam::testMemory()
{
    if (!readyMapping)
    {
        cameraOn = false;
        return;
    }
    uint8_t val = 0;
    memcpy(&val, pBuffer1, sizeof(uint8_t));
    if (val < 1)
    {
        cameraOn = false;
        return;
    }
    memcpy(&val, pBuffer2, sizeof(uint8_t));
    if (val < 1)
    {
        cameraOn = false;
        return;
    }
    cameraOn = true;
}

void CVCam::unMap()
{
    if (pBuffer1)
    {
        UnmapViewOfFile(pBuffer1);
        pBuffer1 = nullptr;
    }
    if (pBuffer2)
    {
        UnmapViewOfFile(pBuffer2);
        pBuffer2 = nullptr;
    }
    if (hMapFile1)
    {
        CloseHandle(hMapFile1);
        hMapFile1 = NULL;
    }
    if (hMapFile2)
    {
        CloseHandle(hMapFile2);
        hMapFile2 = NULL;
    }
    if (hMutex1)
    {
        CloseHandle(hMutex1);
        hMutex1 = NULL;
    }
    if (hMutex2)
    {
        CloseHandle(hMutex2);
        hMutex2 = NULL;
    }
    readyMapping = false;
}

//////////////////////////////////////////////////////////////////////////
// CVCamStream is the one and only output pin of CVCam which handles 
// all the stuff.
//////////////////////////////////////////////////////////////////////////
CVCamStream::CVCamStream(HRESULT *phr, CVCam *pParent, LPCWSTR pPinName) :
    CSourceStream(NAME("Virtual Cam"),phr, pParent, pPinName), m_pParent(pParent)
{
    // Set the default media type as 720x480x24@30
    GetMediaType(4, &m_mt);
}

CVCamStream::~CVCamStream()
{
} 

HRESULT CVCamStream::QueryInterface(REFIID riid, void **ppv)
{   
    // Standard OLE stuff
    if(riid == _uuidof(IAMStreamConfig))
        *ppv = (IAMStreamConfig*)this;
    else if(riid == _uuidof(IKsPropertySet))
        *ppv = (IKsPropertySet*)this;
    else
        return CSourceStream::QueryInterface(riid, ppv);

    AddRef();
    return S_OK;
}


//////////////////////////////////////////////////////////////////////////
//  This is the routine where we create the data being output by the Virtual
//  Camera device.
//////////////////////////////////////////////////////////////////////////

HRESULT CVCamStream::FillBuffer(IMediaSample *pms)
{

    REFERENCE_TIME rtNow;
    
    REFERENCE_TIME avgFrameTime = ((VIDEOINFOHEADER*)m_mt.pbFormat)->AvgTimePerFrame;

    rtNow = m_rtLastTime;
    m_rtLastTime += avgFrameTime;
    pms->SetTime(&rtNow, &m_rtLastTime);
    pms->SetSyncPoint(TRUE);

    BYTE *pData;
    long lDataLen;
    pms->GetPointer(&pData);
    lDataLen = pms->GetSize();

start:
    m_pParent->update(); // update mapping
    if (!m_pParent->isReady())
    {
        memset(pData, 0, lDataLen);
    }
    else
    {
        uint8_t val = 0;
        LPVOID ptr = nullptr;
        HANDLE mutex = NULL;
        DWORD dwWaitResult = 0;
        if (!m_pParent->lastUsedFile1)
        {
            dwWaitResult = WaitForSingleObject(m_pParent->hMutex1, INFINITE);
            if (dwWaitResult == WAIT_OBJECT_0)
            {
                memcpy(&val, (uint8_t*)m_pParent->pBuffer1 + sizeof(uint8_t), sizeof(uint8_t));
                if (val < 1) // program is not writing ?
                {
                    ptr = m_pParent->pBuffer1;
                    mutex = m_pParent->hMutex1;
                }
                else
                {
                    ReleaseMutex(m_pParent->hMutex1);
                    goto start;
                }
            }
        }
        else
        {
            dwWaitResult = WaitForSingleObject(m_pParent->hMutex2, INFINITE);
            if (dwWaitResult == WAIT_OBJECT_0)
            {
                memcpy(&val, (uint8_t*)m_pParent->pBuffer2 + sizeof(uint8_t), sizeof(uint8_t));
                if (val < 1) // program is not writing ?
                {
                    ptr = m_pParent->pBuffer2;
                    mutex = m_pParent->hMutex2;
                }
                else
                {
                    ReleaseMutex(m_pParent->hMutex2);
                    goto start;
                }
            }
        }
        //if (!ptr)
        //{
        //    if (m_pParent->lastUsedFile1)
        //    {
        //        dwWaitResult = WaitForSingleObject(m_pParent->hMutex1, INFINITE);
        //        if (dwWaitResult == WAIT_OBJECT_0)
        //        {
        //            memcpy(&val, (uint8_t*)m_pParent->pBuffer1 + sizeof(uint8_t), sizeof(uint8_t));
        //            if (val < 1) // program is not writing ?
        //            {
        //                ptr = m_pParent->pBuffer1;
        //                mutex = m_pParent->hMutex1;
        //            }
        //            else
        //                ReleaseMutex(m_pParent->hMutex1);
        //        }
        //    }
        //    else
        //    {
        //        dwWaitResult = WaitForSingleObject(m_pParent->hMutex2, INFINITE);
        //        if (dwWaitResult == WAIT_OBJECT_0)
        //        {
        //            memcpy(&val, (uint8_t*)m_pParent->pBuffer2 + sizeof(uint8_t), sizeof(uint8_t));
        //            if (val < 1) // program is not writing ?
        //            {
        //                ptr = m_pParent->pBuffer2;
        //                mutex = m_pParent->hMutex2;
        //            }
        //            else
        //                ReleaseMutex(m_pParent->hMutex2);
        //        }
        //    }
        //}
        /*if (!ptr)
        {
            memset(pData, 0, lDataLen);
            return S_OK;
        }*/
        m_pParent->lastUsedFile1 = !m_pParent->lastUsedFile1;
        memcpy(pData, (uint8_t*)ptr + 2*sizeof(uint8_t), lDataLen); // copy data
        if(dwWaitResult == WAIT_OBJECT_0)
            ReleaseMutex(mutex);
    }

    return S_OK;
} // FillBuffer


//
// Notify
// Ignore quality management messages sent from the downstream filter
STDMETHODIMP CVCamStream::Notify(IBaseFilter * pSender, Quality q)
{
    return E_NOTIMPL;
} // Notify

//////////////////////////////////////////////////////////////////////////
// This is called when the output format has been negotiated
//////////////////////////////////////////////////////////////////////////
HRESULT CVCamStream::SetMediaType(const CMediaType *pmt)
{
    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->Format());
    HRESULT hr = CSourceStream::SetMediaType(pmt);
    return hr;
}

// See Directshow help topic for IAMStreamConfig for details on this method
HRESULT CVCamStream::GetMediaType(int iPosition, CMediaType *pmt)
{
    if(iPosition < 0) return E_INVALIDARG;
    if(iPosition > 8) return VFW_S_NO_MORE_ITEMS;

    if(iPosition == 0) 
    {
        *pmt = m_mt;
        return S_OK;
    }

    DECLARE_PTR(VIDEOINFOHEADER, pvi, pmt->AllocFormatBuffer(sizeof(VIDEOINFOHEADER)));
    ZeroMemory(pvi, sizeof(VIDEOINFOHEADER));

    pvi->bmiHeader.biCompression = BI_RGB;
    pvi->bmiHeader.biBitCount    = 24;
    pvi->bmiHeader.biSize       = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth      = VIDEO_WIDTH;
    pvi->bmiHeader.biHeight     = VIDEO_HEIGHT;
    pvi->bmiHeader.biPlanes     = 1;
    pvi->bmiHeader.biSizeImage  = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;

    pvi->AvgTimePerFrame = UNITS / 30;

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    pmt->SetType(&MEDIATYPE_Video);
    pmt->SetFormatType(&FORMAT_VideoInfo);
    pmt->SetTemporalCompression(TRUE);

    // Work out the GUID for the subtype from the header info.
    const GUID SubTypeGUID = GetBitmapSubtype(&pvi->bmiHeader);
    pmt->SetSubtype(&SubTypeGUID);
    pmt->SetSampleSize(pvi->bmiHeader.biSizeImage);
    
    return NOERROR;

} // GetMediaType

// This method is called to see if a given output format is supported
HRESULT CVCamStream::CheckMediaType(const CMediaType *pMediaType)
{
    VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *)(pMediaType->Format());
    if(*pMediaType != m_mt) 
        return E_INVALIDARG;
    return S_OK;
} // CheckMediaType

// This method is called after the pins are connected to allocate buffers to stream data
HRESULT CVCamStream::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties)
{
    CAutoLock cAutoLock(m_pFilter->pStateLock());
    HRESULT hr = NOERROR;

    VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER *) m_mt.Format();
    pProperties->cBuffers = 1;
    pProperties->cbBuffer = pvi->bmiHeader.biSizeImage;

    ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProperties,&Actual);

    if(FAILED(hr)) return hr;
    if(Actual.cbBuffer < pProperties->cbBuffer) return E_FAIL;

    return NOERROR;
} // DecideBufferSize

// Called when graph is run
HRESULT CVCamStream::OnThreadCreate()
{
    m_rtLastTime = 0;
    return NOERROR;
} // OnThreadCreate


//////////////////////////////////////////////////////////////////////////
//  IAMStreamConfig
//////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CVCamStream::SetFormat(AM_MEDIA_TYPE *pmt)
{
    DECLARE_PTR(VIDEOINFOHEADER, pvi, m_mt.pbFormat);
    m_mt = *pmt;
    IPin* pin; 
    ConnectedTo(&pin);
    if(pin)
    {
        IFilterGraph *pGraph = m_pParent->GetGraph();
        pGraph->Reconnect(this);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetFormat(AM_MEDIA_TYPE **ppmt)
{
    *ppmt = CreateMediaType(&m_mt);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetNumberOfCapabilities(int *piCount, int *piSize)
{
    *piCount = 8;
    *piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CVCamStream::GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC)
{
    *pmt = CreateMediaType(&m_mt);
    DECLARE_PTR(VIDEOINFOHEADER, pvi, (*pmt)->pbFormat);

    if (iIndex == 0) iIndex = 4;

    pvi->bmiHeader.biCompression = BI_RGB;
    pvi->bmiHeader.biBitCount   = 24;
    pvi->bmiHeader.biSize       = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth      = VIDEO_WIDTH;
    pvi->bmiHeader.biHeight     = VIDEO_HEIGHT;
    pvi->bmiHeader.biPlanes     = 1;
    pvi->bmiHeader.biSizeImage  = GetBitmapSize(&pvi->bmiHeader);
    pvi->bmiHeader.biClrImportant = 0;

    pvi->AvgTimePerFrame = UNITS / 30;

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    (*pmt)->majortype = MEDIATYPE_Video;
    (*pmt)->subtype = MEDIASUBTYPE_RGB24;
    (*pmt)->formattype = FORMAT_VideoInfo;
    (*pmt)->bTemporalCompression = TRUE;
    (*pmt)->bFixedSizeSamples= FALSE;
    (*pmt)->lSampleSize = pvi->bmiHeader.biSizeImage;
    (*pmt)->cbFormat = sizeof(VIDEOINFOHEADER);
    
    DECLARE_PTR(VIDEO_STREAM_CONFIG_CAPS, pvscc, pSCC);
    
    pvscc->guid = FORMAT_VideoInfo;
    pvscc->VideoStandard = AnalogVideo_None;
    pvscc->InputSize.cx = VIDEO_WIDTH;
    pvscc->InputSize.cy = VIDEO_HEIGHT;
    pvscc->MinCroppingSize.cx = VIDEO_WIDTH / 8;
    pvscc->MinCroppingSize.cy = VIDEO_HEIGHT / 8;
    pvscc->MaxCroppingSize.cx = VIDEO_WIDTH;
    pvscc->MaxCroppingSize.cy = VIDEO_HEIGHT;
    pvscc->CropGranularityX = VIDEO_WIDTH / 8;
    pvscc->CropGranularityY = VIDEO_HEIGHT / 8;
    pvscc->CropAlignX = 0;
    pvscc->CropAlignY = 0;

    pvscc->MinOutputSize.cx = VIDEO_WIDTH / 8;
    pvscc->MinOutputSize.cy = VIDEO_HEIGHT / 8;
    pvscc->MaxOutputSize.cx = VIDEO_WIDTH;
    pvscc->MaxOutputSize.cy = VIDEO_HEIGHT;
    pvscc->OutputGranularityX = VIDEO_WIDTH / 8;
    pvscc->OutputGranularityY = VIDEO_HEIGHT / 8;
    pvscc->StretchTapsX = 0;
    pvscc->StretchTapsY = 0;
    pvscc->ShrinkTapsX = 0;
    pvscc->ShrinkTapsY = 0;
    pvscc->MinFrameInterval = UNITS / 60;   // 60 fps
    pvscc->MaxFrameInterval = UNITS / 10; // 10 fps
    pvscc->MinBitsPerSecond = (pvscc->MinOutputSize.cx * pvscc->MinOutputSize.cy * 24) * 10;
    pvscc->MaxBitsPerSecond = (pvscc->MaxOutputSize.cx * pvscc->MaxOutputSize.cy * 24) * 60;

    return S_OK;
}

//////////////////////////////////////////////////////////////////////////
// IKsPropertySet
//////////////////////////////////////////////////////////////////////////


HRESULT CVCamStream::Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData, 
                        DWORD cbInstanceData, void *pPropData, DWORD cbPropData)
{// Set: Cannot set any properties.
    return E_NOTIMPL;
}

// Get: Return the pin category (our only property). 
HRESULT CVCamStream::Get(
    REFGUID guidPropSet,   // Which property set.
    DWORD dwPropID,        // Which property in that set.
    void *pInstanceData,   // Instance data (ignore).
    DWORD cbInstanceData,  // Size of the instance data (ignore).
    void *pPropData,       // Buffer to receive the property data.
    DWORD cbPropData,      // Size of the buffer.
    DWORD *pcbReturned     // Return the size of the property.
)
{
    if (guidPropSet != AMPROPSETID_Pin)             return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY)        return E_PROP_ID_UNSUPPORTED;
    if (pPropData == NULL && pcbReturned == NULL)   return E_POINTER;
    
    if (pcbReturned) *pcbReturned = sizeof(GUID);
    if (pPropData == NULL)          return S_OK; // Caller just wants to know the size. 
    if (cbPropData < sizeof(GUID))  return E_UNEXPECTED;// The buffer is too small.
        
    *(GUID *)pPropData = PIN_CATEGORY_CAPTURE;
    return S_OK;
}

// QuerySupported: Query whether the pin supports the specified property.
HRESULT CVCamStream::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport)
{
    if (guidPropSet != AMPROPSETID_Pin) return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY) return E_PROP_ID_UNSUPPORTED;
    // We support getting this property, but not setting it.
    if (pTypeSupport) *pTypeSupport = KSPROPERTY_SUPPORT_GET; 
    return S_OK;
}
