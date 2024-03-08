/* File:
 *    parte3.c
 *
 *
 *
 * Compile:  mpicc -g -Wall -o parte3 parte3.c -lpthread -lrt
 * Usage:    mpiexec -n 3 ./parte3
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>
#include <mpi.h>

#define BUFFER_SIZE 6 // Númermo máximo de tarefas enfileiradas

// Não precisa de relogio aleatorio
// Thread mensageira

typedef struct Clock
{
    int p[3];
} Clock;

typedef struct msg
{
    int remetente;
    int destinatario;
    Clock clock;
} Msg;

Clock qEntrada[BUFFER_SIZE];
Msg qSaida[BUFFER_SIZE];

Clock clockMain = {{0, 0, 0}};

int qEntradaCount = 0;
int qSaidaCount = 0;

pthread_mutex_t mutexEntrada;
pthread_mutex_t mutexSaida;

pthread_cond_t condFullEntrada;
pthread_cond_t condEmptyEntrada;

pthread_cond_t condFullSaida;
pthread_cond_t condEmptySaida;

void printarRelogio(Clock *clock, int pid, int action)
{
    switch (action)
    {
    case 1:
        printf("[EVENT]     Processo: %d | Relogio: (%d, %d, %d)\n", pid, clock->p[0], clock->p[1], clock->p[2]);
        break;
    case 2:
        printf("[SEND]      Processo: %d | Relogio: (%d, %d, %d)\n", pid, clock->p[0], clock->p[1], clock->p[2]);
        break;
    case 3:
        printf("[RECEIVE]   Processo: %d | Relogio: (%d, %d, %d)\n", pid, clock->p[0], clock->p[1], clock->p[2]);
        break;
    default:
        break;
    }
}

void Event(int pid, Clock *clock)
{
    clock->p[pid]++;
    printarRelogio(&clockMain, pid, 1);
}

void Send(int remetente, int destinatario)
{
    pthread_mutex_lock(&mutexSaida);
    clockMain.p[remetente]++;
    printarRelogio(&clockMain, remetente, 2);
    while (qSaidaCount == BUFFER_SIZE)
    {
        pthread_cond_wait(&condFullSaida, &mutexSaida);
    }

    Msg *msg = (Msg *)malloc(sizeof(Msg));
    msg->clock = clockMain;
    msg->remetente = remetente;
    msg->destinatario = destinatario;

    qSaida[qSaidaCount] = *msg;
    qSaidaCount++;

    pthread_mutex_unlock(&mutexSaida);
    pthread_cond_signal(&condEmptySaida);
}

void Receive(int pid)
{
    pthread_mutex_lock(&mutexEntrada);
    clockMain.p[pid]++;
    while (qEntradaCount == 0)
    {
        pthread_cond_wait(&condEmptyEntrada, &mutexEntrada);
    }

    Clock clock = qEntrada[0];

    for (int i = 0; i < qEntradaCount; i++)
    {
        qEntrada[i] = qEntrada[i + 1];
    }
    qEntradaCount--;

    for (int i = 0; i < 3; i++)
    {
        if (clock.p[i] > clockMain.p[i])
        {
            clockMain.p[i] = clock.p[i];
        }
    }

    printarRelogio(&clockMain, pid, 3);
    pthread_mutex_unlock(&mutexEntrada);
    pthread_cond_signal(&condFullEntrada);
}

void submitSaida()
{
    pthread_mutex_lock(&mutexSaida);

    while (qSaidaCount == 0)
    {
        pthread_cond_wait(&condEmptySaida, &mutexSaida);
    }

    int *resultados;
    resultados = calloc(3, sizeof(int));

    Msg resposta = qSaida[0];
    for (int i = 0; i < qSaidaCount - 1; i++)
    {
        qSaida[i] = qSaida[i + 1];
    }
    qSaidaCount--;

    for (int i = 0; i < 3; i++)
    {
        resultados[i] = resposta.clock.p[i];
    }

    MPI_Send(resultados, 3, MPI_INT, resposta.destinatario, resposta.remetente, MPI_COMM_WORLD);

    pthread_mutex_unlock(&mutexSaida);
    pthread_cond_signal(&condFullSaida);
}

void getEntrada(long myPid)
{
    int *resultados;
    resultados = calloc(3, sizeof(int));
    Clock *clock = (Clock *)malloc(sizeof(Clock));
    MPI_Recv(resultados, 3, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    for (int i = 0; i < 3; i++)
    {
        clock->p[i] = resultados[i];
    }

    free(resultados);

    pthread_mutex_lock(&mutexEntrada);

    while (qEntradaCount == BUFFER_SIZE)
    {
        pthread_cond_wait(&condFullEntrada, &mutexEntrada);
    }

    qEntrada[qEntradaCount] = *clock;
    qEntradaCount++;

    pthread_mutex_unlock(&mutexEntrada);
    pthread_cond_signal(&condEmptyEntrada);
}

void *startThreadEntrada(void *args)
{
    long myPid = (long)args;
    int cont = 0;
    switch (myPid)
    {
    case 0:
        cont = 2;
        break;
    case 1:
        cont = 2;
        break;
    case 2:
        cont = 1;
        break;

    default:
        break;
    }
    // while (1 == 1)
    // {
    //     getEntrada(myPid);
    //     printf("LOOP DA MORTE IN\n");
    // }
    int i = 0;
    for (i; i < cont; i++)
    {
        getEntrada(myPid);
    }
    return NULL;
}

void *startThreadMeio(void *args)
{
    long p = (long)args;

    if (p == 0)
    {
        Event(0, &clockMain);
        Send(0, 1);
        Receive(0);
        Send(0, 2);
        Receive(0);
        Send(0, 1);
        Event(0, &clockMain);
    }

    if (p == 1)
    {
        // Event(1, &clockMain);
        Send(1, 0);
        Receive(1);
        Receive(1);
    }

    if (p == 2)
    {
        Event(2, &clockMain);
        Send(2, 0);
        Receive(2);
    }
    return NULL;
}

void *startThreadSaida(void *args)
{
    long myPid = (long)args;

    int cont = 0;

    switch (myPid)
    {
    case 0:
        cont = 3;
        break;
    case 1:
        cont = 1;
        break;
    case 2:
        cont = 1;
        break;

    default:
        break;
    }
    // while (1 == 1)
    // {
    //     submitSaida();
    //     printf("LOOP DA MORTE OUT\n");
    // }
    int i = 0;
    for (i; i < cont; i++)
    {
        submitSaida();
    }

    return NULL;
}

void createThreads(int n)
{
    pthread_t tEntrada;
    pthread_t tMeio;
    pthread_t tSaida;

    pthread_cond_init(&condEmptyEntrada, NULL);
    pthread_cond_init(&condEmptySaida, NULL);
    pthread_cond_init(&condFullSaida, NULL);
    pthread_cond_init(&condFullEntrada, NULL);
    pthread_mutex_init(&mutexEntrada, NULL);
    pthread_mutex_init(&mutexSaida, NULL);

    if (pthread_create(&tMeio, NULL, &startThreadMeio, (void *)n) != 0)
    {
        perror("Falha ao criar a thread");
    }

    if (pthread_create(&tEntrada, NULL, &startThreadEntrada, (void *)n) != 0)
    {
        perror("Falha ao criar a thread");
    }

    if (pthread_create(&tSaida, NULL, &startThreadSaida, (void *)n) != 0)
    {
        perror("Falha ao criar a thread");
    }

    if (pthread_join(tMeio, NULL) != 0)
    {
        perror("Falha ao dar join na thread");
    }

    if (pthread_join(tEntrada, NULL) != 0)
    {
        perror("Falha ao dar join na thread");
    }

    if (pthread_join(tSaida, NULL) != 0)
    {
        perror("Falha ao dar join na thread");
    }

    pthread_cond_destroy(&condEmptyEntrada);
    pthread_cond_destroy(&condEmptySaida);
    pthread_cond_destroy(&condFullSaida);
    pthread_cond_destroy(&condFullEntrada);
    pthread_mutex_destroy(&mutexEntrada);
    pthread_mutex_destroy(&mutexSaida);
}

int main(int argc, char *argv[])
{
    int my_rank;

    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    if (my_rank == 0)
        createThreads(0);

    else if (my_rank == 1)
        createThreads(1);

    else if (my_rank == 2)
        createThreads(2);

    MPI_Finalize();
    return 0;
}
