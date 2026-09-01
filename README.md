# Tuya OEM Config / Flash Scanner (herramienta)

> Rama de herramienta, separada del firmware de conversión. **No se mezcla con lo
> que va al upstream.** El valor está en el estado final del escáner + este README.

## Propósito

Firmware de diagnóstico para leer la configuración de fábrica Tuya (mapeo de pines
`rl1_pin` / `bt1_pin` / `netled1_pin` …, bloque OEM) desde la flash de un módulo
Telink TLSR8258 **antes** de convertirlo, para deducir el `config_str` correcto de
ese modelo. NO forma parte del firmware de producción.

## Uso paso a paso

1. Flashear este firmware en el módulo (OTA custom→custom, o por cable).
2. En Zigbee2MQTT, leer los atributos `genBasic` de volcado OEM:
   - `0xFF03` = cabecera / chunk 0.
   - `0xFF04`..`0xFF0A` = chunks de 64 B (v6).
   - Read por `/set {"read":{"cluster":"genBasic","attributes":[65283, 65284, …]}}`;
     el resultado sale en el log de z2m como `zhc:tz: Read result`.
3. Concatenar los chunks y decodificar el bloque OEM → pines y configuración.
4. Traducir los pines a GPIO con la tabla de abajo y construir el `config_str`.

## Numeración de pines Tuya → GPIO (TLSR8258, encapsulado de 32 pines)

| Tuya | GPIO | Tuya | GPIO | Tuya | GPIO |
|------|------|------|------|------|------|
| 0  | A0 | 6  | B6 | 12 | C4 |
| 1  | A1 | 7  | B7 | 13 | D2 |
| 2  | A7 | 8  | C0 | 14 | D3 |
| 3  | B1 | 9  | C1 | 15 | D4 |
| 4  | B4 | 10 | C2 | 16 | D7 |
| 5  | B5 | 11 | C3 |    |    |

> **⚠️ Tabla deducida, no oficial.** Verificada con **seis puntos de un único módulo
> TLSR8258 de 32 pines** (cruzando el bloque OEM de fábrica de un Aubess PM:
> `rl1_pin:5→RB5`, `bt1_pin:12→SC4`, `netled1_pin:13→ID2i`, …). En **otros
> encapsulados** (48 pines, variantes) la numeración **puede variar**. Contrástala
> siempre con pines conocidos del propio device (p. ej. el relé o el LED de red, que
> puedes verificar físicamente) antes de fiarte de ella para pines no comprobados.

## Límite de 64 B por atributo CHAR_STR

Telink **no fragmenta** las Read Response ZCL: el presupuesto útil de string en un
frame único es ~74 B, y en la práctica un `CHAR_STR` de 118 B da **timeout** (silencio
total). Por eso el volcado va en chunks de **≤64 B**, con la cabecera en su propio
atributo (`0xFF03`) y los bloques de datos en `0xFF04`..`0xFF0A` (evolución v5→v6).

## Truco del ancla ofuscada

Al escanear la flash buscando un literal (p. ej. `rl1_pin`), ese mismo literal aparece
también en el `.rodata` del propio firmware → **falsos positivos**. Se guarda el
literal **ofuscado** (cada byte +1) y se decodifica a RAM justo antes de buscar; se
registran varios hits y se elige el de **dirección más alta** (≥ zona de datos OEM,
`>= 0xF0000`), no el del código.

## ⚠️ Aviso de seguridad

Esta herramienta **EXPONE CONTENIDO DE LA FLASH por Zigbee** — incluida posible
información sensible (claves de red, material de emparejamiento). Es solo para
diagnóstico puntual en banco.

- **NO es para uso en producción.**
- **NO dejar este firmware en dispositivos que permanezcan en la red Zigbee:**
  cualquier nodo con acceso podría leer el volcado. Úsalo aislado y, al terminar,
  **reflashea firmware normal** y saca el módulo de cualquier red operativa.
