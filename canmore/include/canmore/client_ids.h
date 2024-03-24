#ifndef CANMORE__CLIENT_IDS_H_
#define CANMORE__CLIENT_IDS_H_

/*
 * This file contains the client ID assignments for all devices on the CAN bus, as well as the bus ID assignments for
 * each CAN bus the devices are on.
 *
 * Note that client ID 0 is reserved
 */

// Internal CAN Bus
#define CANMORE_BUS_ID_INTERNAL 1

#define CANMORE_CLIENT_ID_POWER_BOARD 1
#define CANMORE_CLIENT_ID_ESC_BOARD0 2
#define CANMORE_CLIENT_ID_ESC_BOARD1 3
#define CANMORE_CLIENT_ID_CAMERA_CAGE_BB 4
#define CANMORE_CLIENT_ID_ACTUATOR_BOARD 5

// External CAN Bus
#define CANMORE_BUS_ID_EXTERNAL 2

#define CANMORE_CLIENT_ID_SMART_BATERY_PORT 1
#define CANMORE_CLIENT_ID_SMART_BATERY_STBD 2
#define CANMORE_CLIENT_ID_DOWNWARDS_CAMERA 3

#endif
