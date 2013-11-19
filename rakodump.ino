#define EDGE_TIME_MIN    450
#define EDGE_TIME_MAX    600

#define RAKO_MSG_TYPE_COMMAND    0
#define RAKO_MSG_TYPE_DATA       2

// NOTE: Field order in the RakoMsg bit fields is reversed due 
//       to the uint32_t type being stored in little-endian order.
typedef union _RakoMsg RakoMsg;
union _RakoMsg
{
	struct
	{
		uint32_t data : 28;
		uint32_t type : 4;

	} unknown;
  
	struct
	{
		uint32_t padding : 3;

		uint32_t check   : 1;
		uint32_t command : 4;
		uint32_t channel : 4;
		uint32_t room    : 8;
		uint32_t house   : 8;
		uint32_t type    : 4;

	} command;

	struct
	{
		uint32_t padding : 3;

		uint32_t check   : 1;
		uint32_t address : 8;
		uint32_t data    : 8;
		uint32_t house   : 8;
		uint32_t type    : 4;

	} data;

	uint32_t raw;

} __attribute__((packed));

void printmsg(RakoMsg* msg);

RakoMsg g_msg;
int g_msgReady = 0;

void setup()
{
	Serial.begin(9600);

	pinMode(2, INPUT);  
  
	Serial.println("Ready");
}

void newbit()
{
	static int numBits = 0;
	static unsigned long lastTime = 0;
	static int last = 0;
	static int haveStartMark = 0;
	static int sum;

	unsigned long currentTime = micros();
	int duration = currentTime - lastTime;;
	int current = digitalRead(2);

	// If there have been too many level transitions then bail out and restart
	if(numBits >= sizeof(g_msg.raw) * 8)
		goto RESTART;

	// If the level transition came too quickly then it 
	// must be noise, so bail out and restart
	if(duration < EDGE_TIME_MIN)
		goto RESTART;

	if(last == 0)
	{
		// This edge is transitioning from a low to a high.

		// If it was low for too long then bail out and restart. 
		if(duration > EDGE_TIME_MAX)
			goto RESTART;
	}
	else
	{
		// This edge is transitioning from a high to a low.

		if(duration > EDGE_TIME_MIN && duration < EDGE_TIME_MAX)
		{
			// It was high for '1T' so shift a zero into the data
			g_msg.raw = (g_msg.raw << 1) | 0;
			++numBits;
		}
		else if(duration > EDGE_TIME_MIN * 2 && duration < EDGE_TIME_MAX * 2)
		{
			// It was high for '2T' so shift a one into the data
			g_msg.raw = (g_msg.raw << 1) | 1;
			++sum;
			++numBits;
		}
		else if(duration > EDGE_TIME_MIN * 4 && duration < EDGE_TIME_MAX * 4)
		{
			// It was high for '4T' so this is mark

			if(haveStartMark == 1)
			{
				// This is the end mark

				// Don't count the check bit in the sum
				if((g_msg.raw & 1))
					--sum;

				// If we have the right number of bits and the check bit matches
				// then we have a message ready for processing.
				if(numBits == 29 && (g_msg.raw & 1) == (sum & 1))
				{
					g_msg.raw <<= 3;
					Serial.print("end mark: "); Serial.println(numBits);
					g_msgReady = 1;
				}

				goto RESTART;
			}
			else
			{
				// This is the start mark, reset the checksum and bit count
				// ready for the new message.

				haveStartMark = 1;
				sum = 0;
				numBits = 0;
			}
		}
		else
		{
			// It was high for too long, bail out and restart
			goto RESTART;
		}
	}

	lastTime = currentTime;
	last = current;
	return;

RESTART:
	lastTime = currentTime;
	last = current;
	numBits = 0;
	haveStartMark = 0;
}

void printmsg(RakoMsg* msg)
{
	static char* commandNames[] = 
	{
		"OFF",          // 0
		"RAISE",        // 1
		"LOWER",        // 2
		"SCENE1",       // 3
		"SCENE2",       // 4
		"SCENE3",       // 5
		"SCENE4",       // 6
		"PROGRAM-MODE", // 7
		"IDENT",        // 8
		"SCENEX",       // 9
		"LOW-BATTERY",  // 10
		"EEPROM-WRITE", // 11
		"SET-LEVEL",    // 12
		"STORE",        // 13
		"EXIT",         // 14
		"STOP"          // 15
	};


	Serial.println("================");
	if(msg->unknown.type == RAKO_MSG_TYPE_COMMAND)
	{
		Serial.println("   Type: COMMAND");
		Serial.print("  House: "); Serial.println(msg->command.house);
		Serial.print("   Room: "); Serial.println(msg->command.room);
		Serial.print("Channel: "); Serial.println(msg->command.channel);
		Serial.print("Command: "); 
		Serial.print(msg->command.command);
		Serial.print(": ");
		Serial.println(commandNames[msg->command.command]);
	}
	else if(msg->unknown.type == RAKO_MSG_TYPE_DATA)
	{
		Serial.println("   Type: DATA");
		Serial.print("  House: "); Serial.println(msg->data.house);
		Serial.print("   Data: "); Serial.println(msg->data.data);
		Serial.print("Address: "); 
		Serial.print(msg->data.address); 
		Serial.print(": ");
		switch(msg->data.address)
		{
			case 0x01:
				Serial.println("SCENE1-LEVEL");
				break;
			case 0x02:
				Serial.println("SCENE2-LEVEL");
				break;
			case 0x03:
				Serial.println("SCENE3-LEVEL");
				break;
			case 0x04:
				Serial.println("SCENE4-LEVEL");
				break;
			case 0x09:
				Serial.println("START-MODE");
				break;
			case 0x16:
				Serial.println("IGNORE-PROGRAM-MODE");
				break;
			case 0x17:
				Serial.println("IGNORE-GROUP-COMMANDS");
				break;
			case 0x18:
				Serial.println("IGNORE-HOUSE-COMMANDS");
				break;
			case 0x1a:
				Serial.println("USE-PROFILE");
				break;
			case 0x22:
				Serial.println("FADE-RATE");
				break;
			case 0x24:
				Serial.println("FADE-DECAY-RATE");
				break;
			case 0x28:
				Serial.println("MANUAL-FADE-RATE-MAX");
				break;
			case 0x30:
				Serial.println("MANUAL-FADE-RATE-ACCELERATION");
				break;
			case 0x32:
				Serial.println("MANUAL-FADE-RATE-START");
				break;
			default:
				if(msg->data.address >= 0x3f)
				{
					Serial.print("PROFILE[");
					Serial.print(msg->data.address - 0x3f);
					Serial.println("]");
				}
				else
					Serial.println("UNKNOWN");
				break;
		}
	}
	else
	{
		Serial.print("   Type: "); Serial.print(msg->unknown.type); Serial.println(": UNKNOWN");
		Serial.print("   Data: "); Serial.println(msg->unknown.data, HEX);
	}
}

void loop()
{
	static int last = 0;
	int current = 0;

	// Wait for a level transition
	while((current = digitalRead(2)) == last)
		;
	last = current;

	// Process the level transition
	newbit();
    
	// If it completed a message then print it
	if(g_msgReady)
	{
		printmsg(&g_msg);
		g_msgReady = 0;
	}
}

