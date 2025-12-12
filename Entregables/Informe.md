# PROYECTO FINAL: SISTEMA DE CONTROL DE SALA

**Materia:** Estructuras Computacionales  
**Docente:** Samuel Andrés Cifuentes Muñoz  
**Integrantes:** María de los Ángeles Prieto Ortega – Mariana Zuluaga Yepes  
**Diciembre de 2025**


## MATERIALES

- Microcontrolador STM32L476RG  
- Pantalla OLED SSD1306 (128×64)  
- Teclado matricial 4×4  
- Módulo WiFi ESP-01  
- Sensor de temperatura DHT11  
- Resistencias  
- Jumpers  


## OBJETIVO

Diseñar un sistema capaz de controlar el acceso a una habitación mediante contraseña, visualizar información en pantalla, supervisar la temperatura y activar un ventilador de forma automática o manual. Además, se incluye un modo de emergencia y envío de alertas mediante el módulo ESP-01.


## PROCEDIMIENTO

### 1. Inicialización general

En la función `room_control_init()` se configuran:

- Estado inicial: **LOCKED**  
- Contraseña por defecto: **2222**  
- Limpieza del buffer de ingreso  
- Configuración del pin de puerta  
- PWM del ventilador en 0%  


### 2. Funcionamiento por estados (Máquina de estados)


### **Estado 1: ROOM_STATE_LOCKED (Bloqueado)**

- Pantalla: **“SISTEMA BLOQUEADO”**
- Puerta cerrada
- Si se presiona un número → pasa a ingresar contraseña


### **Estado 2: ROOM_STATE_INPUT_PASSWORD (Ingreso de contraseña)**

- Pantalla: “**CLAVE**” + asteriscos  
- **C** borra dígitos  
- Se evalúan 4 dígitos:

  - Correcta → **UNLOCKED**  
  - Incorrecta → **ACCESS_DENIED**  

- Inactividad 10 segundos → vuelve a LOCKED


### **Estado 3: ROOM_STATE_UNLOCKED (Acceso concedido)**

Pantalla muestra:

- **ACCESO OK**  
- Temperatura  
- Nivel del ventilador  
- Modo (MANUAL o AUTO)

Acciones:

- Cambiar modo del ventilador  
- Control manual (0–3)  
- Activar modo AUTO con **A**  
- Bloquear con **B**  
- Emergencia con **D**

Puerta abierta.


### **Estado 4: ROOM_STATE_ACCESS_DENIED (Acceso denegado)**

- Muestra: “**ACCESO DENEGADO**”  
- Envía al ESP-01: `"ALERTA: Acceso denegado"`  
- Luego de 3 segundos vuelve a LOCKED


### **Estado 5: ROOM_STATE_EMERGENCY (Emergencia)**

- Pantalla: “**EMERGENCIA**” y “**SALGA**”  
- Puerta se abre  
- Ventilador al 100%  
- Envía: `"ALERTA: Emergencia activada"`  
- Salida con tecla **#**


## 3. Principales funciones del código

### 'room_control_init()'

-Configura los valores iniciales.

-Deja el sistema en estado bloqueado.

-Enciende PWM con valor inicial 0%.

### 'room_control_update()'

-Revisa el estado actual y ejecuta acciones:
 bloquear puerta, mostrar mensajes, manejar tiempo de espera.
 
-Luego actualiza el display, el ventilador y la puerta.


### 'room_control_process_key()'

-Reacciona inmediatamente cuando se presiona una tecla.

-Según el estado ejecuta acciones específicas:
Agregar dígitos, borrar, cambiar modo ventilador, activar emergencia.

### 'room_control_update_fan()'

-Traduce el nivel del ventilador a un valor PWM real.

### 'room_control_update_door()'

-Activa o desactiva un pin para abrir/cerrar.

### 'room_control_update_display()'

-Muestra el estado correspondiente en la pantalla OLED.

### 'room_control_calculate_fan_level()'

-Niveles automáticos según temperatura:
OFF,  BAJO, MEDIO, ALTO.


## 4. Funcionamiento del teclado

| Tecla | Acción |
|-------|--------|
| 0–9 | Digitar clave / controlar ventilador |
| A | Modo automático |
| B | Bloquear sistema |
| C | Borrar |
| D | Emergencia |
| 0–3 | Niveles manuales de ventilador |
| # | Salir de emergencia |

**Pines:**

- Filas (Outputs): PC0–PC3  
- Columnas (Inputs Pull-up): PC4–PC7


## 5. Control del ventilador

### Automático

- Menor que 25°C → OFF  
- Entre 25–28°C → BAJO  
- Entre 28–31°C → MEDIO  
- Mayor que 31°C → ALTO  

### Manual

- 0 → OFF  
- 1 → BAJO  
- 2 → MEDIO  
- 3 → ALTO  

**Pin:** PA6 (TIM3_CH1)  
**Duty cycle:** 0–99


## 6. Control de la puerta

Se abre en:

- UNLOCKED  
- EMERGENCY  

Se cierra en:

- LOCKED  
- ACCESS_DENIED  

**Pin:** PA4 – GPIO Output  
- LOW → cerrada  
- HIGH → abierta


## 7. Pantalla OLED SSD1306

- Actualización solo si `display_update_needed == true`  
- Pines: PB6 (SCL), PB7 (SDA)


## Comunicación con ESP-01

Mensajes UART:

- `"ALERTA: Acceso denegado\r\n"`  
- `"ALERTA: Emergencia activada\r\n"`

Pines:

- PB10 – USART3_TX  
- PB11 – USART3_RX  


## Optimizaciones

- PWM inicializado una única vez  
- Actualización de pantalla solo cuando cambia algo  
- Máquina de estados modular y escalable  
- Control eficiente por temperatura  


## Resultados

- Figura 1: Circuito montado

![WhatsApp Image 2025-12-11 at 11.16.13 AM](WhatsApp%20Image%202025-12-11%20at%2011.16.13%20AM.jpeg)

- Figura 2: Diagrama

![WhatsApp Image 2025-12-11 at 9.57.18 AM](WhatsApp%20Image%202025-12-11%20at%209.57.18%20AM.jpeg)

- Figura 3: Sistema funcionando

 ![WhatsApp Image 2025-12-11 at 11.47.22 AM](WhatsApp%20Image%202025-12-11%20at%2011.47.22%20AM.jpeg)



## Observaciones

- El diseño modular permitió depuración sencilla  
- Máquina de estados mejoró la estabilidad  
- Teclado presentó ruido → solucionado con debounce  
- Modo manual del ventilador puede activarse accidentalmente  
- Emergencia funciona, pero sería útil pedir confirmación  
- El teclado es crítico para todo el sistema

## Nota

Durante el desarrollo del sistema, se presentaron dificultades al intentar implementar la comunicación inalámbrica mediante el módulo ESP-01. Estos inconvenientes afectaron temporalmente la integración de los datos y la transmisión de alertas, aunque no comprometieron el funcionamiento general del control de la habitación.


## Conclusiones

- El STM32 controló todos los módulos de forma estable  
- La pantalla permitió interacción clara con el usuario  
- Ventilador respondió bien en modo manual y automático  
- Modo emergencia resultó esencial  
- Sistema cumple objetivos y es ampliable en el futuro





