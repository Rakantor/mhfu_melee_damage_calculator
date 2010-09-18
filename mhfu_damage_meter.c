/*******************************************************/
/* Written by: Manuel Veigel, Austria                  */
/* Name des Programms: mhfu_melee_damage_calculator    */
/* Datum der Fertigstellung: 18.09.2010                */
/*******************************************************/
/* Programmbeschreibung: Dieses Programm dient zur     */
/* Berechnung des tatsächlichen Schadens aller         */
/* Nahkampfwaffen in Monster Hunter Freedom Unite.     */
/*                                                     */
/* Alle Dinge, die für die Berechnungsformel benötigt  */
/* werden, müssen vom entsprechenden Benutzer          */
/* per Eingabeprompt angegeben werden.                 */
/*                                                     */
/* Alle Werte (Waffen, Attacken, Monster, Hitzonen)    */
/* stehen in Tabellen am Anfang der Datei. Das         */
/* Hauptprogramm liest nur noch aus diesen Tabellen.   */
/*                                                     */
/* Fehler aus der ersten Version sind mit              */
/* "BEKANNTER FEHLER" markiert und absichtlich         */
/* nicht behoben.                                      */
/*******************************************************/

//Präprozessoranweisungen (Beginn)
#include<stdio.h>
#include<stdlib.h>

//Anzahl der Einträge einer Tabelle
#define ANZAHL(tabelle) ((int)(sizeof(tabelle) / sizeof((tabelle)[0])))

//Tabelle und ihre Anzahl (für Strukturen mit Zeiger + Anzahl)
#define TABELLE(tabelle) (tabelle), ANZAHL(tabelle)

//Welche Hitzonen-Werte eine Waffe benutzt (Waffe.hitzonen_art)
#define HITZONE_CUT     1       //Schild und Schwert, Doppelklinge, Gewehrlanze, Großschwert, Langschwert
#define HITZONE_IMPACT  2       //Hammer, Jagdhorn
#define HITZONE_LANZE   3       //Lanze: der größere Wert von cut und impact

//Aufteilung der Schläge auf zwei Elementar-Eigenschaften (Attacke.aufteilung)
#define AUFTEILUNG_KEINE    1   //Alle Schläge für Element I, Element II wird nicht berechnet
#define AUFTEILUNG_HAELFTE  2   //Element I: schlag_anzahl / 2, Element II: schlag_anzahl / 2
#define AUFTEILUNG_MINUS    3   //Element I: schlag_anzahl - 1, Element II: schlag_anzahl - 2
//Präprozessoranweisungen (Ende)


//Deklaration und Beschreibung aller Tabellen-Typen (Anfang)

//Eine Attacke einer Waffe
typedef struct {
    const char *gruppe;         //Überschrift im Menü vor dieser Attacke (sonst NULL)
                                //(Bsp.: "Daemonisiert - Zustand")
    const char *name;           //Text im Attacken-Menü
    const char *ausgabe;        //Komplette Zeile für die Datenausgabe (Teil 12)
    double waffe_dmg;           //Attacken - Multiplikation
                                //(Bsp.: SnS(Sprungschnitt) -> waffe_dmg = 0.18)
    int schlag_anzahl;          //Schlaganzahl der Attacke
    int aufteilung;             //AUFTEILUNG_KEINE, AUFTEILUNG_HAELFTE oder AUFTEILUNG_MINUS
} Attacke;

//Eine Waffe
typedef struct {
    const char *name;           //Text im Waffen-Menü
    const char *ausgabe;        //Name in der Datenausgabe (Teil 12)
    const Attacke *attacken;    //Alle Attacken der Waffe
    int attacken_anzahl;
    int hitzonen_art;           //HITZONE_CUT, HITZONE_IMPACT oder HITZONE_LANZE
    double class_auswahl;       //"Special Class" - Multiplikation
                                //(Bsp.: Schild und Schwert -> 1.4)
    double var;                 //"Class Variables" - Multiplikation (Allgemeine Formel)
    double evar;                //"Class Variables" - Multiplikation (Elementare Formel)
    double var2;                //Weitere "Class Variables" - Multiplikationen
    double var3;                //(Bsp.: Großschwert Stufe 1, 2, 3 Aufladung -> 1.10, 1.20, 1.30)
    double var4;                //BEKANNTER FEHLER (nicht behoben): var2, var3 und var4 werden in
                                //keiner Formel verwendet, die Aufladungen zählen also nicht.
} Waffe;

//Ein Gewehrlanzen-Schuss
typedef struct {
    const char *typ;            //Art der Gewehrlanze (Bsp.: "Streu - Typ")
    int stufe;                  //Beschuss-Stufe (1 bis 5)
    int power;                  //"power" - Multiplikation
    int fire;                   //"fire" - Multiplikation
} Schuss;

//Eine Treffer-Art (kritisch/unkritisch)
typedef struct {
    const char *name;           //Text im Menü
    const char *ausgabe;        //Text in der Datenausgabe
    double critical;            //Kritisch/unkritisch - Multiplikation
} Treffer;

//Eine Schärfe-Stufe
typedef struct {
    const char *name;           //Text im Menü
    const char *ausgabe;        //Text in der Datenausgabe
    double sharpness;           //Waffenschärfen - Multiplikation
    double esharpness;          //Elementar-Waffenschärfen - Multiplikation
} Schaerfe;

//Eine Quest Stufe
typedef struct {
    const char *name;           //Text im Menü
    const char *beschreibung;   //Erklärung unter dem Namen
    double def;                 //Monster Defense
} Quest_Stufe;

//Die Werte einer Hitzone
typedef struct {
    double cut;                 //Schadens-Absorbations - Multiplikation für Schnitt-Waffen
    double impact;              //Schadens-Absorbations - Multiplikation für Schlag-Waffen
    double element[5];          //Elementarschadens-Absorbations - Multiplikation
                                //(Feuer, Wasser, Donner, Drache, Eis)
    double fire_hitzone;        //Feuer-Wert für Gewehrlanzen-Schüsse
                                //BEKANNTER FEHLER (nicht behoben): Sollte gleich wie element[0]
                                //(Feuer) sein, weicht aber bei Congalala und Nargacuga ab.
} Hitzone;

//Ein Monster
typedef struct {
    const char *name;                   //Name im Monster-Menü und in der Datenausgabe
    const char *const *hitzonen_namen;  //Hitzonen-Menü (wird von mehreren Monstern benutzt)
    int hitzonen_namen_anzahl;
    const Hitzone *hitzonen;            //Werte der Hitzonen a, b, c, ...
    int hitzonen_anzahl;
    double rage_def;                    //Monster-Defense im wütenden Zustand
    char rage_hitzone;                  //Hitzone mit eigenem Wert im wütenden Zustand (sonst 0)
    double rage_def_hitzone;            //Monster-Defense dieser Hitzone im wütenden Zustand
} Monster;

//Deklaration und Beschreibung aller Tabellen-Typen (Ende)


//Tabellen (Anfang)

///Tabelle: Attacken aller Waffen

