/*

  |=================================================|
  |                                                 |
  |  projekt:         barbers                       |
  |  autor:           Miloslav Ciz (xcizmi00)       |
  |  posledni zmena:  26.12.2011                    |
  |  -----------------------------                  |
  |  popis:                                         |
  |  Zdrojovy kod projektu pro FIT VUT, jedna se o  |
  |  program implementujici problem spiciho holice. |
  |                                                 |
  |=================================================|

*/

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

typedef struct
  {
    sem_t semafor_zakaznik,
          semafor_cekarna,
          semafor_holic,
          semafor_sedi,         /* informuje holice, ze je zakaznik pripraven */
          semafor_ostrihan,     /* informuje holice, ze byl zakaznik ostrihan */
          semafor_odchod,       /* po ostrihani pousti zakaznika z kresla */
          semafor_citac,        /* semafor pristupu k citaci akci */
          semafor_ukoncenych,   /* semafor pro pristup k citaci ukoncenych zakazniku */
          semafor_konec;        /* pro indikaci ukonceni vsech zakazniku */
    int   citac_akci,
          volnych_zidli,        /* pocet aktualne volnych zidli v cekarne */
          ukoncenych_zakazniku;
  } t_sdilene;

t_sdilene *sdilene;

FILE *vystup;                   /* vystupni soubor */

typedef enum                    /* stav zpracovani parametru */
  {
    STAV_OK,                    /* parametry vporadku */
    CHYBA_POCET,                /* spatny pocet parametru */
    CHYBA_NULA,                 /* zadan nulovy parametr */
    CHYBA_FORMAT                /* spatny format ciselneho parametru */
  } t_stav;

//------------------------------

int je_platne_cislo(char *retezec)

/* vraci 1, pokud retezec 'retezec' reprezentuje cele nezaporne cislo v
   mezich INT_MAX, jinak 0 */

{
  int i = 0;
  long pomocna;

  while (retezec[i] != 0)       /* zda retezec obsahuje pouze cislice */
    {
      if (!isdigit(retezec[i]))
        return 0;

      i++;
    }

  if (i == 0)            /* prazdny retezec */
    return 0;

  pomocna = strtol(retezec, NULL, 0);

  if (errno == ERANGE || pomocna > INT_MAX)  /* detekce preteceni a velke hodnoty */
    return 0;

  return 1;
}

//------------------------------

t_stav over_parametry(int pocet, char **argumenty)

/* overi spravnost parametru, 'pocet' je pocet parametru (argc),
   'argumenty' je pole retezcu argumentu (argv), vraci stav zpracovani
   parametru */

{
  int i;

  if (pocet != 6)                    /* kontrola poctu */
    {
      return CHYBA_POCET;
    }

  for (i = 1; i <= 4; i++)           /* kontrola ciselnych parametru */
    if (!je_platne_cislo(argumenty[i]))
      {
        return CHYBA_FORMAT;
      }

  if (atoi(argumenty[1]) == 0 || atoi(argumenty[4]) == 0)
    {
      return CHYBA_NULA;
    }

  return STAV_OK;
}

//-----------------------------

int nahodne_cislo(int max)

/* za pomoci rand() generuje nahodne cislo v intervalu <0;'max'> */

{
  return (int) ( ((float) rand()) / RAND_MAX * max );
}

//------------------------------

void vypis_hlaseni(const char *cast1, int cislo, const char *cast2, FILE *soubor, t_sdilene *sdilene)

/* pokud cislo < 0 pak vypise "A: 'cast1'", jinak "A: 'cast1''cislo'
   'cast2'" (A je hodnota sdileneho citace akci), po provedeni akce
   inkrementuje sdileny citac akci, vse se tiskne do souboru 'soubor',
   funkce je osetrena semafory, 'sdilene' je struktura sdilene pameti */

{
  sem_wait(&(*sdilene).semafor_citac);

  if (cislo < 0)
    fprintf(soubor, "%d: %s", (*sdilene).citac_akci, cast1);
  else
    fprintf(soubor, "%d: %s%d%s", (*sdilene).citac_akci, cast1, cislo, cast2);

  fflush(soubor);

  (*sdilene).citac_akci++;
  sem_post(&(*sdilene).semafor_citac);
}

//------------------------------

void holic(int interval, t_sdilene *sdilene, int celkem)

/* funkce, ktera ovlada cinnost procesu holice, 'interval' je maximalni
   doba strihani v milisekundach, 'sdilene' je struktura sdilene pameti,
   'celkem' je pocet zakazniku, kteri maji byt vygenerovani */

