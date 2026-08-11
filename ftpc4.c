/* Klient FTP 
 * 2005
 * Wersja 1.0513
 */

/**********************************************************

Komendy w programie:

LOGOWANIE (automatycznie):
user [login] - wysyla nazwe uzytkownika
pass [haslo] - wysyla haslo uzytkownika

POLECENIA:
cdup - przejscie katalog wyzej w hierarchii
cwd [nazwa_katalogu] - zmiana biezacego katalogu
help - wyswietla liste polecen na serwerze
list - wyswietla liste plikow
mkd [nazwa_katalogu] - utworzenie katalogu
noop - nic nie robi, ale zmusza serwer do odpowiedzi
pwd - informacja o biezacym katalogu
retr [nazwa_pliku] - kopiuje plik z serwera
rmd [nazwa_katalogu] - usuniecie katalogu
stat
syst - informacja o systemie
quit - wylogowanie z serwera

INNE:
? - pomoc
q, exit - wylogowanie z serwera

**********************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>

#define FTP_PORT 21
#define MAX 1024
#define ZNAK_ZACHETY "FTP> "

char login[20]=""; 
char haslo[20]="";
char log[100]="";
char has[100]="";

char temp[MAX]="";
char polecenie2[MAX]="";

char* host;
int klient,k2; 
int status;

char* s1,s2;

struct sockaddr_in to;
struct sockaddr_in to2;
struct hostent *host_info;

/*
--- nieuzywane! ---
zwraca kod polecenia FTP (trzy pierwsze znaki zamienione na liczbe)
int getcode(char tab[]) { 
  int x=0;
  int d=48; //przesuniecie w tablicy ASCII
  x=100*(tab[0]-d)+10*(tab[1]-d)+tab[2]-d;
  return x;
}
*/

/* oblicza numer portu serwera, z ktorego klient moze pobrac dane */
int numer_portu(char* msg, char* addr) {
  int i=0,j,p1=0,p2=0;
  
  for(j=0;j<4;j++)
    for(i+=1;msg[i]!=',';i++);
  for(i+=1;msg[i]!=',';i++) p1=p1*10+msg[i]-'0';
  for(i+=1;msg[i]!=')';i++) p2=p2*10+msg[i]-'0';
  
  j=p1*256+p2;
  return j;
}

/* odczytuje wielkosc pliku podana w komunikacie serwera */
long int jaki_rozmiar(char* info) {
  int i=0,j,p=0;
  for(j=0;info[j]!='(';j++);
  for(i=j+1;info[i]!=' ';i++) p=10*p+info[i]-'0';
  return p;
}

/* porownuje dwa lancuchy, zwraca 1, gdy prefiks pierwszego
   lancucha jest rowny drugiemu lancuchowi; zwraca 0 wpw */
int porown(char* s1, char* s2) {
  int i;
  for(i=0;i<strlen(s2);i++) {
    if(s1[i]!=s2[i]) return 0;
  }
  return 1;
}

/* obsluguje blad funkcji zapisu/odczytu */
void blad_funkcji() {
  perror("Blad funkcji write/read");
  exit(1);
}

/* czysci podana tablice */
void czysc(char* tab, int ile) {
  memset(tab,0,ile*sizeof(char));
}

/* wysyla polecenie do serwera */
void pisz(int kto, char* polecenie) {
  czysc(polecenie2,MAX);
  
  strcat(polecenie2,polecenie);
  strcat(polecenie2,"\r\n");
  
  status=write(kto,polecenie2,strlen(polecenie2));
  if(status!=strlen(polecenie2)) blad_funkcji();
  printf("%s",polecenie2);
}

/* odczytuje odpowiedz serwera */
void czytaj_odp(int kto) {
  int i;  
  
  status=read(kto,temp,MAX);
  if(status<0) blad_funkcji();
  printf("%s",temp);
  
  if(isdigit(temp[0])&&isdigit(temp[1])&&isdigit(temp[2])&&temp[3]=='-') {
    status=read(kto,temp,MAX);
    if(status<0) blad_funkcji();
    printf("%s",temp);
  }
  czysc(temp,MAX);
}


/* KOMENDY FTP ********************************************/

void kom(char* cmd) {
  pisz(klient,cmd);
  czytaj_odp(klient);
  czysc(cmd,strlen(cmd));
}

void kom_ratuj() {
  printf("# Komendy w programie: cdup, cwd, help, list, mkd, noop,\n");
  printf("# pwd, retr, rmd, stat, syst, quit, q, exit, ?\n");
}

/* pobiera liste plikow z serwera */
void kom_list() {
  int port;
  
  /*pisz(klient,"TYPE A");
  czytaj_odp(klient);
  */
  
  czysc(temp,MAX);
  pisz(klient,"PASV");
  status=read(klient,temp,MAX);
  if(status<0) blad_funkcji();
  printf("%s",temp);

  port=numer_portu(temp,host);
  if(!(k2=socket(AF_INET,SOCK_STREAM,0)))
    printf("Blad przy tworzeniu gniazda!\n");
  to2.sin_family=host_info->h_addrtype;
  memcpy((char*)&to2.sin_addr, host_info->h_addr, host_info->h_length);
  to2.sin_port=htons(port);
  if(connect(k2,(struct sockaddr*)&to2,sizeof(to2))<0)
    printf("Blad polaczenia!\n");

  czysc(temp,MAX);
  pisz(klient,"LIST");
  czytaj_odp(klient);
  czytaj_odp(k2);
  close(k2);
  czytaj_odp(klient);
}

