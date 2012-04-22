//
//  XPNetworking.h
//  XPHelloWorld
//
//  Created by Kyle Miracle on 4/20/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef XPHelloWorld_XPNetworking_h
#define XPHelloWorld_XPNetworking_h

struct FlightData {
    float vspeed;
    float airspeed;
    float altitude;
};

void sendDataToServer(struct FlightData fd);


#endif
