Implement the following feature request by making the smallest, most non-invasive code change possible. Follow existing patterns exactly, reuse existing utilities, and do not refactor, rewrite, or over-engineer adjacent code. There are reasons behind each request I make, however I am not able to always explain all the context behind every decision in every request. 

General Notes

- We can disable STOP2 if needed for these changes.
- Do not try to modify or test for breakage in other spotter apps. For the purposes of this work we can ignore the impact to spotter apps other than the main one`spotter` .
- You can modify the submodules `bm_core` , and `bm_serial` . All of the work being done here is staying in experimental branches so we don’t need to worry too much about breaking other users of these repos.

All of the below changes need to be made to the bridge app (and only the bridge app).
Step 1

The spotter app has been modified so that the NMEA timestamp for each PPS rising edge from the GPS module is sent to the bridge as it comes in. It will be sent using the topic `spotter/utc-pps-time` and type `BM_SERIAL_PTP` instead of the previously used topic and type. Because of this we can now use this new UTC timestamp forwarded from the bridge to set and synchronize the timer on the ADIN2111 networking IC. 

Match how `spotter/utc-time` is handled when writing the code to handle this new subscription. Add a case to the switch statement in the `bm_serial.c` file inside the bm_serial submodule repo. This new case should call a new callback function `bm_serial_adin_utc_cb` which will be written in the next step of this session. 

Step 2

Write a new callback function called `bm_serial_adin_utc_cb` in `src/lib/bm_ncp/ncp_uart.cpp`(and add it to  `bm_serial_callbacks` alongside all the others). It needs to set the seconds and nanoseconds registers in the ADIN2111 when it gets the first UTC value since booting.

After setting the registers it needs to signal to the thread detailed below in step 3 that it needs to start the standard IEEE 1588 peer-to-peer handshake by sending the first message immediately.

After the initial time on the ADIN2111 is set this callback then needs to use a PI loop to adjust the addend register in the ADIN2111 to keep both the phase and frequency of the timer in the ADIN locked to the GPS PPS signal every time a new PPS + date time comes in.

The ADIN2111 has its `TS_CAPT` input muxed to `TEST_1` pin which is connected to the same GPS PPS signal that is connected to this MCU and the main deck MCU. The ADIN2111 takes a snapshot of its internal seconds and nanoseconds registers when a rising edge comes into the `TS_CAPT` pin. This timer snapshot is held in registers defined in the data sheet and the value should be used to servo the timer in the ADIN using the addend register value once the initial rough time has been set.

Please use a subagent to read and distill the important parts of the ADIN2111 data sheet PDF in this folder to fully understand everything needed to accomplish this.

Step 3

Please implement the IEEE 1588 peer to peer handshake and Sync message logic in a new FreeRTOS process. The work to get the current UTC from a GPS module’s PPS signal and parsed NMEA string has already been done by the code written in step 2 above so this process only needs to handle the remaining back and forth with the neighboring device. 

This process should receive a signal from the callback function `bm_serial_adin_utc_cb` . When it first gets the signal this process should send the newly acquired GPS datetime to the bristlemouth neighbor using a Sync and follow-up message. This thread doesn’t need to worry about configuring the ADIN2111 since the initialization step and the callback function `bm_serial_adin_utc_cb` are in charge of this.

Our design uses the ADIN2111 which makes this time propagation process more precise. It does this by capturing the value of its timer when sending and receiving packets. Please use a subagent to read and summarize the important parts of the ADIN2111 datasheet PDF in this folder to fully understand how to use it when implementing IEEE 1588. The 100us timing accuracy requirement for this design isn’t achievable without using these features so it is critical that they are used correctly.

After this initial step is done this process needs to handle all of the steps required for the peer to peer handshake that measures the link delay as part of the IEEE spec. Start with the peer-to-peer link delay measurement process and then the `Sync` and `Follow_Up` message exchange from this device to the downstream device.

If the neighboring port on the network doesn’t support the peer to peer handshake or is offline this process should timeout and retry gracefully (never flood the network, don’t hog resources waiting, handle restarting the process when a neighbor comes back online without getting stuck in a bad state, etc).

Step 4

The PPS signal from the GPS module is coming in on PA0 and is routed to TIM2_CH1.

Please write an ISR and configure everything needed to trigger the ISR on a rising edge coming into the PA0 pin in addition to setting things up to capture the current timer value on each rising edge.

The amount of latency the input filter adds depends on a few variables and we don’t want it sneaking problems in later on. It must be disabled by default. Please also add options to easily enable and configure.

We need to read the captured value from TIM2_CH2 in the ISR described above. The captured value should then be fed into a PI loop that controls the speed of the timer to keep its phase and frequency synchronized to the incoming PPS signal. The approach you use for this needs to tolerate losing and regaining GPS lock (therefore PPS signal) without issues (like sudden timing changes or large swings in the clock period).
