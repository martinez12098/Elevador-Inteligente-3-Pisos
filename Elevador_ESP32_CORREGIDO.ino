/* Elevador educativo de 3 pisos. ESP32 / Wokwi / A4988 / MQTT
   Autores: Jesus Eduardo Martinez Tinajero y Espindola Juarez Doraly
   No apto para transportar personas. No sustituye E-Stop fisico.
*/
#include <WiFi.h>
#include <PubSubClient.h>
#include <string.h>

constexpr int STEP_PIN=18, DIR_PIN=19, EN_PIN=26;
constexpr int PISO1_PIN=32, PISO2_PIN=33, PISO3_PIN=25;
constexpr int STOP_PIN=27, LED_VERDE=16, LED_ROJO=17;
constexpr long PASOS_PISO=200;
constexpr unsigned long PASO_INTERVALO_US=4000, PULSO_ANCHO_US=10;
const char* WIFI_SSID="Wokwi-GUEST";
const char* BROKER="broker.emqx.io";
const char* TOPICO_ESTADO="elevador/isc2026/equipo01/estado";
const char* TOPICO_COMANDO="elevador/isc2026/equipo01/comando";
WiFiClient net;
PubSubClient mqtt(net);
enum Estado {REPOSO,SUBIENDO,BAJANDO,EMERGENCIA,BLOQUEADO};
Estado estado=REPOSO;
long posicion=0,objetivo=0;
int pisoActual=1,pisoDestino=1;
unsigned long tPaso=0,tJSON=0,tBoton=0,tWiFi=0,tMQTT=0;
bool pulsoAlto=false,paroPrevio=false,btnPrev[3]={false,false,false};

bool paro(){return digitalRead(STOP_PIN)==HIGH;}
const char* nombreEstado(){switch(estado){case REPOSO:return "Reposo";case SUBIENDO:return "Subiendo";case BAJANDO:return "Bajando";case EMERGENCIA:return "Emergencia";case BLOQUEADO:return "Bloqueado";}return "Error";}
void pedirPiso(int p){
  if(estado!=REPOSO || paro() || p<1 || p>3)return;
  pisoDestino=p; objetivo=(p-1)*PASOS_PISO;
  if(objetivo>posicion)estado=SUBIENDO;
  else if(objetivo<posicion)estado=BAJANDO;
  Serial.printf("Solicitud: piso %d\n",p);
}
void leerBotones(){
  if(millis()-tBoton<35)return;
  tBoton=millis();
  const int pins[3]={PISO1_PIN,PISO2_PIN,PISO3_PIN};
  for(int i=0;i<3;i++){
    bool actual=digitalRead(pins[i])==LOW;
    if(actual && !btnPrev[i])pedirPiso(i+1);
    btnPrev[i]=actual;
  }
}
void motor(){
  if(estado!=SUBIENDO && estado!=BAJANDO){digitalWrite(STEP_PIN,LOW);digitalWrite(EN_PIN,HIGH);pulsoAlto=false;return;}
  digitalWrite(EN_PIN,LOW);
  digitalWrite(DIR_PIN,estado==SUBIENDO?HIGH:LOW);
  unsigned long ahora=micros();
  if(pulsoAlto){
    if(ahora-tPaso>=PULSO_ANCHO_US){
      digitalWrite(STEP_PIN,LOW); pulsoAlto=false;
      posicion+=(estado==SUBIENDO?1:-1);
      if(posicion==objetivo){pisoActual=pisoDestino;estado=REPOSO;digitalWrite(EN_PIN,HIGH);Serial.printf("Llegada: piso %d\n",pisoActual);}
    }
  }else if(ahora-tPaso>=PASO_INTERVALO_US){digitalWrite(STEP_PIN,HIGH);tPaso=ahora;pulsoAlto=true;}
}
void onMqtt(char* topic,byte* payload,unsigned int n){
  if(strcmp(topic,TOPICO_COMANDO)!=0 || n!=1 || estado!=REPOSO || paro())return;
  if(payload[0]>='1' && payload[0]<='3')pedirPiso(payload[0]-'0');
}
void telemetria(){
  char json[256];
  snprintf(json,sizeof(json),"{\"piso\":%d,\"destino\":%d,\"estado\":\"%s\",\"emergencia\":%s,\"posicion\":%ld,\"tiempo_ms\":%lu}",pisoActual,pisoDestino,nombreEstado(),paro()?"true":"false",posicion,(unsigned long)millis());
  Serial.println(json);
  if(mqtt.connected() && !mqtt.publish(TOPICO_ESTADO,json))Serial.println("AVISO: fallo al publicar MQTT");
}
void red(){
  if(WiFi.status()!=WL_CONNECTED){
    if(millis()-tWiFi>=5000){tWiFi=millis();Serial.println("WiFi: reconectando...");WiFi.begin(WIFI_SSID,"",6);}
    return;
  }
  if(!mqtt.connected()){
    if(millis()-tMQTT>=5000){
      tMQTT=millis();
      String id="Elevador-"+String((uint32_t)ESP.getEfuseMac(),HEX);
      if(mqtt.connect(id.c_str())){mqtt.subscribe(TOPICO_COMANDO);Serial.println("MQTT CONECTADO - suscrito y publicando");}
      else Serial.printf("MQTT SIN CONEXION rc=%d\n",mqtt.state());
    }
  }else mqtt.loop();
}
void setup(){
  Serial.begin(115200);
  pinMode(STEP_PIN,OUTPUT);pinMode(DIR_PIN,OUTPUT);pinMode(EN_PIN,OUTPUT);
  pinMode(PISO1_PIN,INPUT_PULLUP);pinMode(PISO2_PIN,INPUT_PULLUP);pinMode(PISO3_PIN,INPUT_PULLUP);
  pinMode(STOP_PIN,INPUT_PULLUP);pinMode(LED_VERDE,OUTPUT);pinMode(LED_ROJO,OUTPUT);
  digitalWrite(STEP_PIN,LOW);digitalWrite(EN_PIN,HIGH);
  paroPrevio=paro();if(paroPrevio)estado=EMERGENCIA;
  mqtt.setServer(BROKER,1883);mqtt.setCallback(onMqtt);mqtt.setBufferSize(512);mqtt.setSocketTimeout(1);
  WiFi.mode(WIFI_STA);WiFi.begin(WIFI_SSID,"",6);tWiFi=millis();
  Serial.println("ESP32 INICIADO | Monitor serial 115200 | Piso 1");
  Serial.printf("Topic: %s\n",TOPICO_ESTADO);
}
void loop(){
  bool p=paro();
  if(p && !paroPrevio){estado=EMERGENCIA;digitalWrite(STEP_PIN,LOW);digitalWrite(EN_PIN,HIGH);pulsoAlto=false;Serial.println("PARO ACTIVADO");}
  if(!p && paroPrevio){estado=BLOQUEADO;Serial.println("PARO LIBERADO: reiniciar desde piso 1");}
  paroPrevio=p;
  if(!p && (estado==REPOSO || estado==SUBIENDO || estado==BAJANDO)){leerBotones();motor();}
  else {digitalWrite(STEP_PIN,LOW);digitalWrite(EN_PIN,HIGH);}
  digitalWrite(LED_VERDE,estado==SUBIENDO||estado==BAJANDO);
  digitalWrite(LED_ROJO,estado==EMERGENCIA||estado==BLOQUEADO);
  red();
  if(millis()-tJSON>=500){tJSON=millis();telemetria();}
}
