    /*****************************************************************************/
    /* 		Wersja alfa															 */
    /* 					                                                   		 */
    /*****************************************************************************/

#define KLUCZ 1
#define USERS 3 //Wymagana ilość graczy
#define WAITING_USERS 10 //Wymagana ilość graczy
#define LOGIN 12 //Dlugosc loginu - stala
#define WIADOMOSC 210 //Maksymalna dlugosc wiadomosci - UWAGA UZYTKOWNIK MOZE WYSŁAĆ MAKSYMALNIE 180
#define WIADOMOSC_UZYTKOWNIKA 180
#define KARTY_W_TALII 24

/*
FAZY GRY
*/
#define OCZEKIWANIE_NA_GRACZY 0
#define LICYTACJA 1
#define ROZDAWANIE_MUSU 2
#define OCZEKIWANIE_NA_RZUCENIE_KARTY 3
#define GRA_ZAKONCZONA 4

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>


char zalogowany[USERS]; //1 jesli uzytkownik jest zalogowany 0 jewli nie, nr w tablicy to identyfikator użytkownika
char login[USERS][LOGIN]; //Przechowuje nazwy użytkowników, nr w tablicy to identyfikator użytkownika
unsigned int pelnaSuma[USERS]; //Przechowuje punkty użytkownikow z pełnej rozgrywki (do 1000)
unsigned int suma[USERS]; //Przechowuje punkty użytkownikow z pojedynczej gry
char send[WIADOMOSC]; //Wiadomosc do wyslania
unsigned char get[WIADOMOSC]; //wiadomosc otrzymana
char temp_id[WAITING_USERS]; //Tablica tymczasowych identyfiktorów

int kolejka; // id kolejki komunikatów

int musToken = 0; // id gracza który jest "na musie"
int turnToken = 0; // id gracza na którego kartę czekam
int fazaGry = OCZEKIWANIE_NA_GRACZY;

struct msgbuf{             //mtype = argument wysylanie() + 3
    long mtype;           //typy specjalnego przeznaczenia: 1 - wiadomosci od klientow do serwera
    char mtext[210];     // 2- z serwera do niepolaczonych uzytkownikow, 3~3+USERS uzytkownikow, 3+USERS~3+USERS*2
                        // tymczasowe niezalogowanych uzytkownikow
};

struct cardStruct{
    short value;           	// value: wartość 
    char name[3];     		// nazwa: dla 9,10, Q, K, A, J
	char color[2];      	// kolor: pik - P, karo - K, trefl - T, kier(serce) -  S
};

struct msgbuf wiadomosc;

struct cardStruct talia[KARTY_W_TALII];

struct cardStruct reka[USERS][(KARTY_W_TALII)/USERS]; // karty na ręce graczy
struct cardStruct mus[3]; // karty na musie

void wylaczserwer(int esig){
	msgctl(kolejka,IPC_RMID,NULL);
	exit(1);
}

struct cardStruct createCard(short value, char* name, char* color){
	struct cardStruct karta;
	karta.value = value;
	strcpy(karta.name, name);
	strcpy(karta.color, color);

	return karta;
}

void shuffle(struct cardStruct *array, size_t n)
{
    srand(time(NULL));
	if (n > 1) 
	{
		size_t i;
		for (i = 0; i < n - 1; i++) 
		{
			size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
			struct cardStruct t = array[j];
			array[j] = array[i];
			array[i] = t;
		}
	}
}

void stworzTalie(){
	talia[0] = createCard(0, "9","P");
	talia[1] = createCard(0, "9","K");
	talia[2] = createCard(0, "9","T");
	talia[3] = createCard(0, "9","S");

	talia[4] = createCard(10, "10","P");
	talia[5] = createCard(10, "10","K");
	talia[6] = createCard(10, "10","T");
	talia[7] = createCard(10, "10","S");

	talia[8] = createCard(2, "J","P");
	talia[9] = createCard(2, "J","K");
	talia[10] = createCard(2, "J","T");
	talia[11] = createCard(2, "J","S");

	talia[12] = createCard(3, "Q","P");
	talia[13] = createCard(3, "Q","K");
	talia[14] = createCard(3, "Q","T");
	talia[15] = createCard(3, "Q","S");

	talia[16] = createCard(4, "K","P");
	talia[17] = createCard(4, "K","K");
	talia[18] = createCard(4, "K","T");
	talia[19] = createCard(4, "K","S");

	talia[20] = createCard(11, "A","P");
	talia[21] = createCard(11, "A","K");
	talia[22] = createCard(11, "A","T");
	talia[23] = createCard(11, "A","S");

	for(int i = 0; i< KARTY_W_TALII; i++){
		printf("%s - %s - %d\n", talia[i].name, talia[i].color, talia[i].value);
	}
}


