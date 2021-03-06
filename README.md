# DisplayWald - A simple diplay wall driver




## Introduction

This is a simple library for displaying large frame buffers on a
display wall. We do explicitly NOT offer any windowing abstractions,
I/O, etc; this code is EXCLUSIVELY for displaying. In particular, this
code allows for specifying an arrangement of WxH displays (all of
which have to have the same resolution); we then launch one process
per dipslay node, which opens a (usually fullscreen) window and
displays frames when they are done.

We do support stereo, and a library to connect to this display wall
and (in a mpi parallel fashion) write to this display wall from
multiple nodes. Both dw server (that runs the displays) as well as
client access library (which allows an (mpi-parallel) app to write to
this display wall) use MPI for communication. 

For network topologies where the app node(s) cannot 'see' the actual
display nodes directly we also support a 'head node' setup where the
server establishes a dispatcher on a single head node that is visible
from both app and display nodes.

Eventually this code will compress tiles before writing; but this is
NOT yet implemented (look at CompresssedTile::encode/decode() to
implement this, probably using libjpg, libjpeg-turbo, or libpng).




## Compiling the DisplayWald module

DisplayWald is a OSPRay *module*, and thus depends on a version of
OSPRay to be compiled in. To do so, the displayWald source directory
has to be placed into the <ospray>/modules/ directory, where <ospray>
is the directory that contains the OSPRay source directory (the
easiest of putting the module there is to directory clone it into this
directory). 

Once displayWald source is placed into this module/ directory the next
build of OSPRay should automatically build it. Note that the directory
name of the displayWald sources does not matter; ospray will
automatically build _all_ subdirectories it can find under the
modules/ dir.

To be able to compile the displayWald sources the
OSPRAY_BUILD_MPI_DEVICE flag has to be turned on; if you want to _not_
build the display wald code you can turn it off in the cmake config.




### Starting the DisplayWald Server

First: determine if you do want to run with a head node, or
without. If all display nodes can be seen from the nodes that generate
the pixels you do not need a head node (and arguably shouldn't use
one); if the actual display nodes are 'hidden' behind a head node you
have to use a head node.

Second, determine display wall arrangement (NX x NY displays, stereo?, arrangement?). 

Finaly, laucnh the ospDisplayWald executable through mpirun, with the proper paramters.

Example 1: Running on a 3x2 non-stereo display wall, WITHOUT head node

    mpirun -perhost 1 -n 6 ./ospDisplayWald -w 3 -h 2 --no-head-node

Note how we use 6 (==3x2) ranks; which is one per display.

Example 2: Running on a 3x2 non-stereo display wall, WITH dedicated head node

    mpirun -perhost 1 -n 7 ./ospDisplayWald -w 3 -h 2 --head-node
	
Note we use have to use *7* ranks; 6 for the displays, plus one (on rank 0) for the head node.

In all examples, make sure to use the right hosts file that properly
enumerates which ranks run which role. WHen using a head node, rank 0
should be head node; all other ranks are one rank per display,
starting on the lower left and progressing right first (for 'w'
nodes), then upwards ('h' times)

Once the displayWald is started, it should print a hostname and a port
name that it is listening on for connections. To check if everything
is working, use

	./ospDwPrintInfo <hostName> <portNum>
	
Note that the displaywald will _not_ yet show anything; it will only
display anything once a client actually connects and renders to it.




### Running the Test Renderer

To run a test-renderer use

	mpirun -n <numRanks> <other mpi params> ./ospDwTest <hostName> <portNum>
	
This should start rendering a test image on the display wald. Note you
(currently) have to kill and re-start the server every time an
application has rendered on it.



### Running with the OSPRay GlutViewer

DisplayWald also comes with a simple OSPRay pixelop to get frame
buffer tiles onto the display wald. As soon as the displayWald module is
enabled, the GlutViewer will be able to "mirror" its own render window
on the display window (at much higher resolution, of course). To run this, run

	mpirun -n <numRanks> <mpiFlags> ./ospGlutViewer <model> --module displayWald --display-wall <host> <port>
	
(using the host:port settings that the server printed out when starting).

The glutviewer will then automatically query the display wall
resolution, open a second frame buffer at that resolution, and use the
provided pixelOp (in displayWald/ospray/) to get that frame buffer
onto the displays.

