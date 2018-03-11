#include <bv4612_I.h>
#include <Wire.h>
// 128x64 display with 16 way keypad

// 7 bit adddress is used
BV4612 ui(0x35);

#define     PREBLIK         6       //Makro pre nastavenie casu prebliku v sekundach
#define     POCET_PUMP      18      //Pocet pump
#define     KEY_NOK         127     //kovertovanie tlacidiel
#define     KEY_OK          126
#define     KEY_CHMOD       125     //Tlacidlo na zmenu modu čerpania
#define     KEY_CHDIR       124     //Tlačidlo na zmenu smeru čerpania
#define     KEY_CHPARAM     123     //Tlacidlo, CH - change PARAM-parameters, pre MODE (mod 1 zo 4), a DIR - direction, rotujuce 4 hodnoty dokola
#define     UNUSED_CELL     -1      // -1 je lebo to nie je validna hodnta, nikdz nic nebude mat hodnotu -1 s ktorou by sme pracovali (s 0 praciujeme)



//globalne premenne pre vsetky funkcie
int16_t m;  // 0 = display1, -1 = display2,  1 - 17 = jedno cerpadlo vypisane,
            //X01 - X17 kde X<1,5> určuje zadavany parameter Mod=1 / Direction=2 / Volume=3 / Flow=4 / Time=5
long kbuf;  //buffer pre vytváranie viacciferneho čísla
char p_choice;  // premenná na uchovanie hodnoty práve zadavaneho parametra Mod=1 atd
char chparMask; //premenna ktora jednotlivymi bitmi urcuje co sme zmenili v 3. mode menu (to sa bude ukladat)
unsigned long savedTime, currentTime;

// dočasné premenné = temporary, naše pracovné ktoré vidíme dynamicky sa meniť pri výpise 1 čerpadla, po výstupe z funkcie sa zapisuju 
// do hlavných/východzích: pumpModes[POCET_PUMP]; pumpTimesStart[POCET_PUMP]; pumpTimesLength[POCET_PUMP]
//Globálne prem. sa prepíšu podľa pracovných (temp.) až po stlačení tlačidla KEY_OK

uint8_t tempMode;
uint8_t tempDir;
uint16_t tempVol;
uint8_t tempFlow;
uint16_t tempTimeStart; 
uint16_t tempTimeLength;

uint8_t Lnapln[4];
uint8_t Rnapln[4];
uint8_t Lvyprazdni[4];
uint8_t Rvyprazdni[4];
uint8_t dvojbodka[1];
uint8_t ciara[1];


//globalne pole - obsahuje aktualnu hodnotu módov pump (1 zo 4 možných CD,VD..). V kazdom chlieviku jedna pumpa.
uint8_t pumpModes[POCET_PUMP];

//globalne pole pre smery cerpania jednotlivych pump
uint8_t pumpDir[POCET_PUMP];

//globalne pole pre casy spustenia cerpania jednotlivych pump - pociatocne casy
uint16_t pumpTimesStart[POCET_PUMP];

//pole na zadavané časy čerpania z klavesnice - input
uint16_t pumpTimesLength[POCET_PUMP];

//pole na objemy-volume púmp
uint16_t pumpVolumes[POCET_PUMP];

//pole na prietoky-flow púmp
uint8_t pumpFlow[POCET_PUMP];

//Buffer, do ktoreho natlacime nazov (cislo) pumpy a akutalny mod
char pumpPrintBuf[6];


