ESP32-C6 + Quectel BG95-M3 - Examen Técnico Firmware IoT
1. Objetivo
Prototipo funcional desarrollado en C sobre ESP-IDF v5.5.4 para publicar telemetría por MQTT usando exclusivamente red celular mediante un módem Quectel BG95-M3 y un ESP32-C6-WROOM-1U.
El firmware valida SIM, registro celular, activación PDP, obtención de IP, conexión MQTT, publicación JSON periódica y control remoto de un LED RGB mediante comandos MQTT.
---
2. Stack tecnológico
Elemento	Selección
Lenguaje	C ANSI / C99
Framework	ESP-IDF v5.5.4
SDK	ESP-IDF oficial de Espressif
RTOS	FreeRTOS integrado en ESP-IDF
MCU	ESP32-C6-WROOM-1U
Módem	Quectel BG95-M3
Protocolo módem	AT commands por UART
Protocolo IoT	MQTT
Cliente de prueba	MQTTX Desktop
---
3. Hardware validado
Configuración final validada en hardware real:
```c
#define MODEM_TX_PIN         GPIO_NUM_8     // ESP32 TX -> BG95 RX
#define MODEM_RX_PIN         GPIO_NUM_7     // ESP32 RX <- BG95 TX
#define MODEM_PWRKEY_PIN     GPIO_NUM_15

#define LED_RED_PIN          GPIO_NUM_4
#define LED_GREEN_PIN        GPIO_NUM_5
#define LED_BLUE_PIN         GPIO_NUM_11
#define LED_ACTIVE_LEVEL     0              // LED RGB activo en bajo
```
Notas importantes:
La UART funcional real fue TX GPIO8 / RX GPIO7.
El PWRKEY funcional fue GPIO15.
El LED RGB resultó ser activo en bajo.
El LED azul funcional fue GPIO11, no GPIO12.
---
4. Programador FT2232 / ESP-Prog y VPROG
Para programar la PCB se utilizó un programador tipo FT2232 / ESP-Prog.
Consideración crítica
En esta PCB fue necesario retirar o no conectar el pin VPROG del programador.
Motivo:
La PCB ya cuenta con su propia alimentación/regulación.
Conectar VPROG puede provocar back-powering de la PCB.
Puede existir conflicto entre la fuente del programador y los reguladores de la tarjeta.
Puede introducir estados eléctricos indefinidos en el ESP32-C6 o en el BG95-M3.
Checklist antes de programar:
Confirmar GND común entre programador y PCB.
Verificar con multímetro el rail 3V3.
No conectar VPROG si la PCB ya está alimentada.
Confirmar TX/RX cruzados.
Verificar BOOT/EN/RESET.
Mantener contacto estable del header/pogo durante el flash.
---
5. Instalación ESP-IDF en Windows
Recomendado instalar en ruta corta:
```txt
C:\Espressif
```
Pasos:
Descargar e instalar ESP-IDF Tools Installer para Windows.
Seleccionar ESP-IDF v5.5.4.
Abrir ESP-IDF 5.5 PowerShell.
Entrar a la raíz del proyecto:
```powershell
cd C:\Users\alan_\Downloads\bg95_mqtt_exam\bg95_mqtt_exam
```
Configurar target:
```powershell
idf.py set-target esp32c6
```
Compilar:
```powershell
idf.py build
```
Flashear y monitorear:
```powershell
idf.py flash monitor
```
Si el puerto no se detecta automáticamente:
```powershell
idf.py -p COM3 flash monitor
```
---
6. APN celular
APN utilizado en la prueba:
```txt
APN: internet.itelcel.com
Usuario: webgprs
Password: webgprs2002
```
Comandos principales:
```txt
AT+CGDCONT=1,"IP","internet.itelcel.com"
AT+QICSGP=1,1,"internet.itelcel.com","webgprs","webgprs2002",1
AT+QIACT=1
AT+QIACT?
```
Resultado esperado:
```txt
+QIACT: 1,1,1,"<ip_asignada>"
```
---
7. MQTT
Broker público usado:
```txt
broker.hivemq.com:1883
```
Tópicos finales:
```txt
dispositivo/Alan Sandoval/data
dispositivo/Alan Sandoval/cmd
dispositivo/Alan Sandoval/status
```
Payload de telemetría publicado en `data`:
```json
{
  "device": "Alan Sandoval_esp32c6_bg95",
  "uptime": 17,
  "rssi": 26,
  "ip": "10.161.86.12",
  "led": "green",
  "free_heap": 428212,
  "saludo": "hola desde ESP32-C6 usando BG95-M3 por LTE-M"
}
```
---
8. Por qué se usó MQTTX
Se eligió MQTTX Desktop como herramienta de validación externa porque permite conectarse directamente por MQTT TCP al broker público en el puerto 1883, igual que el BG95-M3.
El cliente web público de HiveMQ usa WebSocket, no MQTT TCP directo. En algunas redes o navegadores puede fallar por:
bloqueo de WebSocket,
configuración de puerto incorrecta,
restricciones del navegador,
SSL/TLS,
proxy corporativo o firewall.
Por eso MQTTX fue más estable para validar firmware embebido.
Configuración MQTTX:
```txt
Name: Alan BG95 Test
Host: broker.hivemq.com
Port: 1883
Protocol: mqtt://
Client ID: test_alan_pc_123
Username: vacío
Password: vacío
SSL/TLS: desactivado
```
Suscripción recomendada:
```txt
dispositivo/Alan Sandoval/#
```
Publicar comandos en:
```txt
dispositivo/Alan Sandoval/cmd
```
Payloads soportados:
```json
{"led":"red"}
```
```json
{"led":"green"}
```
```json
{"led":"blue"}
```
```json
{"led":"off"}
```
```json
{"cmd":"status"}
```
---
9. Flujo del firmware
Inicializa UART, PWRKEY y LED RGB.
Verifica comunicación AT.
Si el módem no responde, aplica PWRKEY.
Verifica SIM con `AT+CPIN?`.
Espera registro LTE con `AT+CEREG?`.
Activa PDP con APN.
Obtiene IP con `AT+QIACT?`.
Abre MQTT con `AT+QMTOPEN`.
Conecta con `AT+QMTCONN`.
Se suscribe con `AT+QMTSUB`.
Publica estado y telemetría con `AT+QMTPUB`.
Procesa comandos MQTT y actualiza LED RGB.
Si hay desconexión, entra a rutina de reconexión.
---
10. Máquina de estados recomendada
La solución se puede describir formalmente como:
```txt
MODEM_OFF
MODEM_BOOTING
AT_READY
SIM_READY
NETWORK_SEARCHING
NETWORK_REGISTERED
PDP_ACTIVE
MQTT_CONNECTING
MQTT_CONNECTED
RUNNING
RECOVERY
```
Criterios de recuperación:
Si falla MQTT, reabrir conexión MQTT.
Si falla PDP, ejecutar `QIDEACT` + `QIACT`.
Si falla registro, volver a esperar `CEREG`.
Si falla AT, aplicar PWRKEY y reinicializar.
---
11. Matriz de pruebas
Prueba	Comando / Acción	Resultado esperado	Estado
AT básico	`AT`	`OK`	Pass
Modelo	`ATI`	`BG95-M3`	Pass
SIM	`AT+CPIN?`	`READY`	Pass
Señal	`AT+CSQ`	RSSI válido	Pass
Registro	`AT+CEREG?`	stat 1	Pass
Datos	`AT+CGATT?`	1	Pass
IP	`AT+QIACT?`	IP asignada	Pass
MQTT open	`QMTOPEN`	`0,0`	Pass
MQTT connect	`QMTCONN`	`0,0,0`	Pass
MQTT subscribe	`QMTSUB`	result 0	Pass
MQTT publish	`QMTPUB`	result 0	Pass
LED red	MQTT payload	LED rojo	Pass
LED green	MQTT payload	LED verde	Pass
LED blue	MQTT payload	LED azul	Pass
LED off	MQTT payload	LED apagado	Pass
---
12. Discrepancias encontradas
Discrepancia	Impacto	Solución
Pin físico vs GPIO lógico	Pines iniciales incorrectos	Validar GPIO real en ESP-IDF
UART interpretada al revés	Sin respuesta AT	TX GPIO8 / RX GPIO7
PWRKEY documentado ambiguamente	Arranque inconsistente	GPIO15 + secuencia validada
LEDs STA/NET no confiables	Diagnóstico falso	Usar respuestas AT como fuente de verdad
LED RGB activo en bajo	Colores invertidos	`LED_ACTIVE_LEVEL = 0`
Blue LED documentado diferente	Color azul incorrecto	GPIO11
HiveMQ web client intermitente	No conectaba desde navegador	Usar MQTTX Desktop
---
13. Evidencia
Video de funcionamiento en tiempo real:
```txt
Repositorio Gmail / Google Drive: [PEGAR_LINK_AQUI]
```
Repositorio de código:
```txt
GitHub: [PEGAR_LINK_AQUI]
```
La evidencia debe mostrar:
Monitor serial ESP-IDF.
Registro celular.
IP por QIACT.
MQTTX conectado.
Payloads enviados.
LED RGB cambiando por comandos MQTT.
---
14. Fuentes útiles
ESP-IDF Windows setup: https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32c6/get-started/windows-setup.html
ESP-IDF GitHub: https://github.com/espressif/esp-idf
MQTTX: https://mqttx.app/
HiveMQ public broker: https://www.mqtt-dashboard.com/
HiveMQ WebSocket client: https://github.com/hivemq/hivemq-mqtt-web-client