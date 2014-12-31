//
//
//
//
//


#define BUFFSIZE 20
#define INC(_x) do { _x++; _x = _x % BUFFSIZE; } while(0)

static char responseBuff[255];


/////////////////////////////////////////////////////////////
//
// CmdBuff - Class for handling reception of command strings
//
/////////////////////////////////////////////////////////////
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
    if(c == 13)
    {
      if( lastCmdLen > 0 )
      {
	c = '\0';
	cmdsInBuff++;
	lastCmdLen = 0;
      }
      else
	return;
    }
    else
      lastCmdLen++;

    buffer[insert] = c;
    INC(insert);
  }

  char* get()
  {
    char buff[BUFFSIZE];
    int s = start;
    int i = 0;

    memset(buff, 0, sizeof(buff));
    while(s != insert && buffer[s] != '\0')
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


/////////////////////////////////////////////////////////////
//
// Port - Port base class
//
/////////////////////////////////////////////////////////////
class Port
{
protected:
  int ionum;
public:

  Port(int io);

  virtual int setOValue(int val);
  int getOValue();
};


Port::Port(int io)
{
  ionum = io;
}

int Port::setOValue(int val)
{
  return 0;
}

int Port::getOValue()
{
  return digitalRead(ionum); 
}


/////////////////////////////////////////////////////////////
//
// RelayOutPort - Relay Out port
//
/////////////////////////////////////////////////////////////
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
  if(val == 0 || val == 1)
  {
    digitalWrite(ionum, val); 
    return 1;
  }
  return 0;
}

/////////////////////////////////////////////////////////////
//
// Ports - Collection of ports
//
/////////////////////////////////////////////////////////////
class Ports
{
private:
  Port *ports[20];
  int minPort;
  int numPorts;
public:
  Ports(char *type, int num, int min);
  
  Port *getPort(int io);
};

Ports::Ports(char *type, int num, int min)
{
  minPort = min;
  numPorts = num;
  
  for(int i = 0; i < num; i++)
  {
    if(match(type, "RO"))
      ports[i] = new RelayOutPort(min + i);
    else if(match(type, "DI"))
      ports[i] = new Port(min + i);
    if(match(type, "AI"))
      ports[i] = new Port(min + i);
  }
}

Port* Ports::getPort(int io)
{
  if( io >= minPort && io < minPort+numPorts )
  {
    return ports[io-minPort];
  }
  return NULL;
}

Ports *ROPorts;
Ports *DIPorts;
Ports *AIPorts;

/////////////////////////////////////////////////////////////
//
// 
//
/////////////////////////////////////////////////////////////

void readCmd()
{
  while (Serial.available() > 0)
    commandBuff.put(Serial.read());
}

void sendResponse(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vsprintf(responseBuff, format, args);
  va_end(args);
  Serial.println(responseBuff);
}

int match(const char *s1, const char *s2)
{
  return (strncmp(s1, s2, strlen(s2)) == 0);
}

void handleCommand(char *cmd)
{
  int valid = 0;
  char setOrGet[BUFFSIZE] = "";
  char portType[BUFFSIZE] = "";
  int io = 0;
  int val = 3;
  int args;
  Ports *ports = NULL;

  args = sscanf(cmd, "%s %s %d %d", setOrGet, portType, &io, &val);  

  // Figure out port type first...
  if(args > 2)
  {
    // Relay outputs
    if(match(portType, "RO"))
      ports = ROPorts;
    // Digital inputs
    else if(match(portType, "DI"))
      ports = DIPorts;
    // Analog inputs
    else if(match(portType, "AI"))
      ports = AIPorts;
  }

  // Then handle SET or GET command accordingly...
  if( args  == 4 && match(setOrGet, "SET"))
  {
    if( ports && ports->getPort(io) )
    {
      valid = ROPorts->getPort(io)->setOValue(val);
    }
  }
  else if(args == 3 && match(setOrGet, "GET"))
  {      
    if( ports && ports->getPort(io) )
    {
      valid = 1;
      sendResponse("OK %s %d %d\n", portType, io, ROPorts->getPort(io)->getOValue());
    }
  }

  // If command unsuccessful, give error message...
  if(!valid)
  {
    sendResponse("Invalid command: %s (%s, %s, %d, %d)\n", cmd, setOrGet, portType, io, val);
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
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }

  ROPorts = new Ports("RO", 6, 8);
  DIPorts = new Ports("DI", 6, 2);
  //AIPorts = new Ports("AI", 6, 8);
  AIPorts = NULL;
}

void loop()
{
  readCmd();
  
  if(commandBuff.cmdAvailable())
  {
    char *cmd = commandBuff.get();

    handleCommand(cmd);
  }
}


