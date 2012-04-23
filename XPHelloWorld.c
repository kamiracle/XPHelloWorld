//
//  XPDataWriter.c
//  XPHelloWorld
//
//  Created by Kyle Miracle on 4/14/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#if APL
#if defined(__MACH__)
#include <Carbon/Carbon.h>
#endif
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMProcessing.h"
#include "XPLMMenus.h"
#include "XPLMUtilities.h"
#include "XPLMDataAccess.h"
#include "XPHelloWorld.h"

//Network Includes
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

#define BUFSIZE 300

const char * serverIP = "127.0.0.1";
const char * version = "X-Plane Airlines version 0.1";
int * gSocket;

XPLMDataRef gVerticalSpeedRef = NULL;
XPLMDataRef gIndicatedAirspeedRef = NULL;
XPLMDataRef gAltitudeRef = NULL;
float gVspeed,gAirspeed,gAltitude;

XPLMWindowID	gWindow = NULL;
int				gClicked = 0;
int             drawWinow = 1;

#if APL && __MACH__
int ConvertPath(const char * inPath, char * outPath, int outPathMaxLen);
#endif

void logFlightData();

/* Loop callback prototype. */
float	MyFlightLoopCallback(
                             float                inElapsedSinceLastCall,    
                             float                inElapsedTimeSinceLastFlightLoop,    
                             int                  inCounter,    
                             void *               inRefcon);  

float	SendDataFlightLoopCallback(
                             float                inElapsedSinceLastCall,    
                             float                inElapsedTimeSinceLastFlightLoop,    
                             int                  inCounter,    
                             void *               inRefcon);  

void MyDrawWindowCallback(
                          XPLMWindowID         inWindowID,    
                          void *               inRefcon);    

void MyHandleKeyCallback(
                         XPLMWindowID         inWindowID,    
                         char                 inKey,    
                         XPLMKeyFlags         inFlags,    
                         char                 inVirtualKey,    
                         void *               inRefcon,    
                         int                  losingFocus);    

int MyHandleMouseClickCallback(
                               XPLMWindowID         inWindowID,    
                               int                  x,    
                               int                  y,    
                               XPLMMouseStatus      inMouse,    
                               void *               inRefcon);


PLUGIN_API int XPluginStart(
                            char *		outName,
                            char *		outSig,
                            char *		outDesc)
{    
    strcpy(outName, "Data Writer");
    strcpy(outSig, "kamiracle.xpdatawriter");
    strcpy(outDesc, "A plugin that continuously output ceratain sim data to a local file");
    
    gVerticalSpeedRef = XPLMFindDataRef("sim/flightmodel/position/vh_ind_fpm");
    gIndicatedAirspeedRef = XPLMFindDataRef("sim/flightmodel/position/indicated_airspeed");
    gAltitudeRef = XPLMFindDataRef("sim/flightmodel/position/elevation");
    
	gWindow = XPLMCreateWindow(
                               50, 600, 300, 200,			/* Area of the window. */
                               1,							/* Start visible. */
                               MyDrawWindowCallback,		/* Callbacks */
                               MyHandleKeyCallback,
                               MyHandleMouseClickCallback,
                               NULL);						/* Refcon - not used. */
    
    char	outputPath[255];
    
#if APL && __MACH__
	char outputPath2[255];
	int Result = 0;
#endif
    
    XPLMGetSystemPath(outputPath);
    strcat(outputPath, "XPDataWriter.txt");
    
#if APL && __MACH__
	Result = ConvertPath(outputPath, outputPath2, sizeof(outputPath));
	if (Result == 0)
		strcpy(outputPath, outputPath2);
	else
		XPLMDebugString("TimedProccessing - Unable to convert path\n");
#endif
    
    //gDataOutLog = fopen(outputPath, "w");
    
    /* Register our flight loop callback for once every thirty seconds. */
    
    XPLMRegisterFlightLoopCallback(MyFlightLoopCallback, 1.0, NULL);
    XPLMRegisterFlightLoopCallback(SendDataFlightLoopCallback, 10.0, NULL);
    
    return 1;
}

PLUGIN_API void	XPluginStop(void)
{
	/* Unregister the callback */
	XPLMUnregisterFlightLoopCallback(MyFlightLoopCallback, NULL);
    XPLMUnregisterFlightLoopCallback(SendDataFlightLoopCallback, NULL);
    
    if(*gSocket){
        close(*gSocket);
    }
    
    XPLMDestroyWindow(gWindow);
}

PLUGIN_API void XPluginDisable(void)
{
	/* Flush the file when we are disabled.  This is convenient; you 
	 * can disable the plugin and then look at the output on disk. */
}

PLUGIN_API int XPluginEnable(void)
{
	return 1;
}

PLUGIN_API void XPluginReceiveMessage(
                                      XPLMPluginID	inFromWho,
                                      long			inMessage,
                                      void *			inParam)
{
}

