// ///////////////////////////////////////////////////////////
//
// Arduino FW for controlling the Welleman KA05 IO shield.
//
// Command set:
//
//  SET <port type> <port no> <value>
//  GET <port type> <port no>
//
//  SUBSCRIBE CHANGES <ON|OFF>
//
// Port types:
//
//  RO - Relay Output ports (8-13)
//  DI - Digital Input ports (2-7)
//  AI - Analog Input ports (0-5)
//
// ///////////////////////////////////////////////////////////

const int numIoPortsOfEachType = 6;
const int firstRelayOutPort = 8;
const int firstDigitalInPort = 2;
const int firstAnalogInPort = A0;

#define BUFFSIZE 200
#define INC(_x) do { _x++; _x = _x % BUFFSIZE; } while (0)

static char responseBuff[255];
static int sendChanges = 0;

void sendResponse(const char * format, ...);
int match(const char * s1, const char * s2);

// ///////////////////////////////////////////////////////////
//
// CmdBuff - Class for handling reception of command strings
//
// ///////////////////////////////////////////////////////////
class CmdBuff {

private:
  char buffer[BUFFSIZE];
  int insert;
  int start;
  int cmdsInBuff;
  int lastCmdLen;

public:
  CmdBuff()
  {
    buffer[0] = '\0';
    insert = start = cmdsInBuff = lastCmdLen = 0;
  }

  void put(char c)
  {
    if (c == 13)
    {
      if ( lastCmdLen > 0 )
      {
        c = '\0';
        cmdsInBuff++;
        lastCmdLen = 0;
      }
      else
      {
        return;
      }
    }
    else
    {
      lastCmdLen++;
    }

    buffer[insert] = c;
    INC(insert);
  }

  char * get()
  {
    static char buff[BUFFSIZE];
    int s = start;
    int i = 0;

    memset(buff, 0, sizeof(buff));
    while (s != insert && buffer[s] != '\0')
    {
      buff[i] = buffer[s];
      INC(i);
      INC(s);
    }
    buff[i] = '\0';

    start = s;
    INC(start);

    cmdsInBuff--;

    return buff;
  }

  int cmdAvailable()
  {
    return cmdsInBuff;
  }

  void dump()
  {
    sprintf(responseBuff, "dump: i=%d, s=%d, c=%d, l=%d\n", insert, start, cmdsInBuff, lastCmdLen);
    Serial.println(responseBuff);
  }
};


static CmdBuff commandBuff;


// ///////////////////////////////////////////////////////////
//
// Port - Port base class
//
// ///////////////////////////////////////////////////////////
class Port
{
protected:
  int lastIValue;
  int ionum;
public:

  Port(int io);

  virtual int setOValue(int val);
  virtual int getIValue();

  int getLastIValue();
};


Port::Port(int io)
{
  ionum = io;
  lastIValue = -1;
}

int Port::setOValue(int val)
{
  return 0;
}

int Port::getIValue()
{
  lastIValue = digitalRead(ionum);
  return lastIValue;
}

int Port::getLastIValue()
{
  return lastIValue;
}

// ///////////////////////////////////////////////////////////
//
// RelayOutPort - Relay Out port
//
// ///////////////////////////////////////////////////////////
class RelayOutPort : public Port
{
public:
  RelayOutPort(int io);

  int setOValue(int val);
};

RelayOutPort::RelayOutPort(int io) : Port(io)
{
}

int RelayOutPort::setOValue(int val)
{
  if (val == 0 || val == 1)
  {
    digitalWrite(ionum, val);
    lastIValue = val;
    return 1;
  }
  return 0;
}

// ///////////////////////////////////////////////////////////
//
// AnalogInPort - Analog Input port
//
// ///////////////////////////////////////////////////////////
class AnalogInPort : public Port
{
public:
  AnalogInPort(int io);

  int getIValue();
};

AnalogInPort::AnalogInPort(int io) : Port(io)
{
}

int AnalogInPort::getIValue()
{
  lastIValue = analogRead(ionum);
  return lastIValue;
}

// ///////////////////////////////////////////////////////////
//
// Ports - Collection of ports
//
// ///////////////////////////////////////////////////////////
class Ports
{
private:
  Port * ports[20];
  int minPort;
  int numPorts;
public:
  Ports(const char * type, int num, int min);

  Port * getPort(int io);
};

Ports::Ports(const char * type, int num, int min)
{
  minPort = min;
  numPorts = num;

  for (int i = 0; i < num; i++)
  {
    if (match(type, "RO"))
    {
      ports[i] = new RelayOutPort(min + i);
    }
    else if (match(type, "DI"))
    {
      ports[i] = new Port(min + i);
    }
    else if (match(type, "AI"))
    {
      ports[i] = new AnalogInPort(min + i);
    }
  }
}

