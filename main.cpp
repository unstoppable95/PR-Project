#include <stdio.h>
#include <mpi.h>
#include <pthread.h>
#include <iostream>
#include <algorithm> 
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <queue>
#define nArbiter 2
#define MYTAG 100

using namespace std;

/*
	tablica message
	0 - sender_id
	1 - tag
	2 - timestamp

	TAGI:
	10 - zapytanie o arbita
	20 - odpowiedz do arbitra
	21 - odpowiedz OK
	22 - nie OK?
*/

enum tags
{
    TAG_ARB_QUE = 10, 
    TAG_ARB_ANS_OK = 20,
    TAG_ARB_ANS_NO = 30
};

int size, myrank;
int arbiter = nArbiter;
int lamportClock = 0;
int nAgree = 0;
bool want = false; 
queue <int> myQueue;
int clockWhenStart;

void *receive_loop(void * arg);
void clockUpdate(int valueFromMsg);
void clockUpdate();

pthread_mutex_t	receive_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	send_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t	clock_mutex = PTHREAD_MUTEX_INITIALIZER;


int main(int argc, char **argv)
{
	// Enable thread in MPI 
	int provided = 0;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	//MPI_Init(&argc, &argv);

	//Create thread
	pthread_t receive_thread;
	pthread_create(&receive_thread, NULL, receive_loop, 0);
	
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	int message[3];
	srand(myrank);
	int delay;

	while(1)
	{
		delay = rand() % 5;
		sleep(delay);
		printf("%d:%d\t\tDelay =  %d\n", lamportClock, myrank, delay);
		want = true;
		for (int i = 0; i < size; i++)
		{
			if (i != myrank)
			{
				clockUpdate();
				message[0] = myrank;
				message[1] = TAG_ARB_QUE;
				message[2] = lamportClock;
				MPI_Send(&message, 3, MPI_INT, i, MYTAG, MPI_COMM_WORLD);
				if (i == 0) clockWhenStart = lamportClock;
				printf("%d:%d\t\tWyslalem do %d\n", lamportClock, myrank, i);
				//cout << lamportClock << ":" << myrank << "\t\tWyslalem do " << i << "\n" << flush;
			}	
		}
			
		while (nAgree < size - arbiter) 
		{
			printf("%d:%d\t\tZa malo zgod\n", lamportClock, myrank);
			//cout << lamportClock << ":" << myrank << "\t\t Za malo zgod\n";
			sleep(1);
		}
		printf("%d:%d\t\tCHLANIE! Zgody = %d\n", lamportClock, myrank, nAgree);
		sleep(10);
		printf("%d:%d\t\tKONIEC CHLANIA!\n", lamportClock, myrank);

		want = false;	
		nAgree = 0;
		while(!myQueue.empty())
		{
			clockUpdate();
			message[0] = myrank;
			message[1] = TAG_ARB_ANS_OK;
			message[2] = lamportClock;
			MPI_Send(&message, 3, MPI_INT, myQueue.front(), MYTAG, MPI_COMM_WORLD);
			printf("%d:%d\t\tWyslalem zgode z kolejki do %d\n", lamportClock, myrank, myQueue.front());
			myQueue.pop();
		};

		sleep(5);
	};
		
	MPI_Finalize();
}

void *receive_loop(void * arg) {
	printf("%d:%d\t\treceive loop\n", lamportClock, myrank);
	//cout << lamportClock << ":" <<myrank << "\t\treceive loop" << "\n" << flush;
	MPI_Status status;
	int message[3];
	while (1)
	{
		MPI_Recv(&message, 3, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		clockUpdate(message[2]);
		switch (message[1])
		{
			case TAG_ARB_QUE:
				printf("%d:%d\t\tOdebralem\n", lamportClock, myrank);
				//cout << lamportClock << ":" << myrank << "\t\tOdebralem\n" << flush;
				clockUpdate(message[2]);
				if (want == false)
				{
					message[0] = myrank;
					message[1] = TAG_ARB_ANS_OK;
					message[2] = lamportClock;
					MPI_Send(&message, 3, MPI_INT, status.MPI_SOURCE, MYTAG, MPI_COMM_WORLD);
					printf("%d:%d\t\tWyslalem z czasem\n", lamportClock, myrank);
				}
				else
				{
					printf("HERE\t\t%d\n", status.MPI_SOURCE);
					//if(message[2] < lamportClock)
					if(message[2] + status.MPI_SOURCE < clockWhenStart + myrank)
					{
						message[0] = myrank;
						message[1] = TAG_ARB_ANS_OK;
						message[2] = lamportClock;
						MPI_Send(&message, 3, MPI_INT, status.MPI_SOURCE, MYTAG, MPI_COMM_WORLD);
						printf("%d:%d\t\tWyslalem z czasem\n", lamportClock, myrank);
					}
					else
					{
						//Wstrzymaj
						printf("%d:%d\t\tOdlozylem na kolejke\n", lamportClock, myrank);
						myQueue.push(status.MPI_SOURCE);
					}
				}
				//cout << lamportClock << ":" << myrank << "\t\tWyslalem z czasem\n" << flush;
				break;

			case TAG_ARB_ANS_OK:
				clockUpdate();
				nAgree += 1;
				printf("%d:%d\t\tOdebralem zgode(%d/%d)\n", lamportClock, myrank, nAgree, size - 1);
				//cout << lamportClock << ":" << myrank << "\t\tOdebralem zgode ("<< nAgree << "/" << size - 1 << ")\n" << flush;
				break;
		}
	}
	//cout << lamportClock << ":" <<myrank << "\treceive loop" << "\n";
}

void clockUpdate(int valueFromMsg) {
	pthread_mutex_lock(&clock_mutex);
	lamportClock = max(lamportClock, valueFromMsg) + 1;
	pthread_mutex_unlock(&clock_mutex);
}

void clockUpdate() {
	pthread_mutex_lock(&clock_mutex);
	lamportClock += 1;
	pthread_mutex_unlock(&clock_mutex);
}

