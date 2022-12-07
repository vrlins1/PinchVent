

//Victor Resende Lins
//PEB/COPPE - LEP
//24/10/2022
//Válvula proporcional Vinsp para controle VCV e PCV
//Vexp on-off
//Controle através de um motor nema17 c/ driver tb6600 via controle de PWM, utilizando Arduino UNO

#include <BufferedOutput.h>  //imprime leitura do sensor no serial evitando block
#include <loopTimer.h>
#include <millisDelay.h>
#include <Wire.h>


//-----------------PID-------------------------------------------//
class PID {
  public:

    double erro, erro1, erro2;
    double sample;
    double kP, kI, kD, kt;
    double P, I, D;
    double PID_;
    double setPoint;
    double fluxo_PID;
    long lastProcess;
    int pos, ab_max = 570;
    int modo;

    PID(double _kP, double _kI, double _kD, double _kt) {
      kP = _kP;
      kI = _kI;
      kD = _kD;
      kt = _kt;
    }
    void addnewsample(double _sample) {
      sample = _sample;
    }
    void setSetPoint(double _setPoint) {
      setPoint = _setPoint;
    }
    void posicao(int _pos) {
      pos = _pos;
    }
    void Modo(int _modo) {
      modo = _modo;
    }
    void saidaPID(double _fluxo_PID) {
      fluxo_PID = _fluxo_PID;
    }
    double processo() {
      erro = sample - setPoint ;
      //erro_i = (sample - fluxo_PID) * kt; //tracking
      long currentProcess = micros();
      float deltaTime = (currentProcess - lastProcess) ;/// 1000.0;
      lastProcess = currentProcess;

      if (modo == '1'){
        P = (erro) * kP;
  
        if (pos < 0 || pos > ab_max) {
          I = I;
        }
        else {
          I = I + kI * erro * deltaTime;
        }
  
        D = (erro - erro1) * kD / deltaTime;
        erro2 = erro1;
        erro1 = erro;
  
        PID_ = P + I + D;
      }
      else if (modo == '2') {
         P = (erro) * kP;
  
        if (pos < 0 || pos > ab_max) {
          I = I;
        }
        else {
          I = I + kI * erro * deltaTime;
        }
  
        D = (erro - erro1) * kD / deltaTime;
        erro2 = erro1;
        erro1 = erro;
  
        PID_ = P + I + D;
        if (pos < 0 || pos > ab_max) {
          PID_ = 0.00;
        }
      }

      return PID_;
    }
    double Proporcional() {
      return P;
    }
    double Integrador() {
      return I;
    }
    double Derivador() {
      return D;
    }
};

//---------------------------------------------------------------//

//-----------------ROTINA---------------------------------------//

createBufferedOutput(bufferedOut, 64, DROP_UNTIL_EMPTY);  //aumentando o buffer

const int PIN_f = A0;  //sensor de fluxo
const int PIN_p = A2;
int ref, pos;  //posição zero da vávula
String inputString = "";
double linhaBase_f, linhaBase_p, value_f, value_p, max_val_f = -1000, max_val_p, volt_f, volt_p, ruido_lb, fluxo_rlb, fluxo, pressao, fluxo_pid, pressao_pid, pos_corr,sp, sp_ = 0.00, ff;
unsigned long t, t2, t_ciclo;
int modo;
bool flag;
//--------------Transdutor-----------------------// SENSIRION
/****** endereços para comunicação ******/
#define ADDR 0x40 // Endereço do sensor no barramento I2C
//Sensores
#define FLOW_REG_HIGH 0x10 // endereço de registro da leitura de fluxo
#define FLOW_REG_LOW 0x00
//Informações do sensor
#define SERIAL_REG_HIGH 0x31 // endereço de registro do número de série do sensor
#define SERIAL_REG_LOW 0xAE
/****** parâmetros do sensor para conversão de bits para unidades reais ******/
//Fluxo
#define OFFSET_FLOW 32768
#define SCALE_FLOW 120.0
float flow = 0.0; // convertido para ccm
union {
  uint8_t bytes[2];
  uint16_t integer = 0;
} flow_raw;
union {
  uint8_t bytes[4];
  uint32_t integer = 0L;
} serial;

