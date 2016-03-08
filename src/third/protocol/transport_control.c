/***********************************************************************************
Copyright (c) Nordic Semiconductor ASA
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

  3. Neither the name of Nordic Semiconductor ASA nor the names of other
  contributors to this software may be used to endorse or promote products
  derived from this software without specific prior written permission.

  4. This software must only be used in a processor manufactured by Nordic
  Semiconductor ASA, or in a processor manufactured by a third party that
  is used in combination with a processor manufactured by Nordic Semiconductor.


THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
************************************************************************************/

#include "transport_control.h"

#include "radio_control.h"
#include "mesh_gatt.h"
#include "mesh_packet.h"
#include "timer_control.h"
#include "rbc_mesh.h"
#include "event_handler.h"
#include "timeslot_handler.h"
#include "rbc_mesh_common.h"
#include "version_handler.h"
#include "mesh_aci.h"
#include "app_error.h"
#include "cfg/cs_Boards.h"

#include <string.h>

/* event push isn't present in the API header file. */
extern uint32_t rbc_mesh_event_push(rbc_mesh_event_t* p_evt);

/******************************************************************************
* Local typedefs
******************************************************************************/
typedef struct
{
    uint32_t access_address;
    uint8_t channel;
    bool queue_saturation; /* flag indicating a full processing queue */
} tc_state_t;

/******************************************************************************
* Static globals
******************************************************************************/
static tc_state_t g_state;
/******************************************************************************
* Static functions
******************************************************************************/
static void rx_cb(uint8_t* data, bool success, uint32_t crc);
static void tx_cb(uint8_t* data);


static void order_search(void)
{
    radio_event_t evt;
    
    evt.event_type = RADIO_EVENT_TYPE_RX_PREEMPTABLE;
    evt.start_time = 0;
    evt.access_address = 1;
    evt.channel = g_state.channel;
    evt.callback.rx = rx_cb;
    
    if (!mesh_packet_acquire((mesh_packet_t**) &evt.packet_ptr))
    {
        return; /* something is hogging all the packets */
    }
    if (!radio_order(&evt))
    {
        /* couldn't queue the packet for reception, immediately free its only ref */
        mesh_packet_ref_count_dec((mesh_packet_t*) evt.packet_ptr);
    }
}


static void prepare_event(rbc_mesh_event_t* evt, mesh_adv_data_t* p_mesh_adv_data)
{
    evt->value_handle = p_mesh_adv_data->handle;
    evt->data = &p_mesh_adv_data->data[0];
    evt->data_len = p_mesh_adv_data->adv_data_length - MESH_PACKET_ADV_OVERHEAD;
}

/* radio callback, executed in STACK_LOW */
static void rx_cb(uint8_t* data, bool success, uint32_t crc)
{
    if (success)
    {
        async_event_t evt;
        evt.type = EVENT_TYPE_PACKET;
        evt.callback.packet.payload = data;
        evt.callback.packet.crc = crc;
        evt.callback.packet.timestamp = timer_get_timestamp();
        mesh_packet_ref_count_inc((mesh_packet_t*) data); /* event handler has a ref */
        if (event_handler_push(&evt) != NRF_SUCCESS)
        {
            g_state.queue_saturation = true;
            mesh_packet_ref_count_dec((mesh_packet_t*) data); /* event handler lost its ref */
        }
    }

    /* no longer needed in this context */
    mesh_packet_ref_count_dec((mesh_packet_t*) data);
}

/* radio callback, executed in STACK_LOW */
static void tx_cb(uint8_t* data)
{
    rbc_mesh_event_t tx_event;
    mesh_adv_data_t* p_adv_data = mesh_packet_adv_data_get((mesh_packet_t*) data);
    bool doing_tx_event = false;
    if (p_adv_data != NULL && 
        vh_tx_event_flag_get(p_adv_data->handle, &doing_tx_event) == NRF_SUCCESS 
        && doing_tx_event
    )
    {
        tx_event.event_type = RBC_MESH_EVENT_TYPE_TX;
        tx_event.value_handle = p_adv_data->handle;
        tx_event.data = p_adv_data->data;
        tx_event.data_len = p_adv_data->adv_data_length - MESH_PACKET_ADV_OVERHEAD;
        tx_event.version_delta = 0;

        APP_ERROR_CHECK(rbc_mesh_event_push(&tx_event));
#if RBC_MESH_SERIAL
        mesh_aci_rbc_event_handler(&tx_event);
#endif
    }

    mesh_packet_ref_count_dec((mesh_packet_t*) data); /* radio ref removed (pushed in tc_tx) */
    vh_order_update(timer_get_timestamp()); /* tell the vh, so that it can push more updates */
}


static void radio_idle_callback(void)
{
    /* If the processor is unable to keep up, we should back down, and give it time */
    if (!g_state.queue_saturation)
        order_search();
}