/* pobiera plik z serwera i zapisuje go na lokalnym dysku */
void kom_plik(char* nazwa, char* nazwa_lok) {
  int i,port,ile;
  FILE* plik;
  plik=fopen(nazwa_lok,"wb");
    
  czysc(temp,MAX);
  pisz(klient,"TYPE I");
  status=read(klient,temp,MAX);
  if(status<0) blad_funkcji();
  printf("%s",temp);

  pisz(klient,"PASV");
  czysc(temp,MAX);
  status=read(klient,temp,MAX);
  if(status<0) blad_funkcji();
  printf("%s",temp);

  port=numer_portu(temp,host);
  if(!(k2=socket(AF_INET,SOCK_STREAM,0)))
    printf("Blad przy tworzeniu gniazda!\n");
  to2.sin_family=host_info->h_addrtype;
  memcpy((char*)&to2.sin_addr, host_info->h_addr, host_info->h_length);
  to2.sin_port=htons(port);
  if(connect(k2,(struct sockaddr*)&to2,sizeof(to2))<0)
    printf("Blad polaczenia!\n");

  czysc(temp,MAX);
  pisz(klient,nazwa);
  status=read(klient,temp,MAX);
  if(status<0) blad_funkcji();
  printf("%s",temp);

  ile=jaki_rozmiar(temp);
  czysc(temp,MAX);
  for(i=0;i<ile;i++) { 
    status=read(k2,temp,1); 
    if(status<0) blad_funkcji();
    // zapis do pliku
    status=fwrite(temp,sizeof(char),1,plik);
    if(status!=1) printf("Blad zapisu do pliku.\n");
  }
  fclose(plik);

  close(k2);
  czytaj_odp(klient);
}

/* rozlacza z serwera */
void kom_quit() {
  pisz(klient,"QUIT");
  czytaj_odp(klient);
  exit(0);
}

/* wczytuje login i haslo uzytkownika, wysyla do serwera */
void zaloguj(void) {
  czytaj_odp(klient);

  printf("Login: ");
  scanf("%s",login);   
  strcat(log,"USER ");
  strcat(log,login);
  strcat(log,"\r\n");
  pisz(klient,log);
  czytaj_odp(klient);

  // ukrywa haslo
  strcpy(haslo,getpass("Haslo: "));
  strcat(has,"PASS ");
  strcat(has,haslo);
  strcat(has,"\r\n");
  status=write(klient,has,strlen(has));
  if(status!=strlen(has)) blad_funkcji();
  printf("PASS **********\n");
  czytaj_odp(klient);
}      

/* wczytuje polecenia */
void dzialaj(void) {
  char* komenda;
  while(1) {
    czysc(temp,MAX);
    printf(ZNAK_ZACHETY);
    scanf("%s",komenda);
 
    if(!strcmp(komenda,"?")) kom_ratuj();
    else if(!strcmp(komenda,"noop")||!strcmp(komenda,"pwd")||
	    !strcmp(komenda,"syst")||!strcmp(komenda,"cdup")||
	    !strcmp(komenda,"stat")||!strcmp(komenda,"help")) 
      kom(komenda);
    else if(!strcmp(komenda,"quit")||!strcmp(komenda,"q")||
	    !strcmp(komenda,"exit"))
      kom_quit();
    else if(!strcmp(komenda,"list")) kom_list();
    else if(porown(komenda,"cwd")||porown(komenda,"mkd")||
	    porown(komenda,"rmd")) {
      scanf("%s",polecenie2);
      strcat(komenda," ");
      strcat(komenda,polecenie2);
      kom(komenda);
    }
    else if(porown(komenda,"retr")) {
      scanf("%s",polecenie2);
      strcat(komenda," ");
      strcat(komenda,polecenie2);
      kom_plik(komenda,polecenie2);
    }
    else printf("# Nieznana komenda: %s\n",komenda);
  }
}

/*********************************************************/

main(int argc, char *argv[]) {
   
  if(argc!=2) {
    printf("Skladnia: nazwa_programu nazwa_hosta\n"); exit(0); 
  }
  if(!(klient=socket(AF_INET,SOCK_STREAM,0))) {
    perror("Blad przy tworzeniu gniazda!\n"); exit(1);
  } else printf("Gniazdo utworzone. ");

  host=argv[1];
  if((host_info=gethostbyname(host))==NULL){
    perror("Bledna nazwa hosta!\n"); exit(1);
  } else printf("Host rozpoznany. ");
   
  to.sin_family=host_info->h_addrtype;
  memcpy((char*)&to.sin_addr, host_info->h_addr, host_info->h_length);
  to.sin_port=htons(FTP_PORT);  
   
  if((connect(klient,(struct sockaddr *)&to,sizeof(to)))<0) {
    perror("Blad polaczenia!\n"); exit(1);
  } else printf("Polaczenie nawiazano.");


  printf("\n @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
  printf(" @ Klient FTP  -  2005                          @\n");
  printf(" @ Pomoc: ?                                     @\n");
  printf(" @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
  printf("Serwer: %s\n\n",host);

  zaloguj();
  dzialaj();

  if(close(klient)<0) { perror("Blad zamykania\n"); exit(1); }
  else printf("Zamknieto polaczenie.\n"); 
  exit(0);
}


