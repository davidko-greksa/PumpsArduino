#include <bv4612_I.h>
#include <Wire.h>
// 128x64 display with 16 way keypad

// 7 bit adddress is used
BV4612 ui(0x35);

//Konstanty
#define     PREBLIK             6       //Makro pre nastavenie casu prebliku v sekundach
#define     MAX_PUMPS           64      //Pocet pump - maximalny az 64!!
#define     PUMPS_PER_DISPLAY   8       //Hovori o tom kolko je mozne vypisat pump na obrazovku pre 1 vypis
#define     UNUSED_CELL         -1      // -1 je lebo to nie je validna hodnta, nikdz nic nebude mat hodnotu -1 s ktorou by sme pracovali (s 0 praciujeme)

//Klavesove kody
#define     KEY_NOK             127     //kod pre tlacidlo - potvrdenie
#define     KEY_OK              126     //Kod pre tlacidlo - zamietnutie
#define     KEY_CHPARAM         123     //Tlacidlo, CH - change PARAM-parameters, pre MODE (mod 1 zo 4), a DIR - direction, rotujuce 4 hodnoty dokola, volba v settings
#define     KEY_SETTINGS        122     //Tlacidlo pre nastavenie hl. hodnot, -poč. cerpadiel, kontrast, vyber cerpadla



//globalne premenne pre vsetky funkcie                            
int16_t m;                  // (0 - -7) = home,  1 - 64 = jedno cerpadlo vypisane, 101 - 564 (zmena nastaveni cerpadiel)
long kbuf;                  //buffer pre vytváranie viacciferneho čísla
char p_choice;              //premenná na uchovanie hodnoty práve zadavaneho parametra Mod=1 atd
uint8_t chparMask;          //premenna ktora jednotlivymi bitmi urcuje co sme zmenili v 3. mode menu (to sa bude ukladat)
uint8_t kontrast;           //uklada kontrast (da sa zmenit v settings)
uint8_t pocetPump;          //uklada pocet pump, ktore zariadenie aktualne obsluhuje (da sa zmenit v settings, zmeny sa prejavia az po resete)
unsigned long currentTime;  //sluzi na makky delay
unsigned long savedTime;    //sluzi na makky delay

// dočasné premenné = temporary, naše pracovné ktoré vidíme dynamicky sa meniť pri výpise 1 čerpadla, po výstupe z funkcie sa zapisuju 
// do hlavných/východzích: pumpModes[MAX_PUMPS]; pumpTimesStart[MAX_PUMPS]; pumpTimesLength[MAX_PUMPS]
//Globálne prem. sa prepíšu podľa pracovných (temp.) až po stlačení tlačidla KEY_OK

uint8_t tempMode;
uint8_t tempDir;
uint16_t tempVol;
uint8_t tempFlow;
uint16_t tempTimeStart; 
uint16_t tempTimeLength;
uint8_t tempPocetPump;
uint8_t tempKontrast;
uint8_t tempChoice;         //premenna na definovanie co sa bude zadavat, ci sme v nastaveni kontrastu alebo poctu pump

//polia pre grafiku
uint8_t Lnapln[4];
uint8_t Rnapln[4];
uint8_t Lvyprazdni[4];
uint8_t Rvyprazdni[4];
uint8_t dvojbodka[1];
uint8_t ciara[1];

//Mody pump:
//0 -  CD (continuous dispensing)
//1 -  VD (volume dispensing)
//2 -  D  (dose over time)
//3 -  CF (constant flow rate)

//Sipky:
//0 - Lvyprazdni
//1 - Lnapln
//2 - Rvyprazdni
//3 - Rnapln 

//globalne pole - obsahuje aktualnu hodnotu módov pump (1 zo 4 možných CD,VD..). V kazdom chlieviku jedna pumpa.
uint8_t pumpModes[MAX_PUMPS];

//globalne pole pre smery cerpania jednotlivych pump
uint8_t pumpDir[MAX_PUMPS];

