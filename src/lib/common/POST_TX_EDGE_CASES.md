## One write in the transaction
```c++
PLUART::startTransaction();
PLUART::write(A);
PLUART::endTransaction();
```
1. **user task**::`startTransaction` **opens**:
   - in critical: sets `transactionInProgress` flag and clears `writeDuringTransaction` flag.
   - executes `_preTxFunction()`.
   - **user task**::`startTransaction` **closes**.
   

2. **user task**::`write` **opens**:
   - in critical: sets `writeDuringTransaction` flag.
   - No tokens available in `_postTxCntSemaphore`, so does not enter loop to take.
   - `serialWrite` -> `serialWriteNocopy` -> submit data to `serialTxQueue`.
   - **user task**::`write` **closes**:
   

3. **user task**::`endTransaction` **opens**:
   - in critical: retrieves and interprets `writeDuringTransaction` and `transactionInProgress` flags.
   - First wait for the token to be given to `_postTxCntSemaphore`, otherwise the while loop could shoot through.
   

4. **serialTxTask** takes from `serialTxQueue`:
   - `serialGenericTx` -> submit data to `txStreamBuffer`.
   - enable TXE flag, which will trigger the UART interrupt and `serialGenericUartIRQHandler`.
   

5. **serialGenericUartIRQHandler** is triggered :
   - `handle->getTxBytesFromISR` points to `serialGenericGetTxBytesFromISR`
     - this receives one byte at a time from `handle->txStreamBuffer`, which points to `txStreamBuffer`.
   - When the UART data register is clear while TXE is set, the interrupt will fire again.
   - The TC flag will fire only once when TDR is finally clear and shift register is also empty.  
   Calls postTxCb(`pluartPostTransactionCb`). 
   - each call to `pluartPostTransactionCb` gives the `_postTxSemaphore` semaphore token. 
     

6. (!!!) **user task**::`endTransaction` **open**:
   - Wait for the single token to be given to `_postTxCntSemaphore`.

---

## Two writes in the transaction, [no delay]
```c++
PLUART::startTransaction();
PLUART::write(A);
PLUART::write(B);
PLUART::endTransaction();
```
- 2nd call to write ends up adding data to the stream buffer before 1st write is finished, so identical to 1st scenario.
---

## Two writes in the transaction, [delay between them, or maybe if 1st write is 1 byte]
```c++
PLUART::startTransaction();
PLUART::write(A);
sleep_ms(10);
PLUART::write(B);
PLUART::endTransaction();
```
- 1st write completes and token is given to `_postTxCntSemaphore` before 2nd write is called.
- 2nd write checks token count, and takes token -> 0 tokens in `postTxCntSemaphore`.
- endTransaction is blocked on 2nd write giving token to `postTxCntSemaphore`.
---

## Zero writes in the transaction
```c++
PLUART::startTransaction();
PLUART::endTransaction();
```
### TODO:
## both CBs null
## start CB null
## end CB null
## write before start
## write after end