//Attacken: Schild und Schwert
static const Attacke attacken_sns[] = {
    //Gruppe Menü                                               Ausgabe (Teil 12)                               waffe_dmg schlag_anzahl Aufteilung
    { NULL,  "Regelmaessiger Kombo (3 mal Dreieck)",            "Ausgewaehlte Attacke: Regelmaessiger Kombo\n", 0.46,     4,            AUFTEILUNG_HAELFTE },
    { NULL,  "Drehender Schnitt (1 mal Kreis)",                 "Ausgewaehlte Attacke: Drehender Schnitt\n",    0.27,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Sprungschnitt (Dreieck + Kreis)",                 "Ausgewaehlte Attacke: Sprungschnitt\n",        0.18,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Rollender Schnitt (Dreieck nach Rolle)",          "Ausgewaehlte Attacke: Rollender Schnitt\n",    0.15,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Block-Attacke 1 (Waehrend dem Blocken, Dreieck)", "Ausgewaehlte Attacke: Block-Attacke 1\n",      0.15,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Block-Attacke 2 (Waehrend dem Blocken, Kreis)",   "Ausgewaehlte Attacke: Block-Attacke 2\n",      0.14,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Zug-Attacke (Beim Ziehen der Waffe)",             "Ausgewaehlte Attacke: Zug-Attacke\n",          0.18,     1,            AUFTEILUNG_KEINE }
};

//Attacken: Doppelklinge
//BEKANNTER FEHLER (nicht behoben): Die Ausgabe (Teil 12) ist um eine Attacke
//verschoben, bei "a" wird sogar die Menü-Überschrift ausgegeben.
static const Attacke attacken_ds[] = {
    //Gruppe                    Menü                                       Ausgabe (Teil 12)                                                        waffe_dmg schlag_anzahl Aufteilung
    { "Normal - Zustand",       "Regelmaessiger Kombo (3 mal Dreieck)",    "\n\nWaehlen Sie die gewuenschte Attacke aus!\n\n",                      0.72,     6,            AUFTEILUNG_HAELFTE },
    { NULL,                     "Drehender Schnitt (1 mal Kreis)",         "Ausgewaehlte Attacke: Regelmaessiger Kombo (Normal - Zustand)\n",       0.31,     3,            AUFTEILUNG_MINUS },
    { NULL,                     "Geladener Schnitt (Dreieck + Kreis)",     "Ausgewaehlte Attacke: Drehender Schnitt (Normal - Zustand)\n",          0.23,     2,            AUFTEILUNG_HAELFTE },
    { NULL,                     "Rollender Schnitt (Dreieck nach Rolle)",  "Ausgewaehlte Attacke: Geladener Schnitt (Normal - Zustand)\n",          0.19,     1,            AUFTEILUNG_KEINE },
    { NULL,                     "Zug-Attacke (Beim Ziehen der Waffe)",     "Ausgewaehlte Attacke: Rollender Schnitt (Normal - Zustand)\n",          0.23,     2,            AUFTEILUNG_HAELFTE },
    { "Daemonisiert - Zustand", "Regelmaessiger Kombo (3 mal Dreieck)",    "Ausgewaehlte Attacke: Zug-Attacke (Normal - Zustand)\n",                0.95,     6,            AUFTEILUNG_HAELFTE },
    { NULL,                     "Drehender Schnitt (1 mal Kreis)",         "Ausgewaehlte Attacke: Regelmaessiger Kombo (Daemonisiert - Zustand)\n", 0.82,     6,            AUFTEILUNG_HAELFTE },
    { NULL,                     "Rollender Schnitt (Dreieck nach Rolle)",  "Ausgewaehlte Attacke: Drehender Schnitt (Daemonisiert - Zustand)\n",    0.25,     1,            AUFTEILUNG_KEINE },
    { NULL,                     "Daemonen-Tanz Attacke (Dreieck + Kreis)", "Ausgewaehlte Attacke: Rollender Schnitt (Daemonisiert - Zustand)\n",    1.37,     10,           AUFTEILUNG_HAELFTE }
};

//Attacken: Lanze
static const Attacke attacken_lanze[] = {
    //Gruppe Menü                                    Ausgabe (Teil 12)                                 waffe_dmg schlag_anzahl Aufteilung
    { NULL,  "Normaler-Stich Kombo (3 mal Dreieck)", "Ausgewaehlte Attacke: Normaler-Stich Kombo\n",   0.76,     3,            AUFTEILUNG_MINUS },
    { NULL,  "Aufwaerts-Stich Kombo (3 mal Kreis)",  "Ausgewaehlte Attacke: Aufwaerts-Stich Kombo\n",  0.86,     3,            AUFTEILUNG_MINUS },
    { NULL,  "Durch-Stich (Dreieck + Kreis)",        "Ausgewaehlte Attacke: Durch-Stich\n",            0.20,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Ladungs-Stich Finisher",               "Ausgewaehlte Attacke: Ladungs-Stich Finisher\n", 0.40,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Block-Attacke",                        "Ausgewaehlte Attacke: Block-Attacke\n",          0.20,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Zug-Attacke (Beim Ziehen der Waffe)",  "Ausgewaehlte Attacke: Zug-Attacke\n",            0.28,     1,            AUFTEILUNG_KEINE }
};

//Attacken: Gewehrlanze
static const Attacke attacken_gl[] = {
    //Gruppe Menü                                               Ausgabe (Teil 12)                                waffe_dmg schlag_anzahl Aufteilung
    { NULL,  "Normaler-Stich Kombo (3 mal Dreieck)",            "Ausgewaehlte Attacke: Normaler-Stich Kombo\n",  0.82,     3,            AUFTEILUNG_MINUS },
    { NULL,  "Aufwaerts-Stich Kombo (R-Taste + 3 mal Dreieck)", "Ausgewaehlte Attacke: Aufwaerts-Stich Kombo\n", 0.54,     3,            AUFTEILUNG_MINUS },
    { NULL,  "Rauschender-Stich (1 mal Dreieck)",               "Ausgewaehlte Attacke: Rauschender-Stich\n",     0.34,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Aufwaerts-Schwung (Dreieck + Kreis",              "Ausgewaehlte Attacke: Aufwaerts-Schwung\n",     0.30,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Zug-Attacke (Beim Ziehen der Waffe)",             "Ausgewaehlte Attacke: Zug-Attacke\n",           0.34,     1,            AUFTEILUNG_KEINE }
};

//Attacken: Grossschwert
static const Attacke attacken_gs[] = {
    //Gruppe Menü                                      Ausgabe (Teil 12)                                 waffe_dmg schlag_anzahl Aufteilung
    { NULL,  "Regelmaessiger Schnitt (1 mal Dreieck)", "Ausgewaehlte Attacke: Regelmaessiger Schnitt\n", 0.48,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Seitwaerts-Hieb (1 mal Kreis)",          "Ausgewaehlte Attacke: Seitwaerts-Hieb\n",        0.36,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Aufwaerts-Schlag (Dreieck + Kreis)",     "Ausgewaehlte Attacke: Aufwaerts-Schlag\n",       0.46,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Stufe 1 Aufladung (Dreieck halten)",     "Ausgewaehlte Attacke: Stufe 1 Aufladung\n",      0.65,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Stufe 2 Aufladung (Dreieck halten)",     "Ausgewaehlte Attacke: Stufe 2 Aufladung\n",      0.80,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Stufe 3 Aufladung (Dreieck halten)",     "Ausgewaehlte Attacke: Stufe 3 Aufladung\n",      1.10,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Zug-Attacke (Beim Ziehen der Waffe)",    "Ausgewaehlte Attacke: Zug-Attacke\n",            0.48,     1,            AUFTEILUNG_KEINE }
};

//Attacken: Langschwert
static const Attacke attacken_ls[] = {
    //Gruppe Menü                                            Ausgabe (Teil 12)                                  waffe_dmg schlag_anzahl Aufteilung
    { NULL,  "Schnitt vorwaerts (2 mal Dreieck)",            "Ausgewaehlte Attacke: Schnitt vorwaerts\n",       0.61,     2,            AUFTEILUNG_HAELFTE },
    { NULL,  "Durchdringender-Schnitt (1 mal Kreis)",        "Ausgewaehlte Attacke: Durchdringender-Schnitt\n", 0.20,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Schwung-Hieb (Dreieck + Kreis)",               "Ausgewaehlte Attacke: Schwung-Hieb\n",            0.30,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Aufwaerts-Hieb (Kreis, Dreieck)",              "Ausgewaehlte Attacke: Aufwaerts-Hieb\n",          0.23,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Abwaerts-Hieb (Nach Aufwaerts-Hieb, Dreieck)", "Ausgewaehlte Attacke: Abwaerts-Hieb\n",           0.28,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Nicht-Elan Attacke (R-Taste)",                 "Ausgewaehlte Attacke: Nicht-Elan Attacke\n",      0.16,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Elan Attacke (3 mal R-Taste)",                 "Ausgewaehlte Attacke: Elan Attacke\n",            1.47,     5,            AUFTEILUNG_KEINE },
    { NULL,  "Zug-Attacke (Beim Ziehen der Waffe)",          "Ausgewaehlte Attacke: Zug-Attacke\n",             0.33,     1,            AUFTEILUNG_KEINE }
};

//Attacken: Hammer
static const Attacke attacken_hammer[] = {
    //Gruppe Menü                                       Ausgabe (Teil 12)                               waffe_dmg schlag_anzahl Aufteilung
    { NULL,  "Regelmaessiger Kombo (3 mal Dreieck)",    "Ausgewaehlte Attacke: Regelmaessiger Kombo\n", 1.72,     3,            AUFTEILUNG_MINUS },
    { NULL,  "Seitwaerts-Schwung (1 mal Kreis)",        "Ausgewaehlte Attacke: Seitwaerts-Schwung\n",   0.15,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Spin-Attacke",                            "Ausgewaehlte Attacke: Spin-Attacke\n",         0.40,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Kurze Aufladung (Kurz R-Taste)",          "Ausgewaehlte Attacke: Kurze Aufladung\n",      0.45,     1,            AUFTEILUNG_KEINE },
    { NULL,  "Mittlere Aufladung (Mittellang R-taste)", "Ausgewaehlte Attacke: Mittlere Aufladung\n",   0.80,     2,            AUFTEILUNG_HAELFTE },
    { NULL,  "Super Aufladung (Lange R-Taste)",         "Ausgewaehlte Attacke: Super Aufladung\n",      0.96,     2,            AUFTEILUNG_HAELFTE },
    { NULL,  "Zug-Attacke (Beim Ziehen der Waffe)",     "Ausgewaehlte Attacke: Zug-Attacke\n",          0.20,     1,            AUFTEILUNG_KEINE }
};

//Attacken: Jagdhorn
static const Attacke attacken_hh[] = {
    //Gruppe                Menü                                   Ausgabe (Teil 12)                            waffe_dmg schlag_anzahl Aufteilung
    { "Normal - Modus",     "Schwung-Attacke (1 mal Dreieck)",     "Ausgewaehlte Attacke: Schwung-Attacke\n",   0.27,     1,            AUFTEILUNG_KEINE },
    { NULL,                 "Stoss-Attacke (3 mal Kreis)",         "Ausgewaehlte Attacke: Stoss-Attacke\n",     0.36,     3,            AUFTEILUNG_MINUS },
    { NULL,                 "Ueberkopf-Schlag (Dreieck + Kreis)",  "Ausgewaehlte Attacke: Ueberkopf-Schlag\n",  0.63,     2,            AUFTEILUNG_HAELFTE },
    { NULL,                 "Zug-Attacke (Beim Ziehen der Waffe)", "Ausgewaehlte Attacke: Zug-Attacke\n",       0.27,     1,            AUFTEILUNG_KEINE },
    { "Notenspiel - Modus", "Notenspiel-Beginn",                   "Ausgewaehlte Attacke: Notenspiel-Beginn\n", 0.26,     1,            AUFTEILUNG_KEINE },
    { NULL,                 "Erste Note (Viereck oder Kreis)",     "Ausgewaehlte Attacke: Erste Note\n",        0.38,     1,            AUFTEILUNG_KEINE },
    { NULL,                 "Zweite Note (Dreieck)",               "Ausgewaehlte Attacke: Zweite Note\n",       0.43,     1,            AUFTEILUNG_KEINE },
    { NULL,                 "Dritte Note (Dreieck + Kreis)",       "Ausgewaehlte Attacke: Dritte Note\n",       0.41,     1,            AUFTEILUNG_KEINE },
    { NULL,                 "Notenspiel-Ende",                     "Ausgewaehlte Attacke: Notenspiel-Ende\n",   0.26,     1,            AUFTEILUNG_KEINE }
};

///Tabelle: Waffen
static const Waffe waffen[] = {
    //Menü                  Ausgabe               Attacken                  Hitzonen-Art    class var   evar  var2  var3  var4
    { "Schild und Schwert", "Schild und Schwert", TABELLE(attacken_sns),    HITZONE_CUT,    1.4,  1.0,  1.0,  0.0,  0.0,  0.0 },
    { "Doppelklinge",       "Doppelklingen",      TABELLE(attacken_ds),     HITZONE_CUT,    1.4,  1.0,  0.70, 0.0,  0.0,  0.0 },
    { "Lanze",              "Lanze",              TABELLE(attacken_lanze),  HITZONE_LANZE,  2.3,  1.0,  1.0,  1.12, 0.0,  0.0 },
    { "Gewehrlanze",        "Gewehrlanze",        TABELLE(attacken_gl),     HITZONE_CUT,    2.3,  0.95, 0.95, 0.0,  0.0,  0.0 },
    { "Grossschwert",       "Grossschwert",       TABELLE(attacken_gs),     HITZONE_CUT,    4.8,  1.05, 1.0,  1.10, 1.20, 1.30 },
    { "Langschwert",        "Langschwert",        TABELLE(attacken_ls),     HITZONE_CUT,    4.8,  1.05, 1.0,  1.12, 0.0,  0.0 },
    { "Hammer",             "Hammer",             TABELLE(attacken_hammer), HITZONE_IMPACT, 5.2,  1.0,  1.0,  0.0,  0.0,  0.0 },
    { "Jagdhorn",           "Jagdhorn",           TABELLE(attacken_hh),     HITZONE_IMPACT, 5.2,  1.0,  1.0,  0.0,  0.0,  0.0 }
};

///Tabelle: Gewehrlanzen-Schüsse
static const Schuss gewehrlanzen_schuss[] = {
    //Typ               Stufe power fire
    { "Normal - Typ",   1,    12,   4 },
    { "Normal - Typ",   2,    15,   5 },
    { "Normal - Typ",   3,    18,   6 },
    { "Normal - Typ",   4,    21,   7 },
    { "Normal - Typ",   5,    24,   8 },
    { "Lang - Typ",     1,    18,   9 },
    { "Lang - Typ",     2,    22,   10 },
    { "Lang - Typ",     3,    28,   14 },
    { "Lang - Typ",     4,    32,   16 },
    { "Lang - Typ",     5,    36,   18 },
    { "Streu - Typ",    1,    24,   6 },
    { "Streu - Typ",    2,    32,   8 },
    { "Streu - Typ",    3,    40,   10 },
    { "Streu - Typ",    4,    44,   11 },
    { "Streu - Typ",    5,    48,   12 },
    { "Wyvern - Feuer", 1,    30,   10 },
    { "Wyvern - Feuer", 2,    36,   12 },
    { "Wyvern - Feuer", 3,    42,   14 },
    { "Wyvern - Feuer", 4,    44,   15 },
    { "Wyvern - Feuer", 5,    48,   16 }
};

///Tabelle: Treffer-Arten
static const Treffer treffer[] = {
    //Menü                                                  Ausgabe              critical
    { "Kritisch (Beim Schlag: Roter Streifen)",             "Kritisch",          1.25 },
    { "Normal",                                             "Normal",            1.0  },
    { "Negativ-Kritisch (Beim Schlag: Violetter Streifen)", "Negativ Kritisch",  0.75 }
};

///Tabelle: Schärfe
static const Schaerfe schaerfe[] = {
    //Menü                  Ausgabe      sharpness  esharpness
    { "Rot (Stufe 1)",      "Rot",       0.50,      0.25   },
    { "Orange (Stufe 2)",   "Orange",    0.75,      0.50   },
    { "Gelb (Stufe 3)",     "Gelb",      1.0,       0.75   },
    { "Gruen (Stufe 4)",    "Gruen",     1.125,     1.0    },
    { "Blau (Stufe 5)",     "Blau",      1.25,      1.0625 },
    { "Weiss (Stufe 6)",    "Weiss",     1.30,      1.125  },
    { "Violett (Stufe 7)",  "Violett",   1.50,      1.20   }
};

///Tabelle: Elementar-Eigenschaften (a... Keine, b... Feuer, ...)
static const char *const elemente[] = {
    "Keine", "Feuer", "Wasser", "Donner", "Drache", "Eis"
};

///Tabelle: Quest Stufen
static const Quest_Stufe quest_stufen[] = {
    { "Aeltesten und Normal - Stufe",
      "     (Das sind alle Quests der Dorfaeltesten und\n"
      "      alle Stufe(Stern) 1, 2, 3, 4 und 5 Quests der Versammlungshalle)",       1.0  },
    { "Schwer - Stufe",
      "     (Das sind alle Stufe(Stern) 7 Quests der Katze und\n"
      "      alle Stufe(Stern) 6 Quests der Versammlungshalle)",                      0.95 },
    { "Ober-Aeltesten - Stufe",
      "     (Das sind alle Stufe(Stern) 8 Quests der Katze und\n"
      "      alle Stufe(Stern) 7 Quests der Versammlungshalle)",                      0.80 },
    { "Hoher Rang - Stufe",
      "     (Das sind alle Stufe(Stern) 9 Quests der Katze und\n"
      "      alle Stufe(Stern) 8 Quests der Versammlungshalle)",                      0.75 },
    { "G-Rang - Stufe",
      "     (Das sind alle Stufe G1, G2 und G3 Quests der Versammlungshalle)",         0.70 }
};

///Tabelle: Hitzonen-Menüs (welche Hitzonen bei einem Monster ausgewählt werden können)

//Benutzt von: Yian Kut-Ku, Yian Garuga, Hypnocatrice, Cephadrome, Gypceros, Rathalos, Rathian, Khezu, Monoblos, Diablos, Plesioth, Lavasioth
static const char *const hitzonen_menue_wyvern[] = {
    "Kopf", "Hals", "Ruecken", "Bauch", "Schwanz", "Fluegel", "Fuss"
};

//Benutzt von: Basarios, Gravios
static const char *const hitzonen_menue_basarios[] = {
    "Kopf", "Hals", "Ruecken", "Bauch (Nicht gebrochen)", "Bauch (Gebrochen)", "Schwanz", "Fluegel", "Fuss"
};

//Benutzt von: Congalala, Blangonga
static const char *const hitzonen_menue_congalala[] = {
    "Kopf", "Vordere Beine", "Hintere Beine", "Koerper", "Schwanz"
};

//Benutzt von: Rajang
//BEKANNTER FEHLER (nicht behoben): Im Menü fehlt "d", dafür gibt es "f... Schwanz".
//Für "f" gibt es aber keine Hitzonen-Werte (es werden die alten Werte verwendet).
static const char *const hitzonen_menue_rajang[] = {
    "Kopf", "Vordere Beine", "Hintere Beine", NULL, "Koerper", "Schwanz"
};

//Benutzt von: Daimyo Hermitaur, Shogun Ceanataur
static const char *const hitzonen_menue_krabbe[] = {
    "Kopf", "Koerper", "Panzer", "Fuss", "Zange, Schere", "Arm"
};

//Benutzt von: Tigrex, Nargacuga, Akantor, Ukanlos
static const char *const hitzonen_menue_tigrex[] = {
    "Kopf", "Hals", "Ruecken", "Bauch", "Schwanz", "Vordere Beine", "Hintere Beine"
};

//Benutzt von: Kirin
static const char *const hitzonen_menue_kirin[] = {
    "Kopf", "Koerper", "Horn"
};

//Benutzt von: Kushala Daora, Lunastra, Teostra
static const char *const hitzonen_menue_kushala[] = {
    "Kopf, Hals", "Bauch", "Ruecken", "Schwanz", "Vordere Beine", "Hintere Beine", "Fluegel"
};

//Benutzt von: Chameleos
static const char *const hitzonen_menue_chameleos[] = {
    "Kopf", "Brust", "Ruecken", "Vordere Beine", "Hintere Beine", "Fluegel", "Schwanz"
};

//Benutzt von: Lao-Shan Lung
static const char *const hitzonen_menue_lao_shan[] = {
    "Kopf", "Hals", "Schulter", "Fussstück", "Schwachpunkt", "Ruecken", "Schwanz", "Bauch", "Fuss"
};

//Benutzt von: Yamatsukami
static const char *const hitzonen_menue_yamatsukami[] = {
    "Kopf", "Ruecken", "Auge", "Koerper", "Maul", "Fuehler, Tentakel", "Fuss"
};

//Benutzt von: Shen Gaoren
static const char *const hitzonen_menue_shen_gaoren[] = {
    "Gesicht", "Koerper", "Panzer", "Fuss", "Zange, Schere", "Arm", "Schwachpunkt (Im Panzer)"
};

//Benutzt von: Fatalis
static const char *const hitzonen_menue_fatalis[] = {
    "Gesicht", "Kopf", "Ruecken, Schwanz", "Hals", "Brust", "Fluegel", "Bauch", "Fuss"
};

//Benutzt von: Bulldrome, Giadrome, Velocidrome, Gendrome, Iodrome, Koenig Shakalaka, Vespiodenkoenigin
static const char *const hitzonen_menue_klein[] = {
    "Koerper"
};

///Tabelle: Hitzonen-Werte aller Monster
//(Spalten: cut, impact, Elementar-Werte für Feuer, Wasser, Donner, Drache, Eis, fire_hitzone)

//Yian Kut-Ku (Normal u. Blau)
static const Hitzone hitzonen_aa[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.50, 0.80,  { 0.10, 0.30,  0.30,  0.00,  0.40 }, 0.10 },  //a... Kopf
    { 0.50, 0.60,  { 0.10, 0.80,  0.50,  0.20,  0.50 }, 0.10 },  //b... Hals
    { 0.40, 0.50,  { 0.10, 0.20,  0.15,  0.00,  0.15 }, 0.10 },  //c... Ruecken
    { 0.80, 0.70,  { 0.10, 0.20,  0.15,  0.00,  0.15 }, 0.10 },  //d... Bauch
    { 0.50, 0.60,  { 0.10, 0.20,  0.15,  0.00,  0.15 }, 0.10 },  //e... Schwanz
    { 0.75, 0.70,  { 0.10, 0.10,  0.25,  0.00,  0.15 }, 0.10 },  //f... Fluegel
    { 0.25, 0.30,  { 0.05, 0.05,  0.05,  0.00,  0.05 }, 0.05 }   //g... Fuss
};

//Yian Garuga (Ein Ohr)
static const Hitzone hitzonen_ab[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.65, 0.60,  { 0.00, 0.20,  0.00,  0.05,  0.10 }, 0.00 },  //a... Kopf
    { 0.40, 0.40,  { 0.00, 0.30,  0.00,  0.05,  0.10 }, 0.00 },  //b... Hals
    { 0.10, 0.10,  { 0.05, 0.70,  0.05,  0.15,  0.15 }, 0.05 },  //c... Ruecken
    { 0.10, 0.10,  { 0.15, 0.40,  0.15,  0.30,  0.30 }, 0.15 },  //d... Bauch
    { 0.40, 0.20,  { 0.05, 0.30,  0.05,  0.15,  0.20 }, 0.05 },  //e... Schwanz
    { 0.20, 0.40,  { 0.00, 0.20,  0.00,  0.05,  0.10 }, 0.00 },  //f... Fluegel
    { 0.25, 0.25,  { 0.00, 0.15,  0.00,  0.05,  0.10 }, 0.00 }   //g... Fuss
};

//Yian Garuga (Zwei Ohren)
static const Hitzone hitzonen_ac[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.65, 0.60,  { 0.00, 0.20,  0.00,  0.05,  0.10 }, 0.00 },  //a... Kopf
    { 0.40, 0.40,  { 0.00, 0.30,  0.00,  0.05,  0.10 }, 0.00 },  //b... Hals
    { 0.10, 0.10,  { 0.05, 0.70,  0.05,  0.15,  0.15 }, 0.05 },  //c... Ruecken
    { 0.10, 0.10,  { 0.15, 0.40,  0.15,  0.30,  0.30 }, 0.15 },  //d... Bauch
    { 0.45, 0.25,  { 0.05, 0.30,  0.05,  0.15,  0.20 }, 0.05 },  //e... Schwanz
    { 0.25, 0.45,  { 0.00, 0.20,  0.00,  0.05,  0.10 }, 0.00 },  //f... Fluegel
    { 0.25, 0.25,  { 0.00, 0.15,  0.00,  0.05,  0.10 }, 0.00 }   //g... Fuss
};

//Hypnocatrice
static const Hitzone hitzonen_ad[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.65, 0.50,  { 0.20, 0.05,  0.10,  0.05,  0.10 }, 0.20 },  //a... Kopf
    { 0.40, 0.40,  { 0.10, 0.10,  0.10,  0.05,  0.15 }, 0.10 },  //b... Hals
    { 0.25, 0.30,  { 0.15, 0.05,  0.05,  0.05,  0.15 }, 0.15 },  //c... Ruecken
    { 0.50, 0.40,  { 0.25, 0.05,  0.10,  0.05,  0.05 }, 0.25 },  //d... Bauch
    { 0.40, 0.40,  { 0.10, 0.05,  0.15,  0.05,  0.05 }, 0.10 },  //e... Schwanz
    { 0.25, 0.20,  { 0.15, 0.10,  0.20,  0.05,  0.10 }, 0.15 },  //f... Fluegel
    { 0.35, 0.40,  { 0.05, 0.05,  0.05,  0.05,  0.05 }, 0.05 }   //g... Fuss
};

//Cephadrome
static const Hitzone hitzonen_ae[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.40, 0.50,  { 0.00, 0.10,  0.15,  0.05,  0.30 }, 0.00 },  //a... Kopf
    { 1.10, 1.00,  { 0.05, 0.10,  0.15,  0.05,  0.15 }, 0.05 },  //b... Hals
    { 1.00, 0.90,  { 0.00, 0.12,  0.20,  0.05,  0.17 }, 0.00 },  //c... Ruecken
    { 1.20, 1.30,  { 0.00, 0.15,  0.15,  0.05,  0.20 }, 0.00 },  //d... Bauch
    { 0.60, 0.50,  { 0.00, 0.10,  0.10,  0.05,  0.15 }, 0.00 },  //e... Schwanz
    { 0.40, 0.50,  { 0.00, 0.10,  0.10,  0.05,  0.15 }, 0.00 },  //f... Fluegel
    { 0.35, 0.30,  { 0.00, 0.15,  0.10,  0.05,  0.20 }, 0.00 }   //g... Fuss
};

