/*
QMesh
Copyright (C) 2019 Daniel R. Fay

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "mbed.h"
#include "peripherals.hpp"
#include "params.hpp"
#include "serial_data.hpp"
#include "fec.hpp"
#include "json_serial.hpp"
#include "mesh_protocol.hpp"

RawSerial pc(USBTX, USBRX);
RawSerial pc2(NC, PA_12);
JSONSerial rx_json_ser, tx_json_ser;
Thread tx_serial_thread(4096), rx_serial_thread(4096);
Thread mesh_protocol_thread(4096);
Thread beacon_thread(4096);
Thread nv_log_thread(4096);

system_state_t current_mode = BOOTING;
bool stay_in_management = false;

DigitalOut flash_pwr_ctl(MBED_CONF_APP_FLASH_PWR);
DigitalOut radio_pwr_ctl(MBED_CONF_APP_RADIO_PWR);

#define SLEEP_TIME                  500 // (msec)
#define PRINT_AFTER_N_LOOPS         20

void print_memory_info() {
    // allocate enough room for every thread's stack statistics
    int cnt = osThreadGetCount();
    mbed_stats_stack_t *stats = (mbed_stats_stack_t*) malloc(cnt * sizeof(mbed_stats_stack_t));
 
    cnt = mbed_stats_stack_get_each(stats, cnt);
    for (int i = 0; i < cnt; i++) {
        printf("Thread: 0x%lX, Stack size: %lu / %lu\r\n", (unsigned long) stats[i].thread_id, 
            (unsigned long) stats[i].max_size, (unsigned long) stats[i].reserved_size);
    }
    free(stats);
 
    // Grab the heap statistics
    mbed_stats_heap_t heap_stats;
    mbed_stats_heap_get(&heap_stats);
    printf("Heap size: %lu / %lu bytes\r\n", (unsigned long) heap_stats.current_size, 
        (unsigned long) heap_stats.reserved_size);
}


// main() runs in its own thread in the OS
int main()
{
    led1.LEDSolid();

    // Start the WDT thread
    //wdt_thread.start(wdt_fn);

    // Set the UART comms speed
    pc.baud(9600);
    pc2.baud(9600);

    ThisThread::sleep_for(1000);

    debug_printf(DBG_INFO, "Starting serial threads...\r\n"); // Removing this causes a hard fault???

    // Start the serial handler threads
    tx_serial_thread.start(tx_serial_thread_fn);
    rx_serial_thread.start(rx_serial_thread_fn);

    debug_printf(DBG_INFO, "serial threads started\r\n");

    // Power cycle the SPI flash chip and the RF module
    debug_printf(DBG_INFO, "Powering down the SPI flash and LoRa modules...\r\n");
    flash_pwr_ctl = 0;
    radio_pwr_ctl = 0;
    debug_printf(DBG_INFO, "Powering up the SPI flash...\r\n");
    flash_pwr_ctl = 1;
    debug_printf(DBG_INFO, "Powering up the LoRa module...\r\n");
    radio_pwr_ctl = 1;
    debug_printf(DBG_INFO, "Both modules now powered up!\r\n");
    ThisThread::sleep_for(1000);

    while(true);

    // Mount the filesystem, load the configuration
    init_filesystem();
    load_settings_from_flash();
    save_settings_to_flash();

    // Wait for 2 seconds in MANAGEMENT mode
    current_mode = MANAGEMENT;
    led1.LEDFastBlink();
    ThisThread::sleep_for(2000);
    while(stay_in_management) {
        ThisThread::sleep_for(5000);
    }
    current_mode = RUNNING;

    led1.LEDBlink();
    led2.LEDOff();
    led3.LEDOff();

    while(true);

    // Test the FEC
    debug_printf(DBG_INFO, "Now testing the FEC\r\n");
    auto fec_frame = make_shared<Frame>();  
    debug_printf(DBG_INFO, "Size of fec_frame is %d\r\n", fec_frame->getPktSize());

    print_memory_info();

    {
    auto fec_test_fec = make_shared<FEC>();
    fec_test_fec->benchmark(25);
    auto fec_test_interleave = make_shared<FECInterleave>();
    fec_test_interleave->benchmark(25);
    auto fec_test_conv = make_shared<FECConv>(2, 9);
    fec_test_conv->benchmark(25);
    auto fec_test_rsv = make_shared<FECRSV>(2, 9, 8);
    fec_test_rsv->benchmark(25);
    } 

    // Set up the radio
    init_radio();
    ThisThread::sleep_for(250);

    // Start the NVRAM logging thread
    debug_printf(DBG_INFO, "Starting the NV logger\r\n");
    nv_log_thread.start(nv_log_fn);

    ThisThread::sleep_for(250);

    // Start the mesh protocol thread
    debug_printf(DBG_INFO, "Starting the mesh protocol thread\r\n");
    mesh_protocol_thread.start(mesh_protocol_fsm);

    ThisThread::sleep_for(250);

    // Start the beacon thread
    debug_printf(DBG_INFO, "Starting the beacon thread\r\n");
    beacon_thread.start(beacon_fn);

    ThisThread::sleep_for(250);

    debug_printf(DBG_INFO, "Started all threads\r\n");
}

