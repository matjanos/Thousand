    /*****************************************************************************/
    /* 		Wersja alfa															 */
    /* 					                                                   		 */
    /*****************************************************************************/

#define KLUCZ 1
#define USERS 3 //Wymagana ilość graczy
#define WAITING_USERS 10 //Wymagana ilość graczy
#define LOGIN 12 //Dlugosc loginu - stala
#define WIADOMOSC 510 //Maksymalna dlugosc wiadomosci - UWAGA UZYTKOWNIK MOZE WYSŁAĆ MAKSYMALNIE 180
#define WIADOMOSC_UZYTKOWNIKA 480
#define KARTY_W_TALII 24

/*
FAZY GRY
*/
#define OCZEKIWANIE_NA_GRACZY 0
#define LICYTACJA 1
#define ROZDAWANIE_MUSU 2
#define OCZEKIWANIE_NA_RZUCENIE_KARTY 3
#define GRA_ZAKONCZONA 4

#define SPADE   "\u2660"
#define CLUB    "\u2663"
#define HEART   "\u2665"
#define DIAMOND "\u2666"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>


char zalogowany[USERS]; //jesli uzytkownik nie jest zalogowany 0;  jeśli jest, nr w tablicy to identyfikator użytkownika, a wartosc to nr procesu.
char login[USERS][LOGIN]; //Przechowuje nazwy użytkowników, nr w tablicy to identyfikator użytkownika
signed int pelnaSuma[USERS]; //Przechowuje punkty użytkownikow z pełnej rozgrywki (do 1000)
unsigned int suma[USERS]; //Przechowuje punkty użytkownikow z pojedynczej gry
unsigned int licytacja[USERS]; // Stawki podczas licytacji
char send[WIADOMOSC]; //Wiadomosc do wyslania
unsigned char get[WIADOMOSC]; //wiadomosc otrzymana
char temp_id[WAITING_USERS]; //Tablica tymczasowych identyfiktorów

int kolejka; // id kolejki komunikatów

int aktualnyMus = -1; // idGracza, ktory aktualnie musi ugrac
int musToken = 0; // id gracza który jest "na musie"
int turnToken = 0; // id gracza na którego ruch czekam
char trumf[5]; // kolor aktualnego trumfu


int fazaGry = OCZEKIWANIE_NA_GRACZY;

struct msgbuf{             //mtype = argument wysylanie() + 3
    long mtype;           //typy specjalnego przeznaczenia: 1 - wiadomosci od klientow do serwera
    char mtext[210];     // 2- z serwera do niepolaczonych uzytkownikow, 3~3+USERS uzytkownikow, 3+USERS~3+USERS*2
                        // tymczasowe niezalogowanych uzytkownikow
};

struct cardStruct{
    short value;           	// value: wartość 
    char name[3];     		// nazwa: dla 9, 10, Q, K, A, J
	char color[5];      	// kolor:
};

struct msgbuf wiadomosc;

struct cardStruct talia[KARTY_W_TALII];

struct cardStruct reka[USERS][10]; // karty na ręce graczy, 10 bo swoje po rozdaniu + mus
struct cardStruct mus[3]; // karty na musie
struct cardStruct stol[3]; // karty na stole, index to user

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
//gracz jak jest, -1 jak nie istnieje
int idGracza(int idProcesu){
	for (int i = 0; i < USERS; ++i)
	{
		if(zalogowany[i]==idProcesu)
			return i;
	}
	return -1;
}

char* drukujKarty(struct cardStruct karty[], int ile){
	char* karty_str = malloc(sizeof(char)*WIADOMOSC);
	strcpy(karty_str,"");
	for(int i = 0; i< ile; i++){
		char line[20];
		sprintf(line, "%d) %s %s \n", i, karty[i].name, karty[i].color);
		strcat(karty_str,line);
	}

	return karty_str;
}

