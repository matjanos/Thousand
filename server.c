    /*****************************************************************************/
    /* 		Wersja przetestowana, ostateczna.									 */
    /* 					                                                   		 */
    /*****************************************************************************/

#define KLUCZ 1
#define USERS 3 //Wymagana ilość graczy
#define LOGIN 12 //Dlugosc loginu - stala
#define WIADOMOSC 210 //Maksymalna dlugosc wiadomosci - UWAGA UZYTKOWNIK MOZE WYSŁAĆ MAKSYMALNIE 180
#define WIADOMOSC_UZYTKOWNIKA 180

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>


char zalogowany[USERS]; //1 jesli uzytkownik jest zalogowany 0 jewli nie, nr w tablicy to identyfikator użytkownika
char login[USERS][LOGIN]; //Przechowuje nazwy użytkowników, nr w tablicy to identyfikator użytkownika
int pelnaSuma[USERS][LOGIN]; //Przechowuje punkty użytkownikow z pełnej rozgrywki (do 1000)
int suma[USERS][LOGIN]; //Przechowuje punkty użytkownikow z pojedynczej gry
char send[WIADOMOSC]; //Wiadomosc do wyslania
unsigned char get[WIADOMOSC]; //wiadomosc otrzymana
char temp_id[USERS]; //Tablica tymczasowych identyfiktorów

int kolejka;

struct msgbuf{             //mtype = argument wysylanie() + 3
    long mtype;           //typy specjalnego przeznaczenia: 1 - wiadomosci od klientow do serwera
    char mtext[210];     // 2- z serwera do niepolaczonych uzytkownikow, 3~3+USERS uzytkownikow, 3+USERS~3+USERS*2
                        // tymczasowe niezalogowanych uzytkownikow
};

struct cardStruct{        // value - wartość 
    long value;           //typy specjalnego przeznaczenia: 1 - wiadomosci od klientow do serwera
    char name[2];     // 2- z serwera do niepolaczonych uzytkownikow, 3~3+USERS uzytkownikow, 3+USERS~3+USERS*2
	char color[2];       // tymczasowe niezalogowanych uzytkownikow
};

struct msgbuf wiadomosc;



void wysylanie(char id){
    wiadomosc.mtype = id+3;
    send[WIADOMOSC-1] = '\0';
    memcpy(wiadomosc.mtext,send, sizeof(send));
    msgsnd(kolejka, (void *) &wiadomosc, sizeof(wiadomosc.mtext), IPC_NOWAIT);
}

/**
 * Przedzielanie tymczasowego identyfikatora na potrzby logowania
 *
 * Tymczasowy identyfikator z zakresu 10~20
 * |
 * |char o wartosci -1
 * ||
 * c
 *
 * Ten co mial porzednio ten identyfikator i  nie zalogowal sie pomyslnie
 * otrzymuje komunikat
 *
 * char o wartosci -2
 * |
 * |char o wartosci -1
 * ||
 *
 */
void nowyuser(){
	unsigned char i;
	for(i = 0; i<USERS;i++){
		if(temp_id[i] == 0){
			send[0] = i+USERS;
			send[1] = -1;
			temp_id[i] = 1;
			wysylanie(-1);
			return;
		}
	}
}

//Pobieranie
void pobieranie(){
    msgrcv(kolejka, (void *) &wiadomosc, sizeof(wiadomosc.mtext), 1, 1);
    memcpy(get,wiadomosc.mtext, sizeof(char)*WIADOMOSC_UZYTKOWNIKA);
 		printf("%d\n",get[0]);
		printf("%s\n",get+1);
}

void wylaczserwer(int esig){
    msgctl(kolejka,IPC_RMID,NULL);
    exit(1);
}

void init(){
    memset(zalogowany,0,sizeof(zalogowany[0])*USERS);
    memset(login, 0, sizeof(login[0][0]) * USERS * LOGIN);
    memset(send, 0, sizeof(send[0]) * WIADOMOSC);
    memset(get, 0, sizeof(get[0]) * WIADOMOSC);
    memset(temp_id,0,sizeof(temp_id[0])*USERS);
    kolejka = msgget(KLUCZ, 0666 | IPC_CREAT |IPC_EXCL);
    if(kolejka == -1){
        printf("Blad. Jeden serwer jest juz czynny\n");
        exit(1);
    }
    signal(SIGINT, wylaczserwer);
    strcpy(login[0],"marek"); //Lista użytkowników serwera
    strcpy(login[1],"Tomek");
    strcpy(login[2],"janek");
    return;
}

/*
 * 0 użytkownik pomyślnie wylogowany
 * 1 użytkownik nie był zalogowany
 */