float	MyFlightLoopCallback(
                             float                inElapsedSinceLastCall,    
                             float                inElapsedTimeSinceLastFlightLoop,    
                             int                  inCounter,    
                             void *               inRefcon)
{
    /* Write data to a file */
    //fprintf(gDataOutLog, "Vertical Speed=%f, Airspeed=%f, Altitude=%f.\n", vspeed, airspeed, altitude);
    
    gVspeed = XPLMGetDataf(gVerticalSpeedRef);
    gAirspeed = XPLMGetDataf(gIndicatedAirspeedRef);
    gAltitude = XPLMGetDataf(gAltitudeRef);
    
    logFlightData();
    
    return 1.0;
    
}

float	SendDataFlightLoopCallback(
                             float                inElapsedSinceLastCall,    
                             float                inElapsedTimeSinceLastFlightLoop,    
                             int                  inCounter,    
                             void *               inRefcon)
{
    int sockfd;
    short inPort = 12345;
    char buffer[256];
    struct sockaddr_in serverAddress;
    float	color[] = { 1.0, 1.0, 1.0 }; 	/* RGB White */
    int set = 1;
    
    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        perror( "socket" );
        XPLMDebugString(version);
        XPLMDebugString(": Failed at socket\n");
        return;
    }
    else {
        gSocket = &sockfd;
    }
    
    setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
    
    //bzero(&serverAddress, sizeof(serverAddress));
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(inPort);
    
    inet_pton( AF_INET, serverIP, &serverAddress.sin_addr);
    
    if ( connect( sockfd, (struct sockaddr *)&serverAddress,
                 sizeof(serverAddress)) < 0 ) 
    {
        XPLMDebugString(version);
        XPLMDebugString(": Failed at connect\n");
    }
    
    memset(&buffer, 0, sizeof(buffer));
    sprintf(buffer, "%s airspeed:%f altitude:%f verticalSpeed:%f \n", version, gAirspeed, gAltitude, gVspeed);
    
    if ( write( sockfd, buffer, 255) < 0) {
        XPLMDebugString(version);
        XPLMDebugString(": Failed at write\n");
    }
    
    close(sockfd);
    return 5.0;
}

void MyDrawWindowCallback(
                          XPLMWindowID         inWindowID,    
                          void *               inRefcon)
{
	int		left, top, right, bottom;
    
	float	color[] = { 1.0, 1.0, 1.0 }; 	/* RGB White */
    
	/* First we get the location of the window passed in to us. */
	XPLMGetWindowGeometry(inWindowID, &left, &top, &right, &bottom);

	/* We now use an XPLMGraphics routine to draw a translucent dark
	 * rectangle that is our window's shape. */
	
    if (drawWinow > 0)
    {
        
        XPLMDrawTranslucentDarkBox(left, top, right, bottom);
        
        
        /* Finally we draw the text into the window, also using XPLMGraphics
         * routines.  The NULL indicates no word wrapping. */
        XPLMDrawString(color, left + 5, top - 20, 
                       (char*)(gClicked ? "Clicked" : "Unclicked"), NULL, xplmFont_Basic);
        
        XPLMDrawString(color, left + 5, top - 40, 
                       (char*)(serverIP), NULL, xplmFont_Basic);
        
        XPLMDrawString(color, left + 5, top - 60, 
                       (char*)("Test Connection"), NULL, xplmFont_Basic);
    }
    
    
}

void MyHandleKeyCallback(
                         XPLMWindowID         inWindowID,    
                         char                 inKey,    
                         XPLMKeyFlags         inFlags,    
                         char                 inVirtualKey,    
                         void *               inRefcon,    
                         int                  losingFocus)
{
}  

int MyHandleMouseClickCallback(
                               XPLMWindowID         inWindowID,    
                               int                  x,    
                               int                  y,    
                               XPLMMouseStatus      inMouse,    
                               void *               inRefcon)
{	
	if ((inMouse == xplm_MouseDown) || (inMouse == xplm_MouseUp))
		gClicked = 1 - gClicked;
	
	return 1;
}   

#if APL && __MACH__
#include <Carbon/Carbon.h>
int ConvertPath(const char * inPath, char * outPath, int outPathMaxLen)
{
	CFStringRef inStr = CFStringCreateWithCString(kCFAllocatorDefault, inPath ,kCFStringEncodingMacRoman);
	if (inStr == NULL)
		return -1;
	CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, inStr, kCFURLHFSPathStyle,0);
	CFStringRef outStr = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
	if (!CFStringGetCString(outStr, outPath, outPathMaxLen, kCFURLPOSIXPathStyle))
		return -1;
	CFRelease(outStr);
	CFRelease(url);
	CFRelease(inStr); 	
	return 0;
}
#endif

void logFlightData()
{
    char buffer[BUFSIZE];
    
    sprintf(buffer, "Airspeed:%f Altitude:%f Vertical Speed:%f \n", gAirspeed, gAltitude, gVspeed);
    XPLMDebugString(buffer);
}






                                   