//---------------Vinsp------------------------------//
const int dir = 12;             //dir
const int pul = 11;              //10; //step
const int home_switch = 8;      //microswitch
int intervalo = 25;       //intervalo entre as mudanças de estado do pulso em microseg
boolean pulso = LOW, pp = LOW;  //estado do pulso
int ab_max = 560;

//---------------Vexp-----------------------------//
const int dir_Vexp = 2;             //dir
const int pul_Vexp = 3;              //10; //step
int home_switch_Vexp = 10;      //microswitch
int intervalo_Vexp = 25;       //intervalo entre as mudanças de estado do pulso em microseg
boolean pulso_Vexp = LOW, pp_exp = LOW;  //estado do pulso
int ab_max_Vexp = 900;
int refVexp, posVexp;

//-------- Parâmetros do PID--------------//
PID fluxoPID(.70, 0.01, 0.02, 0.0);  //GANHOS PID ->kp,ki.kd.kt
PID pressaoPID(.00, .000025, .00, 0.0);  //GANHOS PID ->kp,ki.kd.kt
//---------------------------------------//


//----------Parâmetros Ventilatório--------//
unsigned long t_ie = 3000000; 

//-----------------------------------------//
void setup() {
  Wire.begin(); 
  Serial.begin(2000000);
  bufferedOut.connect(Serial);
  delay(1000);
  // posição zero:
  Serial.println("iniciando...");

  
  //transdutor de pressão//
  //  pinMode(PIN_f, INPUT);
  pinMode(PIN_p, INPUT);
  pinMode(home_switch, INPUT_PULLUP);
  pinMode(home_switch_Vexp, INPUT_PULLUP);

  //analogReference(INTERNAL);  //Referência analógica = 1,1 V

  //drivers//
  pinMode(dir, OUTPUT);
  pinMode(pul, OUTPUT);
  pinMode(dir_Vexp, OUTPUT);
  pinMode(pul_Vexp, OUTPUT);
  //  digitalWrite(ena, HIGH); //habilita em low invertida
  digitalWrite(dir, HIGH);  // low CW / high CCW
  digitalWrite(pul, LOW);   //borda de descida


  //zero Vinsp
  while (digitalRead(home_switch) == HIGH) { //move o motor até o fim de curso
    digitalWrite(dir, LOW);
    delayMicroseconds(intervalo);
    Passo();
  }
  delay(2000);

  //zero Vexp
  while (digitalRead(home_switch_Vexp) == HIGH) { //move o motor até o fim de curso
    digitalWrite(dir_Vexp, HIGH);
    delayMicroseconds(intervalo);
    PassoVexp();
  }
  delay(2000);

  //Fechando completamente//
  pos = 0;
  posVexp = 0;
  int initial_pos = ab_max;
  t2 = micros();

  intervalo = 1000;

  while (pos <= initial_pos) {
    digitalWrite(dir, HIGH);
    Passo();
    pos++;
  }

  digitalWrite(dir, LOW);
  digitalWrite(pul, LOW);

  //CALIBRAÇÃO//
  delay(5000);
  Serial.println("calibrando fluxo...");
  setup_sensorFluxo();
  //Corrigindo linha de base do sensor de pressão//
  Serial.println("calibrando pressão...");
  int  i = 0;
  for (i; i < 1000; i++) {
    value_p = analogRead(PIN_p);  //transdutor pressao
    // if (abs(value_p) > abs(max_val_p)) {
    //   max_val_p = value_p;
    // }
    linhaBase_p = linhaBase_p + value_p;
    delay(5);
  }
  linhaBase_p = linhaBase_p / i;

  t = 0.0;
  intervalo = 25;
  //  t2 = 0.0;
  Serial.println("1-PCV / 2- VCV");
  while (Serial.available() == 0) {}
  modo = Serial.read();
  delay(100);
}

