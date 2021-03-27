#include <stdio.h>
#include <string>
#include <map> 
#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>

using namespace std;
#define BUFFER_SIZE 3
#define TELLERS_NUM 3
// read file from input file
void readFile(string path);

// create multiple mutexes into an array
void createMutex(pthread_mutex_t* mutexName, int numOfMutexes);

// lock multiple mutexes
void lockMutex(pthread_mutex_t* mutexName, int numOfMutexes);

// tellers job
void *TELLER(void *param);

// assign clients to tellers whenever there is a client on the line
void *SCHEDULER(void *param);

// clients job
void *CLIENT(void *param);

// return the first available teller in the order A to B
int AVAILABLE();

// reserve the seat
int reserve(int seatNum);

// client struct
typedef struct{
	string name;
	int arrivalTime;
	int serviceTime;
	int seatNumber;
}client;



pthread_attr_t attr;

// scheduler relaese the lock after it assigns a client to teller where teller
// is trying to get the lock
pthread_mutex_t* scheduleLock;

// locks the seat that is meant to be reserved
pthread_mutex_t* seatLock;

// locks the writing operation on the file 
pthread_mutex_t writeLock;

// locks the buffer whenever somebody gets in the line until it is in the line
pthread_mutex_t bufferLockIn;

// locks the buffer whenever client is assigned to a teller
pthread_mutex_t bufferLockOut;

// locks the teller creation to maintain creating order of the tellers
pthread_mutex_t timeLock;


// counting and listing the clients
sem_t* empty= SEM_FAILED;
sem_t* full= SEM_FAILED;


// empty seats return false
bool* seat;

// available tellers return true
bool* available;

//checks if the all clients get served
bool finish=false;


// keeps the line
client* buffer;

// clients
client* clients;

// number of clients
int numOfClients;

// number of seats
int numOfSeats;

// where to put new comers into the buffer
int in=0;

// where to take out clients from the buffer
int out=0;

// number of clients served
int numOfServicedClients=0;

// where to write the output
ofstream output;

int main(int argc, char const *argv[])
{
	// INITIALIZING THE PROGRAM
	readFile(argv[1]);
	output.open(argv[2]);
	pthread_attr_init(&attr);
	buffer=new client[BUFFER_SIZE];
	seat=new bool[numOfSeats];
	available= new bool[TELLERS_NUM];
	for (int i = 0; i < BUFFER_SIZE; ++i){available[i]=false;}	
	for (int i = 0; i < numOfSeats; ++i){seat[i]=false;}	

	//SEMAPHORE CREATION
	sem_unlink("empty");
	sem_unlink("full");
	while(empty==(sem_t *)SEM_FAILED){
		empty=sem_open("empty", O_CREAT|O_TRUNC, 0666, BUFFER_SIZE);
	}
	while(full==(sem_t *)SEM_FAILED){
		full=sem_open("full", O_CREAT|O_TRUNC, 0666, 0);
	}

	//MUTEXES
	scheduleLock= new pthread_mutex_t[TELLERS_NUM];
	createMutex(scheduleLock,TELLERS_NUM);
	lockMutex(scheduleLock,TELLERS_NUM);
	seatLock= new pthread_mutex_t[numOfSeats];
	createMutex(seatLock,numOfSeats);
	pthread_mutex_init(&writeLock, NULL);
	pthread_mutex_init(&bufferLockIn, NULL);
	pthread_mutex_init(&bufferLockOut, NULL);
	pthread_mutex_init(&timeLock, NULL);

	//THREADS CREATING
	output<<"Welcome to the Sync-Ticket!"<<endl;
	pthread_t tellerThread[TELLERS_NUM];
	for (int i = 0; i < TELLERS_NUM; ++i){pthread_mutex_lock(&timeLock);pthread_create(&tellerThread[i],&attr,TELLER,(void *)(size_t)i); }
	
	pthread_t clientThread[numOfClients];
	for (int i = 0; i < numOfClients; ++i){pthread_create(&clientThread[i],&attr,CLIENT,&clients[i]);}

	pthread_t schedulerThread;
	pthread_create(&schedulerThread,&attr,SCHEDULER,NULL); 


	//THREAD JOIN
	for (int i = 0; i<TELLERS_NUM; ++i){pthread_join(tellerThread[i],NULL);}
	for (int i = 0; i<numOfClients ; ++i){pthread_join(clientThread[i],NULL);}
	pthread_join(schedulerThread,NULL);



	sem_close(empty);
	sem_close(full);
  	sem_unlink("empty");
  	sem_unlink("full");
  	output<<"All clients received service."<<endl;
	return 0;
}