//Gypceros (Normal u. Violett)
static const Hitzone hitzonen_af[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.30, 0.80,  { 0.50, 0.20,  0.00,  0.10,  0.20 }, 0.50 },  //a... Kopf
    { 0.50, 0.25,  { 0.30, 0.10,  0.00,  0.10,  0.10 }, 0.30 },  //b... Hals
    { 0.70, 0.10,  { 0.20, 0.10,  0.00,  0.10,  0.10 }, 0.20 },  //c... Ruecken
    { 0.80, 0.30,  { 0.30, 0.10,  0.00,  0.10,  0.10 }, 0.30 },  //d... Bauch
    { 0.90, 0.40,  { 0.30, 0.10,  0.00,  0.00,  0.15 }, 0.30 },  //e... Schwanz
    { 0.50, 0.30,  { 0.30, 0.20,  0.00,  0.10,  0.20 }, 0.30 },  //f... Fluegel
    { 0.20, 0.20,  { 0.10, 0.10,  0.00,  0.10,  0.10 }, 0.10 }   //g... Fuss
};

//Rathalos (Normal)
static const Hitzone hitzonen_ag[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.80, 0.70,  { 0.00, 0.30,  0.15,  0.30,  0.40 }, 0.00 },  //a... Kopf
    { 0.50, 0.50,  { 0.00, 0.20,  0.10,  0.20,  0.30 }, 0.00 },  //b... Hals
    { 0.25, 0.30,  { 0.15, 0.20,  0.15,  0.20,  0.30 }, 0.15 },  //c... Ruecken
    { 0.45, 0.40,  { 0.00, 0.15,  0.15,  0.10,  0.25 }, 0.00 },  //d... Bauch
    { 0.30, 0.30,  { 0.00, 0.10,  0.05,  0.20,  0.20 }, 0.00 },  //e... Schwanz
    { 0.25, 0.20,  { 0.00, 0.30,  0.15,  0.20,  0.40 }, 0.00 },  //f... Fluegel
    { 0.45, 0.50,  { 0.00, 0.10,  0.05,  0.05,  0.20 }, 0.00 }   //g... Fuss
};

//Rathalos (Azur)
static const Hitzone hitzonen_ah[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.80, 0.70,  { 0.00, 0.15,  0.15,  0.30,  0.20 }, 0.00 },  //a... Kopf
    { 0.50, 0.50,  { 0.00, 0.10,  0.10,  0.20,  0.20 }, 0.00 },  //b... Hals
    { 0.20, 0.25,  { 0.10, 0.15,  0.15,  0.20,  0.20 }, 0.10 },  //c... Ruecken
    { 0.45, 0.40,  { 0.00, 0.15,  0.15,  0.10,  0.20 }, 0.00 },  //d... Bauch
    { 0.30, 0.30,  { 0.00, 0.05,  0.05,  0.20,  0.05 }, 0.00 },  //e... Schwanz
    { 0.25, 0.20,  { 0.00, 0.15,  0.15,  0.20,  0.20 }, 0.00 },  //f... Fluegel
    { 0.45, 0.40,  { 0.00, 0.05,  0.05,  0.05,  0.05 }, 0.00 }   //g... Fuss
};

//Rathalos (Silber)
static const Hitzone hitzonen_ai[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.25, 0.40,  { 0.00, 0.30,  0.30,  0.00,  0.10 }, 0.00 },  //a... Kopf
    { 0.35, 0.45,  { 0.00, 0.20,  0.20,  0.00,  0.05 }, 0.00 },  //b... Hals
    { 0.20, 0.25,  { 0.05, 0.20,  0.20,  0.00,  0.05 }, 0.05 },  //c... Ruecken
    { 0.35, 0.45,  { 0.00, 0.15,  0.15,  0.00,  0.10 }, 0.00 },  //d... Bauch
    { 0.25, 0.25,  { 0.00, 0.10,  0.10,  0.00,  0.05 }, 0.00 },  //e... Schwanz
    { 0.50, 0.50,  { 0.00, 0.30,  0.30,  0.00,  0.10 }, 0.00 },  //f... Fluegel
    { 0.35, 0.45,  { 0.00, 0.10,  0.10,  0.00,  0.05 }, 0.00 }   //g... Fuss
};

//Rathian (Normal)
static const Hitzone hitzonen_aj[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.80, 0.70,  { 0.00, 0.15,  0.20,  0.35,  0.15 }, 0.00 },  //a... Kopf
    { 0.50, 0.40,  { 0.00, 0.10,  0.15,  0.20,  0.10 }, 0.00 },  //b... Hals
    { 0.25, 0.30,  { 0.10, 0.10,  0.15,  0.25,  0.10 }, 0.10 },  //c... Ruecken
    { 0.60, 0.45,  { 0.00, 0.05,  0.20,  0.10,  0.05 }, 0.00 },  //d... Bauch
    { 0.30, 0.30,  { 0.00, 0.05,  0.10,  0.25,  0.05 }, 0.00 },  //e... Schwanz
    { 0.25, 0.20,  { 0.00, 0.10,  0.15,  0.15,  0.10 }, 0.00 },  //f... Fluegel
    { 0.45, 0.40,  { 0.00, 0.05,  0.10,  0.05,  0.05 }, 0.00 }   //g... Fuss
};

//Rathian (Pink)
static const Hitzone hitzonen_ak[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.80, 0.70,  { 0.00, 0.15,  0.20,  0.35,  0.10 }, 0.00 },  //a... Kopf
    { 0.50, 0.40,  { 0.00, 0.10,  0.15,  0.20,  0.05 }, 0.00 },  //b... Hals
    { 0.30, 0.35,  { 0.10, 0.10,  0.15,  0.25,  0.10 }, 0.10 },  //c... Ruecken
    { 0.60, 0.55,  { 0.00, 0.05,  0.20,  0.10,  0.05 }, 0.00 },  //d... Bauch
    { 0.30, 0.30,  { 0.00, 0.05,  0.10,  0.25,  0.10 }, 0.00 },  //e... Schwanz
    { 0.20, 0.15,  { 0.00, 0.10,  0.15,  0.20,  0.10 }, 0.00 },  //f... Fluegel
    { 0.55, 0.50,  { 0.00, 0.05,  0.10,  0.05,  0.05 }, 0.00 }   //g... Fuss
};

//Rathian (Gold)
static const Hitzone hitzonen_al[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.25, 0.40,  { 0.00, 0.15,  0.30,  0.00,  0.05 }, 0.00 },  //a... Kopf
    { 0.35, 0.50,  { 0.00, 0.10,  0.20,  0.00,  0.05 }, 0.00 },  //b... Hals
    { 0.20, 0.25,  { 0.05, 0.10,  0.20,  0.00,  0.05 }, 0.05 },  //c... Ruecken
    { 0.35, 0.45,  { 0.00, 0.05,  0.25,  0.00,  0.05 }, 0.00 },  //d... Bauch
    { 0.25, 0.25,  { 0.00, 0.05,  0.10,  0.00,  0.05 }, 0.00 },  //e... Schwanz
    { 0.50, 0.50,  { 0.00, 0.10,  0.30,  0.00,  0.05 }, 0.00 },  //f... Fluegel
    { 0.35, 0.40,  { 0.00, 0.05,  0.10,  0.00,  0.05 }, 0.00 }   //g... Fuss
};

//Khezu (Normal)
static const Hitzone hitzonen_am[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.45, 0.60,  { 0.40, 0.05,  0.00,  0.05,  0.05 }, 0.40 },  //a... Kopf
    { 0.60, 0.50,  { 0.30, 0.05,  0.00,  0.05,  0.05 }, 0.30 },  //b... Hals
    { 0.50, 0.50,  { 0.30, 0.05,  0.00,  0.05,  0.05 }, 0.30 },  //c... Ruecken
    { 0.50, 0.25,  { 0.30, 0.05,  0.00,  0.05,  0.05 }, 0.30 },  //d... Bauch
    { 0.20, 0.20,  { 0.20, 0.05,  0.00,  0.05,  0.05 }, 0.20 },  //e... Schwanz
    { 0.25, 0.25,  { 0.20, 0.05,  0.00,  0.05,  0.05 }, 0.20 },  //f... Fluegel
    { 0.25, 0.20,  { 0.15, 0.05,  0.00,  0.05,  0.05 }, 0.15 }   //g... Fuss
};

//Khezu (Rot)
static const Hitzone hitzonen_an[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.45, 0.60,  { 0.05, 0.40,  0.00,  0.05,  0.05 }, 0.05 },  //a... Kopf
    { 0.60, 0.50,  { 0.05, 0.30,  0.00,  0.05,  0.05 }, 0.05 },  //b... Hals
    { 0.50, 0.50,  { 0.05, 0.30,  0.00,  0.05,  0.05 }, 0.05 },  //c... Ruecken
    { 0.50, 0.25,  { 0.05, 0.30,  0.00,  0.05,  0.05 }, 0.05 },  //d... Bauch
    { 0.20, 0.20,  { 0.05, 0.20,  0.00,  0.05,  0.05 }, 0.05 },  //e... Schwanz
    { 0.25, 0.25,  { 0.05, 0.20,  0.00,  0.05,  0.05 }, 0.05 },  //f... Fluegel
    { 0.25, 0.20,  { 0.05, 0.15,  0.00,  0.05,  0.05 }, 0.05 }   //g... Fuss
};

//Basarios
static const Hitzone hitzonen_ao[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.20, 0.20,  { 0.10, 0.15,  0.10,  0.18,  0.10 }, 0.10 },  //a... Kopf
    { 0.15, 0.20,  { 0.10, 0.15,  0.10,  0.18,  0.10 }, 0.10 },  //b... Hals
    { 0.15, 0.20,  { 0.10, 0.15,  0.10,  0.18,  0.10 }, 0.10 },  //c... Ruecken
    { 0.20, 0.25,  { 0.10, 0.20,  0.10,  0.18,  0.10 }, 0.10 },  //d... Bauch (Nicht gebrochen)
    { 0.75, 0.75,  { 0.25, 0.35,  0.10,  0.33,  0.10 }, 0.25 },  //e... Bauch (Gebrochen)
    { 0.15, 0.15,  { 0.10, 0.15,  0.10,  0.18,  0.10 }, 0.10 },  //f... Schwanz
    { 0.15, 0.20,  { 0.10, 0.15,  0.10,  0.18,  0.10 }, 0.10 },  //g... Fluegel
    { 0.20, 0.25,  { 0.10, 0.10,  0.10,  0.10,  0.10 }, 0.10 }   //h... Fuss
};

//Gravios (Normal)
static const Hitzone hitzonen_ap[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.20, 0.30,  { 0.00, 0.15,  0.05,  0.40,  0.05 }, 0.00 },  //a... Kopf
    { 0.30, 0.30,  { 0.00, 0.30,  0.05,  0.20,  0.10 }, 0.00 },  //b... Hals
    { 0.20, 0.30,  { 0.00, 0.25,  0.05,  0.15,  0.10 }, 0.00 },  //c... Ruecken
    { 0.25, 0.30,  { 0.00, 0.35,  0.05,  0.35,  0.15 }, 0.00 },  //d... Bauch (Nicht gebrochen)
    { 0.80, 0.80,  { 0.15, 0.50,  0.05,  0.50,  0.15 }, 0.15 },  //e... Bauch (Gebrochen)
    { 0.25, 0.30,  { 0.00, 0.30,  0.05,  0.20,  0.10 }, 0.00 },  //f... Schwanz
    { 0.20, 0.20,  { 0.00, 0.20,  0.05,  0.15,  0.05 }, 0.00 },  //g... Fluegel
    { 0.15, 0.25,  { 0.00, 0.30,  0.05,  0.10,  0.05 }, 0.00 }   //h... Fuss
};

//Gravios (Schwarz)
static const Hitzone hitzonen_aq[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.20, 0.30,  { 0.00, 0.15,  0.05,  0.00,  0.00 }, 0.00 },  //a... Kopf
    { 0.20, 0.30,  { 0.00, 0.30,  0.05,  0.00,  0.00 }, 0.00 },  //b... Hals
    { 0.20, 0.30,  { 0.00, 0.25,  0.05,  0.00,  0.00 }, 0.00 },  //c... Ruecken
    { 0.20, 0.25,  { 0.00, 0.40,  0.05,  0.00,  0.00 }, 0.00 },  //d... Bauch (Nicht gebrochen)
    { 0.80, 0.80,  { 0.10, 0.90,  0.05,  0.30,  0.00 }, 0.10 },  //e... Bauch (Gebrochen)
    { 0.25, 0.30,  { 0.00, 0.30,  0.05,  0.00,  0.00 }, 0.00 },  //f... Schwanz
    { 0.20, 0.20,  { 0.00, 0.20,  0.05,  0.00,  0.00 }, 0.00 },  //g... Fluegel
    { 0.15, 0.25,  { 0.00, 0.30,  0.05,  0.00,  0.00 }, 0.00 }   //h... Fuss
};

//Monoblos (Normal u. Weiss)
static const Hitzone hitzonen_ar[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.25, 0.20,  { 0.05, 0.00,  0.20,  0.00,  0.30 }, 0.05 },  //a... Kopf
    { 0.50, 0.80,  { 0.10, 0.00,  0.30,  0.00,  0.20 }, 0.10 },  //b... Hals
    { 0.20, 0.30,  { 0.05, 0.00,  0.20,  0.00,  0.30 }, 0.05 },  //c... Ruecken
    { 0.50, 0.45,  { 0.05, 0.00,  0.30,  0.00,  0.20 }, 0.05 },  //d... Bauch
    { 0.80, 0.60,  { 0.10, 0.00,  0.25,  0.00,  0.20 }, 0.10 },  //e... Schwanz
    { 0.60, 0.50,  { 0.10, 0.00,  0.30,  0.00,  0.20 }, 0.10 },  //f... Fluegel
    { 0.40, 0.45,  { 0.20, 0.00,  0.20,  0.00,  0.10 }, 0.20 }   //g... Fuss
};

//Diablos (Normal u. Schwarz)
static const Hitzone hitzonen_as[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.25, 0.10,  { 0.00, 0.15,  0.15,  0.15,  0.30 }, 0.00 },  //a... Kopf
    { 0.45, 0.65,  { 0.00, 0.10,  0.15,  0.15,  0.20 }, 0.00 },  //b... Hals
    { 0.20, 0.35,  { 0.10, 0.15,  0.15,  0.15,  0.30 }, 0.10 },  //c... Ruecken
    { 0.40, 0.50,  { 0.00, 0.10,  0.15,  0.15,  0.20 }, 0.00 },  //d... Bauch
    { 0.60, 0.55,  { 0.00, 0.10,  0.15,  0.15,  0.20 }, 0.00 },  //e... Schwanz
    { 0.40, 0.55,  { 0.00, 0.10,  0.15,  0.15,  0.20 }, 0.00 },  //f... Fluegel
    { 0.30, 0.35,  { 0.00, 0.05,  0.15,  0.15,  0.10 }, 0.00 }   //g... Fuss
};

//Plesioth (Normal u. Gruen)
static const Hitzone hitzonen_at[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.30, 0.30,  { 0.25, 0.00,  0.25,  0.05,  0.05 }, 0.25 },  //a... Kopf
    { 0.60, 0.50,  { 0.30, 0.00,  0.30,  0.05,  0.05 }, 0.30 },  //b... Hals
    { 0.20, 0.20,  { 0.20, 0.00,  0.25,  0.05,  0.05 }, 0.20 },  //c... Ruecken
    { 1.00, 0.80,  { 0.35, 0.00,  0.35,  0.05,  0.05 }, 0.35 },  //d... Bauch
    { 0.50, 0.40,  { 0.30, 0.00,  0.30,  0.05,  0.05 }, 0.30 },  //e... Schwanz
    { 0.25, 0.25,  { 0.20, 0.00,  0.20,  0.05,  0.05 }, 0.20 },  //f... Fluegel
    { 0.50, 0.40,  { 0.10, 0.00,  0.10,  0.05,  0.05 }, 0.10 }   //g... Fuss
};

//Lavasioth
static const Hitzone hitzonen_au[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.35, 0.35,  { 0.00, 0.25,  0.05,  0.05,  0.15 }, 0.00 },  //a... Kopf
    { 0.35, 0.35,  { 0.00, 0.25,  0.15,  0.05,  0.10 }, 0.00 },  //b... Hals
    { 0.20, 0.45,  { 0.00, 0.20,  0.05,  0.05,  0.05 }, 0.00 },  //c... Ruecken
    { 0.40, 0.35,  { 0.00, 0.30,  0.15,  0.05,  0.15 }, 0.00 },  //d... Bauch
    { 0.40, 0.30,  { 0.00, 0.20,  0.15,  0.05,  0.10 }, 0.00 },  //e... Schwanz
    { 0.25, 0.40,  { 0.00, 0.20,  0.10,  0.05,  0.10 }, 0.00 },  //f... Fluegel
    { 0.30, 0.40,  { 0.00, 0.20,  0.05,  0.05,  0.05 }, 0.00 }   //g... Fuss
};

