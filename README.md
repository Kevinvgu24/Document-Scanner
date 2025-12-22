# Document-Scanner
A local program using external camera to scanning documents and image then save to PDF and JPEG file on your computer
===================Instruction====================

Please read the comments code inside the program to understand how the code runs. You can read documents on OpenCV for more detailed information.

STEP 1: Download and Extract OpenCV
  Go to the homepage: opencv.org/releases
  
  Select the latest version (e.g., 4.10.0 or 4.8.0).
  
  Click the Windows button to download the .exe file.
  
  Run the .exe file. It will ask where to extract it.
  
  Recommended: Extract to the root drive for a shorter path. For example: C:\ or D:\.
  
  After extraction, you will have the folder: C:\opencv.

STEP 2: Install Environment Variables
  This step helps Windows find the OpenCV .dll file when you run the program. If you skip this step, the program will report an error "Missing       opencv_world...dll".

  Press the Windows key, type "Edit the system environment variables," and press Enter.

  Select the Environment Variables button...

  In the frame below (System variables), find the line Path -> Select Edit.

  Click New and paste the OpenCV bin folder path.

  The path is usually: C:\opencv\build\x64\vc16\bin

  (Note: vc16 is for VS 2019/2022. If there is a vc17 folder later, choose the newest one.)

  Click OK repeatedly to close all windows.

  Important: Restart your computer (or sign out) for Windows to recognize the new path.

  STEP 3: Configuration in Visual Studio
  Now you can open your Project DocumentScanner.

    1. Select the x64 platform. The standard OpenCV version currently only supports 64-bit.

    On the Visual Studio toolbar (next to the green Play button), change x86 (or Debug) to x64.

    2. Open the Properties panel.

    Right-click on the Project name (in Solution Explorer) -> Select Properties.

    In the Configuration section (top left corner of the panel), select All Configurations (to apply settings to both Debug and Release).

    3. Add the Include folder (containing the .hpp file).

    Go to: C/C++ -> General.

    In the Additional Include Directories line: Click the arrow -> Edit -> Add the path: C:\opencv\build\include

4. Add the Library folder (containing the .lib file).

    Go to: Linker -> General.

    In the Additional Library Directories line: Click the arrow -> Edit -> Add the path: C:\opencv\build\x64\vc16\lib

    5. Declare the library file name (Linker Input). This step is necessary for both Debug and Release modes.

    For DEBUG mode (used when writing code):

    Change Configuration (top left corner) to Debug.

    Go to: Linker -> Input.

    In the Additional Dependencies line: Add the name of the lib file with the .d extension (debug).

    For example, for version 4.8.0, it would be: opencv_world480d.lib (Check the lib folder in step 4 to see the exact name of your version            number).

    For RELEASE mode (used when exporting executable files):

    Change Configuration to Release.

    Go to: Linker -> Input.

    In the Additional Dependencies line: Add the name of the lib file without the .d extension.

    For example: opencv_world480.lib

   ==============================================================================================