int wylogowywanie(){
	if(zalogowany[get[0]]){
		zalogowany[get[0]] = 0;
		strcpy(send, "  Wylogowano pomyslnie.");
		nowyuser();
		return 0;
	}else
		strcpy(send, "Blad. Uzytkownik nie byl zalogowany.");
		return 1;
}

 /*return 1,gdy nie ma użytkownika w bazie
 * return 0,gdy pomyślnie zalogowano
 * return 2, gdy jest już zalogowany
 *
 * przykładowa informacja zwrotna pomiślnego logowania:
 *
 * id uzytkownika
 *  |
  * |char o wartosci -1
  * ||
 * "c Logowani pomyślne."
 */
int logowanie(){
	char Login[12];
	char* converted_get = (char*)(get);
	strncpy(Login, converted_get + 8, 12);
	int i;
	for(i = 0;i<USERS;i++){
		if(!strcmp(Login,login[i])){
			if(!zalogowany[i]) {
				send[0] = i;
				if(converted_get[0]>=USERS && converted_get[0]<2*USERS)
					temp_id[converted_get[0] - USERS] = '\0';
                else
                    wylogowywanie();
				send[0] = i;
				send[1] = -1;
                zalogowany[i] = 1;
				strcpy(send+2, "Logowanie pomyslne.");
				return 0;
			}else{
				strcpy(send,"Blad. Jestes juz zalogowany.");
				return 2;
			}
		}
	}
	strcpy(send,"Blad. Brak uzytkownika w bazie.");
	return 1;
}


/*
 *Zwraca listę zalogowanych uzytkownikow
 *
 *przykładowa wiadomość zwrotna:
 * "Zalogowani: Marek Zenon Tomasz\0"
 */
void listauzytkownikow(){
	int i;
	strcpy(send, "Zalogowani: ");
	for (i = 0; i < USERS; i++) {
		if(zalogowany[i]){
			strcat(send,login[i]);
			strcat(send," ");
		}
	}
}

/**
 * return 0 wysłano wiadomosc
 * return 1 brak takiego uzytkownika
 * return 2 uzytkownik niedostepny
 */

int wiadomoscuser(){
	char Login[12];
	strcpy(Login,login[get[0]]);
	char dLogin[12];
        int i = 0;
        do{
            dLogin[i] = *(get+strlen(Login)+i+3)==' '?0:*(get+strlen(Login)+i+3);
            i++;
        }while (dLogin[i-1]);
	char* converted_get = (char*)(get);
	for(i = 0; i<USERS;i++){
		if(!strcmp(dLogin,login[i])){
			if(zalogowany[i]){
				strcpy(send,Login);
				strcat(send,": ");
				strcat(send,converted_get + strlen(dLogin)+strlen(Login)+4);
				wysylanie(i);
				return 0;
			} else {
				strcpy(send,"Blad. ");
				strcat(send,dLogin);
				strcat(send, " jest niedostepny.");
				return 2;
			}
		}
	}
	strcpy(send,"Blad. Brak takiego uzytkownika.");
	return 1;
}

void wiadomoscwszyscy(){
	char* converted_get = (char*)(get);
	int i;
	char Login[12];
	strcpy(Login,login[get[0]]);
	strcpy(send,"Globalnie ");
	strcat(send,Login);
	strcat(send,": ");
	strcat(send,strlen(Login)+3+converted_get);
	for (i = 0; i<USERS;i++){
		if(zalogowany[i]){
			wysylanie(i);
		}
	}
}


/**
 * Return 0 jeśli nie musi wysyłać informacji zwrotnej
 * return 1 jeśli musi
 */
int wykonywanie(){
	char* converted_get = (char*)(get);
	char rozkaz[6];
        strncpy(rozkaz,converted_get,6);
	if(!strcmp(rozkaz, "/nuser")){
		nowyuser();
		return 0;
	}

	memcpy(rozkaz,converted_get+1, 6);
	if (!strcmp(rozkaz, "/login")) {
		logowanie();
		return 1;
	} else if (!strcmp(rozkaz, "/lgout")) {
		wylogowywanie();
		return 1;
	}else if (converted_get[0] >= USERS){
		strcpy(send,"Blad. Niezalogowano.");
		return 1;
	} else if (!strcmp(rozkaz, "/ulist")) {
		listauzytkownikow();
		return 1;
	} else if (!strcmp(rozkaz, "/muser")) {
		return wiadomoscuser();
	} else if (!strcmp(rozkaz, "/msall")) {
		wiadomoscwszyscy();
		return 0;
	}else{
		strcpy(send,"Blad. Polecenie niepoprawne.");
		return 1;
	}
}


/*
 * Przykladowa wiadomość od uzytkownika
 *  Identyfikator użytkownika, dodawany automatycznie przez program klienta, 1 char
 *  | UWAGA! MOŻE BYĆ TO 0
 *  |
 *  |
 *  |  _________rozkaz 6 charów
 *  | |   |
 *  | |   |
 *  "c/login Zenon\0"
 */


int main() {

    init();

	while (1) {

		memset(get, '\0', sizeof(char)*WIADOMOSC);
		memset(send, '\0', sizeof(char)*WIADOMOSC);
		pobieranie();
		if(wykonywanie()) {
			wysylanie(get[0]);
		}

	}
}