//Congalala (Normal, Bauch nicht nach aussen)
//BEKANNTER FEHLER (nicht behoben): fire_hitzone stimmt hier nicht immer mit dem Feuer-Wert überein
static const Hitzone hitzonen_av[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.65, 0.75,  { 0.30, 0.10,  0.10,  0.00,  0.15 }, 0.30 },  //a... Kopf
    { 0.45, 0.40,  { 0.25, 0.10,  0.10,  0.00,  0.15 }, 0.15 },  //b... Vordere Beine
    { 0.45, 0.40,  { 0.15, 0.10,  0.10,  0.00,  0.15 }, 0.15 },  //c... Hintere Beine
    { 0.40, 0.40,  { 0.18, 0.10,  0.10,  0.00,  0.15 }, 0.18 },  //d... Koerper
    { 0.40, 0.30,  { 0.15, 0.10,  0.10,  0.00,  0.15 }, 0.15 }   //e... Schwanz
};

//Congalala (Bauch nach aussen)
static const Hitzone hitzonen_aw[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.65, 0.75,  { 0.30, 0.10,  0.10,  0.00,  0.05 }, 0.30 },  //a... Kopf
    { 0.65, 0.15,  { 0.15, 0.10,  0.10,  0.00,  0.15 }, 0.15 },  //b... Vordere Beine
    { 0.65, 0.15,  { 0.15, 0.10,  0.10,  0.00,  0.15 }, 0.15 },  //c... Hintere Beine
    { 0.05, 0.05,  { 0.18, 0.00,  0.00,  0.00,  0.00 }, 0.18 },  //d... Koerper
    { 0.35, 0.30,  { 0.15, 0.10,  0.10,  0.00,  0.05 }, 0.15 }   //e... Schwanz
};

//Congalala (Gruen, Bauch nicht nach aussen)
static const Hitzone hitzonen_ax[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.30, 0.50,  { 0.05, 0.10,  0.10,  0.00,  0.15 }, 0.05 },  //a... Kopf
    { 0.30, 0.25,  { 0.05, 0.10,  0.10,  0.00,  0.15 }, 0.05 },  //b... Vordere Beine
    { 0.30, 0.25,  { 0.05, 0.10,  0.10,  0.00,  0.15 }, 0.05 },  //c... Hintere Beine
    { 0.40, 0.40,  { 0.05, 0.10,  0.10,  0.00,  0.18 }, 0.05 },  //d... Koerper
    { 0.50, 0.30,  { 0.05, 0.10,  0.10,  0.00,  0.40 }, 0.05 }   //e... Schwanz
};

//Congalala (Gruen, Bauch nach aussen)
static const Hitzone hitzonen_ay[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.30, 0.50,  { 0.05, 0.10,  0.10,  0.00,  0.15 }, 0.05 },  //a... Kopf
    { 0.30, 0.25,  { 0.05, 0.10,  0.10,  0.00,  0.15 }, 0.05 },  //b... Vordere Beine
    { 0.30, 0.25,  { 0.05, 0.10,  0.10,  0.00,  0.15 }, 0.05 },  //c... Hintere Beine
    { 0.05, 0.05,  { 0.00, 0.10,  0.10,  0.00,  0.18 }, 0.00 },  //d... Koerper
    { 0.50, 0.30,  { 0.05, 0.10,  0.10,  0.00,  0.40 }, 0.05 }   //e... Schwanz
};

//Blangonga (Normal)
static const Hitzone hitzonen_az[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.70, 0.70,  { 0.30, 0.05,  0.15,  0.00,  0.00 }, 0.30 },  //a... Kopf
    { 0.45, 0.40,  { 0.15, 0.05,  0.15,  0.00,  0.00 }, 0.15 },  //b... Vordere Beine
    { 0.45, 0.40,  { 0.15, 0.05,  0.15,  0.00,  0.00 }, 0.15 },  //c... Hintere Beine
    { 0.45, 0.50,  { 0.20, 0.05,  0.15,  0.00,  0.00 }, 0.20 },  //d... Koerper
    { 0.50, 0.40,  { 0.15, 0.05,  0.15,  0.00,  0.00 }, 0.15 }   //e... Schwanz
};

//Blangonga (Kupfer)
static const Hitzone hitzonen_ba[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.65, 0.70,  { 0.00, 0.15,  0.05,  0.00,  0.30 }, 0.00 },  //a... Kopf
    { 0.45, 0.40,  { 0.00, 0.15,  0.05,  0.00,  0.15 }, 0.00 },  //b... Vordere Beine
    { 0.20, 0.20,  { 0.00, 0.15,  0.05,  0.00,  0.15 }, 0.00 },  //c... Hintere Beine
    { 0.30, 0.30,  { 0.00, 0.15,  0.05,  0.00,  0.20 }, 0.00 },  //d... Koerper
    { 0.50, 0.40,  { 0.10, 0.15,  0.05,  0.00,  0.15 }, 0.10 }   //e... Schwanz
};

//Daimyo Hermitaur (Normal, nicht in Block-Position)
static const Hitzone hitzonen_bb[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.50, 0.60,  { 0.30, 0.10,  0.35,  0.00,  0.20 }, 0.30 },  //a... Kopf
    { 0.40, 0.50,  { 0.15, 0.05,  0.20,  0.00,  0.10 }, 0.15 },  //b... Koerper
    { 0.25, 0.40,  { 0.20, 0.05,  0.25,  0.00,  0.10 }, 0.20 },  //c... Panzer
    { 0.35, 0.40,  { 0.15, 0.05,  0.15,  0.00,  0.10 }, 0.15 },  //d... Fuss
    { 0.25, 0.30,  { 0.35, 0.05,  0.20,  0.00,  0.10 }, 0.35 },  //e... Zange, Schere
    { 0.40, 0.45,  { 0.15, 0.05,  0.15,  0.00,  0.10 }, 0.15 }   //f... Arm
};

//Daimyo Hermitaur (Normal, in Block-Position)
static const Hitzone hitzonen_bc[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.20, 0.40,  { 0.30, 0.10,  0.35,  0.00,  0.20 }, 0.30 },  //a... Kopf
    { 0.20, 0.30,  { 0.15, 0.05,  0.20,  0.00,  0.10 }, 0.15 },  //b... Koerper
    { 0.20, 0.30,  { 0.20, 0.05,  0.25,  0.00,  0.10 }, 0.20 },  //c... Panzer
    { 0.20, 0.30,  { 0.15, 0.05,  0.15,  0.00,  0.10 }, 0.15 },  //d... Fuss
    { 0.40, 0.50,  { 0.35, 0.05,  0.20,  0.00,  0.10 }, 0.35 },  //e... Zange, Schere
    { 0.20, 0.30,  { 0.15, 0.05,  0.15,  0.00,  0.10 }, 0.15 }   //f... Arm
};

//Daimyo Hermitaur (Pflaumenfarben, nicht in Block-Position)
static const Hitzone hitzonen_bd[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.50, 0.60,  { 0.20, 0.10,  0.25,  0.00,  0.35 }, 0.20 },  //a... Kopf
    { 0.40, 0.50,  { 0.10, 0.05,  0.10,  0.00,  0.20 }, 0.10 },  //b... Koerper
    { 0.25, 0.40,  { 0.10, 0.05,  0.20,  0.00,  0.25 }, 0.10 },  //c... Panzer
    { 0.35, 0.40,  { 0.10, 0.05,  0.10,  0.00,  0.15 }, 0.10 },  //d... Fuss
    { 0.25, 0.30,  { 0.10, 0.05,  0.20,  0.00,  0.35 }, 0.10 },  //e... Zange, Schere
    { 0.40, 0.45,  { 0.10, 0.05,  0.10,  0.00,  0.15 }, 0.10 }   //f... Arm
};

//Daimyo Hermitaur (Pflaumenfarben, in Block-Position)
static const Hitzone hitzonen_be[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.20, 0.40,  { 0.20, 0.10,  0.25,  0.00,  0.35 }, 0.20 },  //a... Kopf
    { 0.20, 0.30,  { 0.10, 0.05,  0.10,  0.00,  0.20 }, 0.10 },  //b... Koerper
    { 0.20, 0.30,  { 0.10, 0.05,  0.20,  0.00,  0.25 }, 0.10 },  //c... Panzer
    { 0.20, 0.30,  { 0.10, 0.05,  0.10,  0.00,  0.15 }, 0.10 },  //d... Fuss
    { 0.40, 0.50,  { 0.10, 0.05,  0.20,  0.00,  0.35 }, 0.10 },  //e... Zange, Schere
    { 0.20, 0.30,  { 0.10, 0.05,  0.10,  0.00,  0.15 }, 0.10 }   //f... Arm
};

//Shogun Ceanataur (Normal, mit Panzer)
static const Hitzone hitzonen_bf[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.45, 0.55,  { 0.10, 0.10,  0.35,  0.00,  0.20 }, 0.10 },  //a... Kopf
    { 0.35, 0.40,  { 0.05, 0.05,  0.20,  0.00,  0.10 }, 0.05 },  //b... Koerper
    { 0.25, 0.40,  { 0.05, 0.05,  0.25,  0.00,  0.10 }, 0.05 },  //c... Panzer
    { 0.25, 0.25,  { 0.05, 0.05,  0.15,  0.00,  0.10 }, 0.05 },  //d... Fuss
    { 0.20, 0.30,  { 0.05, 0.05,  0.20,  0.00,  0.10 }, 0.05 },  //e... Zange, Schere
    { 0.20, 0.35,  { 0.05, 0.05,  0.15,  0.00,  0.10 }, 0.05 }   //f... Arm
};

//Shogun Ceanataur (Normal, ohne Panzer)
static const Hitzone hitzonen_bg[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.50, 0.60,  { 0.10, 0.10,  0.35,  0.00,  0.20 }, 0.10 },  //a... Kopf
    { 0.40, 0.50,  { 0.05, 0.05,  0.20,  0.00,  0.10 }, 0.05 },  //b... Koerper
    { 0.25, 0.35,  { 0.05, 0.05,  0.25,  0.00,  0.10 }, 0.05 },  //c... Panzer
    { 0.35, 0.50,  { 0.05, 0.05,  0.15,  0.00,  0.10 }, 0.05 },  //d... Fuss
    { 0.25, 0.35,  { 0.05, 0.05,  0.20,  0.00,  0.10 }, 0.05 },  //e... Zange, Schere
    { 0.30, 0.35,  { 0.05, 0.05,  0.15,  0.00,  0.10 }, 0.05 }   //f... Arm
};

//Shogun Ceanataur (Terra, mit Panzer)
static const Hitzone hitzonen_bh[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.45, 0.55,  { 0.10, 0.10,  0.35,  0.00,  0.20 }, 0.10 },  //a... Kopf
    { 0.35, 0.40,  { 0.05, 0.05,  0.20,  0.00,  0.10 }, 0.05 },  //b... Koerper
    { 0.20, 0.25,  { 0.00, 0.40,  0.05,  0.00,  0.00 }, 0.00 },  //c... Panzer
    { 0.25, 0.25,  { 0.05, 0.05,  0.15,  0.00,  0.10 }, 0.05 },  //d... Fuss
    { 0.25, 0.30,  { 0.05, 0.05,  0.20,  0.00,  0.10 }, 0.05 },  //e... Zange, Schere
    { 0.25, 0.35,  { 0.05, 0.05,  0.15,  0.00,  0.10 }, 0.05 }   //f... Arm
};

//Shogun Ceanataur (Terra, ohne Panzer)
static const Hitzone hitzonen_bi[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.50, 0.60,  { 0.10, 0.10,  0.35,  0.00,  0.20 }, 0.10 },  //a... Kopf
    { 0.60, 0.50,  { 0.05, 0.05,  0.20,  0.00,  0.10 }, 0.05 },  //b... Koerper
    { 0.25, 0.35,  { 0.05, 0.05,  0.25,  0.00,  0.10 }, 0.05 },  //c... Panzer
    { 0.35, 0.35,  { 0.05, 0.05,  0.15,  0.00,  0.10 }, 0.05 },  //d... Fuss
    { 0.25, 0.35,  { 0.05, 0.05,  0.20,  0.00,  0.10 }, 0.05 },  //e... Zange, Schere
    { 0.30, 0.35,  { 0.05, 0.05,  0.15,  0.00,  0.10 }, 0.05 }   //f... Arm
};

//Tigrex (Normal)
static const Hitzone hitzonen_bj[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.75, 0.65,  { 0.00, 0.15,  0.30,  0.20,  0.05 }, 0.00 },  //a... Kopf
    { 0.50, 0.50,  { 0.00, 0.10,  0.20,  0.15,  0.05 }, 0.00 },  //b... Hals
    { 0.45, 0.40,  { 0.00, 0.15,  0.25,  0.05,  0.10 }, 0.00 },  //c... Ruecken
    { 0.25, 0.30,  { 0.15, 0.15,  0.25,  0.15,  0.10 }, 0.15 },  //d... Bauch
    { 0.35, 0.30,  { 0.00, 0.05,  0.15,  0.15,  0.00 }, 0.00 },  //e... Schwanz
    { 0.25, 0.20,  { 0.00, 0.15,  0.20,  0.15,  0.10 }, 0.00 },  //f... Vordere Beine
    { 0.40, 0.45,  { 0.00, 0.05,  0.15,  0.05,  0.00 }, 0.00 }   //g... Hintere Beine
};

//Tigrex (Wuetend)
static const Hitzone hitzonen_bk[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.80, 0.70,  { 0.00, 0.15,  0.35,  0.20,  0.05 }, 0.00 },  //a... Kopf
    { 0.55, 0.55,  { 0.00, 0.10,  0.20,  0.15,  0.05 }, 0.00 },  //b... Hals
    { 0.50, 0.45,  { 0.00, 0.15,  0.25,  0.05,  0.10 }, 0.00 },  //c... Ruecken
    { 0.30, 0.35,  { 0.15, 0.15,  0.25,  0.15,  0.10 }, 0.15 },  //d... Bauch
    { 0.35, 0.30,  { 0.00, 0.05,  0.15,  0.15,  0.00 }, 0.00 },  //e... Schwanz
    { 0.30, 0.25,  { 0.00, 0.15,  0.25,  0.15,  0.10 }, 0.00 },  //f... Vordere Beine
    { 0.40, 0.45,  { 0.00, 0.05,  0.15,  0.05,  0.00 }, 0.00 }   //g... Hintere Beine
};

//Nargacuga
//BEKANNTER FEHLER (nicht behoben): fire_hitzone stimmt hier nicht immer mit dem Feuer-Wert überein
static const Hitzone hitzonen_bl[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.50, 0.50,  { 0.35, 0.10,  0.30,  0.15,  0.10 }, 0.35 },  //a... Kopf
    { 0.30, 0.40,  { 0.25, 0.05,  0.20,  0.10,  0.05 }, 0.35 },  //b... Hals
    { 0.20, 0.20,  { 0.30, 0.05,  0.35,  0.15,  0.10 }, 0.25 },  //c... Ruecken
    { 0.25, 0.25,  { 0.20, 0.00,  0.15,  0.05,  0.00 }, 0.30 },  //d... Bauch
    { 0.35, 0.30,  { 0.25, 0.00,  0.20,  0.10,  0.00 }, 0.20 },  //e... Schwanz
    { 0.25, 0.25,  { 0.20, 0.05,  0.25,  0.10,  0.05 }, 0.25 },  //f... Vordere Beine
    { 0.30, 0.30,  { 0.15, 0.00,  0.10,  0.05,  0.00 }, 0.20 }   //g... Hintere Beine
};

//Rajang
static const Hitzone hitzonen_bm[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.60, 0.60,  { 0.05, 0.05,  0.00,  0.00,  0.30 }, 0.05 },  //a... Kopf
    { 0.45, 0.40,  { 0.00, 0.05,  0.00,  0.00,  0.15 }, 0.00 },  //b... Vordere Beine
    { 0.45, 0.40,  { 0.00, 0.05,  0.00,  0.00,  0.15 }, 0.00 },  //c... Hintere Beine
    { 0.45, 0.50,  { 0.00, 0.05,  0.00,  0.00,  0.15 }, 0.00 },  //d... (nicht im Menue)
    { 0.50, 0.40,  { 0.00, 0.05,  0.00,  0.00,  0.20 }, 0.00 }   //e... Koerper
};

