#ifndef CANMORE__CAMERA_FEED_INTERFACE_H_
#define CANMORE__CAMERA_FEED_INTERFACE_H_

#include "canmore/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Camera Feed Interface
 * *********************
 * This interface exposes a very low bitrate camera feed over CAN bus to allow accessing a debug shell over CAN bus.
 *
 * The client device will transmit frames from its camera over to the agent device, which can be monitored for debug.
 * Note that this protocol is designed for DEBUG ONLY! DO NOT RUN THIS WHILE OTHER CRITICAL SYSTEMS ARE RUNNING!
 * This protocol will put a SIGNIFICANT strain on the CAN bus, resulting in significantly reduced performance for all
 * other protocols running on the bus.
 *
 *
 * JPEG Stream
 * =====================
 * This protocol will transmit a sequence of JPEGs over the bus, one for each video frame. These JPEGs will have a
 * significant reduction in quality compared to the source image, but should be enough to see what the camera is
 * viewing.
 *
 * This is done as frames can unexpectedly drop during transmission. Rather than attempting to recover and retransmit
 * the frame (which will add extra overhead to a protocol already pushing the limits of CAN bus), if a JPEG is not
 * properly reconstructed from the individual frames transmitted, it will be entirely dropped instead. This behavior
 * prevents interframe compression codecs such as H264 from being used by this protocol.
 *
 * The best settings found to work with this protocol is each jpeg is compressed into a 240x<240 size, grayscale jpeg
 * image with 50% compression. This protocol should ideally run on a CAN FD bus as well. Although it is possible to run
 * this on an arbitrary sized jpeg (barring overflowing the 18-bit frame index), making these settings larger will
 * reduce the overall frame rate of the camera to almost unusable. The same can be said if standard CAN is used rather
 * than CAN FD, as 8x the number of frames must be transmitted, at a lower bitrate than the enhanced bitrate FD frames.
 *
 *
 * JPEG Stream Frame Format:
 *   +-*-*-*-*-*-+-*-+-*-+-*-*-*-*-+-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-+
 *   | CLIENT ID | T | D |   NOC   |             FRAME INDEX             |  (Extended 29-bit ID)
 *   +-*-*-*-*-*-+-*-+-*-+-*-*-*-*-+-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-+
 *    28      24  23  22  21    18  17                                 0
 *
 * CLIENT ID, T, D, NOC - Same as Canmore Protocol
 *   - D = Client -> Agent Direction (JPEG Stream only originates from Client)
 * FRAME INDEX: The index for this frame (starts at 0 for the first frame in a new JPEG Image)
 *   - This packet is incremented for every additional frame containing JPEG data
 *
 *
 * JPEG Stream Last Packet
 * =======================
 *
 * A Standard CANmore frame is sent as the last frame in any JPEG transmission. This frame encodes the length and CRC32
 * of the preceding JPEG frame. This frame has several purposes:
 *  - Signals to the receiver that no more frames are to be received for this JPEG, and it is safe to begin decoding
 *  - Performs error checking, such that if a frame transmit drops off from a previous transmission and resumes with the
 *    same frame index on the next image, the receiver won't attempt to decode two randomly spliced JPEGs
 *  - Used by CAN FD to trim padding data off the last frame (as CAN FD long frames can only be certain sizes)
 *
 * The last frame will be an 8 byte long standard frame (encoded for this channel), with the data field matching the
 * following format:
 *
 *   +--------+--------+--------+--------+--------+--------+--------+--------+
 *   | Byte 0 | Byte 1 | Byte 2 | Byte 3 | Byte 4 | Byte 5 | Byte 6 | Byte 7 |
 *   +--------+--------+--------+--------+--------+--------+--------+--------+
 *   |        Encoded JPEG Length        |            JPEG CRC32             |
 *   +--------+--------+--------+--------+--------+--------+--------+--------+
 *
 * Encoded JPEG Length: The length of the encoded JPEG (used to trim excess data off CAN FD frames)
 * JPEG CRC32: The CRC32 for the JPEG (after dummy bytes are trimmed from the last frame)
 *
 *
 * Stream Control
 * ==============
 * There are times when it is useful for the receiver to send commands to the transmitter to adjust its settings, such
 * as enabling/disabling the feed (to reduce the bandwidth when the stream is not in use), or select which camera
 * feed will be viewed. This exposes a one way communication channel from the receiver to the transmitter to configure
 * its behavior.
 *
 * All frames are standard CANmore frames (agent to client direction) assigned to the external camera channel, with the
 * first byte equal to the command to be transmitted. The subsequent bytes depend on the command, and are explained
 * below:
 *
 *
 * Stream Control Commands
 * =======================
 *
 * Command 0: Stream Enable
 * ------------------------
 * Enables/Disables the camera feed
 *
 * Data Format (DLC = 2):
 *   +--------+--------+
 *   | Byte 0 | Byte 1 |
 *   +--------+--------+
 *   | Cmd =0 | Enable |
 *   +--------+--------+
 *
 * Enable: Set to 1 to enable the camera feed, set to 0 to disable.
 *
 *
 * Command 1: Select Stream
 * ------------------------
 * Selects the stream to view. Note that this should be set by the receiver before enabling the camera feed.
 *
 * Data Format (DLC = 2):
 *   +--------+--------+
 *   | Byte 0 | Byte 1 |
 *   +--------+--------+
 *   | Cmd =0 | Str ID |
 *   +--------+--------+
 *
 * Str ID: The stream ID to select. The values for this are defined by the specific application. This defaults to 0 if
 * not set
 *
 * Command 2: Quality
 * ------------------
 * Sets the JPEG encoder quality
 *
 * Data Format (DLC = 2):
 *   +--------+--------+
 *   | Byte 0 | Byte 1 |
 *   +--------+--------+
 *   | Cmd =0 |  Qual. |
 *   +--------+--------+
 *
 * Quality: JPEG encoder quality - must be between 0-100.
 *
 */