//globalne pole pre casy spustenia cerpania jednotlivych pump - pociatocne casy
uint16_t pumpTimesStart[MAX_PUMPS];

//pole na zadavané časy čerpania z klavesnice - input
uint16_t pumpTimesLength[MAX_PUMPS];

//pole na objemy-volume púmp
uint16_t pumpVolumes[MAX_PUMPS];

//pole na prietoky-flow púmp
uint8_t pumpFlow[MAX_PUMPS];

//Buffer, do ktoreho natlacime nazov (cislo) pumpy a akutalny mod
char pumpPrintBuf[6];


//vypise cislo a mod jednej vybranej pumpy pri zobrazení 3x3 na 1 vypis/stranu (f. voláme pri základnom mode 0) 
void vypisPumpu(uint8_t p) {
    uint8_t pMod[2];

    // zistujeme čerpaci mod vybranej pumpy - popis vyssie
    switch (pumpModes[p]) {
        case 0:
            pMod[0] = 0x43;     //hex čislo z ASCI pre C
            pMod[1] = 0x44;     //hex čislo z ASCI pre D
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

    //stlpec
    if(p % PUMPS_PER_DISPLAY == 0 || p % PUMPS_PER_DISPLAY == 3 || p % PUMPS_PER_DISPLAY == 6 ) col= 0;
    if(p % PUMPS_PER_DISPLAY == 1 || p % PUMPS_PER_DISPLAY == 4 || p % PUMPS_PER_DISPLAY == 7 ) col= 44;
    if(p % PUMPS_PER_DISPLAY == 2 || p % PUMPS_PER_DISPLAY == 5)                                col= 87;
    
    //riadok
    // if(p == 0 || p == 1 || p == 2 || p == 9 || p == 10 || p == 11 || p == 18 || p == 19 || p == 20 || p == 27 || p == 28 || p == 29 || p == 36 || p == 37 || p == 38 || p == 45 || p == 46 || p == 47 || p == 54 || p == 55 || p == 56 || p == 63) row = 0;
    // if(p == 3 || p == 4 || p == 5 || p == 12 || p == 13 || p == 14 || p == 21 || p == 22 || p == 23 || p == 30 || p == 31 || p == 32 || p == 39 || p == 40 || p == 41 || p == 48 || p == 49 || p == 50 || p == 57 || p == 58 || p == 59)            row = 3;
    // if(p == 6 || p == 7 || p == 8 || p == 15 || p == 16 || p == 17 || p == 24 || p == 25 || p == 26 || p == 33 || p == 34 || p == 35 || p == 42 || p == 43 || p == 44 || p == 51 || p == 52 || p == 53 || p == 60 || p == 61 || p == 62)            row = 6
    
    if (p % PUMPS_PER_DISPLAY == 0 || p % PUMPS_PER_DISPLAY == 1 || p % PUMPS_PER_DISPLAY == 2) row = 0;
    if (p % PUMPS_PER_DISPLAY == 3 || p % PUMPS_PER_DISPLAY == 4 || p % PUMPS_PER_DISPLAY == 5) row = 3;
    if (p % PUMPS_PER_DISPLAY == 6 || p % PUMPS_PER_DISPLAY == 7)                               row = 6;
    
    // //prvy riadok
    // if(p == 0  || p == 1  || p == 2  || p == 8  || p == 9  || p == 10 ||
       // p == 16 || p == 17 || p == 18 || p == 24 || p == 25 || p == 26 ||
       // p == 32 || p == 33 || p == 34 || p == 40 || p == 41 || p == 42 ||
       // p == 48 || p == 49 || p == 50 || p == 56 || p == 57 || p == 58)   row = 0;
    // //druhy riadok
    // if(p == 3  || p == 4  || p == 5  || p == 11 || p == 12 || p == 13 ||
       // p == 19 || p == 20 || p == 21 || p == 27 || p == 28 || p == 29 ||
       // p == 35 || p == 36 || p == 37 || p == 43 || p == 44 || p == 45 ||
       // p == 51 || p == 52 || p == 53 || p == 59 || p == 60 || p == 61)   row = 3;
    // //treti riadok
    // if(p == 6  || p == 7  || p == 14 || p == 15 || p == 22 || p == 23 ||
       // p == 30 || p == 31 || p == 38 || p == 39 || p == 46 || p == 47 ||
       // p == 54 || p == 55 || p == 62 || p == 63)                         row = 6;
  
    //vypis pumpy a modu
    ui.setCursor(col, row);
    ui.print(pumpPrintBuf);
    delay(10);

    //vypis smeru
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
    if (pumpTimesStart[p] != 0) {
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


//zoberie format HH:MM:SS a prevedie na cislo v sekundach
uint16_t timeToSec(long t) {
    if(t / 10000 >= 18) return 0;  // test ci je dobre cislo, ak nie vrati uint 0, lebo nevie ulozit take velke cislo - 18hodin (oreze MM:SS, preto 1:00:00 => 10000)
    uint16_t timeSec = 0;
    timeSec += t % 100;             // sekundy 
    t = t / 100;                    // oseknutie sekund - poslednzch 2 cisel zo stringu cisel
    timeSec += (t % 100) * 60;      // minuty
    t = t / 100;
    timeSec += (t % 100) * 3600;    // hodiny
    return timeSec;
}

//Výpis 1 vybranej pumpy s detailami
void pumpDetail(uint8_t p) {
    ui.clear();
    ui.setCursor(0,0);
    ui.print("pumpa: ");
    ui.print(p);
    ui.setCursor(0,1);
    ui.print("Mod: ");
    ui.print(pumpModes[p]);
    ui.setCursor(0,2);
    ui.print("Dir: ");
    ui.print(pumpDir[p]);
    ui.setCursor(0,3);
    ui.print("Volume: ");
    ui.print(pumpVolumes[p]);
    ui.setCursor(0,4);
    ui.print("Flow: ");
    ui.print(pumpFlow[p]);
    ui.setCursor(0,5);
    ui.print("Time: ");
    if(pumpTimesLength[p] != 0){
        ui.print(pumpTimesLength[p] - ((millis() / 1000) - pumpTimesStart[p]));
    } else {
        ui.print(0);  
    }
}

// Keyboard Layout:
//
// |---|---|---|---|
// | 7 | 8 | 9 |CHP|
// |---|---|---|---|
// | 4 | 5 | 6 |SET|
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
        case 4:  return KEY_CHPARAM; break;
        case 5:  return 4; break;
        case 6:  return 5; break;
        case 7:  return 6; break;
        case 8:  return KEY_SETTINGS; break;
        case 9:  return 1; break;
        case 10: return 2; break;
        case 11: return 3; break;
        case 12: break;
        case 13: return KEY_NOK; break;
        case 14: return 0; break;
        case 15: return KEY_OK; break;
        case 16: break;
    } 
    return 0; 
}

//Funkcia na zadavanie kontrastu, poc.cerp.
void settings(){
    char k;
    if(ui.keysBuf()) {      //po staceni klavesy
        k = handleKey(ui.key());
        ui.clrBuf();
        if(k == KEY_OK){
            if(kbuf == UNUSED_CELL){
                kontrast = tempKontrast;
                ui.contrast(kontrast);
                if (tempPocetPump <= MAX_PUMPS) ui.EEwrite(32,tempPocetPump);   //(adresa kam zapisujeme,hodnota), osetrenie validnej hodnoty (maximalne 64 pump)
                m = -1;
            } else { //ak sme nieco uz zapisali
                switch (tempChoice){
                    case 0: 
                        tempKontrast = kbuf;
                        break;
                    case 1:
                        tempPocetPump = kbuf;
                        break;
                }
                kbuf = UNUSED_CELL;
            }
        }
        if(k == KEY_NOK){
            if(kbuf == UNUSED_CELL){
                m = -1;
            } else { //ak sme nieco uz zapisali
                kbuf = UNUSED_CELL; 
            }
        }
        if(k >= 0 && k <= 9 && kbuf != UNUSED_CELL){      
            kbuf = (kbuf * 10) + k;
        }
        if(k >= 0 && k <= 9 && kbuf == UNUSED_CELL){
            kbuf = k;
        }
        if(k == KEY_CHPARAM){
            tempChoice = (tempChoice +1) % 2;       // % 2 lebo iba 2 hodnoty nastavujem , naskor + a potom % aby som dostaval hodnoty 0 a 1
        }
    }
    ui.clear();
    ui.print("settings");
    ui.setCursor(0,1);
    if (tempChoice == 0) ui.print("*");
    ui.print("Kontrast: ");
    if(tempChoice != 0 || (kbuf == UNUSED_CELL && tempChoice == 0)){
        ui.print(tempKontrast);
    } else {
        ui.print(kbuf);  
    }
    ui.setCursor(0,2);
    if (tempChoice == 1) ui.print("*");
    ui.print("Pocet pump:");
    if(tempChoice != 1 || (kbuf == UNUSED_CELL && tempChoice == 1)){
        ui.print(tempPocetPump);
    } else {
        ui.print(kbuf);
    }
    ui.setCursor(0,3);
    ui.print(tempChoice);
    ui.setCursor(0,4);
    ui.print(kbuf);
    delay(300);
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
     
    currentTime = millis();
    if (currentTime >= (savedTime + 100)) {     //porovnavanie casu v sekundach, musi byt >= aby pri zahltenom procesore reagoval na zmenu, nemoze byt ==
        savedTime = currentTime;
        char z = -1 * PUMPS_PER_DISPLAY * m;    // priradenie indexu prvej pumpy pre vypis v danom mode "m" (napr. mod -1 vrati 9, mod -2 vrati 18 atd
        //Vypocet zobrazenia spravnej pumpy,  for( odkial, od ktorej pumpy vypisujem ; pokial vypisujem, po dalsich 9 pump; krok)
        for (char i = z ; i < (pocetPump - z < PUMPS_PER_DISPLAY ? pocetPump : z + PUMPS_PER_DISPLAY); i++)      //ak nie su {} tak berie iba nasledujuci prikaz
            vypisPumpu(i);
        //Ak je pocet pump nasobku 9, vypisu sa presne od najnizsieho indexu "z" pre dane zobrazenie az po najvyssi kt. sa vyrata "z + PUMPS_PER_DISPLAY" 
        //Inak vypisuje postupne po 9 cerpadiel, ale na posledny vypis sa vypisu len ostavajuce pumpy po deleni 9, resp. "pocetPump - z"
    }
}

// aktualizuje hodnoty časov pumpTimeLength v poliach pre každe čerpadlo, (a tiez aktualizuje pumpValues.)
//porovnavame ci sme docerpali, ak hej vyplne sa pumpa a pumpTimeLength sa nastavi na 0 
void updateValues(){
    uint8_t i;  //cislo pumpy
    uint32_t timeDif;   // difference - rozdiel
    for(i = 0; i < pocetPump; i++){
        timeDif =  (millis() / 1000 ) - pumpTimesStart[i];
        if(timeDif >= pumpTimesLength[i]){
            pumpTimesLength[i] = 0;
            pumpTimesStart[i] = 0; 
        }
    }
}

void setup() {

    //Displej
    kontrast = 10;
    ui.contrast(kontrast);
    ui.clear();
    ui.clrBuf();

    //konstanty
    Lnapln[0]=0x98; Lnapln[1]=0xa4; Lnapln[2]=0xc2; Lnapln[3]=0xf1;
    Rnapln[0]=0x8f; Rnapln[1]=0x43; Rnapln[2]=0x25; Rnapln[3]=0x19;
    Rvyprazdni[0]=0xf1; Rvyprazdni[1]=0xc2; Rvyprazdni[2]=0xa4; Rvyprazdni[3]=0x98;
    Lvyprazdni[0]=0x19; Lvyprazdni[1]=0x25; Lvyprazdni[2]=0x43; Lvyprazdni[3]=0x8f;
    ciara[0]=0xff;

    //ui.EEwrite(32,18);
    pocetPump = ui.EEread(32);
    //Inicializacia poli - osetrenie proti vypisu odpadu pred prvym spustenim
    for (uint8_t i = 0; i < pocetPump; i++) {  
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
    p_choice = 4; // 4, aby po 1. stlaceni bolo 1 ciže Mode (rotovanie 0-4 => % 5 a +1)

    zobrazenie();
};

void loop() {   

    currentTime = millis();   //aktualny cas
    
    //1. Uroven m (m = 0 / -1) - ZOBRAZENIE 9+9 čerp. ---------
    if (m <= 0 && m > -32) {      //vyratava pocet zobrazovacich obrazoviek, pocet moze byt od 0 po 31

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
                if(kbuf <= pocetPump) {
                    m = kbuf;
                }
                p_choice = 0;
                chparMask = 0;
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
            if(k == KEY_SETTINGS){
                m = -32;
                kbuf = UNUSED_CELL;
                tempChoice = 0;
                tempKontrast = kontrast;
                tempPocetPump = pocetPump;
            }
            
        }
        
        //Porovnanie časov namiesto funkcie Delay
        if ((currentTime / 1000) >= ((savedTime / 1000) + PREBLIK)) {     //porovnavanie casu v sekundach, musi byt >= aby pri zahltenom procesore reagoval na zmenu, nemoze byt ==
            savedTime = currentTime;  //aktualny cas sa stava novym referencnym po kazdych 6s
            char pocetDisplejov = (pocetPump % PUMPS_PER_DISPLAY == 0 ? (pocetPump / PUMPS_PER_DISPLAY) : ((pocetPump / PUMPS_PER_DISPLAY) + 1));     //Kolko obrazoviek je potrebnych na vypis vsetkych pump
            m = (m - 1) % pocetDisplejov;   // aktualizuj mod
            ui.clrBuf();
            ui.clear();
            zobrazenie(); //zobraz pumpy podla aktualneho modu
        }
    }
  
    //2.Uroven m - MOD = 1, VYPIS 1 CERPADLA  ------------
    if (m >= 1 && m <= pocetPump) {
        currentTime = millis();
        char k;
                
        //KEY_NOK a po zadani aj KEY_OK zabezpecuje skok do modu 0 - zakladne zobrazenie, pri dalsom stlaceni klavesnice    !!!
        // Podmienka pre rozdavanie uloh, podla toho co bolo stlacene
        if (ui.keysBuf()) {  
            char k = handleKey(ui.key());
            ui.clrBuf();
            
            if(k == KEY_CHPARAM) {
                p_choice = (p_choice % 5) + 1;  //%5 -> ostaneme v hraniciach 0 az 4 pre zadavanie, posledna +1 -> ideme v modoch uz 1 az 5
                m = m + (100 * p_choice);
                //  continue;    // continue ???
            }
            if(k == KEY_NOK) {
                m = 0;
                ui.clear();
                zobrazenie();
            }
            if(k == KEY_OK) {
                /*pumpModes[m-1] = tempMode;    // m-1 pumpa (real čislovanie od 1, v programe od 0) sa prepíse hlavný mod podla dočasneho
                    pumpTimesStart[m-1] = tempTimeStart;
                    pumpTimesLength[m-1] = tempTimeLength;*/ //riesime v 3.urovni
                m = 0;
                ui.clear();
                zobrazenie();
            }
        }

        //vypis jednej konkretnej pumpy sa aktualizuje raz za 3s (makky delay)
        if ((currentTime / 1000) >= ((savedTime / 1000) + 3)) {     //porovnavanie casu v sekundach, musi byt >= aby pri zahltenom procesore reagoval na zmenu, nemoze byt ==
            savedTime = currentTime;
            pumpDetail(m-1);    // -1 lebo pumpy su cislovane v poliach od 0, aby boli spojené s poliami, lebo fungujú od 0
        }
    }

    // 3.Uroven m - Výber co sa bude zadavat / nastavovat : Mod=1 / Direction=2 / Volume=3 / Flow=4 / Time=5
    if(m > pocetPump) {
        char tchoice = m / 100;   // z čísla pumpy vydelením 100 ziskame CELOČÍSELNE v akom p_choice sa nachadzame ci Mode/Dir/Vol...
        //(neuvazuje sa obahovanie cisla pumpy napr 417 kde 17 je cislo pumpy) /tchoice - temporary choice
        char tpump = (m % 100) -1 ;   // zistenie cisla pumpy ktoru upravujeme, ale cislovanie z pohladu pola tj. 0-17 (ta -1)
        //osetrenie spravnosti vstupu. Ak nie je spravny, ideme spat na home.
        if((tchoice < 1) || (tchoice > 5) || (tpump < 0) || (tpump >= pocetPump)) {
            m = 0;
        } else {
            if (ui.keysBuf()) {  
                char k = handleKey(ui.key());
                ui.clrBuf();
                if(k == KEY_CHPARAM) {
                    m = (m % 500) + 100;
                    kbuf = UNUSED_CELL;  // pri zadani cisla a naslednom stlaceni CHPARAM sa buffer nuluje a neuklada zadane cislo
                    return;
                }
                if(k == KEY_NOK) {
                    if(kbuf == UNUSED_CELL) {    // ak sme nezadali ziadne cislo ideme späť, určije ktorý NOK je použitý - teraz na vratenie sa do Mode / Dir / Vol.. (nevraciam sa na Vypis 1 pumpy)
                        m = m % 100; // Prechod do Urovne 2
                        chparMask = 0;
                        return; // "loop" zacne odznova, a kedze "m" je globalna tak sa dostaneme do Vypisu 1 pumpy (m = 1)
                    } else {  //if(kbuf != UNUSED_CELL)  - NOK ked sme zadali cislo 
                        kbuf = UNUSED_CELL;
                        return;
                    }
                }
                if(k == KEY_OK) {
                    if(kbuf == UNUSED_CELL) {    /* ak sme nezadali ziadne cislo ideme späť, určuje ktorý OK je použitý - teraz
                                                    na vratenie sa do Mode / Dir / Vol.. (nevraciam sa na Vypis 1 pumpy)*/
                        m = m % 100;  // Prechod do Urovne 2
                        //Ulozenie vsetkeho pomocou chparMask
                        // (chparMask & cislo) bitovo sa iba porovnava, 
                        if((chparMask & 2) == 2)
                            pumpModes[tpump] = tempMode;
                        if((chparMask & 4) == 4)
                            pumpDir[tpump] = tempDir;
                        if((chparMask & 8) == 8)
                            pumpVolumes[tpump] = tempVol;
                        if((chparMask & 16) == 16)
                            pumpFlow[tpump] = tempFlow;
                        if((chparMask & 32) == 32){
                            pumpTimesLength[tpump] = tempTimeLength;
                            pumpTimesStart[tpump] = millis() / 1000; //Aktualizacia pumpTimesStart[] pre vybranu pumpu, ale od zadania casu cerpania 
                        }
                        chparMask = 0;
                        return; // "loop" zacne odznova, a kedze "m" je globalna tak sa dostaneme do Vypisu 1 pumpy (m = 1)
                    } else {  //if(kbuf != UNUSED_CELL) - sekcia niektoreho zadavania bez prechodu do Vypisu 1 pumpy
                        switch(tchoice) {
                            case 1:
                                tempMode = kbuf; chparMask |= 2; // priORuj- setbit, konkretny IBA bit nastavime v maske ci nastala zmena, teda ci sa neskor zapise z temp. premennych do hlavnych 
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
                
                if(k >= 0 && k <= 9 && kbuf != UNUSED_CELL)      // zadavanie cisel - hodnot
                    kbuf = (kbuf * 10) + k;
                
                if(k >= 0 && k <= 9 && kbuf == UNUSED_CELL)
                   kbuf = k;
                
            }
        }
          
        //TU JE ZOBRAZOVACIA CAST
        
        currentTime = millis();
        if (currentTime >= (savedTime + 800)) {     //porovnavanie casu v sekundach, musi byt >= aby pri zahltenom procesore reagoval na zmenu, nemoze byt ==
            savedTime = currentTime;
            ui.clear();
            ui.setCursor(80,6);
            ui.print(m);
            //ui.setCursor(60,5);
            //ui.print(chparMask);
            ui.setCursor(0,6);
            ui.print("input:");          
            ui.setCursor(0,7);
            if(kbuf == -1)
                ui.print("invalid");
            else
                ui.print(kbuf);

            switch (tchoice){
                case 1: 
                    ui.setCursor(0,0);
                    ui.print("Mode");
                    ui.setCursor(0,1);
                    ui.print("0 = CD, 1 = VD");
                    ui.setCursor(0,2);
                    ui.print("2 =  D, 3 = CF");
                    break;
                case 2:
                    ui.setCursor(0,0);
                    ui.print("Direction");
                    ui.setCursor(0,1);
                    ui.print("0 = Lvyprazdni");
                    ui.setCursor(0,2);
                    ui.print("1 = Lnapln");
                    ui.setCursor(0,3);
                    ui.print("2 = Rvyprazdni");
                    ui.setCursor(0,4);
                    ui.print("3 = Rnapln");
                    break;
                case 3:
                    ui.setCursor(0,0);
                    ui.print("Flow [ml / min]");
                    break;      // TO DO - dopis maximalny
                case 4:
                    ui.setCursor(0,0);
                    ui.print("Volume [ ml ]");
                    break;
                case 5:
                    ui.setCursor(0,0);
                    ui.print("Time [ HH:MM:SS ]");
                    break;       
            }

        }
          
    }
    if(m == -32){        
         settings();       
    }
    
    //Tu je aktualizacia časov - kolko sa už prečerpalo - dekrementacia napr. každú sekundu.
    updateValues();
}

//TO DO:
//PORIESIT NEVALIDNE VSTUPY v tempMode zadavani !!!!
//nechat v tempe vo formate HH:MM:SS , 
//Odladit DELAY funkcie + nahradit vo funkcii zobrazenie()
//KONTROLA vsetkych OK-NOK ci pracuju spravne, ukladaju co treba, NOK neuklada, ci to funguje spravne, hranicne priprady
//Vypis z modu 3
//pories preblik pri zadavani Direction


//            Konzultácia - pridaj
// riadit spustenie Start - tlacidlo 
// VYPIS vo formate HH:MM:SS !!
// toto je submenu čo je hotove, lubovolne menu s lubovolnou štrukturou,
// dat pumpy do cyklu, ze vsetky naraz spustí - to iste nastavenie pre vsetky - cize: Nastavit pre vsetky? - Ano/Nie   ( DOROBIT MENU)
// opakovanie ceprania pre 1 cerpadlo - nadavkuj objem- pockaj - po zadanom case znova spusti ten isty cyklus (opakuj) - kazdy den / hodinu / kazdych 10min
// pri stlaceni cisla cerpania preprusit MOD1 a vypisat zadane cislo (cerpadla) - vypis toho čo sa prave stlača
//nastavenie kontrastu v settings casti menu

// RGB svetlá stavu čerpania

/*Settings: 
 * -kontrast
 * -vyber 1 z 3 cerpadiel
 * -zadanie poctu cerpadiel
 EEPROM 
 */
