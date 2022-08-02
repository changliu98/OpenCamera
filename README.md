# OpenCamera

[![Github release](https://img.shields.io/github/downloads/yukimuon/opencamera/total.svg?color=black&style=flat-square&labelColor=f0f0f0)](https://github.com/yukimuon/OpenCamera/releases)

This is an open source version of [DroidCam - Webcam for PC](https://play.google.com/store/apps/details?id=com.dev47apps.droidcam&hl=en_US&gl=US), which enables your phone to work as a webcam.

### What this app does

This app enable you to use Android phone's camera as a webcam on a Windows desktop

### How to use it

A Windows desktop, an Android phone, they are under same WiFi network with local network access

1.   Install the [OpenCamera Windows software](https://github.com/yukimuon/OpenCamera/releases/tag/2.1.2) on your desktop, then install [the OpenCamera Android app](https://play.google.com/store/apps/details?id=com.cns.encom) on your phone.
2.   Open the OpenCamera Windows software, it will create a virtual camera device, note that this app requires Admin privileges
3.   Find the IP of your desktop, and open your app on the phone  
    <img src="Assets/image_2020-12-09_17-03-20.png" width=200px>
4.   Input the IP address of your desktop on the input box, click the "CONNECT" button
    <img src="Assets/Screenshot_20201211-004808.jpg" width=200px>
5.   Your desktop can use the camera through the app on your phone
6.   If you want to stop sharing, simply kill the app on your phone or click "DISCONNECT" button


### Compatibility

Due to DirectShow, not all programs can use the virtual cam on your device.  
Software compatibility:  

![](https://img.shields.io/badge/-OK-black?style=flat&logo=Firefox&logoColor=ff6611)
![](https://img.shields.io/badge/-OK-black?style=flat&logo=Google%20Chrome&logoColor=4285F4)
![](https://img.shields.io/badge/-OK-black?style=flat&logo=microsoft%20edge&logoColor=0078D7)
![](https://img.shields.io/badge/Zoom-OK-black?style=flat&labelColor=black)
![](https://img.shields.io/badge/-OK-black?style=flat&logo=discord&logoColor=7289d9)
![](https://img.shields.io/badge/-INCOMPATIBLE-black?style=flat&logo=skype&logoColor=00aff0)
![](https://img.shields.io/badge/-INCOMPATIBLE-black?style=flat&logo=Telegram)

### Release
<a href="https://github.com/yukimuon/OpenCamera/releases">Github release page</a>  
<a href='https://play.google.com/store/apps/details?id=com.cns.encom'><img alt='Get it on Google Play' src='https://play.google.com/intl/en_us/badges/static/images/badges/en_badge_web_generic.png' width='150px'></a>

### Release logs
v1.0.0 Error "fail to bind cam socket or mic socket" under hotspot. Noticeable lagging under slow network  
v2.0.1 Fixed: animation not running on UI thread  
v2.0.2 Fixed: Disconnect freezing problem  
v2.1.1 Fixed: Ugly purple banner

### Further Plan
Finished the transition to RTSP protocol. Future releases will only be fixes for critical bugs
