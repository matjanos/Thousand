    /*****************************************************************************/
    /* 		Wersja przetestowana, ostateczna.									 */
    /* 					                                                   		 */
    /*****************************************************************************/

#define KLUCZ 1
#define USERS 10 //Maksymalna ilosc uzytkownikow
#define LOGIN 12 //Dlugosc loginu - stala
#define POKOJE 5 //Mozliwa ilosc pokojow
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


struct pokoj{
	char nazwa[LOGIN]; //nazwa pokoju
	char zajety[USERS];//czy dany uzytkownik jest dostepny
	char uzytkownicy[USERS];//lista identyfikatorów uzytkownika
};

char zalogowany[USERS]; //1 jesli uzytkownik jest zalogowany 0 jewli nie, nr w tablicy to identyfikator użytkownika
char login[USERS][LOGIN]; //Przechowuje nazwy użytkowników, nr w tablicy to identyfikator użytkownika
char send[WIADOMOSC];//Wiadomosc do wyslania
char get[WIADOMOSC];//wiadomosc otrzymana
char pokoj_dostepny[POKOJE];//czy dany pokoj jest dostepny nr w tablicy identyfikatorem pokoju
struct pokoj pokoje[POKOJE];//struktura pokoju numer w tablicy jest identyfikatorem pokoju
char temp_id[USERS]; //Tablica tymczasowych identyfiktorów

int kolejka;

struct msgbuf{             //mtype = argument wysylanie() + 3
    long mtype;           //typy specjalnego przeznaczenia: 1 - wiadomosci od klientow do serwera
    char mtext[210];     // 2- z serwera do niepolaczonych uzytkownikow, 3~3+USERS uzytkownikow, 3+USERS~3+USERS*2
                        // tymczasowe niezalogowanych uzytkownikow
};

struct msgbuf wiadomosc;