//Kirin
static const Hitzone hitzonen_bn[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.45, 0.40,  { 0.15, 0.15,  0.00,  0.05,  0.10 }, 0.15 },  //a... Kopf
    { 0.20, 0.25,  { 0.10, 0.10,  0.00,  0.05,  0.10 }, 0.10 },  //b... Koerper
    { 0.90, 1.00,  { 0.05, 0.05,  0.00,  0.05,  0.05 }, 0.05 }   //c... Horn
};

//Akantor (Normal)
static const Hitzone hitzonen_bo[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.50, 0.60,  { 0.00, 0.05,  0.20,  0.40,  0.00 }, 0.00 },  //a... Kopf
    { 0.35, 0.50,  { 0.00, 0.05,  0.15,  0.20,  0.00 }, 0.00 },  //b... Hals
    { 0.20, 0.25,  { 0.10, 0.05,  0.15,  0.20,  0.00 }, 0.10 },  //c... Ruecken
    { 0.35, 0.50,  { 0.00, 0.05,  0.15,  0.20,  0.00 }, 0.00 },  //d... Bauch
    { 0.30, 0.35,  { 0.00, 0.05,  0.30,  0.40,  0.00 }, 0.00 },  //e... Schwanz
    { 0.30, 0.35,  { 0.00, 0.05,  0.15,  0.20,  0.00 }, 0.00 },  //f... Vordere Beine
    { 0.30, 0.30,  { 0.00, 0.05,  0.15,  0.30,  0.00 }, 0.00 }   //g... Hintere Beine
};

//Akantor (Wuetend)
static const Hitzone hitzonen_bp[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.50, 0.60,  { 0.00, 0.20,  0.20,  0.20,  0.00 }, 0.00 },  //a... Kopf
    { 0.35, 0.50,  { 0.00, 0.15,  0.15,  0.10,  0.00 }, 0.00 },  //b... Hals
    { 0.30, 0.30,  { 0.00, 0.15,  0.15,  0.10,  0.00 }, 0.00 },  //c... Ruecken
    { 0.50, 0.60,  { 0.15, 0.25,  0.40,  0.10,  0.00 }, 0.15 },  //d... Bauch
    { 0.30, 0.30,  { 0.00, 0.10,  0.30,  0.10,  0.00 }, 0.00 },  //e... Schwanz
    { 0.30, 0.35,  { 0.00, 0.10,  0.15,  0.15,  0.00 }, 0.00 },  //f... Vordere Beine
    { 0.20, 0.15,  { 0.00, 0.10,  0.15,  0.10,  0.00 }, 0.00 }   //g... Hintere Beine
};

//Ukanlos (Bauch ist nicht gebrochen)
static const Hitzone hitzonen_bq[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.50, 0.60,  { 0.30, 0.05,  0.20,  0.20,  0.00 }, 0.30 },  //a... Kopf
    { 0.35, 0.50,  { 0.20, 0.05,  0.15,  0.10,  0.00 }, 0.20 },  //b... Hals
    { 0.30, 0.35,  { 0.20, 0.05,  0.15,  0.10,  0.00 }, 0.20 },  //c... Ruecken
    { 0.35, 0.50,  { 0.20, 0.05,  0.15,  0.10,  0.00 }, 0.20 },  //d... Bauch
    { 0.30, 0.30,  { 0.30, 0.05,  0.15,  0.15,  0.00 }, 0.30 },  //e... Schwanz
    { 0.20, 0.25,  { 0.20, 0.05,  0.15,  0.10,  0.00 }, 0.20 },  //f... Vordere Beine
    { 0.30, 0.35,  { 0.35, 0.05,  0.30,  0.20,  0.00 }, 0.35 }   //g... Hintere Beine
};

//Ukanlos (Bauch ist gebrochen)
static const Hitzone hitzonen_br[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.50, 0.60,  { 0.30, 0.05,  0.20,  0.15,  0.00 }, 0.30 },  //a... Kopf
    { 0.35, 0.50,  { 0.20, 0.05,  0.15,  0.10,  0.00 }, 0.20 },  //b... Hals
    { 0.30, 0.35,  { 0.25, 0.05,  0.15,  0.10,  0.00 }, 0.25 },  //c... Ruecken
    { 0.50, 0.60,  { 0.20, 0.05,  0.40,  0.10,  0.00 }, 0.20 },  //d... Bauch
    { 0.20, 0.15,  { 0.30, 0.05,  0.15,  0.15,  0.00 }, 0.30 },  //e... Schwanz
    { 0.30, 0.30,  { 0.20, 0.05,  0.15,  0.10,  0.00 }, 0.20 },  //f... Vordere Beine
    { 0.30, 0.30,  { 0.40, 0.05,  0.30,  0.10,  0.00 }, 0.40 }   //g... Hintere Beine
};

//Kushala Daora (Normal)
static const Hitzone hitzonen_bs[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.50, 0.60,  { 0.10, 0.05,  0.30,  0.35,  0.00 }, 0.10 },  //a... Kopf, Hals
    { 0.35, 0.50,  { 0.10, 0.05,  0.15,  0.20,  0.00 }, 0.10 },  //b... Bauch
    { 0.20, 0.25,  { 0.10, 0.05,  0.15,  0.20,  0.00 }, 0.10 },  //c... Ruecken
    { 0.35, 0.30,  { 0.10, 0.05,  0.30,  0.30,  0.00 }, 0.10 },  //d... Schwanz
    { 0.30, 0.25,  { 0.10, 0.05,  0.20,  0.20,  0.00 }, 0.10 },  //e... Vordere Beine
    { 0.30, 0.35,  { 0.10, 0.05,  0.20,  0.30,  0.00 }, 0.10 },  //f... Hintere Beine
    { 0.20, 0.15,  { 0.10, 0.05,  0.15,  0.20,  0.00 }, 0.10 }   //g... Fluegel
};

//Kushala Daora (Wind Modus)
static const Hitzone hitzonen_bt[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.45, 0.55,  { 0.15, 0.05,  0.35,  0.40,  0.00 }, 0.15 },  //a... Kopf, Hals
    { 0.25, 0.40,  { 0.05, 0.00,  0.20,  0.10,  0.00 }, 0.05 },  //b... Bauch
    { 0.20, 0.25,  { 0.05, 0.00,  0.20,  0.10,  0.00 }, 0.05 },  //c... Ruecken
    { 0.35, 0.30,  { 0.05, 0.00,  0.30,  0.40,  0.00 }, 0.05 },  //d... Schwanz
    { 0.25, 0.25,  { 0.05, 0.00,  0.25,  0.10,  0.00 }, 0.05 },  //e... Vordere Beine
    { 0.30, 0.35,  { 0.05, 0.00,  0.25,  0.10,  0.00 }, 0.05 },  //f... Hintere Beine
    { 0.20, 0.15,  { 0.05, 0.00,  0.20,  0.10,  0.00 }, 0.05 }   //g... Fluegel
};

//Kushala Daora (Rostig, normal)
static const Hitzone hitzonen_bu[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.50, 0.60,  { 0.05, 0.35,  0.05,  0.30,  0.00 }, 0.05 },  //a... Kopf, Hals
    { 0.35, 0.50,  { 0.00, 0.20,  0.05,  0.20,  0.00 }, 0.00 },  //b... Bauch
    { 0.20, 0.25,  { 0.00, 0.15,  0.05,  0.15,  0.00 }, 0.00 },  //c... Ruecken
    { 0.35, 0.30,  { 0.00, 0.30,  0.05,  0.25,  0.00 }, 0.00 },  //d... Schwanz
    { 0.30, 0.25,  { 0.00, 0.20,  0.05,  0.20,  0.00 }, 0.00 },  //e... Vordere Beine
    { 0.30, 0.35,  { 0.00, 0.25,  0.05,  0.30,  0.00 }, 0.00 },  //f... Hintere Beine
    { 0.20, 0.15,  { 0.00, 0.15,  0.05,  0.20,  0.00 }, 0.00 }   //g... Fluegel
};

//Kushala Daora (Rostig, Wind Modus)
static const Hitzone hitzonen_bv[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.45, 0.55,  { 0.10, 0.40,  0.05,  0.35,  0.00 }, 0.10 },  //a... Kopf, Hals
    { 0.25, 0.40,  { 0.00, 0.25,  0.00,  0.10,  0.00 }, 0.00 },  //b... Bauch
    { 0.20, 0.25,  { 0.00, 0.20,  0.00,  0.10,  0.00 }, 0.00 },  //c... Ruecken
    { 0.35, 0.30,  { 0.00, 0.40,  0.00,  0.35,  0.00 }, 0.00 },  //d... Schwanz
    { 0.25, 0.25,  { 0.00, 0.20,  0.00,  0.10,  0.00 }, 0.00 },  //e... Vordere Beine
    { 0.30, 0.35,  { 0.00, 0.25,  0.00,  0.10,  0.00 }, 0.00 },  //f... Hintere Beine
    { 0.20, 0.15,  { 0.00, 0.20,  0.00,  0.10,  0.00 }, 0.00 }   //g... Fluegel
};

//Chameleos (Sichtbar)
static const Hitzone hitzonen_bw[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.50, 0.45,  { 0.35, 0.00,  0.20,  0.35,  0.00 }, 0.35 },  //a... Kopf
    { 0.35, 0.25,  { 0.35, 0.00,  0.15,  0.20,  0.05 }, 0.35 },  //b... Brust
    { 0.25, 0.15,  { 0.25, 0.00,  0.15,  0.25,  0.00 }, 0.25 },  //c... Ruecken
    { 0.20, 0.20,  { 0.15, 0.00,  0.20,  0.10,  0.00 }, 0.15 },  //d... Vordere Beine
    { 0.30, 0.25,  { 0.25, 0.00,  0.10,  0.25,  0.00 }, 0.25 },  //e... Hintere Beine
    { 0.20, 0.10,  { 0.10, 0.00,  0.15,  0.15,  0.00 }, 0.10 },  //f... Fluegel
    { 0.20, 0.15,  { 0.10, 0.00,  0.10,  0.05,  0.00 }, 0.10 }   //g... Schwanz
};

//Chameleos (Nicht sichtbar)
static const Hitzone hitzonen_bx[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.50, 0.50,  { 0.35, 0.00,  0.20,  0.35,  0.00 }, 0.35 },  //a... Kopf
    { 0.30, 0.25,  { 0.25, 0.00,  0.15,  0.20,  0.05 }, 0.25 },  //b... Brust
    { 0.20, 0.15,  { 0.15, 0.00,  0.15,  0.25,  0.00 }, 0.15 },  //c... Ruecken
    { 0.10, 0.10,  { 0.10, 0.00,  0.10,  0.10,  0.00 }, 0.10 },  //d... Vordere Beine
    { 0.25, 0.20,  { 0.15, 0.00,  0.15,  0.25,  0.00 }, 0.15 },  //e... Hintere Beine
    { 0.10, 0.10,  { 0.10, 0.00,  0.05,  0.15,  0.00 }, 0.10 },  //f... Fluegel
    { 0.15, 0.15,  { 0.10, 0.00,  0.10,  0.05,  0.00 }, 0.10 }   //g... Schwanz
};

//Lunastra (Normal)
static const Hitzone hitzonen_by[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.40, 0.50,  { 0.00, 0.10,  0.10,  0.30,  0.25 }, 0.00 },  //a... Kopf, Hals
    { 0.35, 0.50,  { 0.00, 0.10,  0.05,  0.20,  0.10 }, 0.00 },  //b... Bauch
    { 0.20, 0.25,  { 0.00, 0.10,  0.05,  0.20,  0.10 }, 0.00 },  //c... Ruecken
    { 0.35, 0.25,  { 0.00, 0.10,  0.05,  0.35,  0.25 }, 0.00 },  //d... Schwanz
    { 0.35, 0.25,  { 0.00, 0.10,  0.05,  0.15,  0.10 }, 0.00 },  //e... Vordere Beine
    { 0.25, 0.40,  { 0.00, 0.10,  0.05,  0.20,  0.15 }, 0.00 },  //f... Hintere Beine
    { 0.20, 0.15,  { 0.00, 0.10,  0.05,  0.20,  0.10 }, 0.00 }   //g... Fluegel
};

//Lunastra (Feuer-Aura Modus)
static const Hitzone hitzonen_bz[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.40, 0.50,  { 0.00, 0.10,  0.10,  0.35,  0.30 }, 0.00 },  //a... Kopf, Hals
    { 0.25, 0.40,  { 0.00, 0.10,  0.05,  0.10,  0.15 }, 0.00 },  //b... Bauch
    { 0.20, 0.25,  { 0.00, 0.10,  0.05,  0.10,  0.15 }, 0.00 },  //c... Ruecken
    { 0.25, 0.25,  { 0.00, 0.10,  0.05,  0.35,  0.25 }, 0.00 },  //d... Schwanz
    { 0.25, 0.25,  { 0.00, 0.10,  0.05,  0.10,  0.15 }, 0.00 },  //e... Vordere Beine
    { 0.25, 0.30,  { 0.00, 0.10,  0.05,  0.10,  0.20 }, 0.00 },  //f... Hintere Beine
    { 0.20, 0.15,  { 0.00, 0.10,  0.05,  0.10,  0.15 }, 0.00 }   //g... Fluegel
};

//Teostra (Normal)
static const Hitzone hitzonen_ca[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.40, 0.40,  { 0.00, 0.25,  0.10,  0.30,  0.10 }, 0.00 },  //a... Kopf, Hals
    { 0.35, 0.50,  { 0.00, 0.10,  0.05,  0.20,  0.10 }, 0.00 },  //b... Bauch
    { 0.20, 0.25,  { 0.00, 0.10,  0.05,  0.20,  0.10 }, 0.00 },  //c... Ruecken
    { 0.35, 0.25,  { 0.00, 0.25,  0.05,  0.35,  0.10 }, 0.00 },  //d... Schwanz
    { 0.35, 0.25,  { 0.00, 0.10,  0.05,  0.15,  0.10 }, 0.00 },  //e... Vordere Beine
    { 0.25, 0.40,  { 0.00, 0.15,  0.05,  0.20,  0.10 }, 0.00 },  //f... Hintere Beine
    { 0.20, 0.15,  { 0.00, 0.10,  0.05,  0.20,  0.10 }, 0.00 }   //g... Fluegel
};

//Teostra (Feuer-Aura Modus)
static const Hitzone hitzonen_cb[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.40, 0.40,  { 0.00, 0.30,  0.10,  0.35,  0.10 }, 0.00 },  //a... Kopf, Hals
    { 0.25, 0.40,  { 0.00, 0.15,  0.05,  0.10,  0.10 }, 0.00 },  //b... Bauch
    { 0.20, 0.25,  { 0.00, 0.15,  0.05,  0.10,  0.10 }, 0.00 },  //c... Ruecken
    { 0.25, 0.25,  { 0.00, 0.25,  0.05,  0.35,  0.10 }, 0.00 },  //d... Schwanz
    { 0.25, 0.25,  { 0.00, 0.15,  0.05,  0.10,  0.10 }, 0.00 },  //e... Vordere Beine
    { 0.25, 0.30,  { 0.00, 0.20,  0.05,  0.10,  0.10 }, 0.00 },  //f... Hintere Beine
    { 0.20, 0.15,  { 0.00, 0.15,  0.05,  0.10,  0.10 }, 0.00 }   //g... Fluegel
};

//Lao-Shan Lung (Normal u. Aschen)
static const Hitzone hitzonen_cc[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.28, 0.20,  { 0.20, 0.05,  0.15,  0.20,  0.05 }, 0.20 },  //a... Kopf
    { 0.20, 0.20,  { 0.20, 0.05,  0.15,  0.20,  0.05 }, 0.20 },  //b... Hals
    { 0.10, 0.20,  { 0.20, 0.05,  0.15,  0.20,  0.05 }, 0.20 },  //c... Schulter
    { 0.10, 0.20,  { 0.20, 0.05,  0.15,  0.20,  0.05 }, 0.20 },  //d... Fussstück
    { 0.80, 0.90,  { 0.50, 0.05,  0.15,  1.00,  0.05 }, 0.50 },  //e... Schwachpunkt
    { 0.10, 0.20,  { 0.20, 0.05,  0.15,  0.20,  0.05 }, 0.20 },  //f... Ruecken
    { 0.10, 0.20,  { 0.20, 0.05,  0.15,  0.20,  0.05 }, 0.20 },  //g... Schwanz
    { 0.55, 0.50,  { 0.20, 0.05,  0.15,  0.30,  0.05 }, 0.20 },  //h... Bauch
    { 0.32, 0.37,  { 0.20, 0.05,  0.15,  0.20,  0.05 }, 0.20 }   //i... Fuss
};