void stworzTalie(){
	talia[0] = createCard(0, "9",SPADE);
	talia[1] = createCard(0, "9",DIAMOND);
	talia[2] = createCard(0, "9",CLUB);
	talia[3] = createCard(0, "9",HEART);

	talia[4] = createCard(10, "10",SPADE);
	talia[5] = createCard(10, "10",DIAMOND);
	talia[6] = createCard(10, "10",CLUB);
	talia[7] = createCard(10, "10",HEART);

	talia[8] = createCard(2, "J",SPADE);
	talia[9] = createCard(2, "J",DIAMOND);
	talia[10] = createCard(2, "J",CLUB);
	talia[11] = createCard(2, "J",HEART);

	talia[12] = createCard(3, "Q",SPADE);
	talia[13] = createCard(3, "Q",DIAMOND);
	talia[14] = createCard(3, "Q",CLUB);
	talia[15] = createCard(3, "Q",HEART);

	talia[16] = createCard(4, "K",SPADE);
	talia[17] = createCard(4, "K",DIAMOND);
	talia[18] = createCard(4, "K",CLUB);
	talia[19] = createCard(4, "K",HEART);

	talia[20] = createCard(11, "A",SPADE);
	talia[21] = createCard(11, "A",DIAMOND);
	talia[22] = createCard(11, "A",CLUB);
	talia[23] = createCard(11, "A",HEART);

	printf("%s",drukujKarty(talia,KARTY_W_TALII));
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
	memset(stol, 0, sizeof(stol[0])*3);

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
	// printf("Wysylam '%s' do %ld\n",send, wiadomosc.mtype);
	memcpy(wiadomosc.mtext,send, sizeof(send));
	msgsnd(kolejka, (void *) &wiadomosc, sizeof(wiadomosc.mtext), IPC_NOWAIT);
}

/**
 * return 0 wysłano wiadomosc
 */

 int wiadomoscuser(char* wiadomosc, int idGracza){
 	strcpy(send,wiadomosc);
	wysylanie(idGracza);
	return 0;
 }

 void wiadomoscwszyscy(char* wiadomosc){
 	int i;
 	strcpy(send,wiadomosc);

 	for (i = 0; i<USERS;i++){
 		if(zalogowany[i]){
 			wysylanie(zalogowany[i]);
 		}
 	}
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

 	// kto w tym rozdaniu jest na musie?
 	musToken = (musToken+1)%USERS;
 	printf("Nowy mus token: %d \n", musToken);
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
 		wiadomoscuser("Twoje karty:",zalogowany[z]);
 		wiadomoscuser(drukujKarty(reka[z],(KARTY_W_TALII-3)/USERS),zalogowany[z]);
 	}

 	printf("mus:\n");
 	for(i=0;i<3;i++){
 		printf("%s-%s ",mus[i].name, mus[i].color);
 	}
 	printf("\n---------\n");
 	printf("Obowiązkowy mus: %s\n",login[musToken]);

 	licytacja[musToken] =100;
 	strcpy(trumf, "");//czyscimy trumf

 	turnToken = (musToken+1)%USERS;
 	char message[WIADOMOSC];
 	strcpy(message,"Rozpoczynamy licytacje. Teraz: ");
 	strcat(message,login[turnToken]);
	
	fazaGry = LICYTACJA;
 	wiadomoscwszyscy(message);
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
	}

	return 0;
 }