{
  while (1)
    {
      sem_wait(&(*sdilene).semafor_ukoncenych);

        if ((*sdilene).ukoncenych_zakazniku == celkem)
          {
            sem_post(&(*sdilene).semafor_konec); /* informuje hlavni proces o svem ukonceni */
            break;
          }

      sem_post(&(*sdilene).semafor_ukoncenych);

      vypis_hlaseni("barber: checks\n", -1, NULL, vystup, sdilene);
      sem_wait(&(*sdilene).semafor_zakaznik);  /* ceka na zakaznika */
      vypis_hlaseni("barber: ready\n", -1, NULL, vystup, sdilene);
      sem_wait(&(*sdilene).semafor_cekarna);   /* zamkne semafor cekarny */
      (*sdilene).volnych_zidli++;              /* jedno misto v cekarne se uvolni */
      sem_post(&(*sdilene).semafor_cekarna);   /* uvolni pristup do cekarny */
      sem_post(&(*sdilene).semafor_holic);     /* odemkne "kreslo holice" */
      sem_wait(&(*sdilene).semafor_sedi);      /* ceka, az je zakaznik pripraven */
      usleep(nahodne_cislo(interval) * 1000);  /* strihani */
      vypis_hlaseni("barber: finished\n", -1, NULL, vystup, sdilene);
      sem_post(&(*sdilene).semafor_odchod);    /* pusti zakaznika z kresla */
      sem_wait(&(*sdilene).semafor_ostrihan);  /* ceka, az zakaznik opusti kreslo */
    }

  exit (0);
}

//------------------------------

void zakaznik(int poradove_cislo, t_sdilene *sdilene)

/* funkce, ktera ovlada cinnost procesu zakaznika, 'poradove_cislo' je
   poradove cislo zakaznika a 'sdilene' je struktura sdilene pameti,
 */

{
  vypis_hlaseni("customer ", poradove_cislo, ": created\n", vystup, sdilene);

  sem_wait(&(*sdilene).semafor_cekarna);      /* zkousi vstoupit do cekarny */
  vypis_hlaseni("customer ", poradove_cislo, ": enters\n", vystup, sdilene);

  sem_wait(&(*sdilene).semafor_ukoncenych);   /* nesmi byt predbehnut holicem, nez zaznamena sve ukonceni */

  if ((*sdilene).volnych_zidli > 0)           /* pokud je v cekarne volno */
    {
      sem_post(&(*sdilene).semafor_ukoncenych);
      (*sdilene).volnych_zidli--;             /* obsadi 1 volnou zidli */
      sem_post(&(*sdilene).semafor_zakaznik); /* vzbudi holice */
      sem_post(&(*sdilene).semafor_cekarna);  /* odblokuje vstup do cekarny */
      sem_wait(&(*sdilene).semafor_holic);    /* pokusi se usednout do kresla */
      vypis_hlaseni("customer ", poradove_cislo, ": ready\n", vystup, sdilene);
      sem_post(&(*sdilene).semafor_sedi);     /* rika holici, ze je pripraven */
      sem_wait(&(*sdilene).semafor_odchod);   /* ceka, nez ho holic pusti z kresla */
      vypis_hlaseni("customer ", poradove_cislo, ": served\n", vystup, sdilene);
      sem_wait(&(*sdilene).semafor_ukoncenych);
      sem_post(&(*sdilene).semafor_ostrihan);   /* rika holici, ze odchazi (aby nesedeli 2 zakaznici v kresle zaraz) */
    }
  else
    {
      sem_post(&(*sdilene).semafor_cekarna);  /* musi odemknout cekarnu */
      vypis_hlaseni("customer ", poradove_cislo, ": refused\n", vystup, sdilene);
    }

  (*sdilene).ukoncenych_zakazniku++;          /* zaznamena sve ukonceni */
  sem_post(&(*sdilene).semafor_ukoncenych);

  exit (0);
}

//------------------------------

int main (int argc, char **argv)

