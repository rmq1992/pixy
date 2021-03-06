//
// begin license header
//
// This file is part of Pixy CMUcam5 or "Pixy" for short
//
// All Pixy source code is provided under the terms of the
// GNU General Public License v2 (http://www.gnu.org/licenses/gpl-2.0.html).
// Those wishing to use Pixy source code, software and/or
// technologies under different licensing terms should contact us at
// cmucam@cs.cmu.edu. Such licensing terms are available for
// all portions of the Pixy codebase presented here.
//
// end license header
//

#include "progblobs.h"
#include "pixy_init.h"
#include "camera.h"
#include "led.h"
#include "conncomp.h"
#include "serial.h"
#include "rcservo.h"
#include "exec.h"

bool g_ledSet = false;

Program g_progBlobs =
{
	"blobs",
	"perform color blob analysis",
	blobsSetup, 
	blobsLoop
};



int blobsSetup()
{
	uint8_t c;

	// setup camera mode
	cam_setMode(CAM_MODE1);
 	
#ifdef DA
	// load lut if we've grabbed any frames lately
	if (g_rawFrame.m_pixels)
		cc_loadLut();
#endif

	// if there have been any parameter changes, we should regenerate the LUT (do it regardless)
	g_blobs->m_clut.generateLUT();	
			
	// setup qqueue and M0
	g_qqueue->flush();
	exec_runM0(0);

	// flush serial receive queue
	while(ser_getSerial()->receive(&c, 1));

	return 0;
}

void handleRecv()
{
	uint8_t i, a;
	static uint8_t state=0;
	static uint16_t w=0xffff;
	uint16_t s0, s1;
	Iserial *serial = ser_getSerial();

	for (i=0; i<10; i++)
	{
		switch(state)
		{	
		case 0: // look for sync byte 0
			if(serial->receive(&a, 1))
			{
				w = a;
				w <<= 8;
				state = 1;
			}
		 	break;

		case 1:	// sync byte 1
			if(serial->receive(&a, 1))
			{
 				w |= a;
				state = 2;
			}

		case 2:	 // receive data byte(s)
			if (w==SYNC_SERVO)
			{	// read rest of data
				if (serial->receiveLen()>=4)
				{
					serial->receive((uint8_t *)&s0, 2);
					serial->receive((uint8_t *)&s1, 2);

					//cprintf("servo %d %d\n", s0, s1);
					rcs_setPos(0, s0);
					rcs_setPos(1, s1);

					state = 0;
				}
			}
			else if (w==SYNC_CAM_BRIGHTNESS)
			{
				if(serial->receive(&a, 1))
				{
					cam_setBrightness(a);
					state = 0;
				}
			}
			else if (w==SYNC_SET_LED)
			{
				if (serial->receiveLen()>=3)
				{
					uint8_t r, g, b;
					serial->receive(&r, 1);
					serial->receive(&g, 1);
					serial->receive(&b, 1);

					led_setRGB(r, g, b);

					g_ledSet = true; // it will stay true until the next power cycle
					state = 0;
				}
			}
			else if(serial->receive(&a, 1)) // not supported, receive byte to resync, you know, throw one byte out, receive 2, throw 1 out....
				state = 0;
			break;

		default:
			state = 0;
			break;
		}
	}
}


int blobsLoop()
{
#if 1
	BlobA *blobs;
	BlobB *ccBlobs;
	uint32_t numBlobs, numCCBlobs;
	static uint32_t drop = 0;

	// create blobs
	if (g_blobs->blobify()<0)
	{
		DBG("drop %d\n", drop++);
		return 0;
	}
	// handle received data immediately
	handleRecv();

	// send blobs
	g_blobs->getBlobs(&blobs, &numBlobs, &ccBlobs, &numCCBlobs);
	cc_sendBlobs(g_chirpUsb, blobs, numBlobs, ccBlobs, numCCBlobs);

	ser_getSerial()->update();

	// if user isn't controlling LED, set it here, according to biggest detected object
	if (!g_ledSet)
		cc_setLED();
	
	// deal with any latent received data until the next frame comes in
	while(!g_qqueue->queued())
		handleRecv();

#endif
#if 0
	Qval qval;
	static int i = 0;
	while(1)
	{
		if (g_qqueue->dequeue(&qval) && qval.m_col==0xffff)
		{
			cprintf("%d\n", i++);
			break;
		}	
	}
#endif
#if 0
	BlobA *blobs;
	BlobB *ccBlobs;
	uint32_t numBlobs, numCCBlobs;
	static uint32_t drop = 0;

	// create blobs
	if (g_blobs->blobify()<0)
	{
		DBG("drop %d\n", drop++);
		return 0;
	}
	g_blobs->getBlobs(&blobs, &numBlobs, &ccBlobs, &numCCBlobs);
	cc_sendBlobs(g_chirpUsb, blobs, numBlobs, ccBlobs, numCCBlobs);

#endif

	return 0;
}
