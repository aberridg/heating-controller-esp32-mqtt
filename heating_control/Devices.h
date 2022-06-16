
#define VALVE_TIME             30000L    // 30 seconds to open/close a valve (on the safe site; takes typically 10 seconds)
#define PUMP_MAINTENANCE_TIME  1300000L // Activates pump maintenance run once per 36 hours. Needed to keep pump working (not implemented right now)
#define PUMP_ACTIVATION_TIME   5000L    // Activates the pump for ca 8 minutes 
#define COOLDOWN_TIME          1800L   // When heating is done, continue water circulation for another 30 minutes
// This enables further dissipation of the heat into the system (typically takes 15 to 30 minutes)

int displayRow = 0;
void flushDisplay() {
  display.display(); // display whatever is in the buffer
}
void clearDisplay() {
  delay(1000);
  display.clear(); // clear the display
  //display.setCursor(0,0);
  displayRow = 0;
}

void printOLED(String toDisplay) {
  display.drawString(0, displayRow * 10, toDisplay);
  displayRow ++;
  if (displayRow > 6) {
    flushDisplay();
    clearDisplay();
  }
}



// Helper classes for IO devices
extern void printTimeStamp(); // defined in main ino file

// IODevice: base class for all IO devices; needs specialization
class IODevice {
    //vars
  protected:
    bool _IsOn;
    int _Pin;
    String _Name;

    //constructor
  public:
    IODevice(int pin, String name)   {
      _IsOn = false;
      _Pin = pin;
      _Name = name;
    }
    //methods
    virtual bool IsOn() = 0; // abstract
    virtual bool IsOff() {   // default for all
      return !IsOn();
    }

    String Name() {
      return _Name;
    }

    void DebugPrintState()   {
      clearDisplay();
      printTimeStamp();
      printOLED(PrintState()); flushDisplay();
    }

    String PrintState() {
      String ret = _Name + ":" + String(_Pin);
      if (_IsOn)
        ret += ("=1");
      else
        ret += ("=0");
      return ret;
    }
};

// Thermostat: reads a digital input
class Switch : public IODevice
{
    //vars
  private:
    int _Counter; // used to prevent reading intermittent switching (dender)

    //constructor
  public:
    Switch(int pin, String name) : IODevice(pin, name)   {
      Serial.println(String(pin) + name + " constructed ");
      _Counter = 0;
      pinMode(_Pin, INPUT_PULLUP);
    }

    //methods
    bool IsOn()   {
      //Serial.println("Switch IsOn");
      if (digitalRead(_Pin) == HIGH  && _IsOn == true) // open contact while on
      {
        if ( _Counter++ > 5) // only act after  5 times the same read out
        {
          _IsOn = false;
          DebugPrintState();
          _Counter = 0;
        }
      }
      else if (digitalRead(_Pin) == LOW  && _IsOn == false) // closed contact while off
      {
        if ( _Counter++ > 5) // only act after  5 times the same read out
        {
          _IsOn = true;
          DebugPrintState();
          _Counter = 0;
        }
      }
      else
      {
        _Counter = 0;
      }
      //Serial.println(_IsOn);
      return _IsOn;
    }
};

class Thermostat : public Switch
{
    // A Thermostat is a Switch that we can read - just like any other
    //constructor
  public:
    Thermostat(int pin, String name) : Switch(pin, name)   {
    }

};

// Manipulator: the most basic working device on a digital output
class Manipulator : public IODevice
{
    //vars
  protected:
    elapsedMillis _sinceStateChanged = 0;
    //constructor
  public:
    Manipulator(int pin, String name)  : IODevice(pin, name)   {
      pinMode(_Pin, OUTPUT);
      digitalWrite(_Pin, HIGH);
    }
    //methods
    void On()    {
      if (_IsOn == false)
      {
        _IsOn = true;
        digitalWrite(_Pin, LOW);
        _sinceStateChanged = 0;
        onSwitch();
      }
    }

    void Off()   {
      if (_IsOn == true)
      {
        _IsOn = false;
        digitalWrite(_Pin, HIGH);
        _sinceStateChanged = 0;
        onSwitch();
      }
    }