{
  int    pamet,               /* shmid sdilene pameti */
         i,
         pocet_zidli,         /* Q */
         pocet_zakazniku,     /* N */
         gen_zakaznik,        /* GenC, v ms */
         gen_obsluha,         /* GenB, v ms */
         cislo_zakaznika = 1, /* poradove cislo zakaznika */
         chyba_procesu = 0;   /* zda nastala chyba pri vytvareni procesu */
  key_t  klic = ftok(argv[0],1);
  pid_t  pid_holic,
         pid_zakaznik;

  switch (over_parametry(argc, argv))
    {
      case CHYBA_FORMAT:
        fprintf(stderr, "Chyba: spatny format argumentu\n");
        return EXIT_FAILURE;

      case CHYBA_NULA:
        fprintf(stderr, "Chyba: nulovy pocet zakazniku nebo zidli\n");
        return EXIT_FAILURE;

      case CHYBA_POCET:
        fprintf(stderr, "Chyba: spatny pocet argumentu\n");
        return EXIT_FAILURE;

      case STAV_OK:
        break;
    }

  if (strcmp(argv[5], "-") == 0)
    vystup = stdout;
  else
    {
      vystup = fopen(argv[5], "w");

      if (vystup == NULL)
        {
          fprintf(stderr, "Chyba: nepodarilo se otevrit soubor '%s'\n", argv[5]);
          return EXIT_FAILURE;
        }
    }

  pocet_zidli     = atoi(argv[1]);
  pocet_zakazniku = atoi(argv[4]);
  gen_zakaznik    = atoi(argv[2]);
  gen_obsluha     = atoi(argv[3]);

  pamet = shmget(klic, sizeof(t_sdilene), IPC_CREAT | 0777); /* alokace sdilene pameti */

  if (pamet == -1)
    {
      shmctl(pamet, IPC_RMID, NULL);
      fprintf(stderr, "Chyba: nepodarilo se alokovat sdilenou pamet\n");
      return EXIT_FAILURE;
    }

  sdilene = (t_sdilene *) shmat(pamet, NULL, 0);

  if (sdilene == (void *) -1)
    {
      shmdt(sdilene);
      shmctl(pamet, IPC_RMID, NULL);
      fprintf(stderr, "Chyba: nepodarilo se pripojit alokovanou pamet\n");
      return EXIT_FAILURE;
    }

  srand((int) time(NULL));           /* nahodna cisla na zaklade casu */

  (*sdilene).citac_akci = 1;
  (*sdilene).volnych_zidli = pocet_zidli;
  (*sdilene).ukoncenych_zakazniku = 0;

  sem_init(&(*sdilene).semafor_zakaznik, 1, 0);
  sem_init(&(*sdilene).semafor_holic, 1, 0);
  sem_init(&(*sdilene).semafor_ostrihan, 1, 0);
  sem_init(&(*sdilene).semafor_ukoncenych, 1, 1);
  sem_init(&(*sdilene).semafor_sedi, 1, 0);
  sem_init(&(*sdilene).semafor_odchod, 1, 0);
  sem_init(&(*sdilene).semafor_cekarna, 1, 1);
  sem_init(&(*sdilene).semafor_citac, 1, 1);
  sem_init(&(*sdilene).semafor_konec, 1, 0);

  pid_holic = fork();

  if (pid_holic == 0)
    holic(gen_obsluha, sdilene, pocet_zakazniku);
  else if (pid_holic < 0)
    {
      fprintf(stderr,"Chyba: nepodarilo se vytvorit proces holice\n");
      chyba_procesu = 1;
    }

  if (!chyba_procesu)
    for (i = 0; i < pocet_zakazniku; i++)
      {
        usleep(nahodne_cislo(gen_zakaznik) * 1000);

        pid_zakaznik = fork();

        if (pid_zakaznik == 0)
          zakaznik(cislo_zakaznika, sdilene);
        else if (pid_zakaznik < 0)
          {
            fprintf(stderr,"Chyba: nepodarilo se vytvorit proces zakaznika %d\n", cislo_zakaznika);
            chyba_procesu = 1;
            break;
          }

        cislo_zakaznika++;
      }

  if (!chyba_procesu)
    sem_wait(&(*sdilene).semafor_konec);  /* ceka na ukonceni holice */

  if (strcmp(argv[5], "-") != 0)
    fclose(vystup);

  sem_destroy(&(*sdilene).semafor_zakaznik);
  sem_destroy(&(*sdilene).semafor_odchod);
  sem_destroy(&(*sdilene).semafor_sedi);
  sem_destroy(&(*sdilene).semafor_ukoncenych);
  sem_destroy(&(*sdilene).semafor_ostrihan);
  sem_destroy(&(*sdilene).semafor_holic);
  sem_destroy(&(*sdilene).semafor_cekarna);
  sem_destroy(&(*sdilene).semafor_citac);
  sem_destroy(&(*sdilene).semafor_konec);

  shmdt(sdilene);                  /* odpojeni pameti a uvolneni pameti */
  shmctl(pamet, IPC_RMID, NULL);

  return chyba_procesu ? EXIT_FAILURE : EXIT_SUCCESS;
}
