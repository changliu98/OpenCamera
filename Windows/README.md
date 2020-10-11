# Windows  

------

### Dependencies  
* SDL2  
* ImGui  
* OpenGL3 (GLEW static lib)  
* Winsock2.2 (`ws2_32.lib`)  
* FFmpeg (x64 and x86 compiled from source as static libs)  
  * `libavcodec.a`
  * `libavformat.a`
  * `libswscale.a`
  * `libavdevice.a`
  * `libavfilter.a`
  * `libavutil.a`
  * `libswresample.a`
* FFmpeg dependencies (Windows libs)
  * `Secur32.lib`
  * `Bcrypt.lib`
  * `Mfplat.lib`
  * `Mfuuid.lib`
  * `strmiids.lib`
* Directshow base lib (Can be downloaded and compiled from [Windows-Samples](https://github.com/microsoft/Windows-classic-samples))  

------

### How to build  
1. extract ```external.zip```  
2. enter ```EncomService``` and open solution file in Visual Studio  

------

### Directshow Filters  
Build `x86` and `x64` directshow dlls in `EncomCamera` project, and register them (require administrator level):  
```cmd
regsvr32 EncomCamerax64.dll
```
or
```cmd
regsvr32 EncomCamerax86.dll
```
Unregister them via:
```cmd
regsvr32 /U EncomCamerax64.dll
```

------

### Special Tricks  
* Fake camera input on Windows  
  In order to simulate camera signal on Windows, we need to write our own [Directshow Filters](https://docs.microsoft.com/en-us/windows/win32/directshow/directshow-reference)  
  In ```EncomCamera``` folder, we wrote a video source push filter adapted from [```Capture Source Filter```](http://tmhare.mvps.org/downloads.htm)  
* Send data to directshow filters  
  Since directshow filters are running by attaching to a program (such as zoom, discord, and firefox), we need to send received image buffer to these filters  
  A possible solution is to create [Windows Named Shared Memory File](https://docs.microsoft.com/en-us/windows/win32/memory/creating-named-shared-memory), with self-designed synchronization locks, and grant access to dll  
* Use FFmpeg to handle rtsp stream, and write decoded RGB24 frames to global shared memory.  

-----

### Administrator Requirement  
In order to allocate global memory that is accessible for directshow dlls, __this program requires administrator level__.