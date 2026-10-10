/*
  ELEVADOR INTELIGENTE - ENSAYO AUTOMATICO ACELERADO
  Autores: Jesus Eduardo Martinez Tinajero y Espindola Juarez Doraly
  Wokwi ESP32 + A4988 + Node-RED via MQTT. Sin delay().
  NOTA: 30 min VIRTUALES no equivalen a 30 min reales de estres.
*/
#include <WiFi.h>
#include <PubSubClient.h>
#include <string.h>

const int PIN_STEP=18, PIN_DIR=19, PIN_EN=26;
const int BTN_1=32, BTN_2=33, BTN_3=25;
const int PIN_STOP=27, LED_VERDE=16, LED_ROJO=17;
const char* WIFI_SSID="Wokwi-GUEST";
const char* MQTT_HOST="broker.emqx.io";
const char* TOPIC_ESTADO="elevador/isc2026/equipo01/estado";
const char* TOPIC_COMANDO="elevador/isc2026/equipo01/comando";
WiFiClient net;
PubSubClient mqtt(net);

// Tiempo escalado: 30 min VIRTUALES = 60 s del reloj interno ESP32.
// Si deseas prueba REAL de 30 min, cambia FACTOR_TIEMPO a 1.
const unsigned long FACTOR_TIEMPO=30;
const unsigned long DURACION_VIRTUAL_MS=1800000UL;
const unsigned long PERIODO_TELEMETRIA_MS=500;
const unsigned long DESCANSO_ENTRE_PISOS_MS=350;
const unsigned long PERIODO_PASO_US=3000;
const unsigned long ANCHO_PULSO_US=10;
const long PASOS_POR_PISO=200;

enum Estado { REPOSO, SUBIENDO, BAJANDO, EMERGENCIA, BLOQUEADO, FINALIZADO };
Estado estado=REPOSO;
long posicion=0, meta=0;
int pisoActual=1, pisoDestino=1;
const int ruta[]={2,3,1};
int indiceRuta=0;
unsigned long inicio=0, ultimoReporte=0, ultimoBoton=0, ultimoWifi=0, ultimoMQTT=0;
unsigned long ultimoPaso=0, llegada=0;
bool pulsoAlto=false, paroAnterior=false;
bool prev1=false,prev2=false,prev3=false;
unsigned long recorridos=0;