//vypise cislo a mod jednej vybranej pumpy pri zobrazení 3x3 na 1 vypis/stranu (f. voláme pri základnom mode 0) 
void vypisPumpu(uint8_t p) {
    uint8_t pMod[2];

    // zistujeme čerpaci mod vybranej pumpy - popis vyssie
    switch (pumpModes[p]) {
        case 0:
            pMod[0] = 0x43;
            pMod[1] = 0x44;
            break;
        case 1:
            pMod[0] = 0x56;
            pMod[1] = 0x44;
            break;
        case 2:
            pMod[0] = 0x44;
            pMod[1] = 0x20;
            break;
        case 3:
            pMod[0] = 0x43;
            pMod[1] = 0x46;
            break;
        default: break;
    }

    //priprav vypis prveho riadka (bez sipky)
    if ((p + 1) < 10) {
        sprintf(pumpPrintBuf, "P0%d %c%c", p + 1, pMod[0], pMod[1]);
    } else /*(p>=10) */ {
        sprintf(pumpPrintBuf, "P%d %c%c",  p + 1, pMod[0], pMod[1]);
    }


    // Nastavenie konretnej polohy vypisu pre napr "P10 mod"
    char row,col;

    if(p % 3 == 0 ) col= 0;
    if(p % 3 == 1 ) col= 44;
    if(p % 3 == 2 ) col= 87;
    
    if(p == 0 || p == 1 || p == 2 || p == 9 || p == 10 || p == 11 ) row = 0;
    if(p == 3 || p == 4 || p == 5 || p == 12 || p == 13 || p == 14 ) row = 3;
    if(p == 6 || p == 7 || p == 8 || p == 15 || p == 16 || p == 17 ) row = 6;
  
    ui.setCursor(col, row);
    ui.print(pumpPrintBuf);
    delay(10);

    switch (pumpDir[p]) {
        case 0:
            ui.dataLine(Lvyprazdni, row, col + 36, 4);
            break;
        case 1:
            ui.dataLine(Lnapln, row, col + 36, 4);
            break;
        case 2:
            ui.dataLine(Rvyprazdni, row, col + 36, 4);
            break;
        case 3:
            ui.dataLine(Rnapln, row, col + 36, 4);
            break;
        default: break;
    }

    //po zapnuti (v nenulovom case) a tiez naplni pomocou sprintf  pumpTimesBuf vo formate minuty:sekundy 
    if (pumpTimesStart[p] != 0){
        char timePrintBuf[6];
        char minutes = ((millis() / 1000) - pumpTimesStart[p]) / 60;
        char seconds = ((millis() / 1000) - pumpTimesStart[p]) % 60;
        sprintf ( timePrintBuf, "%d:%d", minutes , seconds);
        ui.setCursor(col,row+1);
        ui.print(timePrintBuf); 
    } else /*Vypis pri vypnutej pumpe*/ {
        ui.setCursor(col,row+1);
        ui.print("OFF");
    }

}


//zoberie format HH:MM:SS a prevedie na string cisel v sekundach
uint16_t timeToSec(long t){
  if(t / 10000 >= 18){  // test ci je dobre cislo, ak nie vrati uint 0, lebo nevie ulozit take velke cislo - 18hodin
     return 0;
  }  
  uint16_t timeSec = 0;
  timeSec += t % 100; // sekundy 
  t = t / 100;      // oseknutie sekund - poslednzch 2 cisel zo stringu cisel
  timeSec += (t % 100) * 60;   // minuty
  t = t / 100;
  timeSec += (t % 100) * 3600;  // hodiny
  return timeSec;
}

//Výpis 1 vybranej pumpy s detailami
void pumpDetail(uint8_t p, uint8_t mode, uint16_t tStart, uint16_t tLength ){
    ui.clear();
    ui.print(p);
    ui.print("\n");
    ui.print(mode);
    /*    DOPLN VYPISOVANIE UDAJOV PRE 1 PUMPU : cas,smer.., NAPATIE !!!!  */
    //ui.print("\n");
    //ui.print(pumpTimesStart[p]);
    //ui.print("\n");
    //ui.print(pumpTimesLength[p]);
}

// Keyboard Layout:
//
// |---|---|---|---|
// | 7 | 8 | 9 |CHP|
// |---|---|---|---|
// | 4 | 5 | 6 |   |
// |---|---|---|---|
// | 1 | 2 | 3 |   |
// |---|---|---|---|
// | F | 0 | T |   |
// |---|---|---|---|

//Funkcia na prepis zadaneho cisla z klavesnice na pouzivane v kode
char handleKey(char c){
    switch (c) {
        case 1:  return 7; break;
        case 2:  return 8; break;
        case 3:  return 9; break;
        case 4:  break;
        case 5:  return 4; break;
        case 6:  return 5; break;
        case 7:  return 6; break;
        case 8:  break;
        case 9:  return 1; break;
        case 10: return 2; break;
        case 11: return 3; break;
        case 12: break;
        case 13: return KEY_NOK; break;
        case 14: return 0; break;
        case 15: return KEY_OK; break;
        case 16:  break;
    } 
    return 0; 
}