Note this method _does_ require an mpirun for both the display wall
server _and_ the ospGlutViewer; though the host:port refers to a
TCP/IP socket that serves the display wall config, the internal
communication requires MPI, so the glutviewer _has_ to be run with an
mpirun.






## User Guide

### Cmd-line parameters for the DisplayWald Server

The DisplayWald server understands the following command line parameters

- --width|-w <Nx>         number of displays in x dimensions
- --height|-h <Ny>        number of displays in y dimension
- --[no-]head-node|-[n]hn Run with resp without dedicated head node on rank 0
- --bezel|-b <rx> <ry>    Bezel width relative to screen size (see below)
- --window-size <Nx> <Ny> resolution of window we are opening (if windowed mode)




### Bezels

Most displays have some non-trivially sized bezels on the sides of the
displays; so the rightmost pixel on one screen and the leftmost pixel
on the one right of that do have a greater distance than two pixels on
the same screen. To avoid the distortions that would be caused by that
DisplayWald allows for specifying a bezel width; if specified, this
will cause the server to create a virtual screen that is larger than
the number of actual pixels on the screen (ie, there are virtual
pixels where the bezels are). 

To specify the bezel width, use the "--bezel|-b <rx> <ry>"
parameters. Those two parameters specify the width of the bezels (in x
and y direction) relative to the actual screen. Eg, if your actual
screen area is 30cmx20cm, you have a left and right bezel of 5mm each,
a top bezel of 2mm, and a bottom bezel of 10mm, then the total bezel
width in x direction is 2x5mm=10mm, and a total bezel height of
2mm+10mm=12mm. Relative to a screen area of x=300mm and y=200mm those
bezels values of x=10mm and y=12mm bezels are x=10/300=0.0333 and
y=12/200=0.6; so you would use "--bezel 0.3333 0.6".







## Programming Guide

### Code organization

The code is organized into two different components organized into differnet libraries:

- "common/" is some shared infrastructure used by both DW client and DW service; like 
  code for encoding/decoding tiles, etc
  
- "service/" implements the display wall "service"; ie, the piece of
  code that opens a connection, receives tiles of pixels, decodes
  them, writes them into a frame buffer, and finally displays them on
  a screen.

- "client/" implements a library that a mpi-distributed renderer can
  use to send pixels to the dispaly wall service; this library opens a
  connection to the display wall, compresses tiles, routes them to the
  proper service nodes, etc.

- "ospray/" contains a specific client that uses OSPRay PixelOp's to
  get a given ospray frame buffer's pixels onto a wall.
  



### Implementing a DisplayWald client

The display wall "client" is the library that a MPI parallel renderer uses
to put distributedly rendered tiels of pixels onto a given display wall.

To use the client library from a parallel renderer, you basically
follow these steps:
- link to the displaywald-client lib
- create a client
- establish a connection (client->establishConnection) [do this together on all ranks!].
  YOu need to specify the MPI port name that the service is running on.
- look at the respective display wall cofig to figure out what frame res to deal with
- do writeTiles() until all of a frame's pixels have been set
- do a endFrame() ONCE (per client) at the end of each frame






### The DisplayWald "service"

Though above described as a single entity, the "service" actually
consists of two parts: First, the part that receives tiles of pixels,
decodes them, puts them into a frame buffer, and finally passes the
ready-assembled frame buffer (for that node) to "somebody" to
"display"; and second, the actual MPI-parallel application that opens
a window, initailizes the receiver service, and sets a hook with the
service library to put the readily-assmbled frame buffer onto the
screen.

In addition to the service library itself, the service/ directory also
contains a sample glut-based application that does exactly the latter.





## TODO

### high priority

- implement full-screen capabilities for glutwindow
- implement full-screen capabilities for glutwindow
- add an abstract API (ospDwInit(), ospDwWriteTile(), ospDwEndFrame())




### low priority 

- implement stereo support; let _client_ request stereo mode (not
  glutwindow), and have server react accordingly. need to modify 'api'
  for writeTile to specify which eye the tile belongs to.
- add some way of upscaling; ie, render at half/quarter the display res and
  upscale during display



## DONE

- add a ospray pixel op to access the wall 
- add new handshake method via port (ie, open tcp port on server rank 0,
  send mpi port to whoever connects on this)
- multi-thread the tile receiving and decoding
- perform some sort of compressoin of tiles (currently using libjpeg-turbo)
- establish connection and route pixels from client to service

  
