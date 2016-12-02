Build and install steps
-----------------------
    This is a pulseaudio module, which allows genivi AudioManager to control the pulse audio stream(s). This section describes how to build and install the pulseaudio router module. More information on the pulseaudio module development can be found at https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/Developer/Modules/ . This page states that "the API's needed to develop pulseaudio modules are considered internal", hence these header files never get installed while installing  pulseaudio or no package installs these files. Even the pulseaudio-dev package, just installs the necessary headers for the pulseaudio application development, but no package installs the header files needed for the pulseaudio module development. The only way to build any pulseaudio module is to build it inside the pulseaudio source tree. In order to build a pulseaudio module outside the pulseaudio source tree, some additional steps are reqeuired. While searching, I came across a specific patch from an engineer Jaska Uimonen created for Tizen.
https://build.tizen.org/package/view_file?expand=1&file=0005-build-sys-install-files-for-a-module-development.patch&package=pulseaudio&project=Tizen

     This patch modifies the pulseaudio build to install the internal header files required to build the modules, header files are installed in the <system header filespath>/pulsemodule directory by creating a new package. The new package is aptly named as pulseaudio-module-dev and this package also installs the package config file, using this package config file the include path of the pulseaudio module developement header files and the modules install path for the system can be retrieved. This is all the information one needs to build this module.


Build
------
This module needs pulseaudio-module-dev package installed in system.

#git clone http://git.projects.genivi.org/AudioManagerPlugins.git
#cd AudioManagerPlugins
#mkdir build
#cd build
#cmake ..
#make install
    The output is a dynamically loadable library module-router.so. This library is installed in the pulseaudio modules directory, most commonly it is /usr/lib/pulse-x.y/modules where x and y are pulseaudio major and minor version respectively.

Running 
------- 

     The router module is a dynamically loadable module. To load it using pulseaudio command is
#pactl load-module module-router.so

      Also the pulseaudio daemon configuration file can be changed to load this module whenever pulseaudio starts. The configuration file is present at 
/etc/pulse/default.pa and add the following lines.
.ifexists module-router.so
load-module module-router
.endif
Note: The audio manager should be running before loading this module.
