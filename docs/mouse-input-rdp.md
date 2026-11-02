# Mouse Input Issues and Workarounds on WSL/VLab or other RDP <!-- omit from toc -->

Based on feedback from students in the individual assignmentand, and following investigation from our staffs, we found that on WSL/VLab or most of remote develop environments, there are mouse input issues, especially when the cursor mode of GLFW is set to `GLFW_CURSOR_DISABLED`. The cursor may drift or move unexpectedly, which can affect camera control and gameplay. 

## Table of Contents <!-- omit from toc -->
- [Workarounds](#workarounds)
  - [Develop on CSE machines (Most recommended)](#develop-on-cse-machines-most-recommended)
  - [If you keep working on WSL/VLab or other RDP](#if-you-keep-working-on-wslvlab-or-other-rdp)
  - [If you have native Mac OS or Linux](#if-you-have-native-mac-os-or-linux)
  - [If you want to develop on Windows platform](#if-you-want-to-develop-on-windows-platform)
- [Why this issue happens](#why-this-issue-happens)
- [Why we insist on compling and working on VLab or CSE machines](#why-we-insist-on-compling-and-working-on-vlab-or-cse-machines)


## Workarounds

Here are some workarounds to bypass the mouse input issues

### Develop on CSE machines (Most recommended)

In CSE computer labs or K17 G3-G5 Help Desk, you can find CSE machines, they are native Linux machines with course required software configuration, we have tested on these machines, the `GLFW_CURSOR_DISABLED` mode works fine on CSE machines (Not VLab). You can use these machines to develop your games and test your code.

### If you keep working on WSL/VLab or other RDP

In this case, we recommend you to use the cursor mode `GLFW_CURSOR_NORMAL` or `GLFW_CURSOR_HIDDEN` instead of `GLFW_CURSOR_DISABLED`. This will allow you to move the mouse without any drift or unexpected movement. 

However, this may affect the gameplay experience, as the cursor will be visible and/or may interfere with the game, since your cursor will touch the window border. If your whole group have nobody with other development environment, we do not recommend you to develop games requiring precise mouse input and large camera movements, like FPS (First Person Shooter). 

You may need to implement your own camera control logic to handle mouse input in this mode, e.g. use right mouse button to control it like you drap in Google Maps, or if you ever play RTS games, like in StarCraft, Warcraft, Age of Empires or Red Alert and etc.

> [!WARNING]
> When you develop on WSL/VLab or other RDP, you should be careful about use `glfwSetCursorPos` to set the cursor, it will lead drifting or unexpected movement similar to `GLFW_CURSOR_DISABLED` mode, even though you use other cusor modes. The reason is expliaine in [Why this issue happens](#why-this-issue-happens).

> [!NOTE]
> Some tutorials or docments may suggest you enable [raw mouse motion](https://www.glfw.org/docs/3.3/input_guide.html#raw_mouse_motion) for develop 3D camera. However, this may also have issues as well on WSL/VLab or other RDP environments. Even though `glfwRawMouseMotionSupported())` returns true, it may not work as expected based on our testing, or you need to reduce the mouse sensitivity to use it. 

### If you have native Mac OS or Linux

In this case, we recommend you develop on these native plaforms, these platforms should be fine with `GLFW_CURSOR_DISABLED` mode and with similar development experience like VLab or CSE machines, but after you finish, you still need to make sure your code can successfully compile and basic gameplay works on VLab or CSE machines.

### If you want to develop on Windows platform

In this case, we recommend you read the [C/C++ for Visual Studio Code instructions](https://code.visualstudio.com/docs/languages/cpp#_set-up-your-c-environment) to know how to set up your C/C++ development environment on Windows. But agiang, you need to make sure your code can successfully compile and basic gameplay works on VLab or CSE machines. 

> [!Note]
> In addition, you should make sure your are developing on Core Profile of OpenGL, and implement shaders correctly and provide shaders for your program to work. See [why we insist on compling and working on VLab or CSE machines](#why-we-insist-on-compling-and-working-on-vlab-or-cse-machines) to know why we insist on compling and working on VLab or CSE machines.

## Why this issue happens

You may curious why this issue happens on WSL/VLab when using `GLFW_CURSOR_DISABLED` mode. 

Firstly, we need to understand how `GLFW_CURSOR_DISABLED` mode works. For this mode, as described in the [Input Gudd of GLFW](https://www.glfw.org/docs/latest/input_guide.html#cursor_mode), when the cursor is disabled, 

> This will hide the cursor and lock it to the specified window. GLFW will then take care of all the details of cursor re-centering and offset calculation and providing the application with a virtual cursor position. 

The issue is that, on WSL/VLab or other RDP, the cursor re-centering or locking/capture only happens to the RDP client, but it is not correctly synchronized with the cursor position in the local machine. So when you move the mouse, the cursor position in the local machine may be different from the cursor position in the RDP client, which will lead to drifting or unexpected movement, since there are large differences between last and current positions. 

For WSLg (Windows Subsystem for Linux GUI) and the TigerVNC (the VNC we use mostly to connect to VLab), they are still open issues, you can find related discussions in [WSLg issue #376](https://github.com/microsoft/wslg/issues/376) and [TigerVNC issue #619](https://github.com/TigerVNC/tigervnc/issues/619), if you are interested in the technical details, you can read these discussions. Maybe in the future, you can find a better solution and contribute to the open source community.

## Why we insist on compling and working on VLab or CSE machines

Use VLab or CSE machines is to ensrue a fair and consistent development environment for all students. 

And for Wndows platform, sometimes, even if you do not implement shaders correctly or enven do not provide shaders, the program may still work, since on Windows, the GPU driver may provide some default shaders for you, allowing the program to function without explicit shader implementation. This is known as the Fixed-Function Pipeline. However, on Mac OS, Linux, VLab or CSE machines, the GPU driver may not provide default shaders, and if you do not implement shaders correctly or do not provide shaders, the program may not work. We requres you to work on Core Profile of OpenGL, you need to implement shaders correctly and provide shaders for your program to work.