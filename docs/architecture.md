# Arquitectura de Software

## Principio principal

Solo una tarea habla directamente con el módem BG95. Esto evita colisiones entre comandos AT.

## Tareas FreeRTOS

```txt
modem_task
  - Controla BG95
  - Inicializa red celular
  - Activa PDP
  - Conecta MQTT
  - Publica telemetría
  - Lee URCs MQTT
  - Maneja reconexión

telemetry_task
  - Espera evento MQTT_READY
  - Cada 60 segundos envía una solicitud de publicación a modem_task

led_task
  - Recibe solicitudes de cambio de color
  - Actualiza GPIO4, GPIO5 y GPIO12
```

## Máquina de estados lógica

```txt
BOOT
  ↓
MODEM_AT_READY
  ↓
SIM_READY
  ↓
NETWORK_REGISTERED
  ↓
PDP_ACTIVE
  ↓
MQTT_CONNECTED
  ↓
RUNNING
```

## Reconexión

Si falla una publicación o se recibe `+QMTSTAT` / `+QMTDISC`, el firmware:

1. Limpia el bit `MQTT_READY`.
2. Cambia LED a rojo.
3. Cierra sesión MQTT.
4. Reintenta conexión completa.