//Vypis celeho HOME - vychodzi zobrazenie
void zobrazenie() {

    ui.clear();
    ui.setCursor(0,2); // (stlpec , riadok)   stlpec:0-124   riadok:0-7
    ui.print("---------------------");    //21 znakov v riadku
    ui.setCursor(0,5); // (stlpec , riadok)   stlpec:0-124   riadok:0-7
    ui.print("---------------------");    //21 znakov v riadku
    for(int i = 0; i <= 7; i++){
        if(i != 2 && i != 5){
            ui.dataLine(ciara, i, 42, 1); //smernik, riadok, stlpec, kolko bytov
            ui.dataLine(ciara, i, 85, 1);
            delay(50);                                                                        //  DELAY!!!
        }
    }

    int c;   
    switch (m){        
        case 0:   // Obrazovka 1 -> cerpadla 1 - 9 (indexy 0-8)
            for (char i = 0; i < 9; i++)      //ak nie su {} tak berie iba nasledujuci prikaz
                vypisPumpu(i);
            delay(10);
            break;
        case -1:  // Obrazovka 2 -> cerpadla 10-18 (indexy 9-17)
            for(char i = 9; i < 18; i++)      //ak nie su {} tak berie iba nasledujuci prikaz
                vypisPumpu(i);
        default: break;
    }
}




void setup()
{

    //Displej
    ui.contrast(10);
    ui.clear();
    ui.clrBuf();

    //konstanty
    Lnapln[0]=0x98; Lnapln[1]=0xa4; Lnapln[2]=0xc2; Lnapln[3]=0xf1;
    Rnapln[0]=0x8f; Rnapln[1]=0x43; Rnapln[2]=0x25; Rnapln[3]=0x19;
    Rvyprazdni[0]=0xf1; Rvyprazdni[1]=0xc2; Rvyprazdni[2]=0xa4; Rvyprazdni[3]=0x98;
    Lvyprazdni[0]=0x19; Lvyprazdni[1]=0x25; Lvyprazdni[2]=0x43; Lvyprazdni[3]=0x8f;
    ciara[0]=0xff;
  
    //Inicializacia poli - osetrenie proti vypisu odpadu pred prvym spustenim
    for (uint8_t i = 0; i < POCET_PUMP; i++) {  
        pumpModes[i] = 0;
        pumpDir[i] = 0;
        pumpVolumes[i] = 0;
        pumpFlow[i] = 0;
        pumpTimesStart[i] = 0;   
        pumpTimesLength[i] = 0;   //zadany cas z klavesnice displeja  
    }

    //Inicializacia premennych
    m = 0; // 0 = display, 1 = jedno cerpadlo... 
    kbuf = UNUSED_CELL;
    savedTime = 0;  // cas ku ktoremu porovnavam, loop stale prebieha, referencny kt. sa porovna s currentTime
    p_choice = 4;
  
    zobrazenie();
};

