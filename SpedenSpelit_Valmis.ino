#include <avr/io.h>
#include <avr/interrupt.h>
#include "display.h"

const int numCount = 100;                       // Kuinka monta numeroa generoidaan
int randomNumbers[numCount];                    // Taulukko, joka sisältää satunnaiset numerot
int userNumbers[numCount];                      // Taulukko käyttäjän syöttämille numeroille
volatile int userInputCount = 0;                // Käyttäjän syötteiden laskuri
volatile bool timeToCheckGameStatus = false;    // Lippu, joka tarkistaa syltteen totuuden
volatile int generatedNumbers = 0;              // Kuinka monta numeroa on näytetty
volatile int correctInputs = 0;                 // Kuinka monta oikeaa syötettä käyttäjä on antanut
volatile bool timerActive = false;              // Lippu, joka osoittaa onko ajastin päällä
volatile bool newTimerInterrupt = false;        // Ajastimen keskeytyslippu
volatile bool numberDisplayed = false;          // Lippu, joka osoittaa onko numero päällä

int currentScore = 0;                           // Käyttäjän nykyinen pistemäärä
int highestScore = 0;                           // Korkein saavutettu pistemäärä

unsigned long inputTimeout = 1500;              // Käyttäjän syötteen timeout
const unsigned long minTimeout = 200;           // Pienin sallittu timeout
unsigned long lastNumberTime = 0;               // Aikaleima viimeksi näytetylle numerolle
const float timeoutReduction = 0.85;            // Kerroin, jolla timeouttia pienennetään
unsigned long ledDelay = 500;                   // Viive LEDin näytölle
const unsigned long minLEDDelay = 50;           // Pienin sallittu LEDin viive

void setup() {
  initializeDisplay();                          // Alustaa 7-segment näytöt
  Serial.begin(9600);
  Serial.println("Welcome To SpenedSpelit");
  Serial.println("Press The Start Button");

  // Alustaa napit ja LEDit
  for (int pin = 2; pin <= 5; pin++) pinMode(pin, INPUT_PULLUP);  // Napit
  pinMode(6, INPUT_PULLUP);                                       // Aloitus nappi
  for (int pin = A2; pin <= A5; pin++) pinMode(pin, OUTPUT);      // LEDit

  // Alustaa random lukugeneraattorin
  randomSeed(analogRead(0));

  // Tervehdys viesti LEDeillä
  flashLEDs(3);

  // Alustaa ajastimen
  initializeTimer();
}

void loop() {
  // Odottaa aloitus napin painallusta pelin aloittamiseksi
  if (digitalRead(6) == LOW) {
    startTheGame();
    delay(500); // Debounce
  }

  // Tarkistetaan timeoutin pelin aikana
  if (timerActive && millis() - lastNumberTime > inputTimeout) {
    indicateLoss();                          // Ilmoittaa häviöstä
    stopTheGame();                           // Lopeta peli
  }
  // Käsitellään uusi ajastimen keskeytys numeron näyttämiseksi
  if (newTimerInterrupt && timerActive && generatedNumbers < numCount) {
    newTimerInterrupt = false;
    if (!numberDisplayed && generatedNumbers < numCount) {
      showNumber(randomNumbers[generatedNumbers]);
      lastNumberTime = millis();
      numberDisplayed = true;
    }
  }

  // Tarkistetaan käyttäjän painallukset
  checkButtons();

  // Vahvistetaan käyttäjän syöte
  if (timeToCheckGameStatus) {
    timeToCheckGameStatus = false;
    checkGame(userInputCount - 1);          // Tarkistaa viimeisen syötteen
  }

  static int lastDisplayedScore = -1;
  if (currentScore != lastDisplayedScore) {
    showResults(currentScore);
    lastDisplayedScore = currentScore;
  }
}

void initializeTimer() {
  cli();                                   // Keskeytyksen poiskytkentä
  TCCR1A = 0;                              // Aseta TCCR1A rekisteri nollaksi
  TCCR1B = 0;                              // Aseta TCCR1B rekisteri nollaksi
  TCNT1 = 0;                               // Alusta laskuri arvo nollaksi
  OCR1A = 15624;                           // Timer1:n vertailuarvo
  TCCR1B |= (1 << WGM12);                  // CTC-tila
  TCCR1B |= (1 << CS12) | (1 << CS10);     // 1024 preskalattori
  TIMSK1 |= (1 << OCIE1A);                 // Keskeytys päälle vertailuarvon saavuttaessa
  sei();                                   // Keskeytysten päällekytkentä
}

ISR(TIMER1_COMPA_vect) {
  if (generatedNumbers < numCount) {
    newTimerInterrupt = true;              // Ilmoita keskeytyksestä loop()-funktiolle

    // Vähennä ajastinta joka kymmenen oikean syötteen jälkeen
    if (correctInputs > 0 && correctInputs % 10 == 0) {
      OCR1A = max(OCR1A * 0.85, 1);        // Vähennä vertailuarvoa
    }
  }
}