//Yamatsukami
static const Hitzone hitzonen_cd[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.50, 0.60,  { 0.00, 0.00,  0.00,  0.30,  0.35 }, 0.00 },  //a... Kopf
    { 0.50, 0.60,  { 0.00, 0.00,  0.00,  0.25,  0.20 }, 0.00 },  //b... Ruecken
    { 0.90, 0.90,  { 0.05, 0.00,  0.00,  0.20,  0.25 }, 0.05 },  //c... Auge
    { 0.65, 0.60,  { 0.00, 0.00,  0.00,  0.30,  0.25 }, 0.00 },  //d... Koerper
    { 0.80, 0.70,  { 0.00, 0.10,  0.00,  0.50,  0.40 }, 0.00 },  //e... Maul
    { 0.55, 0.50,  { 0.00, 0.00,  0.00,  0.20,  0.30 }, 0.00 },  //f... Fuehler, Tentakel
    { 0.50, 0.50,  { 0.00, 0.00,  0.00,  0.30,  0.30 }, 0.00 }   //g... Fuss
};

//Shen Gaoren
static const Hitzone hitzonen_ce[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.38, 0.40,  { 0.20, 0.05,  0.20,  0.20,  0.05 }, 0.20 },  //a... Gesicht
    { 0.40, 0.45,  { 0.15, 0.05,  0.15,  0.30,  0.05 }, 0.15 },  //b... Koerper
    { 0.10, 0.20,  { 0.20, 0.05,  0.15,  0.20,  0.05 }, 0.20 },  //c... Panzer
    { 0.32, 0.37,  { 0.20, 0.05,  0.15,  0.20,  0.05 }, 0.20 },  //d... Fuss
    { 0.10, 0.20,  { 0.20, 0.05,  0.15,  0.20,  0.05 }, 0.20 },  //e... Zange, Schere
    { 0.20, 0.20,  { 0.20, 0.05,  0.15,  0.20,  0.05 }, 0.20 },  //f... Arm
    { 0.80, 0.90,  { 0.20, 0.05,  0.15,  1.00,  0.05 }, 0.20 }   //g... Schwachpunkt (Im Panzer)
};

//Fatalis (Normal)
static const Hitzone hitzonen_cf[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.80, 0.75,  { 0.15, 0.05,  0.05,  0.80,  0.10 }, 0.15 },  //a... Gesicht
    { 0.50, 0.55,  { 0.15, 0.05,  0.05,  0.50,  0.10 }, 0.15 },  //b... Kopf
    { 0.10, 0.20,  { 0.15, 0.05,  0.05,  0.10,  0.10 }, 0.15 },  //c... Ruecken, Schwanz
    { 0.30, 0.25,  { 0.15, 0.05,  0.05,  0.30,  0.10 }, 0.15 },  //d... Hals
    { 0.10, 0.15,  { 0.15, 0.05,  0.05,  0.10,  0.10 }, 0.15 },  //e... Brust
    { 0.30, 0.25,  { 0.15, 0.05,  0.05,  0.10,  0.10 }, 0.15 },  //f... Fluegel
    { 0.20, 0.20,  { 0.15, 0.05,  0.05,  0.20,  0.10 }, 0.15 },  //g... Bauch
    { 0.20, 0.20,  { 0.15, 0.05,  0.05,  0.20,  0.10 }, 0.15 }   //h... Fuss
};

//Fatalis (Rot, normal)
static const Hitzone hitzonen_cg[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.50, 0.45,  { 0.15, 0.05,  0.05,  0.80,  0.05 }, 0.15 },  //a... Gesicht
    { 0.30, 0.25,  { 0.15, 0.05,  0.05,  0.50,  0.05 }, 0.15 },  //b... Kopf
    { 0.10, 0.20,  { 0.15, 0.05,  0.05,  0.10,  0.05 }, 0.15 },  //c... Ruecken, Schwanz
    { 0.30, 0.25,  { 0.15, 0.05,  0.05,  0.30,  0.05 }, 0.15 },  //d... Hals
    { 0.10, 0.15,  { 0.15, 0.05,  0.05,  0.10,  0.05 }, 0.15 },  //e... Brust
    { 0.30, 0.25,  { 0.15, 0.05,  0.05,  0.10,  0.05 }, 0.15 },  //f... Fluegel
    { 0.20, 0.20,  { 0.15, 0.05,  0.05,  0.20,  0.05 }, 0.15 },  //g... Bauch
    { 0.20, 0.20,  { 0.15, 0.05,  0.05,  0.20,  0.05 }, 0.15 }   //h... Fuss
};

//Fatalis (Rot, gehaertete Haut)
static const Hitzone hitzonen_ch[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.10, 0.10,  { 0.10, 0.05,  0.05,  0.10,  0.05 }, 0.10 },  //a... Gesicht
    { 0.10, 0.10,  { 0.10, 0.05,  0.05,  0.10,  0.05 }, 0.10 },  //b... Kopf
    { 0.10, 0.10,  { 0.10, 0.05,  0.05,  0.10,  0.05 }, 0.10 },  //c... Ruecken, Schwanz
    { 0.10, 0.10,  { 0.10, 0.05,  0.05,  0.10,  0.05 }, 0.10 },  //d... Hals
    { 0.10, 0.10,  { 0.10, 0.05,  0.05,  0.10,  0.05 }, 0.10 },  //e... Brust
    { 0.10, 0.10,  { 0.10, 0.05,  0.05,  0.10,  0.05 }, 0.10 },  //f... Fluegel
    { 0.10, 0.10,  { 0.10, 0.05,  0.05,  0.10,  0.05 }, 0.10 },  //g... Bauch
    { 0.10, 0.10,  { 0.10, 0.05,  0.05,  0.10,  0.05 }, 0.10 }   //h... Fuss
};

//Fatalis (Weiss, normal)
static const Hitzone hitzonen_ci[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.80, 0.75,  { 0.15, 0.05,  0.05,  0.80,  0.10 }, 0.15 },  //a... Gesicht
    { 0.50, 0.55,  { 0.15, 0.05,  0.05,  0.50,  0.10 }, 0.15 },  //b... Kopf
    { 0.10, 0.20,  { 0.15, 0.05,  0.05,  0.10,  0.10 }, 0.15 },  //c... Ruecken, Schwanz
    { 0.30, 0.25,  { 0.15, 0.05,  0.05,  0.30,  0.10 }, 0.15 },  //d... Hals
    { 0.10, 0.15,  { 0.15, 0.05,  0.05,  0.10,  0.10 }, 0.15 },  //e... Brust
    { 0.30, 0.25,  { 0.15, 0.05,  0.05,  0.10,  0.10 }, 0.15 },  //f... Fluegel
    { 0.20, 0.20,  { 0.15, 0.05,  0.05,  0.20,  0.10 }, 0.15 },  //g... Bauch
    { 0.20, 0.20,  { 0.15, 0.05,  0.05,  0.20,  0.10 }, 0.15 }   //h... Fuss
};

//Fatalis (Weiss, gehaertete Haut)
static const Hitzone hitzonen_cj[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.10, 0.10,  { 0.10, 0.05,  0.05,  0.10,  0.05 }, 0.10 },  //a... Gesicht
    { 0.10, 0.10,  { 0.10, 0.05,  0.05,  0.10,  0.05 }, 0.10 },  //b... Kopf
    { 0.10, 0.10,  { 0.10, 0.05,  0.05,  0.10,  0.05 }, 0.10 },  //c... Ruecken, Schwanz
    { 0.10, 0.10,  { 0.10, 0.05,  0.05,  0.10,  0.05 }, 0.10 },  //d... Hals
    { 0.10, 0.10,  { 0.10, 0.05,  0.05,  0.10,  0.05 }, 0.10 },  //e... Brust
    { 0.10, 0.10,  { 0.10, 0.05,  0.05,  0.10,  0.05 }, 0.10 },  //f... Fluegel
    { 0.10, 0.10,  { 0.10, 0.05,  0.05,  0.10,  0.05 }, 0.10 },  //g... Bauch
    { 0.10, 0.10,  { 0.10, 0.05,  0.05,  0.10,  0.05 }, 0.10 }   //h... Fuss
};

//Bulldrome
static const Hitzone hitzonen_ck[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.80, 0.70,  { 0.50, 0.50,  0.80,  0.10,  0.40 }, 0.50 }   //a... Koerper
};

//Giadrome
static const Hitzone hitzonen_cl[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 1.00, 1.00,  { 0.60, 0.30,  0.30,  0.10,  0.10 }, 0.60 }   //a... Koerper
};

//Velocidrome
static const Hitzone hitzonen_cm[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 1.00, 1.00,  { 0.50, 0.50,  0.50,  0.10,  0.60 }, 0.50 }   //a... Koerper
};

//Gendrome
static const Hitzone hitzonen_cn[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.90, 0.90,  { 0.35, 0.10,  0.40,  0.10,  0.50 }, 0.35 }   //a... Koerper
};

//Iodrome
static const Hitzone hitzonen_co[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.80, 0.80,  { 0.20, 0.30,  0.30,  0.10,  0.10 }, 0.20 }   //a... Koerper
};

//Koenig Shakalaka
static const Hitzone hitzonen_cp[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 0.40, 0.40,  { 0.35, 0.10,  0.10,  0.10,  0.20 }, 0.35 }   //a... Koerper
};

//Vespiodenkoenigin
static const Hitzone hitzonen_cq[] = {
    //cut   impact   Feuer Wasser Donner Drache Eis       fire_hitzone
    { 1.00, 1.00,  { 0.60, 0.40,  0.10,  0.00,  0.40 }, 0.60 }   //a... Koerper
};

///Tabelle: Monster (Reihenfolge = Eingabe-Code aa, ab, ..., az, ba, ..., cq)
static const Monster monster[] = {
    //Name                                                          Hitzonen-Menü                        Hitzonen-Werte        rage_def Spezial-Hitzone (wütend)
    { "Yian Kut-Ku (Normal u. Blau)",                               TABELLE(hitzonen_menue_wyvern),      TABELLE(hitzonen_aa), 1.0,     0,   0.0 },  //aa
    { "Yian Garuga (Ein Ohr)",                                      TABELLE(hitzonen_menue_wyvern),      TABELLE(hitzonen_ab), 1.0,     0,   0.0 },  //ab
    { "Yian Garuga (Zwei Ohren)",                                   TABELLE(hitzonen_menue_wyvern),      TABELLE(hitzonen_ac), 1.0,     0,   0.0 },  //ac
    { "Hypnocatrice",                                               TABELLE(hitzonen_menue_wyvern),      TABELLE(hitzonen_ad), 1.0,     0,   0.0 },  //ad
    { "Cephadrome",                                                 TABELLE(hitzonen_menue_wyvern),      TABELLE(hitzonen_ae), 1.0,     0,   0.0 },  //ae
    { "Gypceros (Normal u. Violett)",                               TABELLE(hitzonen_menue_wyvern),      TABELLE(hitzonen_af), 1.2,     0,   0.0 },  //af
    { "Rathalos (Normal)",                                          TABELLE(hitzonen_menue_wyvern),      TABELLE(hitzonen_ag), 0.9,     0,   0.0 },  //ag
    { "Rathalos (Azur)",                                            TABELLE(hitzonen_menue_wyvern),      TABELLE(hitzonen_ah), 1.0,     0,   0.0 },  //ah
    { "Rathalos (Silber)",                                          TABELLE(hitzonen_menue_wyvern),      TABELLE(hitzonen_ai), 0.9,     0,   0.0 },  //ai
    { "Rathian (Normal)",                                           TABELLE(hitzonen_menue_wyvern),      TABELLE(hitzonen_aj), 1.0,     0,   0.0 },  //aj
    { "Rathian (Pink)",                                             TABELLE(hitzonen_menue_wyvern),      TABELLE(hitzonen_ak), 1.0,     0,   0.0 },  //ak
    { "Rathian (Gold)",                                             TABELLE(hitzonen_menue_wyvern),      TABELLE(hitzonen_al), 0.9,     0,   0.0 },  //al
    { "Khezu (Normal)",                                             TABELLE(hitzonen_menue_wyvern),      TABELLE(hitzonen_am), 0.9,     0,   0.0 },  //am
    { "Khezu (Rot)",                                                TABELLE(hitzonen_menue_wyvern),      TABELLE(hitzonen_an), 0.9,     0,   0.0 },  //an
    { "Basarios",                                                   TABELLE(hitzonen_menue_basarios),    TABELLE(hitzonen_ao), 1.0,     'e', 0.8 },  //ao
    { "Gravios (Normal)",                                           TABELLE(hitzonen_menue_basarios),    TABELLE(hitzonen_ap), 1.0,     'e', 0.8 },  //ap
    { "Gravios (Schwarz)",                                          TABELLE(hitzonen_menue_basarios),    TABELLE(hitzonen_aq), 1.0,     'e', 0.8 },  //aq
    { "Monoblos (Normal u. Weiss)",                                 TABELLE(hitzonen_menue_wyvern),      TABELLE(hitzonen_ar), 1.0,     0,   0.0 },  //ar
    { "Diablos (Normal u. Schwarz)",                                TABELLE(hitzonen_menue_wyvern),      TABELLE(hitzonen_as), 1.0,     0,   0.0 },  //as
    { "Plesioth (Normal u. Gruen)",                                 TABELLE(hitzonen_menue_wyvern),      TABELLE(hitzonen_at), 1.2,     0,   0.0 },  //at
    { "Lavasioth",                                                  TABELLE(hitzonen_menue_wyvern),      TABELLE(hitzonen_au), 1.2,     0,   0.0 },  //au
    { "Congalala (Normal, Bauch nicht nach aussen)",                TABELLE(hitzonen_menue_congalala),   TABELLE(hitzonen_av), 0.9,     0,   0.0 },  //av
    { "Congalala (Bauch nach aussen)",                              TABELLE(hitzonen_menue_congalala),   TABELLE(hitzonen_aw), 0.9,     0,   0.0 },  //aw
    { "Congalala (Gruen, Bauch nicht nach aussen)",                 TABELLE(hitzonen_menue_congalala),   TABELLE(hitzonen_ax), 0.9,     0,   0.0 },  //ax
    { "Congalala (Gruen, Bauch nach aussen)",                       TABELLE(hitzonen_menue_congalala),   TABELLE(hitzonen_ay), 0.9,     0,   0.0 },  //ay
    { "Blangonga (Normal)",                                         TABELLE(hitzonen_menue_congalala),   TABELLE(hitzonen_az), 0.9,     0,   0.0 },  //az
    { "Blangonga (Kupfer)",                                         TABELLE(hitzonen_menue_congalala),   TABELLE(hitzonen_ba), 1.1,     0,   0.0 },  //ba
    { "Daimyo Hermitaur (Normal, nicht in Block-Position)",         TABELLE(hitzonen_menue_krabbe),      TABELLE(hitzonen_bb), 1.0,     0,   0.0 },  //bb
    { "Daimyo Hermitaur (Normal, in Block-Position)",               TABELLE(hitzonen_menue_krabbe),      TABELLE(hitzonen_bc), 1.0,     0,   0.0 },  //bc
    { "Daimyo Hermitaur (Pflaumenfarben, nicht in Block-Position)", TABELLE(hitzonen_menue_krabbe),      TABELLE(hitzonen_bd), 1.0,     0,   0.0 },  //bd
    { "Daimyo Hermitaur (Pflaumenfarben, in Block-Position)",       TABELLE(hitzonen_menue_krabbe),      TABELLE(hitzonen_be), 1.0,     0,   0.0 },  //be
    { "Shogun Ceanataur (Normal, mit Panzer)",                      TABELLE(hitzonen_menue_krabbe),      TABELLE(hitzonen_bf), 1.0,     0,   0.0 },  //bf
    { "Shogun Ceanataur (Normal, ohne Panzer)",                     TABELLE(hitzonen_menue_krabbe),      TABELLE(hitzonen_bg), 1.0,     0,   0.0 },  //bg
    { "Shogun Ceanataur (Terra, mit Panzer)",                       TABELLE(hitzonen_menue_krabbe),      TABELLE(hitzonen_bh), 1.0,     0,   0.0 },  //bh
    { "Shogun Ceanataur (Terra, ohne Panzer)",                      TABELLE(hitzonen_menue_krabbe),      TABELLE(hitzonen_bi), 1.0,     0,   0.0 },  //bi
    { "Tigrex (Normal)",                                            TABELLE(hitzonen_menue_tigrex),      TABELLE(hitzonen_bj), 1.0,     0,   0.0 },  //bj
    { "Tigrex (Wuetend)",                                           TABELLE(hitzonen_menue_tigrex),      TABELLE(hitzonen_bk), 1.0,     0,   0.0 },  //bk
    { "Nargacuga",                                                  TABELLE(hitzonen_menue_tigrex),      TABELLE(hitzonen_bl), 1.2,     0,   0.0 },  //bl
    { "Rajang",                                                     TABELLE(hitzonen_menue_rajang),      TABELLE(hitzonen_bm), 0.8,     0,   0.0 },  //bm
    { "Kirin",                                                      TABELLE(hitzonen_menue_kirin),       TABELLE(hitzonen_bn), 1.0,     0,   0.0 },  //bn
    { "Akantor (Normal)",                                           TABELLE(hitzonen_menue_tigrex),      TABELLE(hitzonen_bo), 0.8,     0,   0.0 },  //bo
    { "Akantor (Wuetend)",                                          TABELLE(hitzonen_menue_tigrex),      TABELLE(hitzonen_bp), 0.8,     0,   0.0 },  //bp
    { "Ukanlos (Bauch ist nicht gebrochen)",                        TABELLE(hitzonen_menue_tigrex),      TABELLE(hitzonen_bq), 0.9,     0,   0.0 },  //bq
    { "Ukanlos (Bauch ist gebrochen)",                              TABELLE(hitzonen_menue_tigrex),      TABELLE(hitzonen_br), 0.9,     0,   0.0 },  //br
    { "Kushala Daora (Normal)",                                     TABELLE(hitzonen_menue_kushala),     TABELLE(hitzonen_bs), 0.9,     0,   0.0 },  //bs
    { "Kushala Daora (Wind Modus)",                                 TABELLE(hitzonen_menue_kushala),     TABELLE(hitzonen_bt), 0.9,     0,   0.0 },  //bt
    { "Kushala Daora (Rostig, normal)",                             TABELLE(hitzonen_menue_kushala),     TABELLE(hitzonen_bu), 0.9,     0,   0.0 },  //bu
    { "Kushala Daora (Rostig, Wind Modus)",                         TABELLE(hitzonen_menue_kushala),     TABELLE(hitzonen_bv), 0.9,     0,   0.0 },  //bv
    { "Chameleos (Sichtbar)",                                       TABELLE(hitzonen_menue_chameleos),   TABELLE(hitzonen_bw), 1.0,     0,   0.0 },  //bw
    { "Chameleos (Nicht sichtbar)",                                 TABELLE(hitzonen_menue_chameleos),   TABELLE(hitzonen_bx), 1.0,     0,   0.0 },  //bx
    { "Lunastra (Normal)",                                          TABELLE(hitzonen_menue_kushala),     TABELLE(hitzonen_by), 0.9,     0,   0.0 },  //by
    { "Lunastra (Feuer-Aura Modus)",                                TABELLE(hitzonen_menue_kushala),     TABELLE(hitzonen_bz), 0.9,     0,   0.0 },  //bz
    { "Teostra (Normal)",                                           TABELLE(hitzonen_menue_kushala),     TABELLE(hitzonen_ca), 0.9,     0,   0.0 },  //ca
    { "Teostra (Feuer-Aura Modus)",                                 TABELLE(hitzonen_menue_kushala),     TABELLE(hitzonen_cb), 0.9,     0,   0.0 },  //cb
    { "Lao-Shan Lung (Normal u. Aschen)",                           TABELLE(hitzonen_menue_lao_shan),    TABELLE(hitzonen_cc), 0.8,     0,   0.0 },  //cc
    { "Yamatsukami",                                                TABELLE(hitzonen_menue_yamatsukami), TABELLE(hitzonen_cd), 0.8,     0,   0.0 },  //cd
    { "Shen Gaoren",                                                TABELLE(hitzonen_menue_shen_gaoren), TABELLE(hitzonen_ce), 0.8,     0,   0.0 },  //ce
    { "Fatalis (Normal)",                                           TABELLE(hitzonen_menue_fatalis),     TABELLE(hitzonen_cf), 0.75,    0,   0.0 },  //cf
    { "Fatalis (Rot, normal)",                                      TABELLE(hitzonen_menue_fatalis),     TABELLE(hitzonen_cg), 0.8,     0,   0.0 },  //cg
    { "Fatalis (Rot, gehaertete Haut)",                             TABELLE(hitzonen_menue_fatalis),     TABELLE(hitzonen_ch), 0.8,     0,   0.0 },  //ch
    { "Fatalis (Weiss, normal)",                                    TABELLE(hitzonen_menue_fatalis),     TABELLE(hitzonen_ci), 0.8,     0,   0.0 },  //ci
    { "Fatalis (Weiss, gehaertete Haut)",                           TABELLE(hitzonen_menue_fatalis),     TABELLE(hitzonen_cj), 0.8,     0,   0.0 },  //cj
    { "Bulldrome",                                                  TABELLE(hitzonen_menue_klein),       TABELLE(hitzonen_ck), 1.0,     0,   0.0 },  //ck
    { "Giadrome",                                                   TABELLE(hitzonen_menue_klein),       TABELLE(hitzonen_cl), 1.0,     0,   0.0 },  //cl
    { "Velocidrome",                                                TABELLE(hitzonen_menue_klein),       TABELLE(hitzonen_cm), 1.0,     0,   0.0 },  //cm
    { "Gendrome",                                                   TABELLE(hitzonen_menue_klein),       TABELLE(hitzonen_cn), 1.0,     0,   0.0 },  //cn
    { "Iodrome",                                                    TABELLE(hitzonen_menue_klein),       TABELLE(hitzonen_co), 1.0,     0,   0.0 },  //co
    { "Koenig Shakalaka",                                           TABELLE(hitzonen_menue_klein),       TABELLE(hitzonen_cp), 1.0,     0,   0.0 },  //cp
    { "Vespiodenkoenigin",                                          TABELLE(hitzonen_menue_klein),       TABELLE(hitzonen_cq), 0.8,     0,   0.0 }   //cq
};

