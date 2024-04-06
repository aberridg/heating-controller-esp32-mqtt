class HeatingSystem
{
  private:
    LinkedList<HeatingZone*> _zones = LinkedList<HeatingZone*>();
    Manipulator   *_boiler;
    Pump          *_pump;
    elapsedMillis _sinceCoolDownStarted;
    elapsedMillis _sincePumpStarted;

    int GetZoneIndex(LinkedList<HeatingZone*> zones, HeatingZone *zone) {
      for (int i = 0; i < zones.size(); i++) {
        if (zones.get(i) == zone) {
          i;
        }
      }
      return -1; // not found
    }

    bool AreAllZonesOff() {
      //Serial.println("Checking if all zones are off");
      for (int i = 0; i < _zones.size(); i++) {
        HeatingZone *zone = _zones.get(i);
        if (!zone->IsOff()) {
          Serial.println("No - a zone is on");
          return false;
        }
      }
      return true;
    }

    int CountZonesOn() {
      //Serial.println("Counting the number of zones on");
      int count = 0;
      for (int i = 0; i < _zones.size(); i++) {
        HeatingZone *zone = _zones.get(i);
        if (!zone->IsOff()) {
          count++;
        }
      }
      return count;
    }

    bool IsCoolingDown() {
      //Serial.println("Checking if all zones are cooling down");
      for (int i = 0; i < _zones.size(); i++) {
        HeatingZone *zone = _zones.get(i);
        if (zone->IsCoolingDown()) {
          return true;
        }
      }
      return false;
    }

    bool IsBoilerRequired() {
      //Serial.println("Checking if all zones are cooling down");
      for (int i = 0; i < _zones.size(); i++) {
        HeatingZone *zone = _zones.get(i);
        if (zone->IsBoilerRequired()) {
          return true;
        }
      }
      return false;
    }

    bool IsPumpRequired() {
      for (int i = 0; i < _zones.size(); i++) {
        HeatingZone *zone = _zones.get(i);
        if (zone->IsPumpRequired()) {
          return true;
        }
      }
      return false;
    }

  public:
    HeatingSystem(int boilerPin, int pumpPin) {
      _boiler = new Manipulator(boilerPin, "Boiler");
      _pump = new Pump(pumpPin, "Pump");
    }

    String PrintState() {
      String disp;
      for (int i = 0; i < _zones.size(); i++) {
        HeatingZone *zone = _zones.get(i);
        disp += zone->getName() + ": "
                + zone->getValve()->PrintState() + ";";
        if (zone->getThermostat() != NULL) {
          disp += zone->getThermostat()->PrintState();
        }
        disp += "\n";
      }
      return disp;
    }

    void AddZone(HeatingZone *zone) {
      _zones.add(zone);
    }

    void SubscribeToAllTopics() {
      for (int i = 0; i < _zones.size(); i++) {
        HeatingZone *zone = _zones.get(i);
        // Subscribe to the MQTT topic for this zone - important!
        mqttClient.subscribe(String("heating/" + zone->getName()).c_str());
      }
    }

    void Update() {

      for (int i = 0; i < _zones.size(); i++) {
        HeatingZone *zone = _zones.get(i);
        zone->Update();
      }

      if (!IsBoilerRequired() && _boiler->IsOn()) {
        // Switch boiler off!
        Serial.println("Switching boiler off");
        _boiler->Off();
        mqttClient.publish(String("heating/boiler_pub").c_str(), "off", true);
      }

      if (!IsPumpRequired()) {
        mqttClient.publish(String("heating/pump_pub").c_str(), "off", true);
        Serial.println("Switch pump off");
        // Switch off pump
        _pump->Off();
      }

      if (IsBoilerRequired() && _pump->IsOn() && _sincePumpStarted > 10000 && _boiler->IsOff()) {
        Serial.println("Switching boiler on");
        mqttClient.publish(String("heating/boiler_pub").c_str(), "on", true);
        _boiler->On();
      } else if (IsPumpRequired() && _pump->IsOff()) { // start pump - if and only if at least one valve is open!
        Serial.println("Switching pump on");
        mqttClient.publish(String("heating/pump_pub").c_str(), "on", true);
        _pump->On();
        _sincePumpStarted = 0;
      }
    }

    void HandleMqtt(char* topic, byte* payload, unsigned int length) {
      Serial.println("Handle MQTT");
      Serial.println(topic);
      Serial.println((char *)payload);

      for (int i = 0; i < _zones.size(); i++) {
        HeatingZone *zone = _zones.get(i);
        String zoneTopic = String("heating/" + zone->getName());
        //mqttClient.publish("heating/test", zoneTopic.c_str(), true);
        //mqttClient.publish("heating/topic", topic, true);
        if (strcmp(topic, zoneTopic.c_str()) == 0) {
          clearDisplay();
          printOLED("Got message");
          printOLED((char*)payload);

          if (!strncmp((char *)payload, "on", length)) {
            mqttClient.publish(String(zoneTopic + "_pub").c_str(), "on", true);
            zone->Request();
          } else if (!strncmp((char *)payload, "off", length)) {
            mqttClient.publish(String(zoneTopic + "_pub").c_str(), "off", true);
            if (zone->IsOn() && CountZonesOn() == 1) {
              // Only request cool down if this is the one and only zone that is on!
              zone->RequestCoolDown();
            } else {
              zone->RequestShutDown();
            }
          } else if (!strncmp((char *)payload, "inhibit", length)) {
            mqttClient.publish(String(zoneTopic + "_pub").c_str(), "inhibit", true);
            if (zone->IsOn() && CountZonesOn() == 1) {
              // Only request cool down if this is the one and only zone that is on!
              zone->RequestCoolDownWithInhibit();
            } else {
              zone->Inhibit();
            }
          } else if (!strncmp((char *)payload, "uninhibit", length)) {
            mqttClient.publish(String(zoneTopic + "_pub").c_str(), "uninhibit", true);
            zone->Uninhibit();
          }
        }
      }
    }
};
