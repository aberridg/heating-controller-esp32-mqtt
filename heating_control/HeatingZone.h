class HeatingZone
{
  private:
    String _name;
    Valve *_valve;
    Thermostat *_thermostat;
    elapsedMillis sinceCoolDownRequested = 0;
    
  public:
    HeatingZone(String name, Valve *valve, Thermostat *thermostat) {
      _name = name;
      _valve = valve;
      _thermostat = thermostat;
      _state = States::off;
    }

    enum States {
      off, // 0
      inhibited, // 1
      requested, // 2
      shutDownRequested, // 3
      coolDownWithInhibitRequested, // 4
      coolDownRequested, // 5
      on // 6
    };

    States _state;

    void Request() {
      if (_state == States::inhibited) {
        // do nothing
        return;
      } else if (_state != States::requested) {
        getValve()->On();
        _state = States::requested;        
      }
    }

    void RequestShutDown() {
      Serial.print("Req shutdown");
      Serial.println(_state);
      getValve()->Off();
      _state = States::shutDownRequested; // this _should_ immediately shut down            
    }

    void RequestCoolDownWithInhibit() {
      RequestCoolDown();
      _state = States::coolDownWithInhibitRequested;
    }

    // Shut down immediately and inhibit
    void Inhibit() {
      Serial.print("Inhibit");
      Serial.println(_state);
      getValve()->Off();
      _state = States::inhibited; 
    }

    void Uninhibit() {
      if (_state == States::inhibited) {
        _state = States::off;
      }
    }

    void RequestCoolDown() {
      if (!getValve()->IsValveOpen()) {
        // Shut down immediately...
        Serial.println("Requesting immediate shutdown");
        RequestShutDown();       
      } else {
        Serial.println("Requesting Cooldown");
        _state = States::coolDownRequested;
        sinceCoolDownRequested = 0;      
      }
    }

    bool IsCoolingDown() {
      return _state == States::coolDownRequested || _state == States::coolDownWithInhibitRequested;
    }

    bool IsOff() {
      if (_state == States::requested || _state == States::on) return false;
      return _state == States::off || _state == States::shutDownRequested || _state == States::coolDownWithInhibitRequested || _state == States::coolDownRequested;
    }

    bool IsOn() {
      return !IsOff();
    }

    void ProcessThermostat() {
      //Serial.println("Process therm");
      if (getThermostat() != NULL && getThermostat()->IsOn() && _state != States::requested && _state != States::on && _state != States::coolDownWithInhibitRequested) {
        //Serial.println("Therm on");
        mqttClient.publish(String("heating/" + _name + "_thermostat_pub").c_str(), "on", true);
        Request();
      }

      if (getThermostat() != NULL && getThermostat()->IsOff() && _state == States::on) {
        mqttClient.publish(String("heating/" + _name + "_thermostat_pub").c_str(), "off", true);
        RequestCoolDown();
      }
    }

    void Update() {
      ProcessThermostat();
      getValve()->Update();

      if (_state == States::coolDownRequested || _state == States::coolDownWithInhibitRequested) {
        if (sinceCoolDownRequested > COOLDOWN_TIME * 1000) {
          getValve()->Off();        
        }

        if (getValve()->IsOff() && getValve()->IsClosed()) {
          if (_state == States::coolDownWithInhibitRequested) {
            _state = States::inhibited;
          } else {
            _state = States::off;
          }
        }
      }

      if (getValve()->IsValveOpen() && _state == States::requested) {
        _state = States::on;
      }

      if (_state == States::shutDownRequested && getValve()->IsOff() && getValve()->IsClosed()) {
        _state = States::off;
      }

      if (_state == States::coolDownWithInhibitRequested && getValve()->IsClosed()) {
        _state = States::inhibited;
      }

      Serial.println("Zone " + getName() + " update state: ");
      Serial.println(_state);
    }

    Valve *getValve() {
      return _valve;
    }

    Thermostat *getThermostat() {
      return _thermostat;
    }

    String getName() {
      return _name;
    }

    bool IsBoilerRequired() {
      return _state == States::on;
    }

    bool IsPumpRequired() {
      return getValve()->IsValveOpen();
    }
        
}; // End of HeatingZone class
