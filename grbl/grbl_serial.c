/*
  serial.c - Low level functions for sending and recieving bytes via the serial port
  Part of Grbl

  Copyright (c) 2011-2016 Sungeun K. Jeon for Gnea Research LLC
  Copyright (c) 2009-2011 Simen Svale Skogsrud

  Grbl is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Grbl is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Grbl.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "grbl.h"
#include <epicsMutex.h>

#define RX_RING_BUFFER (RX_BUFFER_SIZE+1)
#define TX_RING_BUFFER (TX_BUFFER_SIZE+1)

uint8_t serial_rx_buffer[RX_RING_BUFFER];
uint16_t serial_rx_buffer_head = 0;
volatile uint8_t serial_rx_buffer_tail = 0;

uint8_t serial_tx_buffer[TX_RING_BUFFER];
uint16_t serial_tx_buffer_head = 0;
volatile uint16_t serial_tx_buffer_tail = 0;

epicsMutexId serialRxBufferMutex = NULL;
epicsMutexId serialTxBufferMutex = NULL;

#define MUTEX_LOCK(mutex)              \
  {                                    \
    if (mutex) {                       \
      epicsMutexLock(mutex);           \
    }                                  \
  }                                    \

#define MUTEX_UNLOCK(mutex)            \
  {                                    \
    if (mutex) {                       \
      epicsMutexUnlock(mutex);         \
    }                                  \
  }                                    \

// Returns the number of bytes available in the RX serial buffer.
uint16_t serial_get_rx_buffer_available()
{
  //printf("%s:%s:%d:\n",__FILE__,__FUNCTION__,__LINE__);
  MUTEX_LOCK(serialRxBufferMutex);
  uint16_t rtail = serial_rx_buffer_tail; // Copy to limit multiple calls to volatile
  if (serial_rx_buffer_head >= rtail) { 
    MUTEX_UNLOCK(serialRxBufferMutex);
    return(RX_BUFFER_SIZE - (serial_rx_buffer_head-rtail)); 
  }
  MUTEX_UNLOCK(serialRxBufferMutex);
  return((rtail-serial_rx_buffer_head-1));
}


// Returns the number of bytes used in the RX serial buffer.
// NOTE: Deprecated. Not used unless classic status reports are enabled in config.h.
uint16_t serial_get_rx_buffer_count()
{
  //printf("%s:%s:%d:\n",__FILE__,__FUNCTION__,__LINE__);  

  MUTEX_LOCK(serialRxBufferMutex);

  uint16_t rtail = serial_rx_buffer_tail; // Copy to limit multiple calls to volatile
  if (serial_rx_buffer_head >= rtail) { 
    MUTEX_UNLOCK(serialRxBufferMutex);
    return(serial_rx_buffer_head-rtail); 
  }

  MUTEX_UNLOCK(serialRxBufferMutex);
  return (RX_BUFFER_SIZE - (rtail-serial_rx_buffer_head));
}


// Returns the number of bytes used in the TX serial buffer.
// NOTE: Not used except for debugging and ensuring no TX bottlenecks.
uint16_t serial_get_tx_buffer_count()
{
  //printf("%s:%s:%d:\n",__FILE__,__FUNCTION__,__LINE__);

  MUTEX_LOCK(serialTxBufferMutex);
  
  uint16_t ttail = serial_tx_buffer_tail; // Copy to limit multiple calls to volatile
  if (serial_tx_buffer_head >= ttail) {
    MUTEX_UNLOCK(serialTxBufferMutex);
    return(serial_tx_buffer_head-ttail); 
  }
  
  MUTEX_UNLOCK(serialTxBufferMutex);
  return (TX_RING_BUFFER - (ttail-serial_tx_buffer_head));
}


void serial_init()
{
  printf("%s:%s:%d:\n",__FILE__,__FUNCTION__,__LINE__);
  memset(&serial_rx_buffer[0],0,RX_RING_BUFFER);
  // Create some mutexes to ensure safe communication
  if(!(serialRxBufferMutex = epicsMutexCreate())) {
    printf("%s:%s:%d: Failed create serialRxBufferMutex\n",__FILE__,__FUNCTION__,__LINE__); 
    return;
  }
  //MUTEX_UNLOCK(serialRxBufferMutex);

  if(!(serialTxBufferMutex = epicsMutexCreate())) {
    printf("%s:%s:%d: Failed create serialTxBufferMutex\n",__FILE__,__FUNCTION__,__LINE__); 
    return;
  }
  //MUTEX_UNLOCK(serialTxBufferMutex);

  // Set baud rate
  //#if BAUD_RATE < 57600
  //  uint16_t UBRR0_value = ((F_CPU / (8L * BAUD_RATE)) - 1)/2 ;
  //  UCSR0A &= ~(1 << U2X0); // baud doubler off  - Only needed on Uno XXX
  //#else
  //  uint16_t UBRR0_value = ((F_CPU / (4L * BAUD_RATE)) - 1)/2;
  //  UCSR0A |= (1 << U2X0);  // baud doubler on for high baud rates, i.e. 115200
  //#endif
  //UBRR0H = UBRR0_value >> 8;
  //UBRR0L = UBRR0_value;
//
  //// enable rx, tx, and interrupt on complete reception of a byte
  //UCSR0B |= (1<<RXEN0 | 1<<TXEN0 | 1<<RXCIE0);

  // defaults to 8-bit, no parity, 1 stop bit
}


// Writes one byte to the TX serial buffer. Called by main program.
void serial_write(uint8_t data) {
  //printf("%s:%s:%d:\n",__FILE__,__FUNCTION__,__LINE__);

  MUTEX_LOCK(serialTxBufferMutex);

  // Calculate next head
  uint16_t next_head = serial_tx_buffer_head + 1;
  if (next_head == TX_RING_BUFFER) { next_head = 0; }

  // Wait until there is space in the buffer
  while (next_head == serial_tx_buffer_tail) {
    // TODO: Restructure st_prep_buffer() calls to be executed here during a long print.
    if (sys_rt_exec_state & EXEC_RESET) {
      MUTEX_UNLOCK(serialTxBufferMutex);
      return; 
    } // Only check for abort to avoid an endless loop.
  }

  // Store data and advance head
  serial_tx_buffer[serial_tx_buffer_head] = data;
  serial_tx_buffer_head = next_head;

  MUTEX_UNLOCK(serialTxBufferMutex);

  // Enable Data Register Empty Interrupt to make sure tx-streaming is running
  //UCSR0B |=  (1 << UDRIE0);
}


// Data Register Empty Interrupt handler
char ecmc_get_char_from_grbl_tx_buffer()
//ISR(SERIAL_UDRE)
{
  //printf("%s:%s:%d:\n",__FILE__,__FUNCTION__,__LINE__);

  MUTEX_LOCK(serialTxBufferMutex);

  uint16_t tail = serial_tx_buffer_tail; // Temporary serial_tx_buffer_tail (to optimize for volatile)
  char tempChar=0;
  // Send a byte from the buffer
  
  //UDR0 = serial_tx_buffer[tail];
  tempChar =serial_tx_buffer[tail];
  // Update tail position
  tail++;
  if (tail == TX_RING_BUFFER) { tail = 0; }

  serial_tx_buffer_tail = tail;

  MUTEX_UNLOCK(serialTxBufferMutex);

  // Turn off Data Register Empty Interrupt to stop tx-streaming if this concludes the transfer
  //if (tail == serial_tx_buffer_head) { UCSR0B &= ~(1 << UDRIE0); }
  return tempChar;
}

// Fetches the first byte in the serial read buffer. Called by main program.
uint8_t serial_read()
{
  //printf("%s:%s:%d:\n",__FILE__,__FUNCTION__,__LINE__);

  MUTEX_LOCK(serialRxBufferMutex);
  uint16_t tail = serial_rx_buffer_tail; // Temporary serial_rx_buffer_tail (to optimize for volatile)
  if (serial_rx_buffer_head == tail) {
    MUTEX_UNLOCK(serialRxBufferMutex);
    //printf("tail %u, head %u, available %u\n",serial_rx_buffer_tail,serial_rx_buffer_head,serial_get_rx_buffer_available());
    return SERIAL_NO_DATA;
  } else {
    uint8_t data = serial_rx_buffer[tail];
    serial_rx_buffer[tail]=0;

    tail++;
    if (tail == RX_RING_BUFFER) { 
      tail = 0; 
    }
    //printf("tail %u, head %u, available %u\n",serial_rx_buffer_tail,serial_rx_buffer_head,serial_get_rx_buffer_available());
    serial_rx_buffer_tail = tail;
    MUTEX_UNLOCK(serialRxBufferMutex);
    return data;
  }
}

//ISR(SERIAL_RX)
void ecmc_add_char_to_buffer(char data)
{
  //printf("Adding %c to buffer %s\n",data,serial_rx_buffer);

  //uint8_t data = UDR0;
  uint16_t next_head;

  // Pick off realtime command characters directly from the serial stream. These characters are
  // not passed into the main buffer, but these set system state flag bits for realtime execution.
  switch (data) {
    case CMD_RESET:         mc_reset(); break; // Call motion control reset routine.
    case CMD_STATUS_REPORT: system_set_exec_state_flag(EXEC_STATUS_REPORT); break; // Set as true
    case CMD_CYCLE_START:   system_set_exec_state_flag(EXEC_CYCLE_START); break; // Set as true
    case CMD_FEED_HOLD:     system_set_exec_state_flag(EXEC_FEED_HOLD); break; // Set as true
    default :
      if (data > 0x7F) { // Real-time control characters are extended ACSII only.
        switch(data) {
          case CMD_SAFETY_DOOR:   system_set_exec_state_flag(EXEC_SAFETY_DOOR); break; // Set as true
          case CMD_JOG_CANCEL:   
            if (sys.state & STATE_JOG) { // Block all other states from invoking motion cancel.
              system_set_exec_state_flag(EXEC_MOTION_CANCEL); 
            }
            break; 
          #ifdef DEBUG
            case CMD_DEBUG_REPORT: {uint8_t sreg = SREG; cli(); bit_true(sys_rt_exec_debug,EXEC_DEBUG_REPORT); SREG = sreg;} break;
          #endif
          case CMD_FEED_OVR_RESET: system_set_exec_motion_override_flag(EXEC_FEED_OVR_RESET); break;
          case CMD_FEED_OVR_COARSE_PLUS: system_set_exec_motion_override_flag(EXEC_FEED_OVR_COARSE_PLUS); break;
          case CMD_FEED_OVR_COARSE_MINUS: system_set_exec_motion_override_flag(EXEC_FEED_OVR_COARSE_MINUS); break;
          case CMD_FEED_OVR_FINE_PLUS: system_set_exec_motion_override_flag(EXEC_FEED_OVR_FINE_PLUS); break;
          case CMD_FEED_OVR_FINE_MINUS: system_set_exec_motion_override_flag(EXEC_FEED_OVR_FINE_MINUS); break;
          case CMD_RAPID_OVR_RESET: system_set_exec_motion_override_flag(EXEC_RAPID_OVR_RESET); break;
          case CMD_RAPID_OVR_MEDIUM: system_set_exec_motion_override_flag(EXEC_RAPID_OVR_MEDIUM); break;
          case CMD_RAPID_OVR_LOW: system_set_exec_motion_override_flag(EXEC_RAPID_OVR_LOW); break;
          case CMD_SPINDLE_OVR_RESET: system_set_exec_accessory_override_flag(EXEC_SPINDLE_OVR_RESET); break;
          case CMD_SPINDLE_OVR_COARSE_PLUS: system_set_exec_accessory_override_flag(EXEC_SPINDLE_OVR_COARSE_PLUS); break;
          case CMD_SPINDLE_OVR_COARSE_MINUS: system_set_exec_accessory_override_flag(EXEC_SPINDLE_OVR_COARSE_MINUS); break;
          case CMD_SPINDLE_OVR_FINE_PLUS: system_set_exec_accessory_override_flag(EXEC_SPINDLE_OVR_FINE_PLUS); break;
          case CMD_SPINDLE_OVR_FINE_MINUS: system_set_exec_accessory_override_flag(EXEC_SPINDLE_OVR_FINE_MINUS); break;
          case CMD_SPINDLE_OVR_STOP: system_set_exec_accessory_override_flag(EXEC_SPINDLE_OVR_STOP); break;
          case CMD_COOLANT_FLOOD_OVR_TOGGLE: system_set_exec_accessory_override_flag(EXEC_COOLANT_FLOOD_OVR_TOGGLE); break;
          #ifdef ENABLE_M7
            case CMD_COOLANT_MIST_OVR_TOGGLE: system_set_exec_accessory_override_flag(EXEC_COOLANT_MIST_OVR_TOGGLE); break;
          #endif
        }
        // Throw away any unfound extended-ASCII character by not passing it to the serial buffer.
      } else { // Write character to buffer
        next_head = serial_rx_buffer_head + 1;
        if (next_head == RX_RING_BUFFER) { next_head = 0; }

        // Write data to buffer unless it is full.
        //if (next_head != serial_rx_buffer_tail) {
          serial_rx_buffer[serial_rx_buffer_head] = data;
          serial_rx_buffer_head = next_head;
        //}        
      }
  }
}

// write direct to serial buffer
void ecmc_write_command_serial(char* line) {
  MUTEX_LOCK(serialRxBufferMutex);
  unsigned int i=0;
  for(i=0; i<strlen(line);i++) {
    ecmc_add_char_to_buffer(line[i]);    
  }
  
  //ecmc_add_char_to_buffer('\n');
  
//  printf("Serial Buffer tail %u head %u, avail %u\n",serial_rx_buffer_tail,serial_rx_buffer_head,serial_get_rx_buffer_available()); 
//  for(i = 0;i<RX_RING_BUFFER;i++) {
//    if(serial_rx_buffer[i]==0) {
//      printf("x");
//    } else if ( serial_rx_buffer[i]=='\n' || serial_rx_buffer[i]=='\r' )  {
//      printf("r");
//    } else  {
//      printf("%c",serial_rx_buffer[i]);
//    }
//  }
//  printf("\n");
  MUTEX_UNLOCK(serialRxBufferMutex);
  //if(enableDebugPrintouts) {
  //  printf("Added: %s\n", line);
  //}
  free(line);
}

void serial_reset_read_buffer()
{
  serial_rx_buffer_tail = serial_rx_buffer_head;
}