    elapsedMillis GetSinceStateChanged() {
      return _sinceStateChanged;
    }

    virtual void onSwitch() {  // trigger for child claases; change in on/off state
      DebugPrintState();
    }

    virtual bool IsOn()   {
      return _IsOn;
    }

    virtual bool IsOff() {
      return !_IsOn;
    }
};

// Valve: controls a valve on a digital output via a relay.
// These valves have microswitches to detect when they are open
// loop() must call Update() to keep track if the valve is fully open or closed
class Valve : public Manipulator
{
  private:
    long transitionCount;
    Switch *microswitch;

    enum States {
      open,
      closed,
      failedOpen,
      failedClosed,
      opening,
      closing
    };

    States _state;
    
    //constructor
  public:
    Valve(int pin, int microswitchPin, String name) : Manipulator(pin, name)   {
      Serial.println(name + " valve microswitch:" + String(microswitchPin));
      microswitch = new Switch(microswitchPin, "MS." + name);
      transitionCount = 0;
    }

    bool IsValveOpen()   {
      //Serial.println(microswitch->IsOn());
      return _state == States::open; // valve is requested to be open and has actually opened
    }

    bool IsClosed() {
      return _state == States::failedClosed || _state == States::closed;
    }

  public:
    bool FailedClosed() { // todo: consider making this permanent, so it doesn't get reset every time?
      return _state == States::failedClosed;
    }

    bool FailedOpen() {
      return _state == States::failedOpen;
    }

    bool IsOpening() {
      return _state == States::opening;
    }

    // Execute once per pass in the sketch loop() !!!
    void Update() {
      if (IsOn()) {
        if (microswitch->IsOn()) {
          _state = States::open;
        } else if (_sinceStateChanged > VALVE_TIME) {
          if (!microswitch->IsOn()) {
            //Serial.println("Failed closed");
            _state = States::failedClosed;
          }
        }
      }
      else { // Valve is closing
        if (microswitch->IsOff()) {
          _state = States::closed;
        } else if (_sinceStateChanged > VALVE_TIME) {
          if (microswitch->IsOn()) {
            //Serial.println("Failed open");
            _state = States::failedOpen;
          }
        }
      }
    }
};


// Pump: a pump need to be activated several times a week to keep them going.
// loop() must call Update() to keep track when a maintenance activation is needed
class Pump : public Manipulator
{
    // valves react slowly (3-5 minutes) so this class adds this transition awareness
  private:
    long counter;
    bool doMaintenance;

    //constructor
  public:
    Pump(int pin, String name) : Manipulator(pin, name)   {
      counter = 0;
      doMaintenance = false;
    }

    bool doMaintenanceRun()   {
      return doMaintenance;
    }

    virtual void onSwitch() {  // change in on/off state
      Manipulator::onSwitch();
      counter = 0;
    }

    // run this method every pass in loop()
    void Update()   {
      if (IsOn()) {
        if (counter < PUMP_ACTIVATION_TIME) {
          counter++;
        } else if (doMaintenance) {
          clearDisplay();
          printTimeStamp();
          printOLED(": Pump Maintenance cleared"); flushDisplay();
          doMaintenance = false;
        }
      }
      else {
        if (counter < PUMP_MAINTENANCE_TIME) {
          counter++;
        }  else if (doMaintenance == false) {
          clearDisplay();
          printTimeStamp();
          printOLED(": Pump Maintenance needed"); flushDisplay();
          doMaintenance = true;
        }
      }
    }
};

// LED; besides on/off it offers a method to alternate the LED (1Hz)
// just call Alternate() from the loop() to activate alternation
class LED : public Manipulator
{
  private:
    long counter;

    //constructor
  public:
    LED(int pin, String name) : Manipulator(pin, name)   {
      counter = 0;
    }

    virtual void onSwitch() {  // change in on/off state
      // surpress printing debug output for LEDs
    }

    void Alternate() {
        counter = 0;
        if (IsOn())
          Off();
        else
          On();      
    }
};
