/*
 * Fadecandy Firmware - USB Support
 * 
 * Copyright (c) 2013 Micah Elizabeth Scott
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include <string.h>
#include "WProgram.h"
#include "usb_dev.h"
#include "fc_defs.h"


/*
 * A buffer of references to USB packets.
 */

template <unsigned tSize>
struct fcPacketBuffer
{
    usb_packet_t *packets[tSize];

    fcPacketBuffer()
    {
        // Allocate packets. They'll have zero'ed contents initially.
        for (unsigned i = 0; i < tSize; ++i) {
            usb_packet_t *p = usb_malloc();
            memset(p->buf, 0, sizeof p->buf);
            packets[i] = p;
        }
    }

    void store(unsigned index, usb_packet_t *packet)
    {
        if (index < tSize) {
            // Store a packet, holding a reference to it.
            usb_packet_t *prev = packets[index];
            packets[index] = packet;
            usb_free(prev);
        } else {
            // Error; ignore this packet.
            usb_free(packet);
        }
    }
};


/*
 * Iterator referring to one pixel in the framebuffer
 */

struct fcFramebufferIter
{
    unsigned packet;
    unsigned index;
    unsigned component;

    ALWAYS_INLINE void setNegativeOne()
    {
        // Special case: After one call to next(), this points to LED #0.
        index = -1;
        packet = 0;
        component = -3;
    }

    ALWAYS_INLINE void set(unsigned led)
    {
        packet = led / PIXELS_PER_PACKET;
        index = 1 + (led % PIXELS_PER_PACKET) * 3;
        component = led * 3;
    }

    ALWAYS_INLINE void next()
    {
        index += 3;
        unsigned overflow = index >> 6;
        index = (index + overflow) & 63;
        packet += overflow;
        component += 3;
    }
};


/*
 * Framebuffer
 */

struct fcFramebuffer : public fcPacketBuffer<PACKETS_PER_FRAME>
{
    ALWAYS_INLINE const uint8_t* pixel(fcFramebufferIter iter) const
    {
        return &packets[iter.packet]->buf[iter.index];
    }
};


/*
 * Color Lookup table
 */

struct fcColorLUT : public fcPacketBuffer<PACKETS_PER_LUT>
{
    ALWAYS_INLINE const unsigned entry(unsigned index)
    {
        const uint8_t *p = &packets[index / LUTENTRIES_PER_PACKET]->buf[2 + (index % LUTENTRIES_PER_PACKET) * 2];
        return *(uint16_t*)p;
    }
};


/*
 * All USB-writable buffers
 */

struct fcBuffers
{
    fcFramebuffer *fbPrev;      // Frame we're interpolating from
    fcFramebuffer *fbNext;      // Frame we're interpolating to
    fcFramebuffer *fbNew;       // Partial frame, getting ready to become fbNext

    fcFramebuffer fb[3];        // Triple-buffered video frames

    fcColorLUT lutNew;                   // Partial LUT, not yet finalized
    uint16_t lutCurrent[LUT_SIZE + 1];   // Active LUT, linearized for efficiency, padded on the end.

    fcBuffers()
    {
        fbPrev = &fb[0];
        fbNext = &fb[1];
        fbNew = &fb[2];
    }

    void handleUSB();

private:
    void finalizeFramebuffer();
    void finalizeLUT();
};
