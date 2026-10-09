Implement the following feature request by making the smallest, most non-invasive code change possible. Follow existing patterns exactly, reuse existing utilities, and do not refactor, rewrite, or over-engineer adjacent code. There are reasons behind each request I make, however I am not able to always explain all the context behind every decision in every request.

General Notes

- We can disable STOP2 if needed for these changes.
- Do not try to modify or test for breakage in other spotter apps. For the purposes of this work we can ignore the impact to spotter apps other than the main one`spotter` .
- You can modify the submodules `bm_core` , and `bm_serial` . All of the work being done here is staying in experimental branches so we don’t need to worry too much about breaking other users of these repos.
- NEVER READ FROM THE `TS_SEC_CNT` register at address `0x82` or `TS_NS_CNT` register at address `0x83` in the ADIN. They say they are `R/W` but you always read the last written value from them not the current time.
    - On the bridge only read the `TS_EXT_CAPT0` and `TS_EXT_CAPT1` registers at the addresses `0x89` and `0x8A`. These hold the value of the timer in the ADIN that was captured at the last edge of the PPS signal.

Part 1

The mote app needs to configure the ADIN2111 to pulse the TS_TIMER output signal on pin P1_LED_0 so that the mote can use that signal, which is connected to pin PA1 on the MCU, to get the GPS timebase.

In the initialization code run these steps in this order:

Set the P1_LED_0 pin to the TS_TIMER function by writing to the corresponding bit in LED_CNTRL register.

TS_TIMER_HI = 50000 (needs to be evenly divisible by 16. This makes the high period 50ms long which should be plenty for the MCU to catch on a CCR input pin)

TS_TIMER_LO = 999950000 (Also needs to be evenly divisible by 16. This number of nanoseconds fills in the rest of the period of one second so we get one pulse per second out from the ADIN)

Since we know what the settings are and we are capturing the edge with a timer CCR we don’t need to touch the TS_TIMER_QE_CORR register.

TS_TIMER_START = 10000 (first pulse starts at 10ms past the second boundary. This register can’t be set to a value less than 16 so I decided to send the PPS signal at 10ms past the top of each second.)

Part 2

Please configure the PA1 pin on the MCU to use the alternate function TIM2_CH2 (AF1).

Next configure timer 2 channel 2 to capture rising edges from the input on PA1.

The rising edges of these timing pulses are going to come in 10ms past each second boundary.

Part 3

Update the adin initialization and configuration code to enable capturing both the incoming and outgoing packet timestamps to enable using IEEE 1588 to synchronize the clocks across the network.

Part 4

Please write a new task for the mote app `bm_soft_module` called `handle_ptp`. This task needs to handle the steps required for IEEE 1588 peer to peer time synchronization. Specifically it needs to handle receiving a `Sync` message to start the time synchronization process and a `Follow_Up` messages containing the exact time that the `Sync` message was sent. This task needs to record the time that the IEEE 1588 packets are received and sent. These times are measured by the IEEE handshake and sync process using the ADIN2111 to get highly precise timestamps.

After each transaction is completed (Sync packet + follow_up to get current time along with the peer to peer handshake to measure the link delay) this task needs to use the measured times to calculate the current time, and then it needs to use this to initialize or servo the ADIN2111 using it’s ADDEND register and the TS_TIMER output on the ADIN2111 that was configured in part 1 of these instructions.

The exact math needed is detailed in many places. This website has a clear explanation if you need more details: https://networklessons.com/ip-services/precision-time-protocol-ptp-explained . We can either take the sync packet time and add the delay or we can use the offset result to servo the ADIN on the mote depending on which step of the process we are doing now.

Our design uses the ADIN2111 which makes this time propagation process more precise. It does this by capturing the value of its timer when sending and receiving packets. Please use a subagent to read and summarize the important parts of the ADIN2111 datasheet PDF in this folder to fully understand how to use it when implementing IEEE 1588. The 100us timing accuracy requirement for this design isn’t achievable without using these features so it is critical that they are used correctly.

The precision timing pulse coming from the ADIN was configured in Part 1 of this plan. It arrives exactly 10ms past each second boundary. This signal needs to be used to keep track of the exact time and to keep track of how far off the ADIN is from correct also.
