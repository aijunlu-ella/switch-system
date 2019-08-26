/* Daniel Paul Iuliano
z3101121
diuliano@gmail.com
Thesis Topic University of New South Wales
Supervisor: Tim Moors
The PC Switch

8 port Banyan switching Fabric
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>			// for ceil() function
#include <sys/socket.h>		// socket() & bind()
#include <errno.h>			// for DieWithError() function
#include <arpa/inet.h>		// sockaddr_in
#include <time.h> 



#define INPUT_PORT 50001
#define OUTPUT_PORT 50002
#define LOCAL_ADDRESS "127.0.0.1"
#define MAX_MSG_LEN 200


int n, i, start; // global variable

// create banyan fabric struct


typedef struct packet packet; // packet struct
struct packet {
	char ip_dest[4][9];
	char ip_source[4][9];
	short int dataLength;
	char data[100];
	int frameCheck;
	int fromPort;
	int toPort;
	int sequenceNum;
	int portSequenceNum;
	int timer;
};
packet recvPacket;


//knockout elements 
typedef struct knockoutElement knockoutElement;
struct knockoutElement {
	int elementNumber;
	int leftFlag;      //winner direction
	int rightFlag;     //losser direction
	knockoutElement *left;
	knockoutElement *right;
};
knockoutElement *elements[37];//37 judgements made in KO
knockoutElement *nextElement = NULL;


typedef struct packetBuffer packetBuffer;
// Declare buffer struct that holds packet struct
struct packetBuffer {
	packet queuedPacket;
	packetBuffer *nextPort;// pointer to next port
	packetBuffer *nextSamePort;//pointer to next element in the same output port

}; 

// Fabric functions
int knockout(int opp, knockoutElement *currentElement);
int checkPos(int current);
void fabricElements();


// Misc Functions
void dec2bin(int decimal, char *binary, int length);
int bin2dec(char *binary);
void DieWithError(char *errorMessage);

int main(void) {
	struct sockaddr_in clasOUT_addr;	// client info (classifier) variable, data given when received message
	struct sockaddr_in fabIN_addr;		// struct for incoming packets
	struct sockaddr_in fabOUT_addr;		// struct for outgoing packets
	unsigned int clasAddrLen;			// int to store length of client address (classifier) data when received

	packet recvPacket, outGoing;		// packet struct that will be received
	int start, n, k,sockIN, sockOUT, judge = 1, recvMsgLen, port, timer = 0, count, dropped = 0, symbol=0,num;
	//timer is 0 because first packet has to be between 0 and 99 ,timestamp to record packet arrive time 
	//symbol for making break point in loops
	int sequence[8];                 // store how many packets queued in one port one time
	packetBuffer *current = NULL;		// keep track of where loop is pointing to in queue row
	packetBuffer *newPacket = NULL;		// used when creating new queue element
	packetBuffer *temp = NULL;			// temp packetBuffer element when deleting root of queue row
	packetBuffer *nextSend[8];			// pointer to next element to be sent on port n
	packetBuffer *root[8];				// array of queues, queue 0 is for gateway port, 1-7 corresponding to other ports
	//----------------Network code----------------
	// Open socket
	if((sockIN = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		DieWithError("socket() failed");
	if((sockOUT = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		DieWithError("socket() failed");
	
	// Construct local address structure
	bzero((char *)&fabIN_addr, sizeof(fabIN_addr));	// zeroes out struct before data copied
    fabIN_addr.sin_family = AF_INET;				// specifies Address Family
    fabIN_addr.sin_addr.s_addr = htonl(INADDR_ANY);	// accept from any network interface
    fabIN_addr.sin_port = htons(INPUT_PORT);		// Using port 50001
    // Construct outgoing address structure
	bzero((char *)&fabOUT_addr,sizeof(fabOUT_addr));			// zeroes out struct before data copied
    fabOUT_addr.sin_family = AF_INET;							// specifies Address Family
    fabOUT_addr.sin_addr.s_addr = inet_addr(LOCAL_ADDRESS);		// Connect to local address
    fabOUT_addr.sin_port = htons(OUTPUT_PORT);					// output port 50002
	
    // Binding above data (address/port/etc) to socket descriptor sock for port 50001
    if((bind(sockIN, (struct sockaddr *)&fabIN_addr, sizeof(fabIN_addr))) < 0)
    	DieWithError("bind() failed");
    //--------------End Network code--------------
	srand((unsigned)time(NULL));//for KO random choose
	fabricElements();//initialize 
			for (n = 0; n<8; n++) {
			root[n] = NULL;
			nextSend[n] = NULL;			
			sequence[n] = 0;// zero sequence array
			}


	while(1) {
		if((recvMsgLen = recvfrom(sockIN, (void*)&recvPacket, MAX_MSG_LEN, 0, (struct sockaddr *) &clasOUT_addr, &clasAddrLen)) < 0)
    		DieWithError("recvfrom() failed");

		// if timestamp on this packet is different from timer, then send all the packets received in last timestamp
		while(recvPacket.timer > timer)
		{
			//search all the ports
			for (n = 0; n < 8; n++) {
				//printf("timer!= %i", n);
				for (count = 0; count<36; count++) //for a new port, reset the left and right flag
				{
					elements[count]->leftFlag = 0;
					elements[count]->rightFlag = 0;
				}//check whether there are packets in buffer
				while (root[n] != NULL)
				{	
					i=0;
					k = 0;
					port = n;
					nextSend[n] = root[n];
					num=sequence[port];//get how many packets in port n,if the packets in port n less than 4,send to scheduler
					while (num < 5 && num > 0)
					{
						printf("Packet %i sent out on port %i \n", nextSend[n]->queuedPacket.sequenceNum, n);

						outGoing.dataLength = nextSend[n]->queuedPacket.dataLength;
						strcpy(outGoing.data, nextSend[n]->queuedPacket.data);
						outGoing.frameCheck = nextSend[n]->queuedPacket.frameCheck;
						outGoing.fromPort = nextSend[n]->queuedPacket.fromPort;
						outGoing.toPort = nextSend[n]->queuedPacket.toPort;
						outGoing.sequenceNum = nextSend[n]->queuedPacket.sequenceNum;
						outGoing.portSequenceNum = nextSend[n]->queuedPacket.portSequenceNum;
						outGoing.timer = nextSend[n]->queuedPacket.timer;

						if (sendto(sockOUT, &outGoing, MAX_MSG_LEN, 0, (struct sockaddr *)&fabOUT_addr, sizeof(fabOUT_addr)) != MAX_MSG_LEN)
							DieWithError("sendto() failed");

						temp = nextSend[n];			// assign temp for freeing element
						nextSend[n] = nextSend[n]->nextSamePort;

						free(temp);
						sequence[port]--;
						num--;
					}//if the packets in port n more than 4, we do KO first than send packets
					while (num > 4 &&  sequence[port]>0)
					{
						//after drop or send packets there still more than 4 packets need to go through KO
						while (num - k > 4)
						{
							start = floor((i - 1) / 2);//because packet is assumed to be UDP,start from the first packet in the port
							i++;
							judge = knockout(n, elements[start]);//determin the opp, num indicate the starting elements
							printf("judge=%i   &&   i=%i    \n", judge, nextSend[n]->queuedPacket.sequenceNum);
							if (judge == 10)
							{
								//judge whether to drop packets or not
								outGoing.dataLength = nextSend[n]->queuedPacket.dataLength;
								strcpy(outGoing.data, nextSend[n]->queuedPacket.data);
								outGoing.frameCheck = nextSend[n]->queuedPacket.frameCheck;
								outGoing.fromPort = nextSend[n]->queuedPacket.fromPort;
								outGoing.toPort = nextSend[n]->queuedPacket.toPort;
								outGoing.sequenceNum = nextSend[n]->queuedPacket.sequenceNum;
								outGoing.portSequenceNum = nextSend[n]->queuedPacket.portSequenceNum;
								outGoing.timer = nextSend[n]->queuedPacket.timer;
								dropped++;
								printf("packet%i- %i st pakcet in queue dropped. %i packets dropped in knockout structure\n", nextSend[n]->queuedPacket.sequenceNum, i, dropped);
								k++;//record how many packets been dropped in this port
								temp = nextSend[n];			// assign temp for freeing element
								nextSend[n] = nextSend[n]->nextSamePort;

								free(temp);
								sequence[port]--;
							}
							else {
						
								outGoing.dataLength = nextSend[n]->queuedPacket.dataLength;
								strcpy(outGoing.data, nextSend[n]->queuedPacket.data);
								outGoing.frameCheck = nextSend[n]->queuedPacket.frameCheck;
								outGoing.fromPort = nextSend[n]->queuedPacket.fromPort;
								outGoing.toPort = nextSend[n]->queuedPacket.toPort;
								outGoing.sequenceNum = nextSend[n]->queuedPacket.sequenceNum;
								outGoing.portSequenceNum = nextSend[n]->queuedPacket.portSequenceNum;
								outGoing.timer = nextSend[n]->queuedPacket.timer;

								if (sendto(sockOUT, &outGoing, MAX_MSG_LEN, 0, (struct sockaddr *)&fabOUT_addr, sizeof(fabOUT_addr)) != MAX_MSG_LEN)
									DieWithError("sendto() failed");
								printf("Packet %i sent out on port %i \n", nextSend[n]->queuedPacket.sequenceNum, n);
								// send packet out 
								temp = nextSend[n];			// assign temp for freeing element
								nextSend[n] = nextSend[n]->nextSamePort;
	
								free(temp);
								sequence[port]--;
							}
						}
						//after drop or sent packets, the upcoming packets do not need to go through KO
						outGoing.dataLength = nextSend[n]->queuedPacket.dataLength;
						strcpy(outGoing.data, nextSend[n]->queuedPacket.data);
						outGoing.frameCheck = nextSend[n]->queuedPacket.frameCheck;
						outGoing.fromPort = nextSend[n]->queuedPacket.fromPort;
						outGoing.toPort = nextSend[n]->queuedPacket.toPort;
						outGoing.sequenceNum = nextSend[n]->queuedPacket.sequenceNum;
						outGoing.portSequenceNum = nextSend[n]->queuedPacket.portSequenceNum;
						outGoing.timer = nextSend[n]->queuedPacket.timer;

						if (sendto(sockOUT, &outGoing, MAX_MSG_LEN, 0, (struct sockaddr *)&fabOUT_addr, sizeof(fabOUT_addr)) != MAX_MSG_LEN)
							DieWithError("sendto() failed");
						printf("Packet %i sent out on port %i \n", nextSend[n]->queuedPacket.sequenceNum, n);
						// send packet out 
						temp = nextSend[n];			// assign temp for freeing element
						nextSend[n] = nextSend[n]->nextSamePort;
						
						free(temp);
						sequence[port]--;
					}

					if (num == 0 && sequence[port]==0 )
						root[n] = NULL;//terminate loop

				}

			}

			timer = recvPacket.timer;//refresh timer

		}

		// if timestamp on this packet is same as timer, then save all the packets received in this timestamp
			symbol = 0;//for terminate
			port = recvPacket.toPort;
			current = root[port];
			//printf("timer==\n");
			printf("packet %i arrived to port %i at time %i\n", recvPacket.sequenceNum, port,timer);
			while (current == NULL && symbol != 1)
			{// current packetBuffer is empty on this port,packet goes here
				//printf("current=NULL\n");
				if ((newPacket = (packetBuffer *)malloc(sizeof(packetBuffer))) != NULL) {
					for (n = 0; n<4; n++) {
						strcpy(newPacket->queuedPacket.ip_dest[n], recvPacket.ip_dest[n]);
						strcpy(newPacket->queuedPacket.ip_source[n], recvPacket.ip_source[n]);
					}
					port = recvPacket.toPort;
					sequence[port]++;	
					printf("the packet is in port%i number %i\n",port,sequence[port]);// increments sequence number array corresponding to array it is being sent too
					
					newPacket->queuedPacket.dataLength = recvPacket.dataLength;
					strcpy(newPacket->queuedPacket.data, recvPacket.data);
					newPacket->queuedPacket.frameCheck = recvPacket.frameCheck;
					newPacket->queuedPacket.fromPort = recvPacket.fromPort;
					newPacket->queuedPacket.toPort = recvPacket.toPort;
					newPacket->queuedPacket.sequenceNum = recvPacket.sequenceNum;
					newPacket->queuedPacket.portSequenceNum = recvPacket.portSequenceNum;
					newPacket->queuedPacket.timer = recvPacket.timer;


					newPacket->nextPort = NULL;
					newPacket->nextSamePort = NULL;
					
					root[port] = newPacket;
					
					timer = recvPacket.timer;
				}
				else
					DieWithError("Out of Memory");
				symbol = 1;// terminate loop to save time
			}
	
			while (current != NULL && symbol != 1) {	// check if recvPacket to be queued matches any already queued packet sources
				
				while (current->nextSamePort != NULL)
				{	// add to end of that port queue
					current = current->nextSamePort;
				}
				if ((newPacket = (packetBuffer *)malloc(sizeof(packetBuffer))) != NULL) {
					for (n = 0; n < 4; n++) {
						strcpy(newPacket->queuedPacket.ip_dest[n], recvPacket.ip_dest[n]);
						strcpy(newPacket->queuedPacket.ip_source[n], recvPacket.ip_source[n]);
					}
					port = recvPacket.toPort;
					sequence[port]++;							// increments sequence number array corresponding to array it is being sent too
					printf("the packet is in port%i number %i\n", port, sequence[port]);// increments sequence number array corresponding to array it is being sent too

					newPacket->queuedPacket.dataLength = recvPacket.dataLength;
					strcpy(newPacket->queuedPacket.data, recvPacket.data);
					newPacket->queuedPacket.frameCheck = recvPacket.frameCheck;
					newPacket->queuedPacket.fromPort = recvPacket.fromPort;
					newPacket->queuedPacket.toPort = recvPacket.toPort;
					newPacket->queuedPacket.sequenceNum = recvPacket.sequenceNum;
					newPacket->queuedPacket.portSequenceNum = recvPacket.portSequenceNum;
					newPacket->queuedPacket.timer = recvPacket.timer;

					newPacket->nextPort = NULL;
					newPacket->nextSamePort = NULL;
					
					current->nextSamePort = newPacket;
					current = NULL;	// terminate loop
					timer = recvPacket.timer;
				}
				else
					DieWithError("Out of Memory");
				symbol = 1;
			}
					
		
		
		
	}
	
	return 1;
}




//start to prepare the element in the knockout structure
void fabricElements() {
	int n = 0;
	while (n<37) //element 36 is used to indicate a drop
	{

		if ((elements[n] = (knockoutElement *)malloc(sizeof(knockoutElement))) != NULL)
		{
			elements[n]->elementNumber = n;
			elements[n]->leftFlag = 0;
			elements[n]->rightFlag = 0;
		}
		else
		{
			printf("Out of Memory");
			exit(0);
		}
		n++;
	}

	elements[0]->left = elements[4];
	elements[0]->right = elements[6];
	elements[1]->left = elements[4];
	elements[1]->right = elements[6];
	elements[2]->left = elements[5];
	elements[2]->right = elements[7];
	elements[3]->left = elements[5];
	elements[3]->right = elements[7];

	elements[4]->left = elements[8];
	elements[4]->right = elements[10];
	elements[5]->left = elements[8];
	elements[5]->right = elements[10];
	elements[6]->left = elements[9];
	elements[6]->right = elements[11];
	elements[7]->left = elements[9];
	elements[7]->right = elements[11];

	elements[8]->left = elements[12];//delay to 12
	elements[8]->right = elements[12];//delay to 12
	elements[9]->left = elements[13];
	elements[9]->right = elements[15];
	elements[10]->left = elements[13];
	elements[10]->right = elements[15];
	elements[11]->left = elements[15];
	elements[11]->right = elements[17];


	elements[12]->left = elements[18];//delay to 18
	elements[12]->right = elements[18];//delay to 18
	elements[13]->left = elements[19];
	elements[13]->right = elements[21];
	elements[14]->left = elements[19];//delay to 19
	elements[14]->right = elements[19];//delay to 19
	elements[15]->left = elements[20];
	elements[15]->right = elements[22];
	elements[16]->left = elements[20];//delay to 20
	elements[16]->right = elements[20];//delay to 20
	elements[17]->left = elements[22];//delay to 22
	elements[17]->right = elements[22];//delay to 22

	elements[18]->left = elements[23];//delay to 23
	elements[18]->right = elements[23];//delay to 23
	elements[19]->left = elements[24];
	elements[19]->right = elements[26];
	elements[20]->left = elements[25];
	elements[20]->right = elements[27];
	elements[21]->left = elements[25];
	elements[21]->right = elements[25];//delay to 25
	elements[22]->left = elements[27];
	elements[22]->right = elements[36]; //drop

	elements[23]->left = elements[28];//delay to 28
	elements[23]->right = elements[28];//delay to 28
	elements[24]->left = elements[29];//delay to 29
	elements[24]->right = elements[29];//delay to 29
	elements[25]->left = elements[30];
	elements[25]->right = elements[31];
	elements[26]->left = elements[30];//delay to 30
	elements[26]->right = elements[30];//delay to 30
	elements[27]->left = elements[31];
	elements[27]->right = elements[36];//drop
	elements[28]->left = elements[32];//delay to 32
	elements[28]->right = elements[32];//delay to 32
	elements[29]->left = elements[33];//delay to 33
	elements[29]->right = elements[33];//delay to 33
	elements[30]->left = elements[34];
	elements[30]->right = elements[35];
	elements[31]->left = elements[35];
	elements[31]->right = elements[36];// drop
	elements[35]->right = elements[36];//drop
}




int knockout(int opp, knockoutElement *currentElement)
{
	int ran, judgeport = 100;

	ran = rand() % 10;
	//printf("ran=%i\n", ran);
	//printf("cur=%i\n", currentElement->elementNumber);
	if (currentElement->elementNumber == 0 || currentElement->elementNumber == 1)//if the starting element is start at atelement[0] or the random number is smaller than 5  go to the left
	{
		if (currentElement->leftFlag == 0)// check the leftflag 
		{
			nextElement = currentElement->left;//go to the left
			currentElement->leftFlag = 1;//set the flag
		}
		else//cant use the left node\A3\A8leftflag=1\A3\A9
		{
			nextElement = currentElement->right;//go to the right
			currentElement->rightFlag = 1;//set thee flag
		}
	}
	else if (ran >= 5)// Èç¹ûŽÓ23¿ªÊŒÇÒran>5 ×ßÓÒ²à
	{
		if (currentElement->rightFlag == 0)// check the rightflag 
		{
			nextElement = currentElement->right;//go to the right
			currentElement->rightFlag = 1;//set the flag
		}
		else//cant use the right node
		{
			nextElement = currentElement->left;//go to the right
			currentElement->leftFlag = 1;//set thee flag
		}

	}
	else if (ran < 5)
	{

		if (currentElement->leftFlag == 0)// check the leftflag 
		{
			nextElement = currentElement->left;//go to the left
			currentElement->leftFlag = 1;//set the flag
		}
		else//cant use the left node\A3\A8leftflag=1\A3\A9
		{
			nextElement = currentElement->right;//go to the right
			currentElement->rightFlag = 1;//set thee flag
		}
	}
	if (nextElement->elementNumber == 36)
	{
		judgeport = 10;
		//printf("judge=%i\n",judgeport);
		//return 10;
	}

	if (nextElement->elementNumber == 32 || nextElement->elementNumber == 33 || nextElement->elementNumber == 34 || nextElement->elementNumber == 35)
	{
		judgeport = opp;
		//printf("judge=%i\n",judgeport);
		//return opp;
	}
	if (judgeport == 10 || judgeport == opp)
	{
		//printf("judgeport=%i\n", judgeport);
		return judgeport;
	}
	else {
		judgeport = knockout(opp, nextElement);
	}


}


// Convert binary string into decimal
int bin2dec(char *binary) {
	int n=strlen(binary)-1, i=1, decimal=0;
	
	while(n>=0) {
		if(binary[n] == '1')
			decimal = decimal+i;
		n--;
		i = i*2;
	}
	
	return decimal;
}

void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}
