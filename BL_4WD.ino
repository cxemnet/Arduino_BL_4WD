// CxemCAR 1.0 от 06.01.2013
// Официальная страница проекта: http://cxem.net/uprav/uprav43.php

#include "EEPROM.h"

#define D1 2          // направление вращение двигателя 1
#define M1 3          // ШИМ вывод для управления двигателем 1 (левый)
#define D2 4          // направление вращение двигателя 2
#define M2 5          // ШИМ вывод для управления двигателем 2 (правый)
#define HORN 13       // доп. канал 1
//#define autoOFF 2500  // кол-во миллисекунд через которое робот останавливается при потери связи

#define cmdL 'L'      // команда UART для левого двигателя
#define cmdR 'R'      // команда UART для правого двигателя
#define cmdH 'H'      // команда UART для доп. канала 1 (к примеру сигнал Horn)
#define cmdF 'F'      // команда UART для работы с EEPROM памятью МК для хранения настроек
#define cmdr 'r'      // команда UART для работы с EEPROM памятью МК для хранения настроек (чтение)
#define cmdw 'w'      // команда UART для работы с EEPROM памятью МК для хранения настроек (запись)

char incomingByte;    // входящие данные

char L_Data[4];       // строковый массив для данных левого мотора L
byte L_index = 0;     // индекс массива
char R_Data[4];       // строковый массив для данных правого мотора R
byte R_index = 0;     // индекс массива
char H_Data[1];       // строковый массив для доп. канала
byte H_index = 0;     // индекс массива H
char F_Data[8];       // строковый массив данных для работы с EEPROM
byte F_index = 0;     // индекс массива F
char command;         // команда: передача координат R, L или конец строки

unsigned long currentTime, lastTimeCommand, autoOFF;

void setup() {
  Serial.begin(9600);       // инициализация порта
  pinMode(HORN, OUTPUT);    // дополнительный канал
  pinMode(D1, OUTPUT);      // выход для задания направления вращения двигателя
  pinMode(D2, OUTPUT);      // выход для задания направления вращения двигателя
  /*EEPROM.write(0,255);
  EEPROM.write(1,255);
  EEPROM.write(2,255);
  EEPROM.write(3,255);*/
  timer_init();             // инициализируем программный таймер
}

void timer_init() {
  uint8_t sw_autoOFF = EEPROM.read(0);   // считываем с EEPROM параметр "включена ли ф-ия остановки машинки при потере связи"
  if(sw_autoOFF == '1'){                 // если таймер останова включен
    char var_Data[3];
    var_Data[0] = EEPROM.read(1);
    var_Data[1] = EEPROM.read(2);
    var_Data[2] = EEPROM.read(3);
    autoOFF = atoi(var_Data)*100;        // переменная автовыкл. для хранения кол-ва мс
  }
  else if(sw_autoOFF == '0'){         
    autoOFF = 999999;
  } 
  else if(sw_autoOFF == 255){ 
    autoOFF = 2500;                      // если в EEPROM ничего не записано, то по умолчанию 2.5 сек
  } 
  currentTime = millis();                // считываем время, прошедшее с момента запуска программы
}
 
void loop() {
  if (Serial.available() > 0) {          // если пришли UART данные
    incomingByte = Serial.read();        // считываем байт
    if(incomingByte == cmdL) {           // если пришли данные для мотора L
      command = cmdL;                    // текущая команда
      memset(L_Data,0,sizeof(L_Data));   // очистка массива
      L_index = 0;                       // сброс индекса массива
    }
    else if(incomingByte == cmdR) {      // если пришли данные для мотора R
      command = cmdR;
      memset(R_Data,0,sizeof(R_Data));
      R_index = 0;
    }
    else if(incomingByte == cmdH) {      // если пришли данные для доп. канала 1
      command = cmdH;
      memset(H_Data,0,sizeof(H_Data));
      H_index = 0;
    }    
    else if(incomingByte == cmdF) {      // если пришли данные для работы с памятью
      command = cmdF;
      memset(F_Data,0,sizeof(F_Data));
      F_index = 0;
    }
    else if(incomingByte == '\r') command = 'e';   // конец строки
    else if(incomingByte == '\t') command = 't';   // конец строки для команд работы с памятью
    
    if(command == cmdL && incomingByte != cmdL){
      L_Data[L_index] = incomingByte;              // сохраняем каждый принятый байт в массив
      L_index++;                                   // увеличиваем текущий индекс массива
    }
    else if(command == cmdR && incomingByte != cmdR){
      R_Data[R_index] = incomingByte;
      R_index++;
    }
    else if(command == cmdH && incomingByte != cmdH){
      H_Data[H_index] = incomingByte;
      H_index++;
    }    
    else if(command == cmdF && incomingByte != cmdF){
      F_Data[F_index] = incomingByte;
      F_index++;
    }    
    else if(command == 'e'){                       // если приняли конец строки
      Control4WD(atoi(L_Data),atoi(R_Data),atoi(H_Data));
      delay(10);
    }
    else if(command == 't'){                       // если приняли конец строки для работы с памятью
      Flash_Op(F_Data[0],F_Data[1],F_Data[2],F_Data[3],F_Data[4]);
    }
    lastTimeCommand = millis();                    // считываем текущее время, прошедшее с момента запуска программы
  }
  if(millis() >= (lastTimeCommand + autoOFF)){     // сравниваем текущий таймер с переменной lastTimeCommand + autoOFF
    Control4WD(0,0,0);                             // останавливаем машинку
  }
}

void Control4WD(int mLeft, int mRight, uint8_t Horn){

  bool directionL, directionR;      // направление вращение для L298N
  byte valueL, valueR;              // значение ШИМ M1, M2 (0-255)
  
  if(mLeft > 0){
    valueL = mLeft;
    directionL = 0;
  }
  else if(mLeft < 0){
    valueL = 255 - abs(mLeft);
    directionL = 1;
  }
  else {
    directionL = 0;
    valueL = 0;
  }
 
  if(mRight > 0){
    valueR = mRight;
    directionR = 0;
  }
  else if(mRight < 0){
    valueR = 255 - abs(mRight);
    directionR = 1;
  }
  else {
    directionR = 0;
    valueR = 0;
  }
   
  analogWrite(M1, valueL);            // задаем скорость вращения для L
  analogWrite(M2, valueR);            // задаем скорость вращения для R
  digitalWrite(D1, directionL);       // задаем направление вращения для L
  digitalWrite(D2, directionR);       // задаем направление вращения для R
  
  digitalWrite(HORN, Horn);           // дополнительный канал
}

void Flash_Op(char FCMD, uint8_t z1, uint8_t z2, uint8_t z3, uint8_t z4){

  if(FCMD == cmdr){		      // если команда чтения EEPROM данных 
    Serial.print("FData:");	      // посылаем данные с EEPROM
    Serial.write(EEPROM.read(0));     // считываем значение ячейки памяти с 0 адресом и выводим в UART
    Serial.write(EEPROM.read(1));
    Serial.write(EEPROM.read(2));
    Serial.write(EEPROM.read(3));
    Serial.print("\r\n");	      // маркер конца передачи EEPROM данных
  }
  else if(FCMD == cmdw){	      // если команда записи EEPROM данных
    EEPROM.write(0,z1);               // запись z1 в ячейку памяти с адресом 0
    EEPROM.write(1,z2);
    EEPROM.write(2,z3);
    EEPROM.write(3,z4);
    timer_init();		      // переинициализируем таймер
    Serial.print("FWOK\r\n");	      // посылаем сообщение, что данные успешно записаны
  }
}
