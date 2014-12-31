
#define BUFFSIZE 20
#define INC(_x) do { _x++; _x = _x % BUFFSIZE; } while(0)

static char responseBuff[255];

class cmdBuff {

 private:
  char buffer[BUFFSIZE];
  int insert;
  int start;
  int cmdsInBuff;
  int lastCmdLen;
  
 public:
  cmdBuff()
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


static cmdBuff commandBuff;

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
}

void readCmd()
{
  while (Serial.available() > 0)
    commandBuff.put(Serial.read());
}

void setOValue(int io, int val)
{
  digitalWrite(io, val); 
}

void handleCommand(char *cmd)
{
  int valid = 0;
  char setOrGet[BUFFSIZE] = "";
  int io = 0;
  int val = 3;

  if( sscanf(cmd, "%s %d %d", setOrGet, &io, &val) == 3 )
  {
    if(strncmp(setOrGet, "SET", 3) == 0)
    {
      if( io >= 8 && io <= 13 )
      {
	if(val == 0 || val == 1)
	{
	  valid = 1;
	  setOValue(io, val);
	}
      }
    }
    else if(strncmp(setOrGet, "GET", 3) == 0)
    {
    }
  }
  
  if(!valid)
  {
    sprintf(responseBuff, "Invalid command: %s (%s, %d, %d)\n", cmd, setOrGet, io, val);
    Serial.println(responseBuff);
  }
}

void loop()
{
  readCmd();
  
  if(commandBuff.cmdAvailable())
  {
    char *cmd = commandBuff.get();

    sprintf(responseBuff, "cmd: %s\n", cmd);
    Serial.println(responseBuff);
    
    handleCommand(cmd);
  }
}