void init(){
	memset(zalogowany,0, sizeof(zalogowany[0])*USERS);
	memset(login, 0, sizeof(login[0][0]) * USERS * LOGIN);
	memset(send, 0, sizeof(send[0]) * WIADOMOSC);
	memset(get, 0, sizeof(get[0]) * WIADOMOSC);
	memset(temp_id,0, sizeof(temp_id[0])*WAITING_USERS);
	memset(talia, 0, sizeof(talia[0])*KARTY_W_TALII);
	memset(reka, 0, sizeof(reka[0][0])*(KARTY_W_TALII-3));
	memset(mus, 0, sizeof(mus[0])*3);

	kolejka = msgget(KLUCZ, 1000 | IPC_CREAT |IPC_EXCL);
	if(kolejka == -1){
		printf("Blad. Jeden serwer jest juz czynny\n");
		exit(1);
	}
	signal(SIGINT, wylaczserwer);
	
	// ustaw początkową listę graczy
	int i;
	for(i=0;i<USERS;i++){
    	strcpy(login[i],"");
    	zalogowany[i] = -1;
    }

	stworzTalie();

    return;
}

void wysylanie(char id){
	wiadomosc.mtype = id+3;
	send[WIADOMOSC-1] = '\0';
	memcpy(wiadomosc.mtext,send, sizeof(send));
	msgsnd(kolejka, (void *) &wiadomosc, sizeof(wiadomosc.mtext), IPC_NOWAIT);
}

/**
 * Przedzielanie tymczasowego identyfikatora na potrzby logowania
 *
 * Tymczasowy ID jest z zakresu {USERS} - {WAITING_USERS+USERS}
 */
 void nowyuser(){
 	unsigned char i;
 	for(i = 0; i<WAITING_USERS;i++){
 		if(temp_id[i] == 0){
 			send[0] = i+USERS;
 			send[1] = -1;
 			temp_id[i] = 1;
 			wysylanie(-1);
 			return;
 		}
 	}

	strcpy(send,"Przekroczono ilosc oczekujacych graczy.");
	wysylanie(-1);
	printf("Przekroczono ilosc oczekujacych.");
 }

//Pobieranie
 void pobieranie(){
 	msgrcv(kolejka, (void *) &wiadomosc, sizeof(wiadomosc.mtext), 1, 1);
 	memcpy(get,wiadomosc.mtext, sizeof(char)*WIADOMOSC_UZYTKOWNIKA);
 	printf("%s\n",get+1);
 }

int kompletGraczy(){
	int i;
	for(i=0;i<USERS;i++){
		if(zalogowany[i]==-1)
			return 0;
	}
	return 1;
}