void dodajKarty(struct cardStruct* karty, int currentSize, struct cardStruct* dodane, int newSize){
	memcpy(karty + currentSize, dodane, newSize * sizeof(dodane[0]));
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

int incrementTurn(){
	do{
		turnToken = (turnToken+1)%USERS;
		printf("turn %d\n", licytacja[turnToken]);
	}
	while(licytacja[turnToken]==-1);

	return turnToken;
}

int maxbet(){
	int max = 0;
	for (int i = 0; i < USERS; ++i)
	{
		if(licytacja[i] != -1 && licytacja[i]>max)
			max = licytacja[i];
	}
	printf("maxbet - %d\n", max);
	return max;
}

 void podbijStawke(){
	char stawka[3];
 	char* converted_get = (char*)(get);
 	int clientId = converted_get[0];
 	strncpy(stawka, converted_get + 8, 3);

 	int stawka_int = atoi(stawka);
 	int min_stawka = maxbet()+10;//nie możemy skakać z licytacją co 1, ale co 10.
 	if(stawka_int==0){
 		wiadomoscuser("To blad argumentu. To nie jest liczba lub grasz 0.",clientId);
 		return;
 	}else if(stawka_int<min_stawka){
 		char msg[WIADOMOSC];
	 	strcpy(msg,"Za malo. Minimalnie daj ");
	 	char min_stawka_str[3];
	 	sprintf(min_stawka_str, "%d", min_stawka);
	 	strcat(msg,min_stawka_str);
	 	strcat(msg,". Jak chcesz pasowac to wpisz '/pasuj'");
 		wiadomoscuser(msg, clientId);
 		return;
 	}
 	else{
 		licytacja[idGracza(clientId)] = stawka_int;
 		char message[WIADOMOSC];
	 	strcpy(message,"Gracz ");
	 	strcat(message,login[turnToken]);
	 	strcat(message," gra ");
	 	strcat(message, stawka);
	 	strcat(message, ". Teraz licytuje ");
	 	strcat(message, login[incrementTurn()]);
	 	wiadomoscwszyscy(message);
 	}

 }

 int iluPasuje(){
 	int ilu =0;
 	for (int i = 0; i < USERS; ++i)
 	{
 		if(licytacja[i]==-1)
 			ilu++;
 	}

 	return ilu;
 }

 void pasuj(){
 	int id =idGracza(get[0]);
 	licytacja[id] = -1;
 	
 	char message[WIADOMOSC];
 	strcpy(message,"Gracz ");
 	strcat(message,login[turnToken]);
 	strcat(message," pasuje.");
 	strcat(message, "Teraz licytuje ");
 	strcat(message, login[incrementTurn()]);
 	wiadomoscwszyscy(message);

 	if(iluPasuje()==USERS-1)//tylko jeden gracz nie pasował - rozpoczynamy
 	{
 		fazaGry = ROZDAWANIE_MUSU;
 		char msg[WIADOMOSC];
 		strcpy(msg,"Okej! Mus bierze ");
 		strcat(msg, login[turnToken]);
 		strcat(msg," i gra za ");
 		char maxbet[3];
 		sprintf(maxbet, "%d", licytacja[turnToken]);
 		strcat(msg,maxbet);
 		strcat(msg, ". Teraz rozdaje po karcie.");
 		wiadomoscwszyscy(msg);
 		if(licytacja[turnToken]==100){
    		wiadomoscuser("Mus:",zalogowany[turnToken]);
    		wiadomoscuser(drukujKarty(mus,3),zalogowany[turnToken]);
    	}
    	else{
    		wiadomoscwszyscy("Mus:");
    		wiadomoscwszyscy(drukujKarty(mus,3));
    	}

    	dodajKarty(reka[turnToken],7,mus,3);
    	wiadomoscuser(drukujKarty(reka[turnToken],10),zalogowany[turnToken]);
    	wiadomoscuser("Aby wydać karte wpisz /oddaj [nrkarty] [user], np: /oddaj 7 adam, lub /ulist aby zobaczyc graczy",zalogowany[turnToken]);
 	}
 }

int IdGraczaPoLoginie(char* userLogin)
{
	for (int i = 0; i < USERS; ++i)
	{
		if(!strcmp(userLogin,login[i]))
			return i;
	}
	return -1;
}

void shortenArray(struct cardStruct* cards, int size){
	int i=0;
	int j=size-1;
	while(i<j){
		while(strcmp(cards[i].name,"")) i++;
		while(!strcmp(cards[j].name,"")) j--;
		if(i<j){
			memcpy(cards+i,cards+j, sizeof(cards[j]));
			strcpy (cards[j].name,"");
			strcpy (cards[j].color,"");
			cards[j].value=0;
			i++;
			j--;
		}
	}
}
int iloscKart(struct cardStruct* cards, int size){
	int c = 0;
	for (int i = 0; i < size; ++i)
	{
		if(strcmp(cards[i].name,"")){
			c++;
		}
	}
	return c;
}


//return index
int zwyciezca(){
	for (int i = 0; i < USERS; ++i)
	{
		if(pelnaSuma[i]>=1000){
			return i;
		}
	}

	return -1;
}

void oddajkarte(){
	char karta[1];
	char user[LOGIN];
 	char* converted_get = (char*)(get);
 	strncpy(karta, converted_get + 8, 1);
 	strncpy(user, converted_get + 10, LOGIN);
 	int karta_int = atoi(karta);
 	int graczId = IdGraczaPoLoginie(user);
 	if(graczId==-1 || graczId==turnToken){
 		wiadomoscuser("Nie ma takiego gracza. Sprobuj ponownie.",zalogowany[turnToken]);
 		return;
 	}

 	memcpy(reka[graczId] + 7, reka[turnToken]+karta_int, sizeof(reka[turnToken][karta_int]));

 	wiadomoscuser("Dostales karte. Twoj obecny zestaw:",zalogowany[graczId]);
 	wiadomoscuser(drukujKarty(reka[graczId],8),zalogowany[graczId]);

 	strcpy (reka[turnToken][karta_int].name,"");
 	strcpy (reka[turnToken][karta_int].color,"");
 	reka[turnToken][karta_int].value = 0;

 	wiadomoscuser("Oddales karte. Wybierz kolejna:",zalogowany[turnToken]);
 	wiadomoscuser(drukujKarty(reka[turnToken],10),zalogowany[turnToken]);
 	if(iloscKart(reka[turnToken],10)==8){
 		shortenArray(reka[turnToken],10);
 		wiadomoscwszyscy("Rozdano!");
 		wiadomoscuser("Twój obecny zestaw.",zalogowany[turnToken]);
 		wiadomoscuser(drukujKarty(reka[turnToken],8),zalogowany[turnToken]);
 		aktualnyMus = turnToken;
 		fazaGry = OCZEKIWANIE_NA_RZUCENIE_KARTY;
 		char msg[WIADOMOSC];
 		strcpy(msg,"Zaczyna ");
 		strcat(msg,login[turnToken]);
 		wiadomoscwszyscy(msg);
 	}
}

void wezLewe(int ktoKladl){
	int idMaxKarty = ktoKladl;

	for (int i = 0; i < USERS; ++i)
	{
		if(strcmp(stol[idMaxKarty].color, stol[i].color)){//nie jest pod kolor
			if(!strcmp(trumf,stol[i].color)){ // jest trumf
				if(strcmp(stol[idMaxKarty].color, stol[i].color) || stol[idMaxKarty].value<stol[i].value){// czy inny kolor lub najwyzszy trumf
					idMaxKarty = i;
				}
			}
			continue;
		}
		else //jest pod kolor
		{
			if(stol[idMaxKarty].value<stol[i].value)
				idMaxKarty = i;
		}
	}
	printf("max karta: %s%s, wartosc %d", stol[idMaxKarty].name,stol[idMaxKarty].color,stol[idMaxKarty].value);
	int wyn = 0;
	for (int i = 0; i < USERS; ++i)
	{
		wyn += stol[i].value;
	}
	suma[idMaxKarty] += wyn;
	char msg[WIADOMOSC];
	strcpy(msg, "Lewe wzial ");
	strcat(msg, login[idMaxKarty]);
	strcat(msg, ". Uzbierał ");
	char res_str[4];
	sprintf(res_str,"%d",wyn);
	strcat(msg, res_str);
	if(strcmp(trumf,"")){
		strcat(msg, "\nObecny trumf to ");
		strcat(msg, trumf);
	}
	wiadomoscwszyscy(msg);

	turnToken = idMaxKarty;
	strcpy(msg, "Teraz ");
 	strcat(msg, login[turnToken]);
 	wiadomoscwszyscy(msg);

	memset(stol, 0, sizeof(stol[0])*3);

	for (int i = 0; i < USERS; ++i)
	{
		wiadomoscuser(drukujKarty(reka[i],iloscKart(reka[i],8)),zalogowany[i]);
	}
}

//return id karty jesli sie udalo zameldowac
//return -1 jesli sie nie udalo zameldowac
signed int melduj(){
	if(iloscKart(stol,3)>0){
		wiadomoscuser("Meldowac mozna tylko kladac pierwsza karte.", zalogowany[turnToken]);
		return -1;
	}

	char karta[2];
 	char* converted_get = (char*)(get);
 	strncpy(karta, converted_get + 8, 2);
 	int karta_int = atoi(karta);
 	int meldunekPkt;
 	struct cardStruct k = reka[turnToken][karta_int];
 	if(!strcmp(k.name,"K") || !strcmp(k.name,"Q")){ //czy rzucona to krol albo dama?
 		for (int i = 0; i < iloscKart(reka[turnToken],8); ++i)
 		{
 			if(i==karta_int) continue; //nie liczymy tej którą rzucono
 			if((!strcmp(reka[turnToken][i].name,"K") || !strcmp(reka[turnToken][i].name,"Q")) 
 			&& !strcmp(reka[turnToken][i].color, k.color)){ // czy mamy odpowiednik w tym kolorze
 				if(!strcmp(k.color,HEART)) meldunekPkt = 100;
 				else if(!strcmp(k.color,DIAMOND)) meldunekPkt = 80;
 				else if(!strcmp(k.color,CLUB)) meldunekPkt = 60;
 				else if(!strcmp(k.color,SPADE)) meldunekPkt = 40;
				
 				char msg[WIADOMOSC];
 				strcpy(msg,login[turnToken]);
 				strcat(msg," zameldowal ");
 				strcat(msg, k.color);
 				strcat(msg," za ");
 				char mel_str[4];
 				sprintf(mel_str,"%d",meldunekPkt);
 				strcat(msg,mel_str);
				wiadomoscwszyscy(msg);

				suma[turnToken]+=meldunekPkt;
				printf("ZAMELDOWANO: %s\n", karta);
				strcpy(trumf, k.color);
 				return karta_int;
 			}
 		}
 		wiadomoscuser("Nie masz karty do pary", zalogowany[turnToken]);
 	}
 	else
 		wiadomoscuser("Nie mozesz meldowac ta karta", zalogowany[turnToken]);

 	return -1;
}

void rzuckarte(signed int karta_int){
 	printf("RZUCAM: %d\n", karta_int);
 	if(karta_int==-1)
 		return;

 	if(karta_int>=iloscKart(reka[turnToken],8)){
 		wiadomoscuser("Za wysoki numer karty.", zalogowany[turnToken]);
 		return;
 	}
 	
 	//kopiuj na stol
 	memcpy(stol+turnToken, reka[turnToken]+karta_int,sizeof(reka[turnToken][karta_int]));
 	
 	//usun z reki
 	strcpy(reka[turnToken][karta_int].name,"");
 	strcpy(reka[turnToken][karta_int].color,"");
 	reka[turnToken][karta_int].value =0;

 	shortenArray(reka[turnToken],8);
 	char msg[WIADOMOSC];
 	strcpy(msg,"Gracz ");
 	strcat(msg,login[turnToken]);
 	strcat(msg," rzucil karte ");
 	strcat(msg,stol[turnToken].name);
 	strcat(msg,stol[turnToken].color);

 	turnToken = (turnToken+1)%USERS;//kolejny gracz.

 	strcat(msg, ". Teraz ");
 	strcat(msg,login[turnToken]);
 	wiadomoscwszyscy(msg);

 	////zbieranie lewy
 	if(iloscKart(stol,3)==3){
 		//skoro juz wszystko polozono, a wyzej przekazalismy token to teraz jest turnToken ma pierwszy gracz
 		wezLewe(turnToken);
 	}

 	//// zakonczenie rozdania
 	if(iloscKart(reka[turnToken],8)==0){
		wiadomoscwszyscy("Koniec rozdania");

		//czy ugrano tyle ile sie powinno
		strcpy(msg,login[aktualnyMus]);
		if(suma[aktualnyMus]>=licytacja[aktualnyMus]){
			pelnaSuma[aktualnyMus] += licytacja[aktualnyMus];
		}
		else{
			pelnaSuma[aktualnyMus] -= licytacja[aktualnyMus];
			strcat(msg," nie");
		}
		strcat(msg,"ugral to co musial. ");
		strcat(msg,"Mialo byc ");
		char wyn[5];
		sprintf(wyn,"%d",licytacja[aktualnyMus]);
		strcat(msg,wyn);
		strcat(msg,", a bylo ");
		sprintf(wyn,"%d",suma[aktualnyMus]);
		strcat(msg,wyn);
		wiadomoscwszyscy(msg);

		for (int i = 0; i < USERS; ++i)
		{

			strcpy(msg,login[i]);
			if(i!=aktualnyMus)
			{
				pelnaSuma[i]+=suma[i];
				strcat(msg," ugral ");
				sprintf(wyn,"%d",suma[i]);
				strcat(msg,wyn);
			}
			strcat(msg,". Razem ma ");
			sprintf(wyn,"%d",pelnaSuma[i]);
			strcat(msg,wyn);
			wiadomoscwszyscy(msg);
			suma[i]=0;
		}
		
		if(zwyciezca()==-1){
			noweRozdanie();
		}
 	}

 	////zakonczenie gry
 	if(zwyciezca()!=-1){
 		wiadomoscwszyscy("---------------------");
 		wiadomoscwszyscy("-----Koniec gry!-----");
 		wiadomoscwszyscy("---------------------");
		for (int i = 0; i < USERS; ++i)
		{
 			char msg[WIADOMOSC];
			strcpy(msg,login[i]);
			strcat(msg," razem ma ");
			char wyn[5];
			sprintf(wyn,"%d",pelnaSuma[i]);
			strcat(msg,wyn);
			wiadomoscwszyscy(msg);
			suma[i]=0;
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

	printf("faza gry: %d \n", fazaGry);
 	if(fazaGry==OCZEKIWANIE_NA_GRACZY){

	 	if (!strcmp(rozkaz, "/login")) {
	 		logowanie();
	 		return 0;
	 	} else if (!strcmp(rozkaz, "/lgout")) {
	 		wylogowywanie();
	 		return 1;
	 	} 
	}
	else if(fazaGry==LICYTACJA){
		if(converted_get[0]!=zalogowany[turnToken]){
			wiadomoscuser("To nie Twoja kolej. Poczekaj.",converted_get[0]);
			return 0;
		}
		else if (!strcmp(rozkaz, "/riseb")) {
			podbijStawke();
	 		return wiadomoscuser("",zalogowany[0]);
	 	} else if (!strcmp(rozkaz, "/pasuj")) {
	 		pasuj();
	 		wiadomoscwszyscy("");
	 		return 0;
	 	}
 	}else if(fazaGry==ROZDAWANIE_MUSU){

		if(converted_get[0]!=zalogowany[turnToken]){
			wiadomoscuser("Trwa rozdawanie po jednej karcie. Poczekaj.",converted_get[0]);
			return 0;
		}
		else if (!strcmp(rozkaz, "/oddaj")) {
			oddajkarte();
	 		return 0;
	 	}
 	}else if(fazaGry==OCZEKIWANIE_NA_RZUCENIE_KARTY){

		if(converted_get[0]!=zalogowany[turnToken]){
			wiadomoscuser("To nie Twoja kolej. Poczekaj!",converted_get[0]);
			return 0;
		}
		else if (!strcmp(rozkaz, "/karta")) {
			char karta[2];
		 	char* converted_get = (char*)(get);
		 	strncpy(karta, converted_get + 8, 2);
		 	int karta_int = atoi(karta);
			rzuckarte(karta_int);
	 		return 0;
	 	}
		else if (!strcmp(rozkaz, "/meldu")) {
			rzuckarte(melduj());
	 		return 0;
	 	}
 	}

 	// niezależne od fazy
 	if (!strcmp(rozkaz, "/ulist")) {
		listauzytkownikow();
		return 1;
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
