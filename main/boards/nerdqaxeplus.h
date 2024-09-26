#pragma once

#include "asic.h"
#include "bm1368.h"
#include "board.h"

class NerdQaxePlus : public Board {
  protected:
    int num_tps_phases;

    void LDO_enable();
    void LDO_disable();

    BM1368 asics;

  public:
    NerdQaxePlus();

    virtual bool init();

// abstract common methos
    virtual bool set_voltage(float core_voltage);
    virtual uint16_t get_voltage_mv();

    virtual void set_fan_speed(float perc);
    virtual void get_fan_speed(uint16_t *rpm);

    virtual float read_temperature(int index);

    virtual float get_vin();
    virtual float get_iin();
    virtual float get_pin();
    virtual float get_vout();
    virtual float get_iout();
    virtual float get_pout();

    virtual Asic* getAsics() { return &asics; }
};