void loop()
{   //1. Uroven m (m = 0 / -1) - ZOBRAZENIE 9+9 čerp. ---------
    currentTime = millis();   //aktualny cas
    //unsigned int ctr;
    //mod zobrazenie 9 cerpadiel
    if (m == 0 || m == -1) { 

        //handler zmeny modu
        char k;   
        if(ui.keysBuf()) {      //po staceni klavesy zmen mod
            k = handleKey(ui.key());
            ui.clrBuf();
            //podmienky na rozoznanie či pri zadavaní VIACCIFERNEHO čisla už máme niečo zadané alebo nie 
            if(k >= 0 && k <= 9 && kbuf != UNUSED_CELL){      
                kbuf = (kbuf * 10) + k;
            }
            if(k >= 0 && k <= 9 && kbuf == UNUSED_CELL){
                kbuf = k;
            }
            if(k == KEY_OK){
                if(kbuf <= POCET_PUMP)
                    {m = kbuf;}
                    p_choice = 4;
                //pripradovanie hlavnych dlhodobych hodnot do kratkodobych -docastnych pre okamzite zobrazenie-vypis
                tempMode = pumpModes[m-1];    // prva pumpa je indexovana v poli ako 0
                tempDir = pumpDir[m-1];
                tempVol = pumpVolumes[m-1];
                tempFlow = pumpFlow[m-1];
                tempTimeStart = pumpTimesStart[m-1];
                tempTimeLength = pumpTimesLength[m-1];
                kbuf = UNUSED_CELL;
            }
            if(k == KEY_NOK){
                kbuf = UNUSED_CELL;
            }
        }
        
        //Porovnanie časov namiesto funkcie Delay
        if ((currentTime / 1000) >= ((savedTime / 1000) + PREBLIK)) {     //porovnavanie casu v sekundach, musi byt >= aby pri zahltenom procesore reagoval na zmenu, nemoze byt ==
            savedTime = currentTime;  //aktualny cas sa stava novym referencnym po kazdych 6s
            if (m == 0) {     //detekuje sa zobrazenie 1.vypisu, zobrazí druhych 9 cerpadiel
                m = -1;
                ui.clrBuf();
                ui.clear();
                zobrazenie();    // zobrazujeme až tu, aby vypis prebiehal az kazdych PREBLIK sekund, cize stiha vypisovat
            } else  if (m == -1) {    //ak je zobrazenych druhych 9 cerpadiel vypis prve
                m = 0;
                ui.clrBuf();
                ui.clear();
                zobrazenie();    // zobrazujeme až tu, aby vypis prebiehal az kazdych PREBLIK sekund, cize stiha vypisovat
            } else {          //po stlaceni klavesy -handler- vycisti buffery, konci zo zobrazovanim a ide do modu jedneho cerpadla alebo zadavania casov
                ui.clrBuf();
                ui.clear();
            }
        }
    }
  
    //2.Uroven m - MOD = 1, VYPIS 1 CERPADLA  ------------
    if (m >= 1 && m <= POCET_PUMP) {
        currentTime = millis();
        char k;
        
        
        //KEY_NOK a po zadani aj KEY_OK zabezpecuje skok do modu 0 - zakladne zobrazenie, pri dalsom stlaceni klavesnice    !!!
        // Podmienka pre rozdavanie uloh, podla toho co bolo stlacene
        if (ui.keysBuf()) {  
            char k = handleKey(ui.key());
            ui.clrBuf();
            if(k == KEY_CHPARAM){
                p_choice = ((p_choice + 1) % 5) + 1;  // prva +1 -> navysovanie, %5 -> ostaneme v hraniciach 0 az 4 pre zadavanie, posledna +1 -> ideme v modoch uz 1 az 5
                m = m + (100 * p_choice);
              //  continue;    // continue ???
            }
            if(k == KEY_NOK){
                m = 0;
                ui.clear();
                zobrazenie();
            }
            if(k == KEY_OK){
                /*pumpModes[m-1] = tempMode;    // m-1 pumpa (real čislovanie od 1, v programe od 0) sa prepíse hlavný mod podla dočasneho
                pumpTimesStart[m-1] = tempTimeStart;
                pumpTimesLength[m-1] = tempTimeLength;*/ //riesime v 3.urovni
                m = 0;
                ui.clear();
                zobrazenie();
            }
        }

        //vypis jednej konkretnej pumpy sa aktualizuje raz za 3s
        if ((currentTime / 1000) >= ((savedTime / 1000) + 3)) {     //porovnavanie casu v sekundach, musi byt >= aby pri zahltenom procesore reagoval na zmenu, nemoze byt ==
            savedTime = currentTime;
            pumpDetail(m-1, tempMode, tempTimeStart, tempTimeLength);    // -1 lebo pumpy su cislovane v poliach od 0, aby boli spojené s poliami, lebo fungujú od 0
        }
    }

    // 3.Uroven m - Výber co sa bude zadavat / nastavovat : Mod=1 / Direction=2 / Volume=3 / Flow=4 / Time=5
    if(m > POCET_PUMP){
        char tchoice = m / 100;   // z čísla pumpy vydelením 100 ziskame celočíselne v akom p_choice sa nachadzame ci Mode/Dir/Vol...
        //(neuvazuje sa obahovanie cisla pumpy napr 417 kde 17 je cislo pumpy) /tchoice - temporary choice
        char tpump = (m % 100) -1 ;   // zistenie cisla pumpy ktoru upravujeme, ale cislovanie z pohladu pola tj. 0-17 (ta -1)
        //osetrenie spravnosti vstupu. Ak nie je spravny, ideme spat na home.
        if((tchoice < 1) || (tchoice > 5) || (tpump < 0) || (tpump >= POCET_PUMP)){
            m = 0;
        }
        else {
            if (ui.keysBuf()) {  
                char k = handleKey(ui.key());
                ui.clrBuf();
                if(k == KEY_CHPARAM){
                    m = ((m + 100) % 500) + 100;
                    kbuf = UNUSED_CELL;  // pri zadani cisla a naslednom stlaceni CHPARAM sa buffer nuluje a neuklada zadane cislo
                    return;
                }
                if(k == KEY_NOK){
                    if(kbuf == UNUSED_CELL){    // ak sme nezadali ziadne cislo ideme späť, určije ktorý NOK je použitý - teraz na vratenie sa do Mode / Dir / Vol.. (nevraciam sa na Vypis 1 pumpy)
                      m = m % 100; // Prechod do Urovne 2
                      chparMask = 0;
                      return; // "loop" zacne odznova, a kedze "m" je globalna tak sa dostaneme do Vypisu 1 pumpy (m = 1)
                    }else{  //if(kbuf != UNUSED_CELL)  - NOK ked sme zadali cislo 
                        kbuf = UNUSED_CELL;
                        return;
                    }
                }
                if(k == KEY_OK){
                    if(kbuf == UNUSED_CELL){    // ak sme nezadali ziadne cislo ideme späť, určije ktorý OK je použitý - teraz na vratenie sa do Mode / Dir / Vol.. (nevraciam sa na Vypis 1 pumpy)
                      m = m % 100;  // Prechod do Urovne 2
                       //Ulozenie vsetkeho pomocou chparMask
                       // (chparMask & cislo) bitovo sa iba porovnava, 
                       if(chparMask & 2 == 2){
                        pumpModes[tpump] = tempMode; }
                       if(chparMask & 4 == 4){
                        pumpDir[tpump] = tempDir; }
                        if(chparMask & 8 == 8){
                        pumpVolumes[tpump] = tempVol; }
                        if(chparMask & 16 == 16){
                        pumpFlow[tpump] = tempFlow; }
                        if(chparMask & 32 == 32){
                        pumpTimesLength[tpump] = tempTimeLength; } 
                      pumpTimesStart[tpump] = millis() / 1000; //Aktualizacia pumpTimesStart[]
                      chparMask = 0;
                      return; // "loop" zacne odznova, a kedze "m" je globalna tak sa dostaneme do Vypisu 1 pumpy (m = 1)
                    }else{  //if(kbuf != UNUSED_CELL) - sekcia niektoreho zadavania bez prechodu do Vypisu 1 pumpy
                        switch(tchoice){
                          case 1: tempMode = kbuf; chparMask |= 2; // priORuj- setbit, konkretny IBA bit nastavime v maske ci nastala zmena, teda ci sa neskor zapise z temp. premennych do hlavnych 
                                  //TO DO - PORIESIT NEVALIDNE VSTUPY !!!!
                                  break; 
                          case 2: tempDir = kbuf; chparMask |= 4; break;
                          case 3: tempVol = kbuf; chparMask |= 8; break;   
                          case 4: tempFlow = kbuf; chparMask |= 16; break;
                          case 5: tempTimeLength = timeToSec(kbuf); chparMask |= 32; break;
                          //TO DO - nechat v tempe vo formate HH:MM:SS koli vypisu???
                        }
                      kbuf = UNUSED_CELL;
                      return;
                    }
                 }
                  if(k >= 0 && k <= 9 && kbuf != UNUSED_CELL){      // zadavanie cisel - hodnot
                    kbuf = (kbuf * 10) + k;
                  }
                  if(k >= 0 && k <= 9 && kbuf == UNUSED_CELL){
                   kbuf = k;
                  }  
                }
            }
          
            //TU BUDE ZOBRAZOVACIA CAST  
            
        }
    }
}
