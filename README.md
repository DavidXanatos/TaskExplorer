# TaskExplorer

Task Explorer is a powerful task management tool designed not only to monitor running applications but to provide deep insight into what those applications are doing. Its user interface prioritizes speed and efficiency, delivering real-time data on processes with minimal interaction. Instead of requiring multiple windows or sub-windows, Task Explorer displays relevant information in accessible panels. When selecting a process, detailed information is displayed in the lower half of the screen, allowing you to navigate through the data seamlessly using the arrow keys. The dynamic data refresh allows users to observe changes in real-time, offering additional clarity and insight into system performance and behavior.

## Features

Task Explorer offers an array of advanced features to provide comprehensive visibility into the system. The **Thread Panel** displays a stack trace for the selected thread, offering immediate insights into the current actions of an application, which is particularly useful for diagnosing deadlocks or performance bottlenecks. The **Memory Panel** allows users to view and edit process memory, featuring an advanced memory editor with string search capabilities. In the **Handles Panel**, all open handles are displayed, including essential details such as file names, current file positions, and sizes, giving a clear view of the disk operations a program is performing. 

The **Socket Panel** provides visibility into all open connections or sockets for each process, with additional data rate information. It also has the option to show pseudo UDP connections based on ETW data, allowing users to monitor network communications effectively. The **Modules Panel** lists all loaded DLLs and memory-mapped files, with the ability to unload or inject DLLs as needed. Additionally, the application includes a variety of other useful panels, including **Token**, **Environment**, **Windows**, **GDI**, and **.NET** panels.

By double-clicking a process, you can open the **Task Info Panels** in a separate window, enabling the simultaneous inspection of multiple processes. The system monitoring capabilities are robust as well, featuring toolbar graphs that show real-time usage of system resources such as CPU, handles, network traffic, and disk access. The **System Info Panels** display all open files and sockets and allow users to control system services, including drivers. Dedicated performance panels for CPU, Memory, Disk I/O, Network, and GPU resources offer detailed graphs, making it easy to monitor and optimize system performance.

For users who need more screen space, the **System Info Panel** can be fully collapsed or opened in a separate window, maximizing the available area for the task panels.

## System Requirements

Task Explorer is compatible with Windows 7 or higher, on both 32-bit and 64-bit systems.

## Additional Information

Task Explorer is built using the Qt Framework, ensuring a cross-platform user interface with plans to eventually port the tool to Linux, which could make it one of the first advanced, GUI-based task managers for the platform. On Windows, Task Explorer leverages the Process Hacker library and uses a custom-compiled version of the systeminformer.sys driver from the [SystemInformer](https://github.com/winsiderss/systeminformer/) project, ensuring robust performance and system monitoring capabilities.

## Support

If you find Task Explorer useful, please consider supporting the project on Patreon: [https://www.patreon.com/DavidXanatos](https://www.patreon.com/DavidXanatos)

Icons provided by [Icons8](http://icons8.com/).
