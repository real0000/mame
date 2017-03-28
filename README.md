
# **VR MAME** #

What is VR MAME?
=============
VR MAME is a fork of MAME that add openvr and nvidia physx to emulate real machine

How to compile?
===============

1. install vs2015, since It's works only with d3d9 with 11, so I take vs2015 and msbuild as main IDE and compiler.

2. install fbx sdk(2017), set enviroment value ADSK_FBX_SDK_2017 to installed path

3. get access authority of nvidia gamework(https://developer.nvidia.com/gameworks-physx-overview)

4. run "make vs2015", and open "mame.sln".

VR machine list
===============
punch mania 2

License
=======

Information about the MAME content can be found at https://github.com/mamedev/mame/blob/master/README.md

Information about the MAME license particulars and disclaimers can be found at https://github.com/mamedev/mame/blob/master/LICENSE.md