void wysylanie(char id){
    wiadomosc.mtype = id+3;
    send[WIADOMOSC-1] = '\0';
    memcpy(wiadomosc.mtext,send, sizeof(send));
    msgsnd(kolejka, (void *) &wiadomosc, sizeof(wiadomosc.mtext), IPC_NOWAIT);
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
    memset(pokoj_dostepny, 0, sizeof(pokoj_dostepny[0]) * POKOJE);
    memset(temp_id,0,sizeof(temp_id[0])*USERS);
   // memset(pokoje,0,sizeof(pokoje)*POKOJE);
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
	strncpy(Login,get + 8 , 12);
	int i;
	for(i = 0;i<USERS;i++){
		if(!strcmp(Login,login[i])){
			if(!zalogowany[i]) {
				send[0] = i;
				if(get[0]>=USERS &&get[0]<2*USERS)
					temp_id[get[0] - USERS] = 0;
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
 * 0 użytkownik pomyślnie wylogowany
 * 1 użytkownik nie był zalogowany
 */
int wylogowywanie(){
    int i;
    int j;
    for(i = 0; i<POKOJE;i++){
        for( j = 0; j<USERS;j++){
            if(get[0] == pokoje[i].uzytkownicy[j]){
                pokoje[i].zajety[j] = 0;
            }
        }
    }
	if(zalogowany[get[0]]){
		zalogowany[get[0]] = 0;
		strcpy(send, "  Wylogowano pomyslnie.");
		nowyuser();
		return 0;
	}else
		strcpy(send, "Blad. Uzytkownik nie byl zalogowany.");
		return 1;
}

/*
 *Zwraca listę pokojów
 *
 *przykładowa informacja zwrotna:
 * "Pokoje: pokoj_a pokoj_b pokoj_c\0"
 */

void listapokojow() {
	int i;
	strcpy(send, "Pokoje: ");
	for (i = 0; i < POKOJE; i++) {
		if(pokoj_dostepny[i]){
			strcat(send,pokoje[i].nazwa);
			strcat(send," ");
		}
	}
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
 * Funkcja wchodzenia do pokoju
 * return 0 wejście pomyślne
 * return 1 brak miejsca w pokoju
 * return 2 brak takiego pokoju
 * return 3 uzytkownik juz jest w pokoju
 */
int wejdzdopokoju() {
	char Pokoj[12];
	strncpy( Pokoj,get + 8, 12);
	int i;
	int j;
	int k = -1;
	for (i = 0; i < POKOJE; i++) {
		if (pokoj_dostepny[i]&&!strcmp(Pokoj,pokoje[i].nazwa)) {
				for(j = 0;j<USERS;j++){
					if(!pokoje[i].zajety[j]){
						k = j;
					}else if(get[0] == pokoje[i].uzytkownicy[j]){
						strcpy(send,"Uzytkownik jest juz w pokoju.");
						return 4;
					}
				}
				if(k>-1) {
					pokoje[i].zajety[k] = 1;
					pokoje[i].uzytkownicy[k] = get[0];
					strcpy(send,"Pomyslnie dołączono do pokoju: ");
                    strcat(send,Pokoj);
					return 0;
				}else{
					strcpy(send, "Blad brak miejsca w pokoju.");
					return 1;
				}
		}
	}
	strcpy(send,"Blad. Brak takiego pokoju.");
	return 2;
}


/**
 * Funkcja wchodzenia do pokoju
 * return 0 wyjście pomyślne
 * return 1 użytkownik nie był w pokoju
 * return 2 brak takiego pokoju
 */
int wyjdzzpokoju() {
	char Pokoj[12];
	strncpy(Pokoj,get + 8,  12);
	int i,j,k;
	for (i = 0; i < POKOJE; i++) {
		if (pokoj_dostepny[i] && !strcmp(Pokoj,pokoje[i].nazwa)) {
				for(j = 0;j<USERS;j++){
					if(pokoje[i].zajety[j] && pokoje[i].uzytkownicy[j] == get[0]){
						pokoje[i].zajety[j] = 0;
                        for(j = 0;j<USERS;(j++) & (k+=pokoje[i].zajety[j]));
                        if(!k){
                            pokoj_dostepny[i] = 0;
                        }
						strcpy(send,"Pomyslnie opuszczono pokoj: ");
						strcat(send,pokoje[i].nazwa);
						return 0;
					}
				}
				strcpy(send,"Blad. Użytkownik nie byl w pokoju.");
				return 1;
		}
	}
	strcpy(send,"Blad. Brak takiego pokoju.");
	return 2;
}


/**
 * return 0 pomyślnie dodano
 * return 1 Pokój o podanej nazwie już występuje
 * return 2 Maksymalna ilość pokojów
 *
 */
int utworzpokoj(){
	char Pokoj[12];
	strncpy( Pokoj, get + 8, 12);
	int i;
	int k = -1;
	for(i = 0;i<POKOJE;i++){
		if(pokoj_dostepny[i]&&!strcmp(pokoje[i].nazwa,Pokoj)){
			strcpy(send,"Blad. Pokoj o podanej nazwie juz wystepuje.");
			return 1;
		}else if(!pokoj_dostepny[i])
			k = i;
	}
	if(k>-1){
		strcpy(send,"Pomyslnie utworzono pokoj: ");
		pokoj_dostepny[k] = 1;
		memset(pokoje[k].nazwa,0,sizeof(char)*LOGIN);
		memset(pokoje[k].zajety,0,sizeof(char)*USERS);
		strcpy(pokoje[k].nazwa,Pokoj);
		strcat(send,Pokoj);
		return 0;
	}else{
		strcpy(send,"Blad. Osiqgnieto maksymalna liczbe pokojow.");
		return 2;
	}

}

/**
 * return 0 podaje liste uzytkownikow
 * return 1 brak takiego pokoju
 */
int listawpokoju(){
	char Pokoj[12];
	strncpy( Pokoj,get + 8, 12);
	int i;
	int j;
	for (i = 0; i < POKOJE; i++) {
		if (pokoj_dostepny[i] && !strcmp(Pokoj,pokoje[i].nazwa)) {
			strcpy(send,"Uzytkownicy w ");
			strcat(send,Pokoj);
			strcat(send,": ");
			for(j = 0;j<USERS;j++){
				if(pokoje[i].zajety[j]){
					strcat(send,login[pokoje[i].uzytkownicy[j]]);
					strcat(send, " ");
				}
			}
			return 0;
		}
	}
	strcpy(send,"Blad. Brak takiego pokoju.");
	return 1;
}


/**
 * return 0 wysłano wiadomosc
 * return 1 brak takiego pokoju
 */
int wiadomoscpokoj(){
	char Login[12];
	strcpy(Login,login[get[0]]);
	char Pokoj[12];
        int i = 0;
        do{
            Pokoj[i] = *(get+strlen(Login)+i+3)==' '?0:*(get+strlen(Login)+i+3);
            i++;
        }while (Pokoj[i-1]);
	int j;
	for (i = 0; i < POKOJE; i++) {
		if (pokoj_dostepny[i] && !strcmp(Pokoj,pokoje[i].nazwa)) {
			strcpy(send,Login);
			strcat(send,"(");
			strcat(send,Pokoj);
			strcat(send,"):");
            strcat(send,get + strlen(Pokoj)+strlen(Login)+3);
			for(j = 0;j<USERS;j++){
				if(pokoje[i].zajety[j]){
					wysylanie(pokoje[i].uzytkownicy[j]);
				}
			}
			return 0;
		}
	}
	strcpy(send,"Blad. Brak takiego pokoju.");
	return 1;
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

	for(i = 0; i<USERS;i++){
		if(!strcmp(dLogin,login[i])){
			if(zalogowany[i]){
				strcpy(send,Login);
				strcat(send,": ");
				strcat(send,get + strlen(dLogin)+strlen(Login)+4);
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
	int i;
	char Login[12];
	strcpy(Login,login[get[0]]);
	strcpy(send,"Globalnie ");
	strcat(send,Login);
	strcat(send,": ");
	strcat(send,strlen(Login)+3+get);
	for (i = 0; i<USERS;i++){
		if(zalogowany[i]){
			wysylanie(i);
		}
	}
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
	char i;
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


/**
 * Return 0 jeśli nie musi wysyłać informacji zwrotnej
 * return 1 jeśli musi
 */
int wykonywanie(){
	char rozkaz[6];
        strncpy(rozkaz,get,6);

	if(!strcmp(rozkaz, "/nuser")){
		nowyuser();
		return 0;
	}

	memcpy(rozkaz,get+1, 6);
	if (!strcmp(rozkaz, "/login")) {
		logowanie();
		return 1;
	} else if (!strcmp(rozkaz, "/lgout")) {
		wylogowywanie();
		return 1;
	}else if (get[0] >= USERS){
		strcpy(send,"Blad. Niezalogowano.");
		return 1;
	} else if (!strcmp(rozkaz, "/rlist")) {
		listapokojow();
		return 1;
	} else if (!strcmp(rozkaz, "/ulist")) {
		listauzytkownikow();
		return 1;
	} else if (!strcmp(rozkaz, "/eroom")) {
		wejdzdopokoju();
		return 1;
	} else if (!strcmp(rozkaz, "/lroom")) {
		wyjdzzpokoju();
		return 1;
	} else if (!strcmp(rozkaz, "/croom")) {
		utworzpokoj();
		return 1;
	} else if (!strcmp(rozkaz, "/uroom")) {
		listawpokoju();
		return 1;
	} else if (!strcmp(rozkaz, "/mroom")) {
		return wiadomoscpokoj();
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
