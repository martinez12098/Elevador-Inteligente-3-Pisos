# Elevador inteligente de 3 pisos

**Materia:** Automatización y Robótica  
**Integrantes:** Jesús Eduardo Martínez Tinajero y Espindola Juarez Doraly

## Descripción

Proyecto de un elevador de tres pisos simulado con ESP32 y un motor paso a paso controlado mediante A4988 en Wokwi. El ESP32 publica telemetría en formato JSON por MQTT; Node-RED permite consultar el piso, el destino, el estado y la posición del motor.

## Archivos

- `sketch.ino`: programa del ESP32 con recorrido automático, botones, indicadores, paro simulado y MQTT.
- `diagram.json`: conexiones y componentes del simulador.
- `libraries.txt`: biblioteca PubSubClient.
- `Elevador.json`: flujo de control y dashboard en Node-RED.
- `Registro.json`: registro y descarga de CSV desde Node-RED.

## Uso

1. Abrir un proyecto ESP32 en Wokwi y colocar `sketch.ino` y `diagram.json`.
2. Añadir `PubSubClient` en Library Manager (o usar `libraries.txt`).
3. En Windows ejecutar `node-red` y abrir `http://127.0.0.1:1880`.
4. Importar los flujos `Elevador.json` y `Registro.json` y pulsar **Deploy**. Evitar duplicar un flujo anterior si ya está instalado.
5. Iniciar la simulación de Wokwi y abrir `http://127.0.0.1:1880/dashboard/elevador`.

El programa utiliza `broker.emqx.io:1883` y los tópicos:

- Estado: `elevador/isc2026/equipo01/estado`
- Comando: `elevador/isc2026/equipo01/comando`

El broker es público; para otro equipo conviene definir un tópico único tanto en el ESP32 como en Node-RED.

## Prueba de funcionamiento

El firmware contempla recorridos automáticos por los pisos 1, 2 y 3, con envío de telemetría por MQTT. El tiempo virtual acelerado se utiliza para demostrar la lógica del sistema; **30 minutos virtuales no equivalen a 30 minutos de operación continua real**.

Resultados observados en las capturas del proyecto: 30.00 minutos virtuales, 8.61 minutos reales, 70 muestras y 5 intervalos de recepción mayores de 2 segundos. Estos datos corresponden al ensayo documentado, no a una validación nueva de este paquete.

## Seguridad

El paro de emergencia es una lógica simulada del ESP32. No sustituye a un interruptor de seguridad físico ni al corte de potencia de un elevador real. El esquema de Wokwi tampoco demuestra aislamiento galvánico entre control y potencia.
