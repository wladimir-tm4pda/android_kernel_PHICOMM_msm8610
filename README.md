
appr to Karbonn Titanium Plus 
=============================
wladimir_tm@ubuntu:~/android/dump/vendor/lib/egl$ strings libEGL_adreno.so
...
Adreno-EGL
Invalid client version
1.4.1
(dpy: %d, major: %p, minor: %p)
EGL 1.4 QUALCOMM build: AU_LINUX_ANDROID_LNX.LA.3.5.3_RB1.04.04.02.043.029_msm8610_LNX.LA.3.5.3_RB1__release_AU ()
OpenGL ES Shader Compiler Version: E031.24.00.14
Build Date: 05/14/14 Wed
Local Branch:
Remote Branch: quic/LNX.LA.3.5.3_RB1.4
Local Patches: NONE
Reconstruct Branch: AU_LINUX_ANDROID_LNX.LA.3.5.3_RB1.04.04.02.043.029 +  NOTHING
EGL API Trace Start (eglInitialize)

=============================

wladimir_tm@ubuntu:~/LNX.LA.3.5.3_RB1.4/kernel$ git log
commit e606d9ce1e954a9e0a5fc7df6f2b645b321f1e8a
Merge: a380f3e fc00eda
Author: Linux Build Service Account <lnxbuild@localhost>
Date:   Tue May 13 00:56:49 2014 -0700

    Merge "Merge defa99300b90f927a8a61502c7bc37437fde6830 on remote branch"

commit fc00eda9956d86671f966de70b645f359497803b
Merge: a380f3e defa993
Author: Linux Build Service Account <lnxbuild@localhost>
Date:   Tue May 13 01:01:48 2014 -0600

    Merge defa99300b90f927a8a61502c7bc37437fde6830 on remote branch

    Change-Id: Iaf010248d0e2aa94b0d6b509ae067d82b23ba8f9



....
=============================
