#define WIADOMOSC 210 //Maksymalna dlugosc wiadomosci - UWAGA UZYTKOWNIK MOZE WYSŁAĆ MAKSYMALNIE 180
#define WIADOMOSC_UZYTKOWNIKA 180
#define KLUCZ 1
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>

char id;
int kolejka;
long int shmid;
char * ptr;

struct msgbuf{             //mtype = argument wysylanie() + 3
    long mtype;           //typy specjalnego przeznaczenia: 1 - wiadomosci od klientow do serwera
    char mtext[210];     // 2- z serwera do niepolaczonych uzytkownikow, 3~3+USERS uzytkownikow, 3+USERS~3+USERS*2
    // tymczasowe niezalogowanych uzytkownikow
};

void wyloguj(int signum){
    struct msgbuf wiadomosc;
    strcpy(wiadomosc.mtext+1,"/lgout");
    wiadomosc.mtext[0]=ptr[0];
    mtype = 1;
    msgsnd(kolejka, (void *) &wiadomosc, sizeof(wiadomosc.mtext), IPC_NOWAIT);
    sleep(1);
    shmctl(shmid,IPC_RMID,0);
    exit(0);
}


void init(){
    struct msgbuf wiadomosc;
    wiadomosc.mtype = 1;
    signal(SIGINT, wyloguj);
    long int i = 1;
    do{
        shmid = shmget(i++, sizeof(char), 0666 | IPC_EXCL | IPC_CREAT ); //tworzenie unikalnej pamieci wspoldzielone
    }while (shmid == -1);
    ptr = shmat(shmid, 0, 0);
    kolejka = msgget(KLUCZ, 0);
    if(kolejka == -1){
        printf("Błąd. Brak serwera.\n");
        exit(1);
    }

    strcpy(wiadomosc.mtext,"/nuser"); //Uzyskiwanie tymczasowego id
    msgsnd(kolejka, (void *) &wiadomosc, sizeof(wiadomosc.mtext), IPC_NOWAIT);
    msgrcv(kolejka, (void *) &wiadomosc, sizeof(wiadomosc.mtext), 2, 1);
    id = wiadomosc.mtext[0];
    ptr[0] = id;
}


int main() {
    id = -1;
    init();
    int a = fork();
    if (!a) {
        while (1) {
            struct msgbuf wiadomosc;
            char send[WIADOMOSC];
            int i;
            i = 0;
            memset(send, 0, sizeof(send[0]) * WIADOMOSC);
            memset(wiadomosc.mtext,0,sizeof(wiadomosc.mtext));
            while ((send[++i] = getchar()) != '\n');
            send[i] = 0;
            send[0] = ptr[0];
            id = ptr[0];
            wiadomosc.mtype = 1;
            send[WIADOMOSC_UZYTKOWNIKA - 1] = '\0';
            memcpy(wiadomosc.mtext, send, sizeof(send));
            msgsnd(kolejka, (void *) &wiadomosc, sizeof(wiadomosc.mtext), IPC_NOWAIT);
        }
    }
    else {
        while (1) {
            struct msgbuf wiadomosc;
            char get[WIADOMOSC];
            memset(get, 0, sizeof(get[0]) * WIADOMOSC);
            memset(wiadomosc.mtext,0,sizeof(wiadomosc.mtext));
            msgrcv(kolejka, (void *) &wiadomosc, sizeof(wiadomosc.mtext), id +3, 1);
            memcpy(get, wiadomosc.mtext, sizeof(char) * WIADOMOSC);
            if(get[1] == -1){
                ptr[0]=get[0];
                id = get[0];
                printf("%s\n",get+2);
            }else
                printf("%s\n", get);
        }
    }
}
