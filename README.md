log2csv
=======

Log2csv is a very small and basic framework logging Heating Ventilation and Air
Conditioning (HVAC)-related data into a CSV file. It consists of a main 
application, log2csv and sub modules connecting data sources like HVAC 
controllers or sensor equipment installed.
Currently only a connector accessing UVR 61-3 devices manufactured by 
Technische Alternative Elektronische Steuerungsgerätegesellschaft m.b.H(TA) 
(www.ta.co.at) is provided. The framework application is located at the 
CSVLogger directory. It reads the provided configuration, loads given modules,
initiates loading the data and stores the results into a CSV file.

The program was initially written to read data from some temperature sensors
using a self-made "field-bus" not ready to be published yet. It was then 
extended to read parameters provided by the UVR 61-3 controller using a D-LOGG
data logging device connected via USB and is still on an early stage of 
development. The framework is limited to real-time like data and doesn't 
provide any means of reading historical data. If you just want to access data 
from devices manufactured by Technische Alternative using Linux, give d-logg-
linux (http://d-logg-linux.roemix.de/) a try first! If you need some source of 
inspiration concerning the implementation of TA's protocol or if your device is
not supported by d-logg-linux please feel free to browse the source code and 
try to install the program. If you find any bugs or if you're willing to test 
any new features I will be glad if you open a ticket or contact me via e-mail.

Thanks to the helping hand of TA's support team it was possible to develop the
D-LOGG module using their original specification! If you need the protocol 
specification, please contact the TA support team too. The project repository
won't contain any specification of TA's protocols. I also thank Dipl.-Ing. 
Guenther Gridling from the Institute of Computer Aided Automation, Vienna 
Technical University who enabled writing the first version of the software in 
the context of the curriculum "Technische Informatik" (computer engineering).

# Features

* Create CSV files and write a headline
* Log to CSV files
* Reading custom configurations
* Synchronization mechanism between different field-bus modules
* Configuration of individual data channels and channel headers
* Flexible design allowing to include further modules
* Individual time-stamp format
* String, double and integer values supported

Devices:
* Up to two DL-Bus devices
* UVR 61-3 controller
* D-LOGG USB data logger, version 2.9 or higher

Platform:
* Linux (Tested with Kernel 3.8 and 4.4 only) on
* ARM-Architecture (Tested with BeagleBone Black)

# Installation

The logging framework and each module suite has to be compiled separately using
the makefile provided. On cross-compiling the CROSS_COMPILE variable as well as
the LIB_DIR variable might be set accordingly. The application requires at 
least libconfig to be installed properly. If the USE_LIBFTDI variable located 
in DLoggModule/Makefile is set to "true" another back-end will be used to 
access the D-LOGG device. The alternative backend additionally requires 
libusb 1.0 and libftdi 1.1.

Any installation currently has to be undertaken manually. The program assumes 
that the configuration file is located at /etc/log2csv.cnf but an alternating 
path might be passed using the -c switch. The configuration file has to 
reference the module *.so files properly. See CSVLogger/etc/log2csv.cnf for a 
documented configuration example.

## Example Build Process

The following example build process was executed on a BeagleBone Black running 
Debian 9.6. It includes installing necessary programs and libraries as
well as compiling log2csv. Two versions are given, one which uses the default 
Linux interfaces and one which uses libftdi.

### Default Build Process

First, programs available via the package manager are installed.
Pleas note that the libraries have to include the header files (*-dev) to build
the programs.

`$ sudo apt-get install git binutils make gcc`
`$ sudo apt-get install libconfig-dev`

Afterwards, the source code is compiled. The current build process isn't really
well developed and hence every module needed has to compiled on it's own:

`$ git clone git@github.com:Spiegel0/log2csv.git log2csv`
`$ cd log2csv/DLoggModule/`
`$ make binary`

`$ cd ../CSVLogger/`
`$ make binary`

Now every available module should be built successfully. The default make 
target "all" is also available but requires doxygen to create the source-code 
documentation. If you do not have doxygen installed, call the "binary" make 
target instead of "all".
You may now want to edit the program's configuration file 
`CSVLogger/etc/log2csv.cnf` with your favorite text editor and try log2csv:

`$ vim etc/log2csv.cnf`
`$ ./log2csv -c etc/log2csv.cnf`

If something does not work as expected, please open a ticket on GitHub. Note, 
that the program is still in a very early beta stage and may not behave as 
expected.

## Alternative libftdi Back-End

If the default Linux driver doesn't work reliably (i.e. suddenly stops 
working), an alternative back-end is available. The back-end uses the 
libftdi 1.1 library to access the FTDI chip in the DLogg module.

`$ sudo apt-get install git binutils make gcc`
`$ sudo apt-get install libconfig-dev libftdi1-dev`

Now, it should be possible to clone and compile the logging application. The 
variable USE_LIBFTDI=true instructs the make to use the alternative back-end.

`$ git clone git@github.com:Spiegel0/log2csv.git log2csv`
`$ cd log2csv/DLoggModule/`
`$ make USE_LIBFTDI=true binary`

`$ cd ../CSVLogger/`
`$ make binary`

# Limitations

Since the Linux kernel module implementation of the USB UART adapter (FT232R)
used to connect the D-LOGG device seems buggy on ARM Linux platforms (tested 
with kernel 3.8.13 on a BeagleBone Black) a userspace library-based version was 
added. The alternative version requires more libraries to be installed but does 
not rely on the module implementation shipped with the kernel. The alternative 
UART API can be enabled in the Makefile of the DLogg module. Unfortunately, also
the alternative implementation seems to crash eventually but less frequently. A 
quick fix is to reset the usb device by first writing "-1" to 
`/sys/bus/usb/devices/usb1/bConfigurationValue`. After approximately one second,
the device can be enabled again by writing "1" to the same file.

It turned out that newer kernel versions (e.g. 4.4) are less sensitive to that 
USB UART bug and do not require the reset-workaround anymore. Still, sometimes 
missing messages are observed.

# License and Warranty

log2csv

Copyright (C) 2019 Michael Spiegel
E-Mail: michael.h.spiegel@gmail.com

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

