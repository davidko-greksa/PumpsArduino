#include <bv4612_I.h>
#include <Wire.h>
// 128x64 display with 16 way keypad

// 7 bit adddress is used
BV4612 ui(0x35);

#define     PREBLIK         6       //Makro pre nastavenie casu prebliku v sekundach
#define     POCET_PUMP      18      //Pocet pump
#define     KEY_NOK         127     //kovertovanie tlacidiel
#define     KEY_OK          126

//globalne premenne pre vsetky funkcie
char m; // 0 = display1, -1 =display2,  1 = jedno cerpadlo... 
unsigned long savedTime, currentTime;

uint8_t Lnapln[4];
uint8_t Rnapln[4];
uint8_t Lvyprazdni[4];
uint8_t Rvyprazdni[4];
uint8_t dvojbodka[1];
uint8_t ciara[1];

// Flagy - Ciselne oznacenia z bytu "pumpModes[]"

//Mody pump:  maska 0000 0011 = 0x03
//0 -  CD (continuous dispensing)
//1 -  VD (volume dispensing)
//2 -  D  (dose over time)
//3 -  CF (constant flow rate)

//Sipky:  maska 0000 1100 = 0x0C
//0 - Lvyprazdni
//4 - Lnapln
//8 - Rvyprazdni
//12 - Rnapln 

//globalne pole - obsahuje aktualnu hodnotu modov pump. V kazdom chlieviku jedna pumpa.
uint8_t pumpModes[POCET_PUMP];

//globalne pole pre casy spustenia cerpania jednotlivych pump - pociatocne casy
uint16_t pumpTimesStart[POCET_PUMP];

//pole na zadavané časy čerpania z klavesnice - input
uint16_t pumpTimesLength[POCET_PUMP];

//Buffer, do ktoreho natlacime nazov (cislo) pumpy a akutalny mod
char pumpPrintBuf[6];


//vypise cislo a mod jednej vybranej pumpy 
void vypisPumpu(uint8_t p) {
    uint8_t pMod[2];

    // zistujeme cerpaci mod vybranej pumpy - popis vyssie
    switch (pumpModes[p] & 0x03) {
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

    //Vypis sipky podla smeru cerpania, 3. a 4. bit z kolonky "pumpModes[ ]"
    // z-endujeme pumpModes o 2 bity z lava od konca pre zistenie smeru => bitovy end & (1x&) s hexa 0x0C  (C=12)
    switch (pumpModes[p] & 0x0C) {
        case 0:
            ui.dataLine(Lvyprazdni, row, col + 36, 4);
            break;
        case 4:
            ui.dataLine(Lnapln, row, col + 36, 4);
            break;
        case 8:
            ui.dataLine(Rvyprazdni, row, col + 36, 4);
            break;
        case 12:
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

//Funkcia na prepis zadaneho cisla z klavesnice na pouzivane v kode
char handleKey(char c){
  switch (c) {
    case 1:  return 7; break;
    case 2:  return 8; break;
    case 3:  return 9; break;
    case 4:   break;
    case 5:  return 4; break;
    case 6:  return 5; break;
    case 7:  return 6; break;
    case 8:   break;
    case 9:  return 1; break;
    case 10: return 2; break;
    case 11: return 3; break;
    case 12:  break;
    case 13: return KEY_NOK; break;
    case 14: return 0; break;
    case 15: return KEY_OK; break;
    case 16:  break;
  } 
  return 0; 
}

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
            delay(50);
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
        pumpModes[i] =  i % 16; // testovaci vstup
        pumpTimesStart[i] = 0;   
        pumpTimesLength[i] = 0;   //zadany cas z klavesnice displeja 
    }

    //Inicializacia premennych
    m = 0; // 0 = display, 1 = jedno cerpadlo... 
    savedTime = 0;  // cas ku ktoremu porovnavam, loop stale prebieha, referencny kt. sa porovna s currentTime
  
    zobrazenie();
};

void loop()
{
    currentTime = millis();   //aktualny cas
    //unsigned int ctr;
    //mod zobrazenie 9 cerpadiel
    if (m == 0 || m == -1) { 
	
        //handler zmeny modu
        char k, buf[30];      
        if(ui.keysBuf()) {      //po staceni klavesy zmen mod
            k = ui.key();
            if (k >= 1 && k <= POCET_PUMP) {
                m = k;
                ui.clrBuf();
                ui.clear();
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
  
    //MOD 1 VYPIS 1 CERPADLA  ------------
    if (m >= 1 && m <= POCET_PUMP) {
        currentTime = millis();
        char k, mbuf[30];
    
        if (ui.keysBuf()) {     //zabezpecuje skok do modu 0 - zakladne zobrazenie, pri dalsom stlaceni klavesnice    !!!
            m = 0;
            ui.clrBuf();
            ui.clear();
            zobrazenie();
        }
    
        if ((currentTime / 1000) >= ((savedTime / 1000) + 2)) {     //porovnavanie casu v sekundach, musi byt >= aby pri zahltenom procesore reagoval na zmenu, nemoze byt ==
            savedTime = currentTime;
            ui.setCursor(10,6);
            sprintf(mbuf,"Mode = %d",m);
            ui.print(mbuf);
        }
    }
}