Port * Ports::getPort(int io)
{
  if ( io >= minPort && io < minPort + numPorts )
  {
    return ports[io - minPort];
  }
  return NULL;
}

Ports * ROPorts;
Ports * DIPorts;
Ports * AIPorts;

// ///////////////////////////////////////////////////////////
//
//
//
// ///////////////////////////////////////////////////////////

void readCmd()
{
  while (Serial.available() > 0)
  {
    commandBuff.put(Serial.read());
  }
}

void sendResponse(const char * format, ...)
{
  va_list args;

  va_start(args, format);
  vsprintf(responseBuff, format, args);
  va_end(args);
  Serial.println(responseBuff);
}

int match(const char * s1, const char * s2)
{
  return (strncmp(s1, s2, strlen(s2)) == 0);
}

void handleCommand(const char * cmd)
{
  int valid = 0;
  char setOrGet[BUFFSIZE] = "";
  char portType[BUFFSIZE] = "";
  int io = 0;
  int val = 3;
  int args;
  Ports * ports = NULL;

  // Handle the non-port related commands...

  if (match(cmd, "SUBSCRIBE CHANGES ON"))
  {
    sendResponse("OK");
    sendChanges = 1;
    valid = 1;
  }
  else if (match(cmd, "SUBSCRIBE CHANGES OFF"))
  {
    sendResponse("OK");
    sendChanges = 0;
    valid = 1;
  }
  else
  {
    // Handle the SET and GET commands...

    args = sscanf(cmd, "%s %s %d %d", setOrGet, portType, &io, &val);

    // Figure out port type first...
    if (args > 2)
    {
      // Relay outputs
      if (match(portType, "RO"))
      {
        ports = ROPorts;
      }
      // Digital inputs
      else if (match(portType, "DI"))
      {
        ports = DIPorts;
      }
      // Analog inputs
      else if (match(portType, "AI"))
      {
        ports = AIPorts;
      }
    }

    // Then handle SET or GET command accordingly...
    if ( args  == 4 && match(setOrGet, "SET"))
    {
      if ( ports && ports->getPort(io) )
      {
        valid = ports->getPort(io)->setOValue(val);
        sendResponse("OK");
      }
    }
    else if (args == 3 && match(setOrGet, "GET"))
    {
      if ( ports && ports->getPort(io) )
      {
        valid = 1;
        sendResponse("OK %s %d %d\n", portType, io, ports->getPort(io)->getIValue());
      }
    }
  }

  // If command unsuccessful, give error message...
  if (!valid)
  {
    sendResponse("Invalid command: %s\n", cmd);
  }
}

void setup()
{
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);

  pinMode(7, INPUT);
  pinMode(6, INPUT);
  pinMode(5, INPUT);
  pinMode(4, INPUT);
  pinMode(3, INPUT);
  pinMode(2, INPUT);

  // start serial port at 9600 bps and wait for port to open:
  Serial.begin(9600);
  while (!Serial)
  {
    ;             // wait for serial port to connect. Needed for Leonardo only
  }

  ROPorts = new Ports("RO", numIoPortsOfEachType, firstRelayOutPort);
  DIPorts = new Ports("DI", numIoPortsOfEachType, firstDigitalInPort);
  AIPorts = new Ports("AI", numIoPortsOfEachType, firstAnalogInPort);
}

void loop()
{
  static int loopcount;
  int io;

  // Command handling...
  readCmd();
  if (commandBuff.cmdAvailable())
  {
    handleCommand(commandBuff.get());
  }


  // Monitor values and report changes. Poll every 2 seconds (approx.)
  if (sendChanges && (++loopcount > 20))
  {
    loopcount = 0;

    for (io = firstRelayOutPort; io < firstRelayOutPort + numIoPortsOfEachType; io++)
    {
      if ( ROPorts->getPort(io)->getLastIValue() != ROPorts->getPort(io)->getIValue() )
      {
        sendResponse("RO %d %d\n", io, ROPorts->getPort(io)->getLastIValue());
        delay(10);
      }
    }

    for (io = firstDigitalInPort; io < firstDigitalInPort + numIoPortsOfEachType; io++)
    {
      if ( DIPorts->getPort(io)->getLastIValue() != DIPorts->getPort(io)->getIValue() )
      {
        sendResponse("DI %d %d\n", io, DIPorts->getPort(io)->getLastIValue());
        delay(10);
      }
    }

    for (io = firstAnalogInPort; io < firstAnalogInPort + numIoPortsOfEachType; io++)
    {
      if ( AIPorts->getPort(io)->getLastIValue() != AIPorts->getPort(io)->getIValue() )
      {
        sendResponse("AI %d %d\n", io, AIPorts->getPort(io)->getLastIValue());
        delay(10);
      }
    }
  }

  // Don't stress it...
  delay(100);
}
