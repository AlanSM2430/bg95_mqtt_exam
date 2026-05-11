# Decisiones Técnicas

## ESP-IDF

El firmware se desarrolló en ESP-IDF y no en Arduino para cumplir el requisito del examen y permitir una arquitectura basada en FreeRTOS, colas y separación por componentes.

## UART validada

Durante el bring-up se validó que la UART funcional para el BG95-M3 es:

```txt
ESP32 TX = GPIO8
ESP32 RX = GPIO7
Baudrate = 115200
```

## PWRKEY

Se validó que el PWRKEY funcional queda controlado desde:

```txt
GPIO15
```

Secuencia utilizada:

```txt
HIGH -> LOW 800 ms -> HIGH
```

## Red celular

El módem se registra en LTE-M usando:

```txt
AT+CEREG?
AT+CGATT?
```

Luego se activa PDP con:

```txt
AT+CGDCONT=1,"IP","internet.itelcel.com"
AT+QICSGP=1,1,"internet.itelcel.com","webgprs","webgprs2002",1
AT+QIACT=1
AT+QIACT?
```

## MQTT por comandos Quectel

El firmware usa la pila MQTT interna del BG95:

```txt
AT+QMTOPEN
AT+QMTCONN
AT+QMTSUB
AT+QMTPUB
```

La publicación usa modo prompt con Ctrl+Z para finalizar payload.

## Robustez

El firmware incluye:

- Reintentos AT.
- Reintento PWRKEY si el módem no responde.
- Espera de registro celular.
- Activación de PDP con validación de IP.
- Reconexión MQTT ante falla.
- Separación de responsabilidades por componentes.