bool hayParo(){ return digitalRead(PIN_STOP)==HIGH; }
const char* nombreEstado(){
  switch(estado){
    case REPOSO: return "Reposo";
    case SUBIENDO: return "Subiendo";
    case BAJANDO: return "Bajando";
    case EMERGENCIA: return "Emergencia";
    case BLOQUEADO: return "Bloqueado";
    case FINALIZADO: return "Finalizado";
  }
  return "Desconocido";
}
void deshabilitarMotor(){ digitalWrite(PIN_STEP,LOW); digitalWrite(PIN_EN,HIGH); pulsoAlto=false; }
void solicitarPiso(int piso){
  if(estado!=REPOSO || hayParo() || piso<1 || piso>3)return;
  pisoDestino=piso;
  meta=(piso-1)*PASOS_POR_PISO;
  if(meta>posicion)estado=SUBIENDO;
  else if(meta<posicion)estado=BAJANDO;
  Serial.printf("Solicitud automatica piso %d\n",piso);
}
void leerBotones(){
  if(millis()-ultimoBoton<40)return;
  ultimoBoton=millis();
  bool a=digitalRead(BTN_1)==LOW, b=digitalRead(BTN_2)==LOW, c=digitalRead(BTN_3)==LOW;
  if(a&&!prev1)solicitarPiso(1);
  else if(b&&!prev2)solicitarPiso(2);
  else if(c&&!prev3)solicitarPiso(3);
  prev1=a;prev2=b;prev3=c;
}
void mover(){
  if(estado!=SUBIENDO && estado!=BAJANDO){deshabilitarMotor();return;}
  digitalWrite(PIN_EN,LOW);
  digitalWrite(PIN_DIR,estado==SUBIENDO?HIGH:LOW);
  unsigned long ahora=micros();
  if(pulsoAlto){
    if(ahora-ultimoPaso>=ANCHO_PULSO_US){
      digitalWrite(PIN_STEP,LOW);
      pulsoAlto=false;
      posicion+=(estado==SUBIENDO?1:-1);
      if(posicion==meta){
        pisoActual=pisoDestino;
        estado=REPOSO;
        recorridos++;
        llegada=millis();
        deshabilitarMotor();
        Serial.printf("Llegada piso %d, movimientos %lu\n",pisoActual,recorridos);
      }
    }
  }else if(ahora-ultimoPaso>=PERIODO_PASO_US){
    digitalWrite(PIN_STEP,HIGH);
    ultimoPaso=ahora;
    pulsoAlto=true;
  }
}
void mensajeMQTT(char* topic,byte* payload,unsigned int n){
  if(strcmp(topic,TOPIC_COMANDO)!=0 || n!=1)return;
  if(estado!=REPOSO)return;
  if(payload[0]>='1' && payload[0]<='3')solicitarPiso(payload[0]-'0');
}
void publicar(){
  unsigned long transcurrido=millis()-inicio;
  unsigned long virtualMs=(transcurrido>=DURACION_VIRTUAL_MS/FACTOR_TIEMPO)
    ? DURACION_VIRTUAL_MS : transcurrido*FACTOR_TIEMPO;
  char json[320];
  snprintf(json,sizeof(json),
    "{\"piso\":%d,\"destino\":%d,\"estado\":\"%s\",\"emergencia\":%s,"
    "\"posicion\":%ld,\"tiempo_ms\":%lu,\"modo\":\"Wokwi_acelerado\","
    "\"minutos_virtuales\":%.2f,\"segundos_reales_simulador\":%lu,\"recorridos\":%lu}",
    pisoActual,pisoDestino,nombreEstado(),hayParo()?"true":"false",posicion,
    (unsigned long)millis(),virtualMs/60000.0,(unsigned long)(transcurrido/1000),recorridos);
  Serial.println(json);
  if(mqtt.connected())mqtt.publish(TOPIC_ESTADO,json);
}
void conexiones(){
  if(WiFi.status()!=WL_CONNECTED){
    if(millis()-ultimoWifi>=5000){ultimoWifi=millis();WiFi.begin(WIFI_SSID,"",6);}
    return;
  }
  if(!mqtt.connected()){
    if(millis()-ultimoMQTT>=5000){
      ultimoMQTT=millis();
      String id="auto-elevador-"+String((uint32_t)ESP.getEfuseMac(),HEX);
      if(mqtt.connect(id.c_str())){
        mqtt.subscribe(TOPIC_COMANDO);
        Serial.println("MQTT CONECTADO");
      } else Serial.printf("MQTT error %d\n",mqtt.state());
    }
  }else mqtt.loop();
}
void setup(){
  Serial.begin(115200);
  pinMode(PIN_STEP,OUTPUT);pinMode(PIN_DIR,OUTPUT);pinMode(PIN_EN,OUTPUT);
  pinMode(BTN_1,INPUT_PULLUP);pinMode(BTN_2,INPUT_PULLUP);pinMode(BTN_3,INPUT_PULLUP);
  pinMode(PIN_STOP,INPUT_PULLUP);pinMode(LED_VERDE,OUTPUT);pinMode(LED_ROJO,OUTPUT);
  deshabilitarMotor();
  paroAnterior=hayParo();
  if(paroAnterior)estado=EMERGENCIA;
  WiFi.mode(WIFI_STA);WiFi.begin(WIFI_SSID,"",6);
  mqtt.setServer(MQTT_HOST,1883);
  mqtt.setCallback(mensajeMQTT);
  mqtt.setBufferSize(512);
  mqtt.setSocketTimeout(1);
  inicio=millis();llegada=inicio;
  Serial.println("INICIO: Wokwi ejecuta movimientos REALES de simulador; reloj de prueba virtual x30");
}
void loop(){
  bool paro=hayParo();
  if(paro && !paroAnterior){estado=EMERGENCIA;deshabilitarMotor();Serial.println("EMERGENCIA");}
  if(!paro && paroAnterior){estado=BLOQUEADO;deshabilitarMotor();Serial.println("BLOQUEADO: reiniciar desde piso 1");}
  paroAnterior=paro;
  // Revisar primero seguridad y movimiento, luego comunicaciones.
  if(!paro && (estado==REPOSO || estado==SUBIENDO || estado==BAJANDO)){
    leerBotones();
    mover();
    unsigned long duracionReal=DURACION_VIRTUAL_MS/FACTOR_TIEMPO;
    if(millis()-inicio>=duracionReal && estado==REPOSO){
      estado=FINALIZADO;
      deshabilitarMotor();
      Serial.printf("FIN: 30 minutos VIRTUALES, %lu recorridos\n",recorridos);
    }else if(estado==REPOSO && millis()-llegada>=DESCANSO_ENTRE_PISOS_MS
             && millis()-inicio<duracionReal){
      solicitarPiso(ruta[indiceRuta]);
      indiceRuta=(indiceRuta+1)%3;
    }
  } else deshabilitarMotor();
  digitalWrite(LED_VERDE,estado==SUBIENDO || estado==BAJANDO);
  digitalWrite(LED_ROJO,estado==EMERGENCIA || estado==BLOQUEADO);
  conexiones();
  if(millis()-ultimoReporte>=PERIODO_TELEMETRIA_MS){ultimoReporte=millis();publicar();}
}
