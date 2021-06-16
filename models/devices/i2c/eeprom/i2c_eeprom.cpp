/*
 * Copyright (C) 2021 GreenWaves Technologies, SAS, ETH Zurich and
 *                    University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "i2c_eeprom.hpp"

#include "stdio.h"
#include <cassert>

I2c_eeprom_memory::I2c_eeprom_memory(void)
    :  current_address(0x0),
    number_of_pages(-1),
    page_size(-1),
    default_value(0)
{
}

void I2c_eeprom_memory::set_address(int address)
{
    if (address >= (this->page_size * this->number_of_pages))
    {
        this->current_address = 0x0;
    }
    else
    {
        this->current_address = address;
    }
}

void I2c_eeprom_memory::write(uint8_t byte)
{
    int page_number = this->current_address / this->page_size;
    int page_index = this->current_address % this->page_size;

    this->memory[page_number][page_index] = byte;

    this->increment_address();
}

uint8_t I2c_eeprom_memory::read(void)
{
    int page_number = this->current_address / this->page_size;
    int page_index = this->current_address % this->page_size;

    uint8_t byte = this->memory[page_number][page_index];

    this->increment_address();
    return byte;
}

void I2c_eeprom_memory::increment_address(void)
{
    int page_number = this->current_address / this->page_size;
    int page_index = this->current_address % this->page_size;

    this->current_address = page_number * this->page_size + ((page_index + 1) % this->page_size);
    fprintf(stderr, "EEPROM current_address = %d\n", this->current_address);
}

void I2c_eeprom_memory::initialize_memory(int number_of_pages, int page_size, uint8_t default_value)
{
    assert(number_of_pages > 0);
    assert(page_size > 0);

    this->number_of_pages = number_of_pages;
    this->page_size = page_size;
    this->default_value = default_value;

    for (int i = 0; i < this->number_of_pages; i++)
    {
        std::vector<uint8_t> page;
        for (int j = 0; j < this->page_size; j++)
        {
            page.push_back(this->default_value);
        }

        this->memory.push_back(page);
    }
}

void I2c_eeprom_memory::erase_memory(void)
{
    for (int i = 0; i < this->number_of_pages; i++)
    {
        for (int j = 0; j < this->page_size; j++)
        {
            this->memory[i][j] = 0;
        }
    }
}

I2c_eeprom::I2c_eeprom(js::config* config)
    : vp::component(config),
    i2c_helper(this,
            &this->i2c_itf,
            std::bind(&I2c_eeprom::i2c_enqueue_event,
                this,
                std::placeholders::_1,
                std::placeholders::_2)
            )
{
    this->page_size = 8;
    this->number_of_pages = 128;
    this->i2c_address = 80;

    js::config* properties = config->get("properties");

    if (NULL != properties)
    {
        /* configure address */
        js::config* address_elt = properties->get("address");
        if (NULL != address_elt)
        {
            this->i2c_address = address_elt->get_int();
        }

        /* configure page size */
        js::config* page_size_elt = properties->get("page_size");
        if (NULL != page_size_elt)
        {
            this->page_size = page_size_elt->get_int();
        }
        /* configure number of pages*/
        js::config* number_of_pages_elt= properties->get("number_of_pages");
        if (NULL != number_of_pages_elt)
        {
            this->number_of_pages = number_of_pages_elt->get_int();
        }
    }

    assert(this->i2c_address > 0 && this->i2c_address < 128);
    assert(this->page_size > 0);
    assert(this->number_of_pages > 0);

    /* set helper callback */
    this->i2c_helper.register_callback(std::bind(&I2c_eeprom::i2c_helper_callback,
                this,
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3));

    /* initialize eeprom memory */
    this->memory.initialize_memory(this->number_of_pages, this->page_size, 0x55);
}