void startTheGame() {                      // Nollataan pelin tila
  initializeGame();
  timerActive = true;
  numberDisplayed = false;

  currentScore = 0;                        // Resettaa pisteet
  showResults(currentScore);               // Päivittää näytön

  Serial.println("Game Started!");
  Serial.print("Match This Number: ");
  Serial.println(randomNumbers[generatedNumbers] + 1);
  showNumber(randomNumbers[generatedNumbers]);
  lastNumberTime = millis();
  numberDisplayed = true;
}

void stopTheGame() {
  timerActive = false;
  TIMSK1 &= ~(1 << OCIE1A);                // Poista Timer1:n keskeytys käytöstä
  indicateEnd();                           // Ilmoita pelin päättymisestä LEDeillä
  Serial.print("Your Score: ");
  Serial.println(currentScore);
  showResults(currentScore);
  // Päivitä korkein pistemäärä Serial Monitorille tarvittaessa
  if (currentScore > highestScore) {
    highestScore = currentScore;
    Serial.print("New Highest Score: ");
    Serial.println(highestScore);
  } else {
    Serial.print("Highest Score: ");
    Serial.println(highestScore);
  }
  currentScore = 0;

  Serial.println("Game Over. Press The Button To Start Again");
}

void initializeGame() {
  generatedNumbers = 0;
  userInputCount = 0;
  correctInputs = 0;
  memset(userNumbers, -1, sizeof(userNumbers)); // Nollataan käyttäjän syötteet
  timeToCheckGameStatus = false;
  currentScore = 0;

  inputTimeout = 1500;                          // Resettaa aloitus arvoon
  ledDelay = 500;                               // Resettaa aloitus arvoon
  generateRandomSequence();
}

// Luo random numerosekvenssi, jossa sama luku  ei toistu enemmän kuin kahesti peräkkäin
void generateRandomSequence() {
  for (int i = 0; i < numCount; i++) {
    int newNumber;
    do {
      newNumber = random(0, 4);                 // Satunnainen luku väliltä 0-3
    } while (i > 1 && newNumber == randomNumbers[i - 1] && newNumber == randomNumbers[i - 2]);
    randomNumbers[i] = newNumber;
  }

  Serial.println("Generated Random Number: ");
  for (int i = 0; i < numCount; i++) {
    Serial.print(randomNumbers[i] + 1);
    Serial.print(" ");
  }
  Serial.println();
}

// Tarkistaa, että käyttäjän antama syöte vastaa oikeaa randomia lukua.
void checkGame(byte lastButtonIndex) {
  if (userNumbers[lastButtonIndex] != randomNumbers[lastButtonIndex]) {
    Serial.println("Wrong input!");
    indicateLoss();
    stopTheGame();
  } else {
    correctInputs++;
    currentScore++;
    Serial.print("Current Score: ");
    Serial.println(currentScore);

    // Vähennä timeouttia ja LED viivettä joka kymmenen oikean syötteen jälkeen
    showResults(currentScore); // Päivittää näytön
    if (correctInputs % 10 == 0) {
      inputTimeout = max(inputTimeout * timeoutReduction, minTimeout);
      ledDelay = max(ledDelay * timeoutReduction, minLEDDelay);
      Serial.print("Timeout Reduced! New Timeout: ");
      Serial.print(inputTimeout);
      Serial.println(" ms");
      Serial.print("LED Delay Reduced! New LED Delay: ");
      Serial.print(ledDelay);
      Serial.println(" ms");
    }
    generatedNumbers++;
    // Näytä seuraava numero tai ilmoita voitosta
    if (generatedNumbers < numCount) {
      numberDisplayed = false;
      showNumber(randomNumbers[generatedNumbers]);
      lastNumberTime = millis();
      numberDisplayed = true;
    } else {
      indicateWin();
      stopTheGame();
    }
  }
}

void checkButtons() {
  for (int i = 2; i <= 5; i++) {
    if (digitalRead(i) == LOW) {
      int buttonNumber = i - 2;

      // Tallenna käyttäjän syöte ja merkitse tarkistettavaksi
      if (timerActive && buttonNumber >= 0 && buttonNumber <= 3) {
        userNumbers[userInputCount++] = buttonNumber;
        timeToCheckGameStatus = true;
        Serial.println(buttonNumber + 1);
        delay(200);  // Debounce
      }
    }
  }
}

void showNumber(int number) {
  // Sammutetaan kaikki LEDit ja sytytetään vastaava LED
  for (int pin = A2; pin <= A5; pin++) {
    digitalWrite(pin, LOW);
  }
  delay(ledDelay);
  digitalWrite(A2 + number, HIGH);
}

void flashLEDs(int flashes) {
  for (int i = 0; i < flashes; i++) {
    for (int pin = A2; pin <= A5; pin++) digitalWrite(pin, HIGH);
    delay(200);

    for (int pin = A2; pin <= A5; pin++) digitalWrite(pin, LOW);
    delay(200);
  }
}

void indicateWin() {
  Serial.println("You Win!");
  flashLEDs(5);
}

void indicateLoss() {
  for (int i = 0; i < 3; i++) {
    for (int pin = A2; pin <= A5; pin++) digitalWrite(pin, LOW);
    delay(200);
    for (int pin = A2; pin <= A5; pin++) digitalWrite(pin, HIGH);
    delay(200);
  }
}

void indicateEnd() {
  flashLEDs(2);
}