/*
 * 0 użytkownik pomyślnie wylogowany
 * 1 użytkownik nie był zalogowany
 */
 int wylogowywanie(){
 	int i;
 	for(i=0; i<USERS;i++){
	 	if(zalogowany[i]==get[0]){
	 		zalogowany[i] = -1;
	 		strcpy(send, "  Wylogowano pomyslnie.");
	 		nowyuser();
	 		return 0;
	 	}
 	}
 	strcpy(send, "Blad. Uzytkownik nie byl zalogowany.");
 	return 1;
 }


 void noweRozdanie(){
 	shuffle(talia, KARTY_W_TALII);

 	// kto w tym rozdaniu jes tna musie?
 	musToken = (musToken+1)%USERS;
 	
 	//kto pierwszy kładzie kartę / licytuje?
 	turnToken = musToken;


 	int i; //enumerator idący po kartach w danej kupce.
 	int j=0; //enumerator idący po kartach w talii.
 	int z=0; //enumerator idący po graczach

 	//na mus
 	for(i=0;i<3;i++){
 		mus[i] = talia[j++];
 	}

 	//na reke
 	for(i=0;i<(KARTY_W_TALII-3)/USERS;i++){
 		for(z=0;z<USERS;z++){
 			reka[z][i] = talia[j++];
 		}
 	}

 	for(z=0;z<USERS;z++){
 		printf("user %s:\n", login[z]);
 		for(i=0;i<(KARTY_W_TALII-3)/USERS;i++){
 			printf("%s-%s ",reka[z][i].name, reka[z][i].color);
 		}
 		printf("\n\n");
 	}

 	printf("mus:\n");
 	for(i=0;i<3;i++){
 		printf("%s-%s ",mus[i].name, mus[i].color);
 	}
 	printf("\n---------\n");
 	printf("Obowiązkowy mus: %s\n",login[musToken]);
 }

 /*
 * return 0,gdy pomyślnie zalogowano
 * return 1, gdy jest już zalogowany
 * return 2, gdy nie ma już miejsc
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
 	char Login[LOGIN];
 	char* converted_get = (char*)(get);
 	int clientId = converted_get[0];
 	strncpy(Login, converted_get + 8, LOGIN);
 	printf("loging %s, clientId=  %d \n", Login, clientId);
 	int i;
 	int freeSlot = -1;

 	for(i=0;i<USERS;i++){
 		if(zalogowany[i]==-1 && freeSlot == -1){
 			freeSlot = i;
 		}
 		else if(strcmp(Login,login[i])==0){
			strcpy(send,"Blad. Jestes juz zalogowany.");
			return 1;
		}
 	}

	if(freeSlot==-1){
		strcpy(send,"Blad. Mamy juz komplet graczy.");
		return 2;
	}

	send[0] = clientId;
	send[1] = -1;
	zalogowany[freeSlot] = clientId;
	strcpy(login[freeSlot], Login);
	strcpy(send+2, "Logowanie pomyslne. Witaj ");
	strcat(send+2, login[freeSlot]);

	printf("%s at userId %d\n", login[freeSlot], freeSlot);

	//jeśli mamy komplet - zaczynamy rozgrywkę
	if(kompletGraczy()){
		noweRozdanie();
		fazaGry = LICYTACJA;
	}

	return 0;
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
 		if(zalogowany[i]!=-1){
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
 	char rozkaz[6];//rozkaz ma zawsze 6
 	strncpy(rozkaz,converted_get,6);
 	if(!strcmp(rozkaz, "/nuser")){
 		nowyuser();
 		return 0;
 	}
	memcpy(rozkaz,converted_get+1, 6);

	// czy jesteś zalogowany?
 	if (strcmp(rozkaz, "/login")){
 		int u;
 		for(u=0;u<USERS;u++)
 		{
 			if(zalogowany[u]==converted_get[0]){
	 			printf("Query from user id:%d\n", u);
 				break;
 			}
 		}
 		if(u==USERS){
	 		strcpy(send,"Blad. Niezalogowano.");
	 		return 1;
	 	}
	}


 	if(fazaGry==OCZEKIWANIE_NA_GRACZY){

	 	if (!strcmp(rozkaz, "/login")) {
	 		logowanie();
	 		return 1;
	 	} else if (!strcmp(rozkaz, "/lgout")) {
	 		wylogowywanie();
	 		return 1;
	 	} else if (!strcmp(rozkaz, "/ulist")) {
	 		listauzytkownikow();
	 		return 1;
	 	}
	}
	else if(fazaGry==LICYTACJA){
		if (!strcmp(rozkaz, "/muser")) {
	 		return wiadomoscuser();
	 	} else if (!strcmp(rozkaz, "/msall")) {
	 		wiadomoscwszyscy();
	 		return 0;
	 	}
 	}

 	strcpy(send,"Blad. Polecenie niepoprawne.");
 	return 1;
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
