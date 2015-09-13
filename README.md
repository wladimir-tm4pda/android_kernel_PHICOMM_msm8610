
kernel_working_but_backlight

some modif of files 
+ added sensors from c230wEU_kernel
+ added Karbonn configs

=============================
wladimir_tm@ubuntu:~/temporary/git/android_kernel_PHICOMM_msm8610$ git status
On branch LNX.LA.3.5.3_RB1.4
Changes not staged for commit:
  (use "git add <file>..." to update what will be committed)
  (use "git checkout -- <file>..." to discard changes in working directory)

        modified:   README.md
        modified:   arch/arm/mach-msm/smd_init_dt.c
        modified:   drivers/Kconfig
        modified:   drivers/Makefile
        modified:   drivers/input/Kconfig
        modified:   drivers/input/misc/Kconfig
        modified:   drivers/input/misc/Makefile
        modified:   drivers/input/touchscreen/gt9xx/goodix_tool.c
        modified:   drivers/input/touchscreen/gt9xx/gt9xx.c
        modified:   drivers/input/touchscreen/gt9xx/gt9xx.h
        modified:   drivers/input/touchscreen/gt9xx/gt9xx_firmware.h
        modified:   drivers/input/touchscreen/gt9xx/gt9xx_update.c
        modified:   drivers/media/platform/msm/camera_v2/Kconfig
        modified:   drivers/media/platform/msm/camera_v2/sensor/Makefile
        modified:   sound/soc/codecs/msm8x10-wcd-tables.c
        modified:   sound/soc/msm/msm8x10.c
        modified:   sound/soc/msm/qdsp6v2/rtac.c

Untracked files:
  (use "git add <file>..." to include in what will be committed)

        arch/arm/configs/Karbonn_Titanium_S1_Plus_defconfig
        arch/arm/configs/Karbonn_Titanium_S1_Plus_new_defconfig
        arch/arm/mach-msm/include/mach/fdv.h
        arch/arm/mach-msm/include/mach/fdv_alsprx.h
        arch/arm/mach-msm/include/mach/fdv_camera.h
        arch/arm/mach-msm/include/mach/fdv_geom.h
        arch/arm/mach-msm/include/mach/fdv_gsensor.h
        arch/arm/mach-msm/include/mach/fdv_gyro.h
        arch/arm/mach-msm/include/mach/fdv_lcd.h
        arch/arm/mach-msm/include/mach/fdv_nand.h
        arch/arm/mach-msm/include/mach/fdv_tp.h
        arch/arm/mach-msm/smd_rpc_sym.c
        drivers/freecomm_kernel/
        drivers/input/misc/bma222.c
        drivers/input/misc/tmd2771x.c
        drivers/media/platform/msm/camera_v2/sensor/gc2035.c
        drivers/media/platform/msm/camera_v2/sensor/gc2235.c
        include/linux/input/tmd2771x.h


=============================

appr. to Karbonn Titanium Plus 

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
...

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

