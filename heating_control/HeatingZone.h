class HeatingZone
{
  private:
    String _name;
    Valve *_valve;
    Thermostat *_thermostat;
    bool _inhibited;
    bool _coolDownRequired = false;
    elapsedMillis sinceCoolDownRequested = 0;
    
  public:
    HeatingZone(String name, Valve *valve, Thermostat *thermostat) {
      _name = name;
      _valve = valve;
      _thermostat = thermostat;
      _inhibited = false;
      _state = States::off;
    }

    enum States {
      off, // 0
      inhibited, // 1
      requested, // 2
      shutDownRequested, // 3
      shutDownWithInhibitRequested, // 4
      coolDownRequested, // 5
      on, // 6
      coolingDown // 7
    };

    States _state;

    void Request() {
      if (_state == States::inhibited) {
        // do nothing
        return;
      } else if (_state != States::requested) {
        getValve()->On();
        _state = States::requested;
        _coolDownRequired = true;
      }
    }

    void RequestShutDown() {
      Serial.print("Req shutdown");
      Serial.println(_state);
      if (_state == States::on) {
        _state = States::shutDownRequested;
        getValve()->Off();
      }
    }

    void RequestShutDownWithInhibit() {
      RequestShutDown();
      _state = States::shutDownWithInhibitRequested;
    }

    void Uninhibit() {
      if (_state == States::inhibited) {
        _state = States::off;
      }
    }

    void RequestCoolDown() {
      Serial.println("Requesting Cooldown");
      if (_coolDownRequired && _state != States::coolDownRequested) {
        getValve()->On();
        _state = States::coolDownRequested;
        sinceCoolDownRequested = 0;
      }
    }

    bool IsCoolingDown() {
      return _state == States::coolDownRequested;
    }

    bool IsOff() {
      if (_state == States::requested || _state == States::on) return false;
      return _state == States::off || _state == States::shutDownRequested || _state == States::shutDownWithInhibitRequested || _state == States::coolingDown;
    }

    void ProcessThermostat() {
      //Serial.println("Process therm");
      if (getThermostat() != NULL && getThermostat()->IsOn() && _state != States::requested && _state != States::on) {
        //Serial.println("Therm on");
        Request();
      }

      if (getThermostat() != NULL && getThermostat()->IsOff() && _state == States::on) {
        RequestShutDown();
      }
    }

    void Update() {
      ProcessThermostat();
      getValve()->Update();

      if (_state == States::coolDownRequested) {
        if (sinceCoolDownRequested > COOLDOWN_TIME * 1000) {
          _coolDownRequired = false;
          getValve()->Off();        
        }

        if (getValve()->IsClosed()) {
          _state = States::off;
        }
      }

      if (getValve()->isValveOpen() && _state != States::coolDownRequested && _state != States::shutDownRequested && _state != States::shutDownWithInhibitRequested && _state != States::inhibited) {
        _state = States::on;
      }

      if (getValve()->isValveOpen() && _state == States::on) {
        _coolDownRequired = true;
      }

      if (_state == States::shutDownRequested && getValve()->IsClosed()) {
        _state = States::off;
      }

      if (_state == States::shutDownWithInhibitRequested && getValve()->IsClosed()) {
        _state = States::inhibited;
      }

      Serial.println("Zone " + getName() + " update state: ");
      Serial.println(_state);
    }

    void HandleMqtt(String zoneTopic, byte* payload, unsigned int length) {
      /// Important!!!! Do not publish anything until topic and payload have been read! Doing so will overwrite topic/payload
      clearDisplay();
      printOLED("Got message");
      printOLED((char*)payload);
      flushDisplay();
      if (!strncmp((char *)payload, "on", length)) {
        client.publish(String(zoneTopic + "_pub").c_str(), "on", true);
        Request();
      } else if (!strncmp((char *)payload, "off", length)) {
        client.publish(String(zoneTopic + "_pub").c_str(), "off", true);
        RequestShutDown();
      } else if (!strncmp((char *)payload, "inhibit", length)) {
        client.publish(String(zoneTopic + "_pub").c_str(), "inhibit", true);
        RequestShutDownWithInhibit();
      } else if (!strncmp((char *)payload, "uninhibit", length)) {
        client.publish(String(zoneTopic + "_pub").c_str(), "uninhibit", true);
        Uninhibit();
      }
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

    void setInhibited(bool inhibited) {
      _inhibited = inhibited;
    }

    bool isInhibited() {
      //Serial.println("In isInhibited");
      //Serial.println(_inhibited);
      return _inhibited;
    }

    void setCoolDownRequired(bool coolDownRequired) {
      _coolDownRequired = coolDownRequired;
    }

    bool isCoolDownRequired() {
      return _coolDownRequired;
    }

    bool IsBoilerRequired() {
      return _state == States::on;
    }
        
}; // End of HeatingZone class