void *TELLER(void *param){
	int n = (int)(size_t)param;
	char teller = (char)('A'+n);
	int reservedSeat=-1;
	client current;

	
		output<<"Teller "<<teller<<" has arrived."<<endl;
	pthread_mutex_unlock(&timeLock);

	// all tellers are available
	available[n]=true;

	do{

		// every teller has to get permisson to service from scheduler by using scheduleLock
		pthread_mutex_lock(&scheduleLock[n]);

		// servicing the client locks the output buffer
		pthread_mutex_lock(&bufferLockOut);

			// when the last client gets serviced, program finishes
			if(finish){
				pthread_mutex_unlock(&bufferLockOut);
				available[n]=true;
				sem_post(empty);
				break;
			}
			current=buffer[out];
			out=(out+1)%BUFFER_SIZE;
			numOfServicedClients++;
			if(numOfServicedClients>=numOfClients)
				finish=true;

		// release the lock
		pthread_mutex_unlock(&bufferLockOut);

		// keeps the reserved seat 
		reservedSeat=reserve(current.seatNumber-1)+1;

		// service time
		usleep(current.serviceTime*1000);

		// output message for who gets which seats by which teller
		if(reservedSeat==0){
			pthread_mutex_lock(&writeLock);
			output<<current.name<<" requests seat " <<current.seatNumber
			<<", reserves None. Signed by Teller "<<teller<<"."<<endl;
			pthread_mutex_unlock(&writeLock);
		}else{
			pthread_mutex_lock(&writeLock);
			output<<current.name<<" requests seat " <<current.seatNumber
			<<", reserves seat "<<reservedSeat<<". Signed by Teller "<<teller<<"."<<endl;
			pthread_mutex_unlock(&writeLock);
		}

		// tellers is availbe again
		available[n]=true;

		// lets the new client to be in line
		sem_post(empty);
		
	// if ever client does not get serviced, keep giving the service
	}while(numOfServicedClients<numOfClients);
	
	pthread_exit(NULL);
}

// after waiting for their arrival times, they get into line when there is space in the line
void *CLIENT(void* param){
	client* customer=((client *)param);
	usleep(customer->arrivalTime);
	usleep(customer->arrivalTime*1000);

	sem_wait(empty);
	pthread_mutex_lock(&bufferLockIn);
		buffer[in]=*customer;
		in=(in+1)%BUFFER_SIZE;
	pthread_mutex_unlock(&bufferLockIn);
	sem_post(full);
	pthread_exit(NULL);
}

// it assigns clients to tellers until every clients get assigned to a teller
void *SCHEDULER(void* param){
	int scheduledClients=0;

	while(scheduledClients<numOfClients){
		sem_wait(full);
		pthread_mutex_unlock(&scheduleLock[AVAILABLE()]);
		scheduledClients++;
	}
	pthread_mutex_unlock(&scheduleLock[0]);
	pthread_mutex_unlock(&scheduleLock[1]);
	pthread_mutex_unlock(&scheduleLock[2]);
	pthread_exit(NULL);
}
// reserve the seat the client wants, unless it is already taken.
int reserve(int seatNum){
	
	if(seatNum<numOfSeats){
		pthread_mutex_lock(&seatLock[seatNum]);
		if(!seat[seatNum]){
			seat[seatNum]=true;
			pthread_mutex_unlock(&seatLock[seatNum]);
			return seatNum;
		}
		pthread_mutex_unlock(&seatLock[seatNum]);
	}
		
	for (int i = 0; i < numOfSeats; ++i)
	{		
			pthread_mutex_lock(&seatLock[i]);
			if(!seat[i]){
				seat[i]=true;
				pthread_mutex_unlock(&seatLock[i]);
				return i;
			}
			pthread_mutex_unlock(&seatLock[i]);
	}

	
	return -1;
}

int AVAILABLE(){
	int i;
	for (i = 0; i < TELLERS_NUM; ++i)
	{
		if (available[i])		
		{	
			available[i]=false;
			break;
		}
	}
	return i;
}

void readFile(string path){
	map <string,int> saloons;
	saloons["OdaTiyatrosu"]=60;
	saloons["UskudarStudyoSahne"]=80;
	saloons["KucukSahne"]=200;
	fstream fin;
	fin.open(path,ios::in);
	string line,token;
	getline(fin,line);
	stringstream splitline(line);
    splitline >> token;
	numOfSeats=saloons.at(token);
	getline(fin,line);
	numOfClients=stoi(line);
	clients=new client[numOfClients];
	for(int i=0;i<numOfClients ;++i){
		getline(fin, line);
	 	stringstream splitline(line);
           for(int j=0;getline(splitline,token,',');j++)
            {	
            	if (j==0)
            	{
            		clients[i].name=token;
            	}else if (j==1)
            	{
            		clients[i].arrivalTime=stoi(token);

            	}else if (j==2)
            	{
            		clients[i].serviceTime=stoi(token);
            	}else{
            		clients[i].seatNumber=stoi(token);
            	}
            }
	 }
}
void createMutex(pthread_mutex_t* mutexName, int numOfMutexes){
	for (int i = 0; i < numOfMutexes; ++i)
	{
		pthread_mutex_init(&mutexName[i], NULL);
	}
}
void lockMutex(pthread_mutex_t* mutexName, int numOfMutexes){
	for (int i = 0; i < numOfMutexes; ++i)
	{
		pthread_mutex_lock(&mutexName[i]);
	}
}