static void mesh_app_packet_handle(mesh_adv_data_t* p_mesh_adv_data, uint64_t timestamp)
{
//    LOGi("_1");
    int16_t delta = vh_get_version_delta(p_mesh_adv_data->handle, p_mesh_adv_data->version);
    vh_data_status_t data_status = vh_rx_register(p_mesh_adv_data, timestamp);
   
    /* prepare app event */
    rbc_mesh_event_t evt;
    evt.version_delta = delta;

    switch (data_status)
    {
        case VH_DATA_STATUS_NEW:

            /* notify application */
            prepare_event(&evt, p_mesh_adv_data);
            evt.event_type = RBC_MESH_EVENT_TYPE_NEW_VAL;
            if (rbc_mesh_event_push(&evt) == NRF_SUCCESS)
            {                
                mesh_gatt_value_set(p_mesh_adv_data->handle, 
                    p_mesh_adv_data->data, 
                    p_mesh_adv_data->adv_data_length - MESH_PACKET_ADV_OVERHEAD);
            }
#ifdef RBC_MESH_SERIAL
            mesh_aci_rbc_event_handler(&evt);
#endif
            break;

        case VH_DATA_STATUS_UPDATED:

            /* notify application */
            prepare_event(&evt, p_mesh_adv_data);
            evt.event_type = RBC_MESH_EVENT_TYPE_UPDATE_VAL;
            if (rbc_mesh_event_push(&evt) == NRF_SUCCESS)
            {
                mesh_gatt_value_set(p_mesh_adv_data->handle, 
                    p_mesh_adv_data->data, 
                    p_mesh_adv_data->adv_data_length - MESH_PACKET_ADV_OVERHEAD);
            }
#ifdef RBC_MESH_SERIAL
            mesh_aci_rbc_event_handler(&evt);
#endif
            break;

        case VH_DATA_STATUS_OLD:
            /* do nothing */
            break;
            
        case VH_DATA_STATUS_SAME:
            /* do nothing */
            break;

        case VH_DATA_STATUS_CONFLICTING:

            prepare_event(&evt, p_mesh_adv_data);
            evt.event_type = RBC_MESH_EVENT_TYPE_CONFLICTING_VAL;
            rbc_mesh_event_push(&evt); /* ignore error - will be a normal packet drop */
#ifdef RBC_MESH_SERIAL
            mesh_aci_rbc_event_handler(&evt);
#endif
            break;

        case VH_DATA_STATUS_UNKNOWN:
            break;
    }
}

/******************************************************************************
* Interface functions
******************************************************************************/
void tc_init(uint32_t access_address, uint8_t channel)
{
    g_state.access_address = access_address;
    g_state.channel = channel;
}

void tc_radio_params_set(uint32_t access_address, uint8_t channel)
{
    if (channel < 40)
    {
        g_state.access_address = access_address;
        g_state.channel = channel;
        timeslot_restart();
    }
}

void tc_on_ts_begin(void)
{
    radio_init(g_state.access_address, radio_idle_callback);
}

uint32_t tc_tx(mesh_packet_t* p_packet)
{
    TICK_PIN(PIN_MESH_TX);
    
    /* queue the packet for transmission */
    radio_event_t event;
    memset(&event, 0, sizeof(radio_event_t));
    event.start_time = 0;
    mesh_packet_ref_count_inc(p_packet); /* queue will have a reference until tx_cb */
    event.packet_ptr = (uint8_t*) p_packet;
    event.access_address = 0;
    event.channel = g_state.channel;
    event.callback.tx = tx_cb;
    event.event_type = RADIO_EVENT_TYPE_TX;
    if (!radio_order(&event))
    {
        mesh_packet_ref_count_dec(p_packet); /* queue couldn't hold the ref */
        return NRF_ERROR_NO_MEM;
    }
    
    return NRF_SUCCESS;
}

//void printArray(uint8_t* arr, uint16_t len) {
//	uint8_t* ptr = (uint8_t*)arr;
//	for (int i = 0; i < len; ++i) {
//		_log(DEBUG, " %02X", ptr[i]);
//		if ((i+1) % 30 == 0) {
//			_log(DEBUG, "\r\n");
//		}
//	}
////	printInlineArray(arr, len);
//	_log(DEBUG, "\r\n");
//}

/* packet processing, executed in APP_LOW */
void tc_packet_handler(uint8_t* data, uint32_t crc, uint64_t timestamp)
{
//	LOGi("_6");
    SET_PIN(PIN_RX);
    mesh_packet_t* p_packet = (mesh_packet_t*) data;
//	printArray(p_packet, sizeof(mesh_packet_t));

    if (p_packet->header.length > MESH_PACKET_OVERHEAD + RBC_MESH_VALUE_MAX_LEN)
    {
//    	LOGi("_2");
        /* invalid packet, ignore */
        CLEAR_PIN(PIN_RX);
        mesh_packet_ref_count_dec(p_packet); /* from rx_cb */
        return;
    }
    
    ble_gap_addr_t addr;
    memcpy(addr.addr, p_packet->addr, BLE_GAP_ADDR_LEN);
    addr.addr_type = p_packet->header.addr_type;
    
    mesh_adv_data_t* p_mesh_adv_data = mesh_packet_adv_data_get(p_packet);
    
    
    if (p_mesh_adv_data != NULL)
    {
//    	LOGi("_3");
        /* filter mesh packets on handle range */
        if (p_mesh_adv_data->handle <= RBC_MESH_APP_MAX_HANDLE)
        {
//        	LOGi("_4");
            mesh_app_packet_handle(p_mesh_adv_data, timestamp);
        }
    }
    
    /* this packet is no longer needed in this context */
    mesh_packet_ref_count_dec(p_packet); /* from rx_cb */

    if (g_state.queue_saturation)
    {
//    	LOGi("_5");
        order_search();
        g_state.queue_saturation = false;
    }
    
    CLEAR_PIN(PIN_RX);
}