//Tabellen (Ende)


//Hilfsfunktionen (Anfang)

static char dummy = 0;          //Dient zur sicheren Überprüfung von Benutzereingaben
static char fail = 0;           //Dient zur Speicherung, ob eine Frage richtig oder falsch beantwortet wurde


///Funktion: Meldung bei falscher Eingabe und Programmende
static void falsche_eingabe(void) {
    printf("\n\nFalsche Eingabe!\n\n\n");
    printf("Programm mit ENTER beenden...");
    scanf("%c", &fail);
    fflush(stdin);

    exit(1);
}


///Funktion: Einlesen eines Buchstabens (Bsp.: Menü-Auswahl "a" bis "h")
//BEKANNTER FEHLER (nicht behoben): fflush(stdin) ist laut C-Standard nicht definiert
//und leert den Eingabepuffer nur unter Windows (Visual C++).
static void eingabe_buchstabe(char *eingabe, char von, char bis) {
    scanf("%c%c", eingabe, &dummy);
    fflush(stdin);

    if((*eingabe < von || *eingabe > bis) || (dummy != '\n')) {
        falsche_eingabe();
    }
}


///Funktion: Einlesen einer Zahl (Bsp.: Angriffsstärke 0 bis 2500)
static void eingabe_zahl(int *eingabe, int von, int bis) {
    scanf("%d%c", eingabe, &dummy);
    fflush(stdin);

    if((*eingabe < von || *eingabe > bis) || (dummy != '\n')) {
        falsche_eingabe();
    }
}


///Funktion: Ausgabe einer Überschrift innerhalb eines Menüs (Bsp.: "Normal - Zustand")
static void menue_gruppe(int nummer, const char *gruppe) {
    if(nummer > 0) {
        printf("\n");
    }

    printf("%s\n\n", gruppe);
}


///Funktion: Ausgabe eines Menü-Eintrags (Bsp.: "a... Schild und Schwert")
//Bei text == NULL wird der Buchstabe übersprungen
static void menue_eintrag(int nummer, const char *gruppe, const char *text) {
    if(gruppe != NULL) {
        menue_gruppe(nummer, gruppe);
    }

    if(text != NULL) {
        printf("%c... %s\n", 'a' + nummer, text);
    }
}


///Funktion: Abschluss eines Menüs
static void menue_ende(void) {
    printf("\n");
    printf("Ihre Eingabe --> ");
}


///Funktion: Ausgabe des Attacken-Menüs einer Waffe
static void attacken_menue(const Waffe *waffe) {
    int i = 0;

    printf("\n\nWaehlen Sie die gewuenschte Attacke aus!\n\n");

    for(i = 0; i < waffe->attacken_anzahl; i++) {
        menue_eintrag(i, waffe->attacken[i].gruppe, waffe->attacken[i].name);
    }

    menue_ende();
}


///Funktion: Elementare Formel für eine Elementar-Eigenschaft
static double elementar_formel(int elementar_dmg, double esharpness, double ehitzone,
                               double def, double rage_def, double evar, int schlaege) {
    return ((elementar_dmg * esharpness * ehitzone * def *
    rage_def * evar) / 10) * schlaege;
}

//Hilfsfunktionen (Ende)