void setup_sensorFluxo(){
  uint8_t crc;

  delay(5000);
  
  double last_sample = micros();// Para marcar o tempo gasto na inicialização

//    shift_counter = 0;
    Wire.beginTransmission(ADDR);
    Wire.write(SERIAL_REG_HIGH); // número de série
    Wire.write(SERIAL_REG_LOW);
    Wire.endTransmission();

    Wire.requestFrom(ADDR, 6, true); //O número de série é como um inteiro de 32 bits em pares de bytes com CRC próprios
    Serial.print("No Serie: ");
    if(Wire.available()>=6){
      serial.bytes[3] = Wire.read();
      serial.bytes[2] = Wire.read();
      crc = Wire.read();
      serial.bytes[1] = Wire.read();
      serial.bytes[0] = Wire.read();
      crc = Wire.read();

      Serial.println(serial.integer);
    } else{
      Serial.println("Error! Failed to receive serial number");
    }
    

  Serial.print("Time elapsed: ");
  Serial.println(micros()-last_sample); 

  last_sample = millis();

  Wire.beginTransmission(ADDR);
  Wire.write(FLOW_REG_HIGH);
  Wire.write(FLOW_REG_LOW);
  Wire.endTransmission();

  //A primeira leitura é inválida e demora 0.5 ms
  delay(1);
  Wire.requestFrom(ADDR, 3, true); // leitura nos dois primeiros bytes e crc no ao final
  while(Wire.available()){
      Wire.read();
  }
}

//-----------------Leitura do sensor de fluxo----------------//
float le_fluxo() {
  uint8_t crc;
    //Leitura da medição de fluxo  
    Wire.requestFrom(ADDR, 3, true);
    if(Wire.available()>=3){
        flow_raw.bytes[1] = Wire.read();
        flow_raw.bytes[0] = Wire.read();
        crc = Wire.read();
    }
    flow = (flow_raw.integer-OFFSET_FLOW)/SCALE_FLOW;
    return flow;
}


void Passo() {
  pulso = !pulso;
  digitalWrite(pul, pulso);
  delayMicroseconds(intervalo);
  pulso = !pulso;
  digitalWrite(pul, pulso);
  delayMicroseconds(intervalo);
}


void PassoVexp() {
  pulso_Vexp = !pulso_Vexp;
  digitalWrite(pul_Vexp, pulso_Vexp);
  delayMicroseconds(intervalo_Vexp);
  pulso_Vexp = !pulso_Vexp;
  digitalWrite(pul_Vexp, pulso_Vexp);
  delayMicroseconds(intervalo_Vexp);
}

float processaInput() {  //leitura dda entrada do serial em array e depois transformando em float, para evitar bloqueio (reduzir delay)
  float numero = 0;
  while (Serial.available()) {
    char val = (char)Serial.read();
    if (val == '\n') {
      numero = atof(inputString.c_str());
      //Serial.println(numero, 4);
      //Serial.println(inputString);
      inputString = "";
    } else {
      inputString += val;
    }
  }
  return numero;
}

int motorVinsp(int x) {        //movimento e posição do motor
  //movimento do motor, 1 passo por loop//

  if (x < pos && pos >= 0) {
    digitalWrite(dir, LOW);  //fecha
    Passo();
    pos--;
  }

  else if (x > pos && pos <= ab_max) {
    digitalWrite(dir, HIGH);  //abre
    Passo();
    pos++;
  }
}


int motorVexp(int x) {        //movimento e posição do motor
  //movimento do motor, 1 passo por loop//

  if (x < posVexp && posVexp > 0) {
    digitalWrite(dir_Vexp, HIGH);  //abre
    PassoVexp();
    posVexp--;
  }

  else if (x > posVexp && pos < ab_max_Vexp) {
    digitalWrite(dir_Vexp, LOW);  //fecha
    PassoVexp();
    posVexp++;
  }

}

void vcv() {   //modo volume controlado
  ff = -58.3 * sp + 515.7;  //FEEDFORWARD 14/11 ->SMF3300, mediamovel(janela=250)
  fluxoPID.setSetPoint(sp);
  fluxoPID.addnewsample(fluxo);
  fluxoPID.Modo(modo);

  //Processo//
  fluxo_pid = (fluxoPID.processo());  //saida PID


  //Correção-posição-Vinsp
  pos_corr = fluxo_pid;

  if (digitalRead(home_switch) == LOW) {
    ref = ref;
    pos = 0;
    //Vexp fechada
  }

  if ((sp == 0.00)) {
    //abre Vexp
    ref = ab_max;
//    if ((fluxo >= .005)) {
//      ref++;
////      pos = ab_max;
//
//    }
//    if (fluxo < .005) {
//      ref = ref;
//      pos = ab_max;
//    }
  }

  else {
    ref = ff + pos_corr;
    //Vexp fechada

  }


}