// ========================================
// Remote TTY ID Definitions
// ========================================

// Macros for computing IDs
#define CANMORE_CAMERA_FEED_CALC_FRAME_ID(client_id, frame_index)                                                      \
    CANMORE_CALC_EXT_ID(client_id, CANMORE_TYPE_UTIL, CANMORE_DIRECTION_CLIENT_TO_AGENT, CANMORE_CHAN_CAMERA_FEED,     \
                        frame_index)

#define CANMORE_CAMERA_FEED_CALC_LAST_FRAME_ID(client_id) CANMORE_CALC_UTIL_ID_C2A(client_id, CANMORE_CHAN_CAMERA_FEED)

#define CANMORE_CAMERA_FEED_CALC_CTRL_ID(client_id) CANMORE_CALC_UTIL_ID_A2C(client_id, CANMORE_CHAN_CAMERA_FEED)

// ID Mask: Match everything except extra data
#define CANMORE_CAMERA_FEED_STD_ID_MASK CANMORE_CALC_FILTER_MASK(1, 1, 1, 1)
#define CANMORE_CAMERA_FEED_EXT_ID_MASK CANMORE_CALC_EXT_FILTER_MASK(1, 1, 1, 1, 0)

// ========================================
// Last Packet Encoding
// ========================================

#define CANMORE_CAMERA_FEED_LAST_FRAME_LEN 8

typedef union canmore_camera_feed_last_frame {
    uint8_t data[CANMORE_CAMERA_FEED_LAST_FRAME_LEN];
    struct __attribute__((packed)) {
        uint32_t len;
        uint32_t crc32;
    } pkt;
} canmore_camera_feed_last_frame_t;
static_assert(sizeof(canmore_camera_feed_last_frame) == CANMORE_CAMERA_FEED_LAST_FRAME_LEN, "Failed to pack");

// ========================================
// Control Channel Commands
// ========================================

// Define Stream Control Commands
#define CANMORE_CAMERA_FEED_CMD_ENABLE 0
#define CANMORE_CAMERA_FEED_CMD_STREAM_ID 1
#define CANMORE_CAMERA_FEED_CMD_QUALITY 2

#define CANMORE_CAMERA_FEED_CMD_MAX_LEN 2

// Camera feed command struct
typedef union canmore_camera_feed_cmd {
    uint8_t data[CANMORE_CAMERA_FEED_CMD_MAX_LEN];
    struct __attribute__((packed)) {
        uint8_t cmd;
        union {
            uint8_t enable;
            uint8_t stream_id;
            uint8_t quality;
        } data;
    } pkt;
} canmore_camera_feed_cmd_t;
static_assert(sizeof(canmore_camera_feed_cmd_t) == CANMORE_CAMERA_FEED_CMD_MAX_LEN, "Failed to pack");

#ifdef __cplusplus
}
#endif

#endif
