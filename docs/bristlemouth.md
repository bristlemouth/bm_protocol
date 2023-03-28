# Bristlemouth Architecture - WIP

[High-level network architecture](https://lucid.app/lucidchart/25c8a78b-49d6-4f67-b30b-0b3f39434e51/edit?invitationId=inv_f1d747dc-6abc-43eb-a22d-099c5a8bdbdb&referringApp=slack&page=wG.ouvT5LZG6#)

[Work in Progress architecture](https://lucid.app/lucidchart/25c8a78b-49d6-4f67-b30b-0b3f39434e51/edit?invitationId=inv_f1d747dc-6abc-43eb-a22d-099c5a8bdbdb&referringApp=slack&page=-Vppkv3JeIIy#)

^^ Blocks in Red are in In progress or yet to be completed.

## Network Setup (lwip)
We are using lwip (lightweight tcp/ip stack) to help create the bristlemouth network. This is set up in `bristlemouth/src/lib/bcl/src/bristlemouth.c` where we create a network interface, join the `ff03::1` multicast group, and create a Raw UDP PCB (Protocol Control Block) on port 2222. The UDP PCB, port, and network interface are passed to `bm_network_init`, which coordinates services like neighbor discovery. The network interface is created with an init function of `bm_l2_init`.

## ADIN2111 Driver
`bristlemouth/src/lib/drivers/adin2111`
The ADIN2111 driver is made up of 3 logical parts:

1. Driver files provided by Analog Devices including:
   * adin2111.c
   * adi_mac.c
   * adi_phy.c
   * adi_spi_generic.c
   * adi_spi_oa.c
   * adi_hal.c - This was slighly modified to quickly map IRQ-related methods to the proper NVIC methods.
  &nbsp;
1. `adi_bsp.c` - This file is needed for board-specifc mappings of the Analog Devices adi_hal.c file. The driver files they have provided will routinely call functions like `HAL_SpiReadWrite`, so we just map these to the ST-specific methods. The driver files need to map 2 specific callbacks (DATA_RDY gpio line and SPI complete), so we have 2 threads in this file that will call those registered callbacks (`adin_bsp_spi_thread` and `adin_bsp_gpio_thread`)

2. `eth_adin2111.c` - This is an intermediary driver between the L2 layer and the Analog Device provided driver files. The most important functions are as follows:

   `adin2111_hw_init` - This should be called once per ADIN2111 device supported by the system (this is a TODO for this driver as we only are set up to support one device). The function performs the ADIN, ADIN-BSP, and buffer submission setup (necessary for Analog Device ADIN2111 rx/tx) and returns a `adin2111_DeviceHandle_t` used by the Bristlemouth L2 layer.

   `adin2111_rx_cb` - This callback is assigned to all rx buffers submitted to the ADIN driver. Both the port and the specific ADIN2111 device handle are received as parameters and placed into a Queue consumed by the `adin2111_service_thread`

   `adin2111_service_thread` - This thread waits on 2 things:
      1. A TX semaphore that is given by `adin2111_tx` when a buffer is ready to be sent out on a port
      2. A RX Queue that `adin2111_rx_cb` places structs (with a rx buffer and rx port) into. The element is then passed up to the L2 layer.
     &nbsp;

   `adin2111_tx` - Called by the L2 layer to submit a buffer to the ADIN driver on a specific set of ports. In addition to sending the data, this function will also modify the src IPv6 address to include the egress port of the node. However, since the node can support multiple network devices, it also provides a port_offset to let the ADIN know which bits to modify in the port bitmap.

TODO: This driver should eventually be able to support multiple ADIN2111 instances.

## Network Device Config
`bristlemouth/src/apps/adin_test/bm_config.c`
We specify the network interfaces available on the system in the array `bm_netdev_config`. Currently, we are only able to support one ADIN2111 device, but we want to support multiple devices down the road.

Example:
```
bm_netdev_config_t bm_netdev_config[] = {{BM_NETDEV_TYPE_ADIN2111}, {BM_NETDEV_TYPE_NONE}};
```

## Bristlemouth L2 Layer
The L2 layer is responsible for initializing all the network devices, passing frames to ethernet input (RX frames) and output (TX frames) functions, and forwarding RX packets.

The initialization function `bm_l2_init` iterates through the array `bm_netdev_config`, intializes each net device, and stores their handles. Once setting up the output functions of the net interface, it then creates both an rx and tx thread to serve as a middleware between the net device driver and the lwip layer.

`bm_l2_link_output` acts as a wrapper for `bm_l2_tx` which places the outgoing frame in a queue to be sent out in `bm_l2_tx_thread`. `bm_l2_link_output` defaults to multicast and sets the port bitmap variable of `bm_l2_tx` to all valid ports. This function is called by the lwip layer (specifically `ethernet_output`) and provides a fully constructed ethernet frame.

The `bm_l2_tx_thread` waits on data to be placed in the TX Queue. Once it's unblocked, it iterates through all the available network devices and calls the respective tx function with the port bitmap popped off the queue (the tx function is called once per network device). TODO: Can optimize by not calling the tx function if there are no ports to send on for that specific network device.

`bm_l2_rx` is called by network devices drivers upon receiving data over the wire. In the current bristlemouth implementation, this is called in the `adin2111_service_thread`. The frame is placed onto a queue that the `bm_l2_rx_thread` is waiting on. Once unblocked, the frame is copied into a pbuf and sent to the lwip input function `tcpip_input`. Before submitting the frame to the input function, the src IPv6 address is modified to note the ingress port the frame was received on (global port # not the net device port #). Currently, `bm_l2_rx_thread` is re-broadcasting the frame on every port (except the one it was received on) by re-submitting the frame (with a modified port bitmap) to `bm_l2_tx`. TODO: Only rebroadcast if the message is a global multicast.

## Bristlemouth Network

The Raw UDP PCB created in `bristlemouth.c` has its callback set to `bcl_rx_cb` where incoming packets are placed into a queue consumed by `bcl_rx_thread`. In this thread, the ingress and egress ports are first cleared from the src IPv6 address and the message is then handled based on its specific type (these CBOR types are defined and generated using CDDL). Currently only 3 messages are handled MSG_BM_ACK, MSG_BM_HEARTBEAT, and MSG_BM_DISCOVER_NEIGHBORS. (TODO: Neighbor table request/responses logic needs to be ported from last demo)

The bulk of the logic for neighbor discovery is in `bm_network.c`. In `bm_network_init` timers are created for each specific potential neighbor, as well as a heartbeat timer for the node itself. (TODO/FIXME: The max number of neighbors and nodes in the network are currently hard-coded in `bm_neighbor.h`). Upon starting the network, `send_discover_neighbors` is called and a message of type MSG_BM_DISCOVER_NEIGHBORS is sent out over the raw UDP PCB.

If the `bcl_rx_thread` receives a MSG_BM_DISCOVER_NEIGHBORS message, it will send a MSG_BM_ACK back to the src, kick off the heartbeat timer, kick of the neighbor timeout timer, and store the neighbor in the neighbor table (with reachable set to 1). If the neighbor is already in the neighbor table, then the neighbor is set to reachable and the neighbor timeout timer is restarted.

If a MSG_BM_ACK is received, the network performs the above logic, but an ACK is not sent to the src address.

IF a MSG_BM_HEARTBEAT is received, the neighbor is set to be reachable and the neighbor timeout is restarted. If the neighbor address does not match the address in the neighbor table then re-call the `send_discover_neighbors`.

FIXME: The heartbeat timer is re-started each time a new neighbor is stored. Is this what we want to do...? Could this theoretically cause a heartbeat to be delayed long enough for a neighbor to consider the node unreachable?

## CDDL message generation

New message types can be created in the `bristlemouth/tools/scripts/cddl/msgs` directory. The messages follow the [ROS/ROS2 message specifications](http://wiki.ros.org/msg). Running `cddl_code_gen.sh` found in `$bristlemouth/tools/scripts/cddl` will then generate new encode, decode, and message type files and place them in `bristlemouth/src/lib/bcl/include`. These message types can then be added to the switch case statement in `bcl_rx_thread` of `bristlemouth.c` to be handled accordingly.

TODO: When more ports are available (a single ADIN2111 only supports 2 ports), the Table Response message should change the neighbor array from size 2 to N.

## Bristlemouth DFU

The DFU subsystem is initialized by `bcl_init` where a SerialHandle_t is passed to `bm_dfu_init` (in this case, we are using USB CDC 1, where USB CDC 0 is reserved for the CLI). The serial handle is found in `app_main.cpp` and we have increased both the txBufferSize and rxBufferSize for the worst-case packet size. `bm_dfu_init` creates a Raw UDP PCB on port 3333 with `bm_dfu_rx_cb` as the assigned callback.

2 threads are created which do the following:
   1. `bm_dfu_node_thread` receives data from ADIN2111 -> L2 -> lwip and processes the messages from other nodes. The message is converted into its corresponding `bm_dfu_event_t` type and submitted into the `dfu_event_queue` ONLY if the destination address matches the node's address. If the message is submitted into the queue, then it will be popped off and consumed by the `bm_dfu_event_thread`.
   2. `bm_dfu_event_thread` runs the state machine any time a new event is received in the queue.

The DFU state machine is then created in the INIT state, which moves to the IDLE state upon completion of `bm_dfu_init`. Each state in the state machine has a run, entry, and exit function. The order of operations is as follows:
    1. `bm_dfu_event_t` placed into `dfu_event_queue`
    2. `bm_dfu_event_thread` pops event off queue and runs the current state
    3. The `run` method for the current state is run
    4. `bm_dfu_check_transitions` is called which checks if there is a pending state change. If not, then we stay in the current state.
    5. If we are changing states, we then call the `exit` function of the current state.
    6. We finish with the `entry` function of the new state, before the `bm_dfu_event_thread` is blocked waiting for the next event to be placed into the queue.

In certain cases, a state change may be needed within an `entry` or `exit` function of a state. However, this state change does not take place because the `bm_dfu_event_thread` will wait for a new event to be placed onto the queue. For this reason, the `bm_dfu_set_pending_state_change` will place a NOP event onto the queue that will force a state change in these edge cases. FIXME: We don't need to do this for all state changes, so we should probably add a bool for sending the NOP event or not.

There are several helper functions in `bm_dfu_core.cpp` that can be called by either Host or Client Nodes (`bm_dfu_update_end`, `bm_dfu_req_next_chunk`, and `bm_dfu_send_ack`). Client Nodes will call these functions to send messages to other Nodes (lwip + UDP + ADIN2111).

 A Node can be designated as a `DFU_HOST` by setting the -DDFU_HOST=1 flag in cmake. This will specify that the node will be communicating with the Desktop/PC to perform a DFU (an additional serial port is also used with this flag) and the host subsystem will be initialized. Furthermore, the following threadss will be created.

   1. `serialGenericRxTask` is a generic serial task used to receive and process incoming data over USB. Our callback `bm_dfu_rx_cb` is called for each individual byte. `bm_dfu_rx_cb` first aligns the message by waiting for a 4-byte header (temporarily 0xDECAFBAD) followed by a 2-byte length (N). The callback then stores N bytes in a buffer which is then submitted to a queue consumed by `bm_dfu_desktop_thread`.

    +-------+------+---+-------+---+----+-------+-------+-------+
    | Header [4 bytes] | Length [2 bytes] | Byte 1 |...| Byte N |
    +-------+------+---+-------+---+----+-------+-------+-------+

   2. `bm_dfu_desktop_thread` processes reconstructed messages from `bm_dfu_rx_cb`. As of now, the messages are received from a laptop or desktop over USB. The only 2 `BM_DFU_FRM_TYPE` that are handled in this thread are `BM_DFU_BEGIN_HOST` and `BM_DFU_PAYLOAD`. Both frame types are used to populate a `bm_dfu_event_t` which is then placed into a queue to be consumed by the `bm_dfu_event_thread`.

Unlike the Client Nodes, the Host Nodes use the helper functions in `bm_dfu_core.cpp` to send messages directly to the PC/Desktop over USB serial.

### DFU Python Script

The script to intiate a DFU is found in `bristlemouth/tools/scripts/dfu/bm_dfu.py`.

Currently, the following options are required for the script to run:
   * i - image to send (.bin)
   * p - port
   * major - Major Version (WIP)
   * minor - Minor Version (WIP)
   * ip - Target IP Address (of Client)

The baud option (-b) has been made optional since we are using USB (script now defaults to 921600)

```
python3 bm_dfu.py -i blinky.bin  -p /dev/tty.usbmodem1234563 --major 1 --minor 0 --ip 2001:0db8:0000:0000:0000:0000:0000:0002
```

### Single-hop DFU
[DFU Process (single-hop)](https://lucid.app/lucidchart/25c8a78b-49d6-4f67-b30b-0b3f39434e51/edit?invitationId=inv_f1d747dc-6abc-43eb-a22d-099c5a8bdbdb&referringApp=slack&page=yZcyRe0n8QYj#)

There are 2 roles for the single-hop DFU: the Desktop and Client Node.

On a successful initialization of the DFU subsystem, the Client Node will be in the IDLE state, waiting for the `BM_DFU_BEGIN_HOST` frame from the Desktop. Upon receiving it, the Node enters the REQ_UPDATE state where it checks the destination address to see if it should perform a self-update. If there is a match, the state machine is immediately put back into the IDLE state with the `DFU_EVENT_RECEIVED_UPDATE_REQUEST` event placed into the event queue.

From here, the Node will check the image info (size and minor/major version) and open+erase the Flash memory region for the secondary image. The state machine goes to the IDLE state if the image is too big or the ERROR state if there are any flash errors. TODO: We are not using the major/minor version numbers currently but the values are still being sent over by the DFU python script. Barring any errors or violations, we then move into the `BM_DFU_STATE_CLIENT_RECEIVING` State.

Upon entry into the `BM_DFU_STATE_CLIENT_RECEIVING` state, the Client Node directly sends a CHUNK_REQ (chunk #0) to the Desktop/PC over the USB serial console. The Python script then sends the required chunk, which is received by the `bm_dfu_desktop_thread`, passed to the `bm_dfu_event_thread` as an event, and processed by the client state machine in `s_client_receiving_run` (the chunk is written directly to flash memory in the secondary image location). A timer is kicked off before each chunk request, and if the timer expires, then a chunk timeout event is created. The Client will then attempt to retry the chunk request, up until a certain number of retries, before it aborts the DFU.

After receiving the final chunk, the client will write the chunk to flash and proceed to the `BM_DFU_STATE_CLIENT_VALIDATING` state. Upon entry, the Node will verify that the length and CRC16 of the image match with the image info received in the update request. If both match, then the node will immediately send a success message back to the Desktop/PC, enter the `BM_DFU_STATE_CLIENT_ACTIVATING` state, set the pending image bit for MCUBoot, and reset the device. If either the length or CRC are mismatched, then the device send a update failed message to the Desktop/PC, briefly enters the `BM_DFU_STATE_ERROR` state, and return to the `BM_DFU_STATE_IDLE` state.


### Multi-hop DFU
[DFU Process (multi-hop)](https://lucid.app/lucidchart/25c8a78b-49d6-4f67-b30b-0b3f39434e51/edit?invitationId=inv_f1d747dc-6abc-43eb-a22d-099c5a8bdbdb&referringApp=slack&page=Qe-r9iQeK2mq#)

There are 3 roles for the multi-hop DFU: the Desktop, Host Node, and Client Node.

Multi-hop DFU works similarly to single-hop, but with distinct Host and Client nodes. Any nodes in between the host and the client will forward the payload along without processing the data.

On a successful initialization of the DFU subsystem, the Client Node will be in the IDLE state, waiting for the `BM_DFU_BEGIN_HOST` frame from the Desktop. Upon receiving it, the Node enters the REQ_UPDATE state where it checks the destination address to see if it should perform a self-update. In this case (multi-hop), the Host sends the `BM_DFU_START` message to the Client and starts an ACK timer, waiting for a response from the Client.

The Client node will check the image info (size and minor/major version) and open+erase the Flash memory region for the secondary image. The state machine goes to the IDLE state if the image is too big or the ERROR state if there are any flash errors. Barring any errors or violations, we then move into the `BM_DFU_STATE_CLIENT_RECEIVING` State. An ACK message is sent back to the host which will report success/failure and any errors back to the PC/Desktop. If the ACK is not received before the timeout, the Host node will re-try sending the `BM_DFU_START` until a set number of retries is reached. Upon a successful ACK, the Host Node enters the `BM_DFU_STATE_HOST_UPDATE`. The Host also kicks off a "heartbeat timer" which times out if the Client Node does not request a chunk within a certain timeout (this timer also has an associated retry limit).

Upon entry into the `BM_DFU_STATE_CLIENT_RECEIVING` state, the Client Node  sends a CHUNK_REQ (chunk #0) to the Host Node, which is subsequently sent to the PC/Desktop. Receiving this `CHUNK_REQ` resets the Host's "heartbeat timer". The Client Node also starts a "chunk timer" where it expects to receive the requested chunk before a timeout. We included this in case the Host needs to retrieve the chunk from a machine over Satellite link where there could be considerable delay. TODO: Currently the Host sends a single heartbeat message (`BM_DFU_HEARTBEAT`) back to the client, but this should be on a timer to send something periodic.

The script will send the chunks through the Host to the Client, and the Client will Validate the CRC and length of the image after receiving the final chunk. Regardless of success/failure/errors, the Client will send a `BM_DFU_END` message to both the Host and PC/Desktop to end the update.