//Hauptprogramm (Beginn)
int main(void) {

    //Deklaration und Beschreibung aller benötigten Variablen (Anfang)

    //BEKANNTER FEHLER (nicht behoben): Alle Variablen werden nur einmal am Programmanfang
    //gesetzt und vor einer neuen Berechnung (do-while Schleife) nicht zurückgesetzt.
    //Werte der vorherigen Berechnung können so in die nächste Berechnung gelangen
    //(Bsp.: Elementar-Schaden bei einer Waffe ohne Elementar-Eigenschaft).

    char waffe_auswaehlen = 0;  //Dient zur Speicherung der Waffen - Angabe
    char waffe_schlag = 0;      //Dient zur Speicherung der Attacken - Angabe
    double waffe_dmg = 0.0;     //Dient zur Speicherung der Attacken - Multiplikation
                                //Hängt ab von: waffe_schlag
    int schlag_anzahl = 0;      //Dient zur Speicherung der Schlaganzahl einer Attacke
    char gewehrlanze_option = 0;//Dient zur Speicherung der Gewehrlanzen-Option (Stich/Schuss)
                                //(früher: hilfs_variable2)
    char gewehrlanze_schuss = 0;//Dient zur Speicherung des Gewehrlanzen-Schusses
                                //(früher: hilfs_variable3)
    int angriffs_staerke = 0;   //Dient zur Speicherung der Schadenspunkte der angegebenen Waffe
    char crit_uncrit = 0;       //Dient zur Speicherung der kritisch/unkritisch - Angabe
    double critical = 0.0;      //Dient zur Speicherung der kritisch/unkritisch Multiplikation
    char waffen_schaerfe = 0;   //Dient zur Speicherung der Waffenschärfen - Angabe
    double sharpness = 0.0;     //Dient zur Speicherung der Waffenschärfen - Multiplikation
    double esharpness = 0.0;    //Dient zur Speicherung der Elementar-Waffenschärfen - Multiplikation
    char elementar_eig1 = 0;    //Dient zur Speicherung der Elementar-Eigenschaft - Angabe NR. 1
    int elementar_dmg1 = 0;     //Dient zur Speicherung der Elementar-Schadens - Angabe NR. 1
    char elementar_eig2 = 0;    //Dient zur Speicherung der Elementar-Eigenschaft - Angabe NR. 2
    int elementar_dmg2 = 0;     //Dient zur Speicherung der Elementar-Schadens - Angabe NR. 2
    char monster_auswahl1 = 0;  //Dient zur Speicherung der Monster-Auswahl - Abfrage (1. Buchstabe)
    char monster_auswahl2 = 0;  //Dient zur Speicherung der Monster-Auswahl - Abfrage (2. Buchstabe)
    char monster_hitzone = 0;   //Dient zur Speicherung der Monster-Hitzone - Abfrage
    double cut = 0.0;           //Dient zur Speicherung der Schadens-Absorbations - Multiplikation
                                //Gilt für SnS, DS, GL, GS, LS und Lanze
    double impact = 0.0;        //Dient zur Speicherung der Schadens-Absorbations - Multiplikation
                                //Gilt für Hammer, HH und Lanze
    double hitzone_absorb = 0.0;//Dient zur Speicherung der Schadens-Absorbations - Multiplikation
                                //Entscheidend hierfür ist die ausgewählte Waffe
                                //(Bsp.: SnS -> hitzone_absorb = cut; HH -> hitzone_absorb = impact)
    double element[5] = {0.0};  //Dient zur Speicherung der Elementarschadens-Absorbations - Multiplikation
                                //für jedes Element (früher: element1 bis element5)
                                //element[0] = Feuer, [1] = Wasser, [2] = Donner, [3] = Drache, [4] = Eis
    double ehitzone1 = 0.0;     //Dient zur Speicherung der Elementarschadens-Absorbations - Multiplikation
                                //Bezieht sich auf elementar_eig1
    double ehitzone2 = 0.0;     //Dient zur Speicherung der Elementarschadens-Absorbations - Multiplikation
                                //Bezieht sich auf elementar_eig2
    double eformel1 = 0.0;      //Dient zur Speicherung des elementaren - Teilschadens (Element NR. 1)
    double eformel2 = 0.0;      //Dient zur Speicherung des elementaren - Teilschadens (Element NR. 2)
    char monster_status = 0;    //Dient zur Speicherung der Rage ja/nein - Abfrage
    double rage_def = 0.0;      //Dient zur Speicherung der Rage ja/nein - Multiplikation
                                //Je geringer der Wert, desto mehr Schaden wird verursacht
    char rank = 0;              //Dient zur Speicherung der Queststufen - Abfrage
    double def = 0.0;           //Dient zur Speicherung der Monster Defense
                                //Hängt ab von der Queststufe (rank)
    double class_auswahl = 0.0; //Dient zur Speicherung der "Special Class" - Multiplikation
                                //Hängt ab von: waffe_auswaehlen
    double var = 0.0;           //Dient zur Speicherung der "Class Variables" - Multiplikation
    double evar = 0.0;          //Dient zur Speicherung der "Class Variables" - Multiplikation (Element)
    int power = 0;              //Dient zum Speichern der "power" - Multiplikation
                                //Wird nur bei Gewehrlanze benötigt
    int fire = 0;               //Dient zum Speichern der "fire" - Multiplikation
                                //Wird nur bei Gewehrlanze benötigt
    double fire_hitzone = 0.0;  //Dient zum Speichern "fire_hitzone" - Multiplikation
                                //Wird nur bei Gewehrlanze benötigt
    int total_dmg1 = 0;         //Dient zur Speicherung des allgemeinen Total-Schadens
                                //Entsteht aus: Allgemeine Formel
    int total_dmg2 = 0;         //Dient zur Speicherung des elementaren Total-Schadens
                                //Entsteht aus: Elementare Formel
    int end_dmg = 0;            //Dient zur Speicherung des endgültigen Total-Schadens
                                //Ensteht aus: Endformel bzw. Spezial-formel
    char wdh = 0;               //Benötigt für: do-while Schleife

    const Waffe *waffe = NULL;      //Zeiger auf die ausgewählte Waffe (Tabelle "waffen")
    const Attacke *attacke = NULL;  //Zeiger auf die ausgewählte Attacke (NULL bei Gewehrlanzen-Schuss)
    const Schuss *schuss = NULL;    //Zeiger auf den ausgewählten Gewehrlanzen-Schuss
    const Monster *gegner = NULL;   //Zeiger auf das ausgewählte Monster (Tabelle "monster")
    const Hitzone *hitzone = NULL;  //Zeiger auf die Werte der ausgewählten Hitzone
    int nummer = 0;                 //Hilfsvariable für: Monster-Auswahl
    int i = 0;                      //Hilfsvariable für: for-Schleifen

    //Deklaration und Beschreibung aller benötigten Variablen (Ende)


    //Aufführung der Hauptformeln (Anfang)

    /// Allgemeine Formel:
    ///
    /// (angriffs_staerke * waffe_dmg * critical * sharpness *
    ///  hitzone_absorb * def * rage_def * var) / (class_auswahl) = TOTAL-SCHADEN (ALLGEMEIN)
    ///
    ///
    /// Elementare Formel:
    ///
    /// [(elementar_dmgX * esharpness * ehitzoneX * def *
    ///  rage_def * evar) / 10] * schlag_anzahl = TOTAL-SCHADEN (ELEMENTAR)
    ///
    ///
    /// Endformel:
    ///
    /// TOTAL-SCHADEN ALLGEMEIN + TOTAL-SCHADEN ELEMENTAR = TOTAL-SCHADEN
    ///

    //Aufführung der Hauptformeln (Ende)

    //Aufführung der Nebenformeln (Anfang)

    /// Gewehrlanzen-Schuss Formel:
    ///
    /// power + [(fire * fire_hitzone) * def] = TOTAL-SCHADEN (Gewehrlanze-Schuss)
    ///

    //Aufführung der Nebenformeln (Ende)


    printf("Monster Hunter Freedom Unite - Meele Damage Calculator!!!");


    do {


        ///Teil 1: Abfrage der gewünschten Waffe
        printf("\n\n\nWaehlen Sie die gewuenschte Waffe aus!\n\n");

        for(i = 0; i < ANZAHL(waffen); i++) {
            menue_eintrag(i, NULL, waffen[i].name);
        }

        menue_ende();
        eingabe_buchstabe(&waffe_auswaehlen, 'a', 'a' + ANZAHL(waffen) - 1);

        waffe = &waffen[waffe_auswaehlen - 'a'];
        ///Ende von Teil 1


        ///Teil 1.1: Abfrage der Attacke
        if(waffe_auswaehlen == 'd') {
            //Gewehrlanze: Zuerst Stich - oder Schuss - Attacke
            printf("\n\nWaehlen Sie die gewuenschte Option aus!\n\n");
            menue_eintrag(0, NULL, "Stich - Attacke");
            menue_eintrag(1, NULL, "Schuss - Attacke");
            menue_ende();
            eingabe_buchstabe(&gewehrlanze_option, 'a', 'b');

            if(gewehrlanze_option == 'a') {
                attacken_menue(waffe);
                eingabe_buchstabe(&waffe_schlag, 'a', 'a' + waffe->attacken_anzahl - 1);

            } else {
                printf("\n\nWaehlen Sie die Art der Gewehrlanze aus!\n\n");

                for(i = 0; i < ANZAHL(gewehrlanzen_schuss); i++) {
                    if(gewehrlanzen_schuss[i].stufe == 1) {
                        menue_gruppe(i, gewehrlanzen_schuss[i].typ);
                    }

                    printf("%c... Beschuss Stufe %d\n", 'a' + i, gewehrlanzen_schuss[i].stufe);
                }

                menue_ende();
                eingabe_buchstabe(&gewehrlanze_schuss, 'a', 'a' + ANZAHL(gewehrlanzen_schuss) - 1);

            }

        } else {
            attacken_menue(waffe);
            eingabe_buchstabe(&waffe_schlag, 'a', 'a' + waffe->attacken_anzahl - 1);

        }

        dummy = 0;
        ///Ende von Teil 1.1


        ///Teil 1.2: Berechnung der Attacken - Multiplikation
        attacke = NULL;

        if(waffe_schlag >= 'a' && waffe_schlag - 'a' < waffe->attacken_anzahl) {
            attacke = &waffe->attacken[waffe_schlag - 'a'];
            waffe_dmg = attacke->waffe_dmg;
            schlag_anzahl = attacke->schlag_anzahl;

        }

        schuss = NULL;

        if(waffe_auswaehlen == 'd' && gewehrlanze_schuss >= 'a' &&
           gewehrlanze_schuss - 'a' < ANZAHL(gewehrlanzen_schuss)) {
            schuss = &gewehrlanzen_schuss[gewehrlanze_schuss - 'a'];

        }
        ///Ende von Teil 1.2


        ///Teil 2: Eingabe der Angriffsstärke
        printf("\n\nGeben Sie Ihre Angriffsstaerke ein!\n");
        printf("(START -> Status -> Angriff)\n\n");
        printf("Ihre Eingabe --> ");
        eingabe_zahl(&angriffs_staerke, 0, 2500);
        ///Ende von Teil 2


        ///Teil 3: Abfrage kritischer Treffer (ja/nein)
        printf("\n\nWie trifft die angegebene Waffe?\n\n");

        for(i = 0; i < ANZAHL(treffer); i++) {
            menue_eintrag(i, NULL, treffer[i].name);
        }

        menue_ende();
        eingabe_buchstabe(&crit_uncrit, 'a', 'a' + ANZAHL(treffer) - 1);
        ///Ende von Teil 3


        ///Teil 3.1: Berechnung der Schlagstärke (abhängig von der Art des Treffers)
        critical = treffer[crit_uncrit - 'a'].critical;
        ///Ende von Teil 3.1


        ///Teil 4: Abfrage der Schärfe
        printf("\n\nGeben Sie die Schaerfe Ihrer Waffe an!\n\n");

        for(i = 0; i < ANZAHL(schaerfe); i++) {
            menue_eintrag(i, NULL, schaerfe[i].name);
        }

        menue_ende();
        eingabe_buchstabe(&waffen_schaerfe, 'a', 'a' + ANZAHL(schaerfe) - 1);
        ///Ende von Teil 4


        ///Teil 4.1: Berechnung der Schlagstärke (abhängig von der Schärfe)
        sharpness = schaerfe[waffen_schaerfe - 'a'].sharpness;
        ///Ende von Teil 4.1


        ///Teil 5: Abfrage der Elementar-Eigenschaft I
        printf("\n\nWelche Elementar-Eigenschaften besitzt Ihre Waffe?\n\n");

        for(i = 0; i < ANZAHL(elemente); i++) {
            menue_eintrag(i, NULL, elemente[i]);
        }

        menue_ende();
        eingabe_buchstabe(&elementar_eig1, 'a', 'a' + ANZAHL(elemente) - 1);

        dummy = 0;
        ///Ende von Teil 5


        ///Teil 5.1: Abfrage des Elementar-Schadens I
        if(elementar_eig1 != 'a') {
            printf("\n\nWie viel Elementar-Schaden besitzt das gewaehlte Element?\n");
            printf("(START -> Ausruest.-Details -> [Waffe] -> [Elementar] Eigsch: [Wert])\n\n");
            printf("Ihre Eingabe --> ");
            eingabe_zahl(&elementar_dmg1, 0, 750);

        }
        ///Ende von Teil 5.1


        ///Teil 5.2: Abfrage der Elementar-Eigenschaft II
        if(elementar_eig1 != 'a') {
            printf("\n\nBesitzt Ihre Waffe eine zweite Elementar-Eigenschaft?\n\n");

            for(i = 0; i < ANZAHL(elemente); i++) {
                menue_eintrag(i, NULL, (i == 0) ? "Nein" : elemente[i]);
            }

            menue_ende();
            eingabe_buchstabe(&elementar_eig2, 'a', 'a' + ANZAHL(elemente) - 1);

        }

        dummy = 0;
        ///Ende von Teil 5.2


        ///Teil 5.3: Abfrage des Elementar-Schadens II
        if(elementar_eig1 != 'a' && elementar_eig2 != 'a') {
            printf("\n\nWie viel Elementar-Schaden besitzt das gewaehlte Element?\n");
            printf("(START -> Ausruest.-Details -> [Waffe] -> [Elementar] Eigsch: [Wert])\n\n");
            printf("Ihre Eingabe --> ");
            eingabe_zahl(&elementar_dmg2, 0, 750);

        }
        ///Ende von Teil 5.3


        ///Teil 5.4: Berechnung der Schlagstärke (abhängig der Elementarschärfe)
        if(elementar_eig1 > 'a' && elementar_eig1 <= 'f') {
            esharpness = schaerfe[waffen_schaerfe - 'a'].esharpness;

        }
        ///Ende von Teil 5.4


        ///Teil 6: Abfrage des Gegners
        printf("\n\nWaehlen Sie den gewuenschten Gegner aus!\n\n");

        for(i = 0; i < ANZAHL(monster); i++) {
            printf("%c%c... %s\n", 'a' + i / 26, 'a' + i % 26, monster[i].name);
        }

        menue_ende();
        scanf("%c%c%c", &monster_auswahl1, &monster_auswahl2, &dummy);
        fflush(stdin);

        //BEKANNTER FEHLER (nicht behoben): Es werden alle Eingaben von "aa" bis "cz" angenommen,
        //die Tabelle geht aber nur bis "cq". Bei "cr" bis "cz" wird kein Monster gefunden
        //und es werden die Werte der vorherigen Berechnung (oder 0) verwendet.
        if((monster_auswahl1 < 'a' || monster_auswahl1 > 'c') ||
           (monster_auswahl2 < 'a' || monster_auswahl2 > 'z') || (dummy != '\n')) {
            falsche_eingabe();
        }

        nummer = (monster_auswahl1 - 'a') * 26 + (monster_auswahl2 - 'a');
        gegner = (nummer < ANZAHL(monster)) ? &monster[nummer] : NULL;
        ///Ende von Teil 6


        ///Teil 7: Abfrage der Hitzone
        printf("\n\nWaehlen Sie die ensprechende Hitzone des Gegners aus!\n");
        printf("(Das ist der Bereich, wo das Monster getroffen wird)\n\n");

        if(gegner != NULL) {
            for(i = 0; i < gegner->hitzonen_namen_anzahl; i++) {
                menue_eintrag(i, NULL, gegner->hitzonen_namen[i]);
            }

            menue_ende();
            eingabe_buchstabe(&monster_hitzone, 'a', 'a' + gegner->hitzonen_namen_anzahl - 1);

        }
        ///Ende von Teil 7


        ///Teil 7.1: Berechnung der Hitzone
        hitzone = NULL;

        if(gegner != NULL && monster_hitzone >= 'a' &&
           monster_hitzone - 'a' < gegner->hitzonen_anzahl) {
            hitzone = &gegner->hitzonen[monster_hitzone - 'a'];

        }

        if(hitzone != NULL) {
            if(elementar_eig1 > 'a' && elementar_eig1 <= 'f') {
                for(i = 0; i < 5; i++) {
                    element[i] = hitzone->element[i];
                }

                ehitzone1 = element[elementar_eig1 - 'b'];

            }

            if(elementar_eig2 > 'a' && elementar_eig2 <= 'f') {
                ehitzone2 = element[elementar_eig2 - 'b'];

            }

            cut = hitzone->cut;
            impact = hitzone->impact;
            fire_hitzone = hitzone->fire_hitzone;

            if(waffe->hitzonen_art == HITZONE_CUT) {
                hitzone_absorb = cut;

            } else if(waffe->hitzonen_art == HITZONE_IMPACT) {
                hitzone_absorb = impact;

            } else {
                //Lanze: der größere Wert von cut und impact
                hitzone_absorb = cut;

                if(cut < impact) {
                    hitzone_absorb = impact;

                }

            }

        }
        ///Ende von Teil 7.1


        ///Teil 8: Abfrage des Monsterstatus (InRage ja/nein)
        printf("\n\nIst das Monster in wuetendem Zustand?\n\n");
        menue_eintrag(0, NULL, "ja");
        menue_eintrag(1, NULL, "nein");
        menue_ende();
        eingabe_buchstabe(&monster_status, 'a', 'b');
        ///Ende von Teil 8


        ///Teil 8.1: Berechnung der Monster-Defense im InRage Zustand
        if(monster_status == 'a') {
            if(gegner != NULL) {
                if(gegner->rage_hitzone != 0 && monster_hitzone == gegner->rage_hitzone) {
                    rage_def = gegner->rage_def_hitzone;

                } else {
                    rage_def = gegner->rage_def;

                }

            }

        } else {
            rage_def = 1.0;

        }
        ///Ende von Teil 8.1


        ///Teil 9: Berechnung der Special Class Variablen
        class_auswahl = waffe->class_auswahl;
        ///Ende von Teil 9


        ///Teil 10: Berechnung der "Class Variablen"
        var = waffe->var;
        evar = waffe->evar;

        //Lanze: Bei impact-Hitzonen wird weniger Schaden verursacht (var2 wäre hier 0.81)
        //BEKANNTER FEHLER (nicht behoben): impact wird mit cut verglichen, bevor mit 0.72
        //multipliziert wird. So kann der schlechtere Wert gewählt werden
        //(Bsp.: cut = 0.50, impact = 0.60 -> 0.60 * 0.72 = 0.43 statt 0.50).
        if(waffe->hitzonen_art == HITZONE_LANZE && impact > cut) {
            var = 0.72;
            evar = 0.72;

        }
        ///Ende von Teil 10


        ///Teil 11: Abfrage der Quest Stufe (Benötigt für: Defense)
        printf("\n\nWaehlen Sie die gewuenschte Quest Stufe aus!\n");
        printf("(Je hoeher die Quest Stufe, desto niedriger die Monster-Defense)\n\n");

        for(i = 0; i < ANZAHL(quest_stufen); i++) {
            printf("%c... %s\n%s\n\n", 'a' + i, quest_stufen[i].name, quest_stufen[i].beschreibung);
        }

        printf("Ihre Eingabe --> ");
        eingabe_buchstabe(&rank, 'a', 'a' + ANZAHL(quest_stufen) - 1);
        ///Ende von Teil 11


        ///Teil 11.1: Berechnung der Defense
        def = quest_stufen[rank - 'a'].def;
        ///Ende von Teil 11.1


        ///Teil 12: Ausgabe aller angegebenen Daten
        printf("\n\n\nIhre eingegebenen Daten lauten:\n\n");
        printf("Ausgewaehlte Waffe: %s\n", waffe->ausgabe);

        if(attacke != NULL) {
            printf("%s", attacke->ausgabe);

        }

        if(schuss != NULL) {
            printf("Ausgewaehlte Attacke: Beschuss Stufe %d (%s)\n", schuss->stufe, schuss->typ);

        }

        printf("Angegebene Angriffsstaerke: %d\n", angriffs_staerke);
        printf("Angegebener Treffer: %s\n", treffer[crit_uncrit - 'a'].ausgabe);
        printf("Angegebene Schaerfe: %s\n", schaerfe[waffen_schaerfe - 'a'].ausgabe);
        printf("Ausgewaehlte Elementareigenschaft I: %s\n", elemente[elementar_eig1 - 'a']);

        //BEKANNTER FEHLER (nicht behoben): Es wird elementar_eig1 (ein Buchstabe) statt
        //elementar_dmg1 ausgegeben (Bsp.: "b" -> 98). Gleiches gilt für Element II.
        printf("Angegebener Elementarschaden I: %d\n", elementar_eig1);

        if(elementar_eig2 >= 'a' && elementar_eig2 <= 'f') {
            printf("Ausgewaehlte Elementareigenschaft II: %s\n", elemente[elementar_eig2 - 'a']);

        }

        printf("Angegebener Elementarschaden II: %d\n", elementar_eig2);

        if(gegner != NULL) {
            printf("Ausgewaehltes Monster: %s\n", gegner->name);

            if(monster_hitzone >= 'a' && monster_hitzone - 'a' < gegner->hitzonen_namen_anzahl &&
               gegner->hitzonen_namen[monster_hitzone - 'a'] != NULL) {
                printf("Ausgewaehlte Hitzone: %s\n", gegner->hitzonen_namen[monster_hitzone - 'a']);

            }

        }
        ///Ende von Teil 12


        ///Teil 13: Berechnung der Formeln
        //BEKANNTER FEHLER (nicht behoben): Bei Gewehrlanzen-Schüssen wird waffe_schlag nicht
        //neu abgefragt. Steht darin noch eine Attacke aus einer vorherigen Berechnung, wird
        //statt dem Schuss die alte Stich-Attacke berechnet.
        if(attacke != NULL) {
            //Allgemeine Formel
            total_dmg1 = (int)((angriffs_staerke * waffe_dmg * critical * sharpness *
            hitzone_absorb * def * rage_def * var) / class_auswahl);

            //Elementare Formel
            if(attacke->aufteilung == AUFTEILUNG_HAELFTE &&
               elementar_eig2 > 'a' && elementar_eig2 <= 'f') {
                eformel1 = elementar_formel(elementar_dmg1, esharpness, ehitzone1, def,
                                            rage_def, evar, schlag_anzahl / 2);

                eformel2 = elementar_formel(elementar_dmg2, esharpness, ehitzone2, def,
                                            rage_def, evar, schlag_anzahl / 2);

            } else if(attacke->aufteilung == AUFTEILUNG_MINUS &&
                      elementar_eig2 > 'a' && elementar_eig2 <= 'f') {
                eformel1 = elementar_formel(elementar_dmg1, esharpness, ehitzone1, def,
                                            rage_def, evar, schlag_anzahl - 1);

                eformel2 = elementar_formel(elementar_dmg2, esharpness, ehitzone2, def,
                                            rage_def, evar, schlag_anzahl - 2);

            } else {
                eformel1 = elementar_formel(elementar_dmg1, esharpness, ehitzone1, def,
                                            rage_def, evar, schlag_anzahl);

            }

            total_dmg2 = (int)(eformel1 + eformel2);

        } else if(schuss != NULL) {
            //Spezial Formel für: Gewehrlanze (Schuss)
            power = schuss->power;
            fire = schuss->fire;

            end_dmg = (int)(power + ((fire * fire_hitzone) * def));

        }
        ///Ende von Teil 13


        ///Teil 13.1: Berechnung der Endformel
        //(Beim Gewehrlanzen-Schuss wurde end_dmg schon in Teil 13 berechnet)
        if(waffe_auswaehlen != 'd' || gewehrlanze_option == 'a') {
            end_dmg = total_dmg1 + total_dmg2;

        }
        ///Ende von Teil 13.1


        ///Teil 14: Ausgabe der Formeln
        printf("\n\n\nDer allgemeine Gesamtschaden betraegt:   %d", total_dmg1);
        printf("\n\nDer elementare Gesamtschaden betraegt:   %d", total_dmg2);
        printf("\n\n\nDer endgueltige, tatsaechlich verursachte Gesamtschaden betraegt:   %d\n\n", end_dmg);
        ///Ende von Teil 14


        ///Teil 15: Wiederholungs - Abfrage
        printf("\n\n\n\nMoechten Sie noch eine Berechnung starten?\n\n");
        menue_eintrag(0, NULL, "Ja");
        menue_eintrag(1, NULL, "Nein");
        menue_ende();
        eingabe_buchstabe(&wdh, 'a', 'b');
        ///Ende von Teil 15


    } while(wdh == 'a');


    return 0;

}

//Hauptprogramm (Ende)