void I2c_eeprom::i2c_sync(void *__this, int scl, int sda)
{
    assert(NULL != __this);
    I2c_eeprom* _this = (I2c_eeprom*) __this;
    _this->i2c_helper.update_pins(scl, sda);
}


void I2c_eeprom::reset(bool active)
{
    if (active)
    {
        this->trace.msg(vp::trace::LEVEL_TRACE, "Resetting component\n");
        //TODO reset memory ?
        //TODO reset i2c interface
        //TODO reset event
    }
}

int I2c_eeprom::build(void)
{
    traces.new_trace("trace", &this->trace, vp::DEBUG);
    this->trace.msg(vp::trace::LEVEL_TRACE, "Building component\n");

    this->i2c_itf.set_sync_meth(&I2c_eeprom::i2c_sync);
    this->new_master_port("i2c", &this->i2c_itf);

    this->new_master_port("clock_cfg", &this->clock_cfg);

    this->event = this->event_new(I2c_eeprom::event_handler);

    return 0;
}

void I2c_eeprom::start(void)
{
    this->trace.msg(vp::trace::LEVEL_TRACE, "Starting component\n");
    this->event_enqueue(this->event, 10);

}

void I2c_eeprom::event_handler(void *__this, vp::clock_event *event)
{
    I2c_eeprom* _this = (I2c_eeprom*) __this;
    _this->trace.msg(vp::trace::LEVEL_TRACE, "event !!\n");
    //TODO
}

void I2c_eeprom::i2c_helper_callback(i2c_operation_e id, i2c_status_e status, int value)
{
    fprintf(stderr, "CALLBACK id=%d, status=%d, value=%d !\n",
            id, status, value);

    static bool starting = false;
    static bool is_addressed = false;
    static bool is_read = false;

    static uint32_t byte_counter = 0;
    static uint32_t current_address = 0x0;

    switch(id)
    {
        case MASTER_START:
            //TODO
            fprintf(stderr, "SL: START!\n");
            starting = true;
            is_addressed = false;
            byte_counter = 0;
            break;
        case MASTER_DATA:
            fprintf(stderr, "SL: DATA!\n");

            if (starting)
            {
                fprintf(stderr, "ADDR: %d\n", value);
                starting = false;
                is_read = value & 1;
                //assert(value >> 1 == this->i2c_address); // used for testing
                if ((value >> 1) == this->i2c_address)
                {
                    is_addressed = true;
                    this->i2c_helper.send_ack(true);
                }
            }
            else if (is_addressed && !is_read)
            {
                if (byte_counter == 0)
                {
                    current_address = (value & 0xFF) << 8;
                }
                else if (byte_counter == 1)
                {
                    current_address = (value & 0xFF) | current_address;
                    this->memory.set_address(current_address);
                }
                else
                {
                    //TODO store incoming data
                    fprintf(stderr, "EEPROM: Storing data=%d into memory\n", value & 0xFF);
                    this->memory.write(value & 0xFF);
                }

                fprintf(stderr, "SL: received data=%d\n", value);
                byte_counter++;

                this->i2c_helper.send_ack(true);
            }
            break;
        case MASTER_ACK:
            //TODO
            starting = false;
            fprintf(stderr, "SL: ACK!\n");
            if (status == MASTER_OK && is_addressed)
            {
                if(is_read)
                {
                    uint8_t byte = this->memory.read();
                    fprintf(stderr, "EEPROM: sending byte=%d\n", byte);
                    this->i2c_helper.send_data(byte);
                }
            }
            break;
        case MASTER_STOP:
            //TODO
            starting = false;
            fprintf(stderr, "SL: STOP!\n");
            break;
        default:
            break;
    }
}

void I2c_eeprom::i2c_enqueue_event(vp::clock_event* event, uint64_t time_ps)
{
    //TODO compute number of cycles according to time_ps
    this->event_enqueue(event, time_ps);
}

extern "C" vp::component *vp_constructor(js::config *config)
{
    return new I2c_eeprom(config);
}