void pcv() {  //modo pressao controlada
  //  if (sp == 0.00) {
  //    ff = ab_max;
  //  }
  //  else {
  //    ff = 500;  //FEEDFORWARD
  //  }
  pressaoPID.setSetPoint(sp);
  pressaoPID.addnewsample(pressao);
  pressaoPID.Modo(modo);

  //Processo//
  pressao_pid = (pressaoPID.processo()); //saida PID

  
  //Correção-posição-Vinsp
  pos_corr = pressao_pid;
  ref =  pos_corr;
  
//  if (digitalRead(home_switch) == LOW) {
//    ref = ref;
//    pos = 0;
//  }
//
//  if ((sp == 0.00)) {
//    ref = ab_max;
//    if (pressao > 0) {
//      ref ++;
//      //      pos = ab_max;
//    }
//    else if (pressao == 0) {
//      ref = ref;
//      //      pos = ab_max;
//    }
//  }
//
//  else {
//    ref =  pos_corr;
//  }

}


//---------------LOOP-------------//
void loop() {

  if (modo == '1') {
    pcv();
  }
  else if (modo == '2') {
    vcv();
  }
//  loopTimer.check(Serial);
//  bufferedOut.nextByteOut();

  //laço para leitura da entrada na porta serial//
  while (Serial.available() > 0) {
    //PID//
    sp_ = processaInput();  //setpoint
  }
  if( micros() - t_ciclo > t_ie){
    flag = !flag;
    t_ciclo = micros();
    }
    if(flag == 0){
     sp = sp_; 
    }
   else{
    sp = 0.00;
   }


  //----------------amostragem  400 Hz----------------//
  if (micros() - t >= 2500) { //inspiração
    fluxo = - le_fluxo()/60.00; //leitura do transdutor de fluxo

    value_p = analogRead(PIN_p);
    volt_p = (value_p - linhaBase_p) * (5.0 / 1023); 
    pressao = volt_p * 3.411146E+1;  //calibrar


    //--------------------DEBUG----------------------//

//    bufferedOut.print(millis());
//    bufferedOut.print("\t");
//    bufferedOut.print(sp);
//    bufferedOut.print("\t");
////    bufferedOut.print(ref);
////    bufferedOut.print("\t");
////    bufferedOut.print(posVexp);
////   bufferedOut.print("\t");
////    bufferedOut.print(pos_corr);
////    bufferedOut.print("\t");
////    bufferedOut.print(pressaoPID.Proporcional());
////    bufferedOut.print("\t");
////    bufferedOut.print(pressaoPID.Integrador());
////    bufferedOut.print("\t");
////    bufferedOut.print(pressaoPID.Derivador());
////    bufferedOut.print("\t");
//    //        bufferedOut.print(fluxo_rlb,4);
//    //        bufferedOut.print("\t");
//    bufferedOut.print(fluxo, 4);
//    bufferedOut.print("\t");
//    bufferedOut.println(pressao, 1);
//    //    bufferedOut.print(fluxo_rlb, 4);
////        bufferedOut.print("\t");

    t = micros();
  }

  //--------Vexp-----------//
  if (sp == 0.00) {
    refVexp = 0;
  }
  else if (sp != 0.00) {
    refVexp = ab_max_Vexp;

  }
  motorVexp(refVexp);
  
  //------Vinsp-----------//

  if(posVexp==refVexp and ref-pos<0) {      //move a Vinsp apenas apos a Vexp concluir o movimento
    motorVinsp(ref);  //move motorVinsp
  }
  else if(ref-pos>= 0 ){
    motorVinsp(ref);  //move motorVinsp
  }
  fluxoPID.posicao(pos);


}